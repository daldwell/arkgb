#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
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
	
	SDL_Surface* drawSurface = SDL_ConvertSurface( surface, screen->format, 0 );
	// SDL_BlitScaled(surface, &srcRect, screen, &destRect );
	// SDL_UpdateWindowSurface(window);

    texture = SDL_CreateTextureFromSurface(accelerated_renderer, drawSurface);
    SDL_RenderCopy(accelerated_renderer,texture,&srcRect, &destRect);
    SDL_RenderPresent(accelerated_renderer);
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(drawSurface);

    // Dynamically resample the audio to track the vsync framerate
    // Without this the video drifts ahead of the audio as the 60hz refresh video rate is slighty faster than the gameboy audio 59.7275 rate
    int queueSize = SDL_GetQueuedAudioSize(audio_device_id);
    // Handle large deviations
    if (queueSize > 8192 + 2048) {
        audioStepSize = 87.85;
    } else if (queueSize < 8192 - 2048) {
        audioStepSize = 87.15;
    } else {
        // Handle smaller deviations
        if (queueSize > 8192) {
            audioStepSize = 87.45;
        } else {
            audioStepSize = 87.31;
        }
    }

    EventDispatch();
}

// FPS counter
// 1. Track time per frame. Milliseconds per frame.
// 2. Build up a buffer of these. Maybe 10 frame timings.
// 3. Get the average time from this in millis
// 4. 1000 / Average time = FPS