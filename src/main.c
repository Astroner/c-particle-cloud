#include <stdio.h>
#include <time.h>

#include <SDL2/SDL.h>
#include <Accelerate/Accelerate.h>

#define WIDTH 1280
#define HEIGHT 720

#define SWARM_SIZE 40000
#define UNIT_SIZE 1
#define SPEED_RATIO 1

#define SECS_TO_MEASURE 60

struct Swarm {
    float x[SWARM_SIZE];
    float y[SWARM_SIZE];

    float dx[SWARM_SIZE];
    float dy[SWARM_SIZE];
} Swarm;

#define XSTR(a) STR(a)
#define STR(a) #a

#if !defined(FPS)
#define FPS 120
#endif

float msPerFrame = 1000.0f / (float)FPS;


void initSwarm() {
#if defined(BLAS)
    int idist = 1;
    
    int seed[] = { 0, 1, 7, 11 };

    int size = SWARM_SIZE * 2;

    slarnv_(&idist, seed, &size, Swarm.x);


    idist = 2;
    slarnv_(&idist, seed, &size, Swarm.dx);

    cblas_sscal(SWARM_SIZE, WIDTH, Swarm.x, 1);
    cblas_sscal(SWARM_SIZE, HEIGHT, Swarm.y, 1);

    cblas_sscal(SWARM_SIZE * 2, SPEED_RATIO, Swarm.dx, 1);
#else

    for(size_t i = 0; i < SWARM_SIZE; i++) {
        Swarm.x[i] = (float)rand() * WIDTH / (float)RAND_MAX;
        Swarm.y[i] = (float)rand() * HEIGHT / (float)RAND_MAX;

        Swarm.dx[i] = (float)rand() * 2 * SPEED_RATIO / (float)RAND_MAX - 1;
        Swarm.dy[i] = (float)rand() * 2 * SPEED_RATIO / (float)RAND_MAX - 1;
    }
#endif
}

void tick() {
#if defined(BLAS)
    cblas_saxpy(SWARM_SIZE * 2, 1, Swarm.dx, 1, Swarm.x, 1);
#else
    for(size_t i = 0; i < SWARM_SIZE; i++) {
        Swarm.x[i] += Swarm.dx[i];
        Swarm.y[i] += Swarm.dy[i];
    }
#endif
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

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    
    void* pixels;
    int pitch;
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, UNIT_SIZE, UNIT_SIZE);
    SDL_LockTexture(texture, NULL, &pixels, &pitch);
    memset(pixels, 255, pitch * UNIT_SIZE);
    SDL_UnlockTexture(texture);



    initSwarm();

    float avDrop = 0;
    float allDrops = 0;

    size_t seconds = 0;
    size_t drops = 0;
    size_t frameCounter = 0;
    float avRatio = 0;

    SDL_Event e;
    int quit = 0;

    while(!quit) {
        Uint64 start = SDL_GetPerformanceCounter();
        frameCounter++;

        while(SDL_PollEvent(&e)) {
            if(e.type == SDL_QUIT) {
                quit = 1;
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

            rect.x = Swarm.x[i];
            rect.y = Swarm.y[i];

            SDL_RenderCopyF(renderer, texture, NULL, &rect);
        }

        SDL_RenderPresent(renderer);


        Uint64 end = SDL_GetPerformanceCounter();

        float timeForFrame = (end - start) * 1000.0f / (float)SDL_GetPerformanceFrequency();

        if(timeForFrame < msPerFrame) {
            SDL_Delay(msPerFrame - timeForFrame);
        } else {
            drops++;  
            avDrop = (avDrop * allDrops + timeForFrame - msPerFrame) / (allDrops + 1);
            allDrops++;
            // printf("NOT "XSTR(FPS)" FPS. ACTUAL: %f\n", 1000.f / timeForFrame);
        }
        if(frameCounter % (FPS) == 0) {
            float ratio = (float)drops/(float)(FPS);
            printf("DropRatio: %f\n", ratio);
            drops = 0;

            avRatio = (avRatio * (float)seconds + ratio) / (float)(seconds + 1);

            seconds += 1;
            if(seconds == SECS_TO_MEASURE) {
                quit = 1;
            }
        }
    }

    printf("Average ration: %f, Average Drop: %f\n", avRatio, avDrop);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}