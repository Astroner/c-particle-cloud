#include <stdio.h>
#include <time.h>
#include <math.h>

#include <SDL2/SDL.h>
#include <Accelerate/Accelerate.h>

#define WIDTH 1280
#define HEIGHT 720

#define SWARM_SIZE 20000
#define UNIT_SIZE 1
#define SPEED_RATIO 1

#define CLICK_RADIUS 100

struct Swarm {
    float x[SWARM_SIZE];
    float y[SWARM_SIZE];

    float dx[SWARM_SIZE];
    float dy[SWARM_SIZE];
} Swarm;

#define XSTR(a) STR(a)
#define STR(a) #a

#if !defined(FPS)
#define FPS 60
#endif

float msPerFrame = 1000.0f / (float)FPS;


void initSwarm() {
    int idist = 1;
    
    int seed[] = { 0, 1, 7, 11 };

    int size = SWARM_SIZE * 2;

    slarnv_(&idist, seed, &size, Swarm.x);


    idist = 2;
    slarnv_(&idist, seed, &size, Swarm.dx);

    cblas_sscal(SWARM_SIZE, WIDTH, Swarm.x, 1);
    cblas_sscal(SWARM_SIZE, HEIGHT, Swarm.y, 1);

    cblas_sscal(SWARM_SIZE * 2, SPEED_RATIO, Swarm.dx, 1);
}

void tick() {
    cblas_saxpy(SWARM_SIZE * 2, 1, Swarm.dx, 1, Swarm.x, 1);
}

int main(void) {
    if(SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("Failed to init SDL\n");

        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "The Swarm",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WIDTH,
        HEIGHT,
        SDL_WINDOW_OPENGL
    );

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
    
    void* pixels;
    int pitch;
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, UNIT_SIZE, UNIT_SIZE);
    SDL_LockTexture(texture, NULL, &pixels, &pitch);
    memset(pixels, 255, pitch * UNIT_SIZE);
    SDL_UnlockTexture(texture);


    initSwarm();


    int isMouseDown = 0;

    float mouseX = 0;
    float mouseY = 0;

    SDL_Event e;
    int quit = 0;

    while(!quit) {
        Uint64 start = SDL_GetPerformanceCounter();

        while(SDL_PollEvent(&e)) {
            if(e.type == SDL_QUIT) {
                quit = 1;
            } else if(e.type == SDL_MOUSEBUTTONDOWN) {
                isMouseDown = 1;
                mouseX = e.button.x;
                mouseY = e.button.y;
            } else if(e.button.type == SDL_MOUSEBUTTONUP) {
                isMouseDown = 0;
            } else if(e.type == SDL_MOUSEMOTION && isMouseDown) {
                mouseX = e.motion.x;
                mouseY = e.motion.y;
            }
        }

        tick();


        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 250);
        SDL_RenderClear(renderer);

        SDL_FRect rect = {
            .h = UNIT_SIZE,
            .w = UNIT_SIZE
        };

        for(size_t i = 0; i < SWARM_SIZE; i++) {

            if(Swarm.x[i] <= 0 || Swarm.x[i] >= WIDTH) {
                Swarm.dx[i] *= -1;
            }

            if(Swarm.y[i] <= 0 || Swarm.y[i] >= HEIGHT) {
                Swarm.dy[i] *= -1;
            }

            if(isMouseDown) {
                float vx = Swarm.x[i] - mouseX;
                float vy = Swarm.y[i] - mouseY;

                if(
                    vx * vx
                    + vy * vy
                    <= CLICK_RADIUS * CLICK_RADIUS
                ) {
                    float len = sqrtf(vx * vx + vy * vy);
                    Swarm.dx[i] = ((Swarm.x[i] - mouseX) * 5 / len) * (1.1 - len / CLICK_RADIUS);
                    Swarm.dy[i] = ((Swarm.y[i] - mouseY) * 5 / len) * (1.1 - len / CLICK_RADIUS);
                }
            }

            rect.x = Swarm.x[i];
            rect.y = Swarm.y[i];

            SDL_RenderCopyF(renderer, texture, NULL, &rect);
        }

        SDL_RenderPresent(renderer);


        Uint64 end = SDL_GetPerformanceCounter();

        float timeForFrame = (end - start) * 1000.0f / (float)SDL_GetPerformanceFrequency();

        if(timeForFrame < msPerFrame) {
            SDL_Delay(msPerFrame - timeForFrame);
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}