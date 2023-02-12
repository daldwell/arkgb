#include <SDL.h>
#include "Typedefs.h"

#pragma once

class Window
{
    public:
        bool running;
        Window();
        ~Window();
        void SetTitle(const char *);
        void RefreshWindow();
        void ClearSurface();
        void DrawPixel(int, int, int);
        int GetRGB(byte r, byte g, byte b);
    private:
        //The window we'll be rendering to
        SDL_Window* window = NULL;
        //The surface contained by the window
        SDL_Surface* screen = NULL;
        SDL_Surface* surface = NULL;
        // Renderer objects
        SDL_Renderer* accelerated_renderer = NULL;
        SDL_Texture* texture = NULL;
        void EventDispatch();
};

extern Window gwindow;