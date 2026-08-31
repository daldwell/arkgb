#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <atomic>
#include <dirent.h>
#include <string.h>
#include "Window.h"
#include "Debugger.h"
#include "GUnit.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl2.h"
#include "imgui/imgui_impl_sdlrenderer2.h"

// Global window object
Window gwindow;

// Audio step size
double audioStepSize = 87;

//Screen dimension constants
const int GAME_WIDTH = 320;
const int DEBUG_WIDTH  = 400;
const int SCREEN_HEIGHT = 288;
const int WINDOW_HEIGHT  = SCREEN_HEIGHT + 20; // 308 pixels total
const int SCREEN_FPS = 60;
const int SCREEN_TICKS_PER_FRAME = 1000 / SCREEN_FPS;

// The gameboy tile map is 256x256 pixels, but this is truncated into the gameboy's display size of 160x144.
// A generously sized canvas buffer is used to render the full 256x256 frame which is clipped for the smaller 160x144 display.
// Use a starting offset for drawing the canvas, which negates the need to check if a pixel xy coordinate is less than 0 (i.e. outside the pixel buffer).
// Anything that falls outside the screen buffer will be clipped when the canvas is blitted into the gameboy display.
const int CANVAS_WIDTH = 512;
const int CANVAS_HEIGHT = 512;
const int CANVAS_XOFFSET = 0;
const int CANVAS_YOFFSET = 0;

Window::Window()
{
    //Initialize SDL
    if( SDL_Init( SDL_INIT_VIDEO ) < 0 )
    {
        printf( "SDL could not initialize! SDL_Error: %s\n", SDL_GetError() );
        exit(1);
    }

    //Create display window
    window = SDL_CreateWindow( "ArkGB", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, GAME_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN );
    if( window == NULL )
    {
        printf( "Window could not be created! SDL_Error: %s\n", SDL_GetError() );
        exit(1);
    }

    //Create canvas surface
    surface = SDL_CreateRGBSurface(0, CANVAS_WIDTH, CANVAS_HEIGHT, 32, 0, 0, 0, 0);
    accelerated_renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    
    // Initialize Dear ImGui Context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    // Connect ImGui to your modern SDL Window and Accelerated Renderer
    ImGui_ImplSDL2_InitForSDLRenderer(window, accelerated_renderer);
    ImGui_ImplSDLRenderer2_Init(accelerated_renderer);
    
    texture = SDL_CreateTexture(
        accelerated_renderer, 
        SDL_PIXELFORMAT_ARGB8888, // Use your surface's native format
        SDL_TEXTUREACCESS_STREAMING, // Streaming mode is highly optimized for frame updates
        160, 144 // The Game Boy's native resolution
    );
    running = true;
}

Window::~Window()
{
    //dumpTraceLines();
    GUShutdown();

    //Destroy window
    SDL_DestroyWindow( window );
    SDL_DestroyRenderer(accelerated_renderer);
    SDL_DestroyTexture (texture);

    //Quit SDL subsystems
    SDL_Quit();
}

void Window::SetTitle(const char * title)
{
    SDL_SetWindowTitle(window, title);
}

void Window::EventDispatch()
{
    //Event dispatcher
    SDL_Event e; 
    while( SDL_PollEvent( &e ) )
    { 
        // Let ImGui process mouse/keyboard input first
        ImGui_ImplSDL2_ProcessEvent(&e);

        if( e.type == SDL_QUIT ) 
        {
            running = false;
        }

        // Dispatch to GB unit
        GUDispatchEvent( &e );
    } 
}

void Window::ClearSurface()
{
    //Fill the surface white
    SDL_FillRect( surface, NULL, SDL_MapRGB( surface->format, 0xFF, 0xFF, 0xFF ) );
}

int Window::GetRGB(byte r, byte g, byte b) {
    return SDL_MapRGB( surface->format, r, g, b );
}

void Window::DrawPixel(int x, int y, int c)
{
    SDL_LockSurface(surface);
    Uint32 *buffer = (Uint32*) surface->pixels;
    buffer[((x+CANVAS_XOFFSET)) + ((y+CANVAS_YOFFSET) * surface->w)] = c;
    SDL_UnlockSurface(surface);
}

void Window::RefreshWindow()
{
    // Construst the user interface for this frame
    BuildUI();
    
    // SDL RENDERING
	SDL_Rect srcRect;
	srcRect.x = CANVAS_XOFFSET;
	srcRect.y = CANVAS_YOFFSET;
	srcRect.w = 160;
	srcRect.h = 144;

    SDL_Rect destRect;
    destRect.x = 0;
    destRect.y = 20;
	destRect.w = GAME_WIDTH;
	destRect.h = SCREEN_HEIGHT;
	
    // Update the existing texture with your surface's fresh pixel data
    // (Replace 'surface->pixels' and 'surface->pitch' with your source surface variables)
    SDL_UpdateTexture(texture, NULL, surface->pixels, surface->pitch);

    // Clear the screen renderer
    SDL_RenderClear(accelerated_renderer);

    // Copy the updated texture to the screen (handles scaling automatically if destRect is larger)
    SDL_RenderCopy(accelerated_renderer, texture, &srcRect, &destRect);

    // Render ImGui on top of your gameplay graphics
    ImGui::Render();
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());

    // 4. Swap the buffers smoothly via VSync
    SDL_RenderPresent(accelerated_renderer);

    // DYNAMIC AUDIO RESAMPLING
    // Calculate the distance between the cursors
    int itemsInBuffer = (bufferWriteCursor.load(std::memory_order_relaxed) - bufferReadCursor.load(std::memory_order_relaxed) + RING_BUF_SIZE) % RING_BUF_SIZE;

    const float BASELINE_STEP = 87.381333f; // 4,194,304 Hz / 48,000 Hz
    const int TARGET_CUSHION = 2048;        // 1024 stereo samples

    // Calculate error (positive means too full, negative means starving)
    int error = itemsInBuffer - TARGET_CUSHION;

    // Proportional adjustment factor (Tweak this value to adjust sensitivity)
    // A value of 0.0005f means if you are 500 items short, the step increases by ~0.25
    const float Kp = 0.0005f; 

    // Adjust the step size lineally: 
    // If error is negative (starving), audioStepSize INCREASES.
    // This reduces the number of samples produced per frame to perfectly match your monitor's VSync delay.
    audioStepSize = BASELINE_STEP + (error * Kp);

    // Clamp the step size so audio pitch doesn't warp noticeably
    if (audioStepSize < 87.1f) audioStepSize = 87.1f;
    if (audioStepSize > 87.8f) audioStepSize = 87.8f;

    // EVENT DISPATCH
    EventDispatch();
}

 void Window::BuildUI()
 {
    // 1. Start the ImGui Frame pipeline
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    // 1. Initialize the Main Top Menu Bar
    static bool showDebugger = false;
    bool triggerRomPopup = false;
    if (ImGui::BeginMainMenuBar()) {
        
        // Add a standard "File" dropdown section
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open ROM...", "Ctrl+O")) {
                triggerRomPopup = true; 
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                running = false; // Kill it
            }
            ImGui::EndMenu();
        }

        // Add an "Emulation" control section
        if (ImGui::BeginMenu("Emulation")) {
            if (ImGui::MenuItem("Reset Game")) {
                GUReset();
            }
            ImGui::EndMenu();
        }

        // Add debugging tools section
        if (ImGui::BeginMenu("Tools")) {
            ImGui::MenuItem("Show Debugger Panel", NULL, &showDebugger);
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    if (triggerRomPopup) {
        ImGui::OpenPopup("ROM Loader");
    }

    static char selectedRomPath[256] = "";
    static char localRoms[100][256];
    static int romCount = -1; // -1 means "we need to scan the folder"

    // Check if the modal has been flagged to open
    if (ImGui::BeginPopupModal("ROM Loader", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        
        // Scan the directory for roms
        if (romCount == -1) {
            romCount = 0; // Reset counter to begin filling our array rows
            
            DIR* dir = opendir("."); // Open the current local execution directory (".")
            if (dir != NULL) {
                struct dirent* entry;
                
                // Read every single file in the folder one by one
                while ((entry = readdir(dir)) != NULL) {
                    // Look for where the file extension period starts (searching from right to left)
                    char* ext = strrchr(entry->d_name, '.');
                    
                    if (ext != NULL) {
                        // Check if the file string matches our target Game Boy formats
                        if (strcmp(ext, ".gb") == 0 || strcmp(ext, ".gbc") == 0) {
                            if (romCount < 100) { // Protect our fixed stack boundaries
                                strncpy(localRoms[romCount], entry->d_name, 255);
                                localRoms[romCount][255] = '\0'; // Enforce safe null termination
                                romCount++;
                            }
                        }
                    }
                }
                closedir(dir); // Close the stream pointer handle cleanly
            }
        }

        ImGui::Text("Select a ROM from the current directory:");
        ImGui::Spacing();

        // File list fame
        if (ImGui::BeginChild("RomListChild", ImVec2(300, 150), ImGuiChildFlags_Border)) {
            
            if (romCount == 0) {
                ImGui::TextDisabled("No .gb or .gbc files found.");
            } else {
                // Loop through our array rows
                for (int i = 0; i < romCount; i++) {
                    bool isSelected = (strcmp(selectedRomPath, localRoms[i]) == 0);
                    
                    // Draw the clickable selection text row item [1.2]
                    if (ImGui::Selectable(localRoms[i], isSelected)) {
                        strncpy(selectedRomPath, localRoms[i], 255);
                    }

                    // Handle rapid double-clicks on a game title row
                    if (isSelected && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                        if (selectedRomPath[0] != '\0') {
                            GUInit(selectedRomPath); 
                            romCount = -1;           // Force a fresh folder rescan next time
                            ImGui::CloseCurrentPopup();
                        }
                    }
                }
            }
            ImGui::EndChild();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Display what is currently chosen at the bottom of the dialogue box
        ImGui::Text("Selected: %s", selectedRomPath[0] != '\0' ? selectedRomPath : "[None]");
        ImGui::Spacing();

        // Load Button
        if (ImGui::Button("Load ROM", ImVec2(120, 0))) {
            if (selectedRomPath[0] != '\0') {
                GUInit(selectedRomPath); 
            }
            romCount = -1; // Set back to -1 so it scans again on the next window boot up
            ImGui::CloseCurrentPopup(); 
        }
        
        ImGui::SameLine();

        // Cancel Button
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            romCount = -1; // Reset to refresh folder contents on next click
            ImGui::CloseCurrentPopup(); 
        }

        ImGui::EndPopup();
    }

    // Size window based on debugger show flag
    // Only resize if the debugger flag has changed this frame so we don't spam window resizes
    static bool last_toggle_state = true;
    if (showDebugger != last_toggle_state) {
        if (showDebugger) {
            // Expand the window to fit the sidebar
            SDL_SetWindowSize(window, GAME_WIDTH + DEBUG_WIDTH, WINDOW_HEIGHT);
        } else {
            // Shrink the window back down to just the game view size!
            SDL_SetWindowSize(window, GAME_WIDTH, WINDOW_HEIGHT);
        }
        last_toggle_state = showDebugger;
    }

    // Show debugger panel
    if (showDebugger) {
        ImGui::SetNextWindowPos(ImVec2(GAME_WIDTH + 10, 25), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(380, SCREEN_HEIGHT - 15), ImGuiCond_Always);
        ImGui::Begin("ArkGB Debugger Panel");
        
        // Core CPU Metrics
        if (ImGui::CollapsingHeader("CPU Core Registers")) {
            ImGui::Text("   Live PC: 0x%04X", regs.PC);
            ImGui::Text("   Live SP: 0x%04X", regs.SP);
            ImGui::Text("CPU Flags:");
            ImGui::Text("   [Z] Zero:  %d", getFlag(ZF));
            ImGui::Text("   [N] Sub:   %d", getFlag(NF));
            ImGui::Text("   [H] Half:  %d", getFlag(HF));
            ImGui::Text("   [C] Carry: %d", getFlag(CF));
        }

        // Display controls
        if (ImGui::CollapsingHeader("Display Controls")) {
            ImGui::Checkbox("Enable Background Layer", &display.drawBg);
            ImGui::Checkbox("Enable Sprite Layer", &display.drawOam);
            ImGui::Checkbox("Enable Text Window Layer", &display.drawWin);
        }

        // Sound controls
        if (ImGui::CollapsingHeader("Sound Board Controls")) {
            ImGui::Text("Audio Mixer Toggles:");
            ImGui::Checkbox("Mute CH1 (Square 1)", &audio.pulseChannel1.mute);
            ImGui::Checkbox("Mute CH2 (Square 2)", &audio.pulseChannel2.mute);
            ImGui::Checkbox("Mute CH3 (Wave)"    , &audio.waveChannel.mute);
            ImGui::Checkbox("Mute CH4 (Noise)"   , &audio.noiseChannel.mute);
        }

        // FPS counter
        if (ImGui::CollapsingHeader("Performance Timing")) {
            ImGui::Text("   Application Speed: %.1f FPS", ImGui::GetIO().Framerate);
            ImGui::Text("   Frame Time:        %.3f ms/frame", 1000.0f / ImGui::GetIO().Framerate);
        }

        // TODO: implement dynamic cheat menu for different ROMs
        // Game specific cheats 
        // if (ImGui::CollapsingHeader("Cheats")) {

        //     // 2. Fetch active combat stats from the permanent base memory bank (wram[0])
        //     // 0xCB1C-0xCB1D is Current Battle HP, 0xCB1E-0xCB1F is Max Battle HP
        //     uint16_t current_hp = ((uint16_t)wram[0][0x0B1C] << 8) | wram[0][0x0B1D];
        //     uint16_t max_hp     = ((uint16_t)wram[0][0x0B1E] << 8) | wram[0][0x0B1F];

        //     ImGui::Text("Active Combat Pokémon");
        //     ImGui::Text("  HP: %d / %d", current_hp, max_hp);

        //     // 3. Render a visual health bar layout element
        //     float health_percentage = (max_hp > 0) ? (float)current_hp / (float)max_hp : 0.0f;
        //     ImGui::ProgressBar(health_percentage, ImVec2(0.0f, 0.0f));

        //     ImGui::Spacing();

        //     // 4. ---- THE GOD MODE BATTLE HEAL BUTTON ----
        //     // This force-overwrites your current battle health with your maximum battle health!
        //     if (ImGui::Button("Battle Elixir (Full Heal)")) {
        //         wram[0][0x0B1C] = wram[0][0x0B1E];
        //         wram[0][0x0B1D] = wram[0][0x0B1F];
        //     }

        //     ImGui::Separator();
        //     ImGui::Text("Overworld Modifications");

        //     // Ghost Mode Toggle
        //     static bool ghost_mode = false;
        //     if (ImGui::Checkbox("Ghost Mode (Walk Through Walls)", &ghost_mode)) { /* handled below */ }
        //     if (ghost_mode) { wram[0][0x02F3] = 0x01; }

        //     // Egg Incubator
        //     if (ImGui::Button("Instant Egg Incubator")) { wram[0][0x05DA] = 0x01; }

        //     ImGui::Separator();
        //     ImGui::Text("Combat Modifications");

        //     // Infinite PP Toggle
        //     static bool infinite_pp = false;
        //     ImGui::Checkbox("Lock Move PP", &infinite_pp);
        //     if (infinite_pp) {
        //         wram[0][0x02F3] = 0x01; // Keep wall bypass stable
        //         wram[0][0x0B14] = 30; wram[0][0x0B15] = 30; wram[0][0x0B16] = 30; wram[0][0x0B17] = 30;
        //     }
        // }

        ImGui::End();
    }
 } 