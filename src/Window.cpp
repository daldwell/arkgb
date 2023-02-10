#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "Window.h"
#include "Control.h"
#include "Debugger.h"
#include "Rom.h"
#include "GUnit.h"

// Global window object
Window gwindow;

//Screen dimension constants
const int SCREEN_WIDTH = 320;
const int SCREEN_HEIGHT = 288;
const int SCREEN_FPS = 60;
const int SCREEN_TICKS_PER_FRAME = 1000 / SCREEN_FPS;

// Frame counter
int startFrameTime = 0;

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

    accelerated_renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED);

    running = true;
}

Window::~Window()
{
    //dumpTraceLines();
    closeRom();

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
	// SDL_BlitScaled(drawSurface, &srcRect, screen, &destRect );
	// SDL_UpdateWindowSurface(window);

    texture = SDL_CreateTextureFromSurface(accelerated_renderer, drawSurface);
    SDL_RenderCopy(accelerated_renderer,texture,&srcRect, &destRect);
    SDL_RenderPresent(accelerated_renderer);
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(drawSurface);

    // TODO: (UPDATE Vsync disabled) screen refresh is vsynced, this works perfectly for a 60hz refresh but will run too fast on a higher rate
    // Need to come up with a good standardised timing mechanisim to cap at 60fps - I have tried counting SDL ticks in a hard loop, but this produces a small but noticible stutter while scrolling
    // (should not do this anyway as it wastes cpu cycles)
    // The other option I tried is timing with SDL delay - this produces an even more inconsistent frame rate 
    // Perhaps SDL isn't up to the task for time keeping?
    // int frameTicks = SDL_GetTicks() - startFrameTime;
    // while( frameTicks < SCREEN_TICKS_PER_FRAME )
    // {
    //     frameTicks = SDL_GetTicks() - startFrameTime;
    // }

    startFrameTime = SDL_GetTicks();
    EventDispatch();
}