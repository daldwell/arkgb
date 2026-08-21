#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <atomic>
#include "Window.h"
#include "Debugger.h"
#include "GUnit.h"

// Global window object
Window gwindow;

// Audio step size
double audioStepSize = 87;

//Screen dimension constants
const int SCREEN_WIDTH = 320;
const int SCREEN_HEIGHT = 288;
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
    window = SDL_CreateWindow( "ArkGB", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN );
    if( window == NULL )
    {
        printf( "Window could not be created! SDL_Error: %s\n", SDL_GetError() );
        exit(1);
    }

    //Get window surface
    screen = SDL_GetWindowSurface( window );

    //Create canvas surface
    surface = SDL_CreateRGBSurface(0, CANVAS_WIDTH, CANVAS_HEIGHT, 32, 0, 0, 0, 0);
    accelerated_renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
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
    //SDL_memset(surface->pixels + ((x+CANVAS_XOFFSET)*sizeof(int)) + ((y+CANVAS_YOFFSET) * surface->pitch), c, sizeof(int));
    SDL_UnlockSurface(surface);
}

void Window::RefreshWindow()
{
	SDL_Rect srcRect;
	srcRect.x = CANVAS_XOFFSET;
	srcRect.y = CANVAS_YOFFSET;
	srcRect.w = 160;
	srcRect.h = 144;

    SDL_Rect destRect;
    destRect.x = 0;
    destRect.y = 0;
	destRect.w = SCREEN_WIDTH;
	destRect.h = SCREEN_HEIGHT;
	
    // 1. Update the existing texture with your surface's fresh pixel data
    // (Replace 'surface->pixels' and 'surface->pitch' with your source surface variables)
    SDL_UpdateTexture(texture, NULL, surface->pixels, surface->pitch);

    // 2. Clear the screen renderer
    SDL_RenderClear(accelerated_renderer);

    // 3. Copy the updated texture to the screen (handles scaling automatically if destRect is larger)
    SDL_RenderCopy(accelerated_renderer, texture, &srcRect, &destRect);

    // 4. Swap the buffers smoothly via VSync
    SDL_RenderPresent(accelerated_renderer);

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

    EventDispatch();
}

// FPS counter
// 1. Track time per frame. Milliseconds per frame.
// 2. Build up a buffer of these. Maybe 10 frame timings.
// 3. Get the average time from this in millis
// 4. 1000 / Average time = FPS