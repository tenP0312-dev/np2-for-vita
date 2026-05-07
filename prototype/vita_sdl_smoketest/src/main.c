#include <SDL2/SDL.h>
#include <psp2/ctrl.h>

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO) != 0) {
    return 1;
  }

  SDL_Window *window = SDL_CreateWindow(
      "NP2 Vita Smoke",
      SDL_WINDOWPOS_UNDEFINED,
      SDL_WINDOWPOS_UNDEFINED,
      960,
      544,
      SDL_WINDOW_SHOWN);
  if (!window) {
    SDL_Quit();
    return 2;
  }

  SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (!renderer) {
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 3;
  }

  int running = 1;
  unsigned frame = 0;

  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        running = 0;
      }
    }

    SceCtrlData pad;
    sceCtrlPeekBufferPositive(0, &pad, 1);
    if (pad.buttons & SCE_CTRL_START) {
      running = 0;
    }

    SDL_SetRenderDrawColor(renderer, 16, 18, 24, 255);
    SDL_RenderClear(renderer);

    SDL_Rect pc98 = {80, 72, 800, 400};
    SDL_SetRenderDrawColor(renderer, 34, 38, 48, 255);
    SDL_RenderFillRect(renderer, &pc98);

    SDL_Rect scan = {80, 72 + (int)(frame % 400), 800, 2};
    SDL_SetRenderDrawColor(renderer, 96, 180, 120, 255);
    SDL_RenderFillRect(renderer, &scan);

    SDL_RenderPresent(renderer);
    SDL_Delay(16);
    frame++;
  }

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}

