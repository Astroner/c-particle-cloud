#include <stdio.h>
#include <time.h>
#include <math.h>

#include <SDL2/SDL.h>
#include <Accelerate/Accelerate.h>
#include <OpenCL/opencl.h>

#include "main.h"

#define WIDTH 1280
#define HEIGHT 720

#if !defined(SWARM_SIZE)
    #define SWARM_SIZE 20000
#endif
#define UNIT_SIZE 1
#define SPEED_RATIO 1

#define CLICK_RADIUS 100

float clickRadius = CLICK_RADIUS;

int swarmSize = SWARM_SIZE;
float width = WIDTH;
float height = HEIGHT;

const size_t global = SWARM_SIZE;

struct Swarm {
    float x[SWARM_SIZE];
    float y[SWARM_SIZE];

    float dx[SWARM_SIZE];
    float dy[SWARM_SIZE];
} Swarm;

float outputX[SWARM_SIZE];
float outputY[SWARM_SIZE];

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

int main(void) {
    if(SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("Failed to init SDL\n");

        return 1;
    }

    char* src = readFile("shader.cl", NULL);

    cl_device_id device = getDevice();

    CHECK_ASSIGN(
        "Context creation",
        cl_context, ctx, 
        clCreateContext(NULL, 1, &device, NULL, NULL, NULL)
    );

    CHECK_ASSIGN(
        "Program creation",
        cl_program, program,
        clCreateProgramWithSource(ctx, 1, (const char**)&src, NULL, NULL)
    );

    CHECK_OPERATION(
        "Program building",
        clBuildProgram(program, 1, &device, NULL, NULL, NULL)
    );

    free(src);

    CHECK_ASSIGN(
        "Kernel creation",
        cl_kernel, kernel,
        clCreateKernel(program, "compute", NULL)
    );

    CHECK_ASSIGN(
        "Queue creation",
        cl_command_queue, queue,
        clCreateCommandQueue(ctx, device, 0, NULL)
    );

    CHECK_ASSIGN(
        "X buffer creation",
        cl_mem, dataX,
        clCreateBuffer(ctx, CL_MEM_READ_WRITE, SWARM_SIZE * sizeof(float), NULL, NULL)
    );

    CHECK_ASSIGN(
        "Y buffer creation",
        cl_mem, dataY,
        clCreateBuffer(ctx, CL_MEM_READ_WRITE, SWARM_SIZE * sizeof(float), NULL, NULL)
    );

    CHECK_ASSIGN(
        "dX buffer creation",
        cl_mem, dataDX,
        clCreateBuffer(ctx, CL_MEM_READ_WRITE, SWARM_SIZE * sizeof(float), NULL, NULL)
    );

    CHECK_ASSIGN(
        "dY buffer creation",
        cl_mem, dataDY,
        clCreateBuffer(ctx, CL_MEM_READ_WRITE, SWARM_SIZE * sizeof(float), NULL, NULL)
    );

    initSwarm();

    CHECK_OPERATION(
        "Enqueue X buffer init",
        clEnqueueWriteBuffer(queue, dataX, CL_FALSE, 0, SWARM_SIZE * sizeof(float), Swarm.x, 0, NULL, NULL)
    );

    CHECK_OPERATION(
        "Enqueue Y buffer init",
        clEnqueueWriteBuffer(queue, dataY, CL_FALSE, 0, SWARM_SIZE * sizeof(float), Swarm.y, 0, NULL, NULL)
    );

    CHECK_OPERATION(
        "Enqueue dX buffer init",
        clEnqueueWriteBuffer(queue, dataDX, CL_FALSE, 0, SWARM_SIZE * sizeof(float), Swarm.dx, 0, NULL, NULL)
    );

    CHECK_OPERATION(
        "Enqueue dY buffer init",
        clEnqueueWriteBuffer(queue, dataDY, CL_FALSE, 0, SWARM_SIZE * sizeof(float), Swarm.dy, 0, NULL, NULL)
    );

    clFinish(queue);


    float mouseInitPos = -1;
    
    CHECK_OPERATION("set X arg",            clSetKernelArg(kernel, 0, sizeof(dataX), &dataX));
    CHECK_OPERATION("set Y arg",            clSetKernelArg(kernel, 1, sizeof(dataY), &dataY));
    CHECK_OPERATION("set dX arg",           clSetKernelArg(kernel, 2, sizeof(dataDX), &dataDX));
    CHECK_OPERATION("set dY arg",           clSetKernelArg(kernel, 3, sizeof(dataDY), &dataDY));
    CHECK_OPERATION("set swarm size arg",   clSetKernelArg(kernel, 4, sizeof(swarmSize), &swarmSize));
    CHECK_OPERATION("set width arg",        clSetKernelArg(kernel, 5, sizeof(width), &width));
    CHECK_OPERATION("set height arg",       clSetKernelArg(kernel, 6, sizeof(height), &height));
    CHECK_OPERATION("set clickRadius arg",  clSetKernelArg(kernel, 7, sizeof(clickRadius), &clickRadius));
    CHECK_OPERATION("set mouseX arg",       clSetKernelArg(kernel, 8, sizeof(mouseInitPos), &mouseInitPos));
    CHECK_OPERATION("set mouseY arg",       clSetKernelArg(kernel, 9, sizeof(mouseInitPos), &mouseInitPos));


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

    int isMouseDown = 0;


    SDL_Event e;
    int quit = 0;

    while(!quit) {
        Uint64 start = SDL_GetPerformanceCounter();

        while(SDL_PollEvent(&e)) {
            if(e.type == SDL_QUIT) {
                quit = 1;
            } else if(e.type == SDL_MOUSEBUTTONDOWN) {
                isMouseDown = 1;
                float mouseCoord = e.button.x;
                clSetKernelArg(kernel, 8, sizeof(float), &mouseCoord);
                mouseCoord = e.button.y;
                clSetKernelArg(kernel, 9, sizeof(float), &mouseCoord);
            } else if(e.button.type == SDL_MOUSEBUTTONUP) {
                isMouseDown = 0;
                float mouseCoord = -1;
                clSetKernelArg(kernel, 8, sizeof(float), &mouseCoord);
            } else if(e.type == SDL_MOUSEMOTION && isMouseDown) {
                float mouseCoord = e.button.x;
                clSetKernelArg(kernel, 8, sizeof(float), &mouseCoord);
                mouseCoord = e.button.y;
                clSetKernelArg(kernel, 9, sizeof(float), &mouseCoord);
            }
        }

        CHECK_OPERATION(
            "enqueue tick",
            clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global, NULL, 0, NULL, NULL)
        );


        CHECK_OPERATION(
            "enqueue read X data",
            clEnqueueReadBuffer(queue, dataX, CL_FALSE, 0, sizeof(outputX), outputX, 0, NULL, NULL)
        );

        CHECK_OPERATION(
            "enqueue read Y data",
            clEnqueueReadBuffer(queue, dataY, CL_FALSE, 0, sizeof(outputY), outputY, 0, NULL, NULL)
        );

        clFinish(queue);
        
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 250);
        SDL_RenderClear(renderer);

        SDL_FRect rect = {
            .h = UNIT_SIZE,
            .w = UNIT_SIZE
        };

        for(size_t i = 0; i < SWARM_SIZE; i++) {
            rect.x = outputX[i];
            rect.y = outputY[i];

            SDL_RenderCopyF(renderer, texture, NULL, &rect);
        }

        SDL_RenderPresent(renderer);


        Uint64 end = SDL_GetPerformanceCounter();

        float timeForFrame = (end - start) * 1000.0f / (float)SDL_GetPerformanceFrequency();

        if(timeForFrame < msPerFrame) {
            SDL_Delay(msPerFrame - timeForFrame);
        }
    }

    clReleaseMemObject(dataX);
    clReleaseMemObject(dataY);
    clReleaseMemObject(dataDX);
    clReleaseMemObject(dataDY);
    clReleaseKernel(kernel);
    clReleaseProgram(program);
    clReleaseCommandQueue(queue);
    clReleaseContext(ctx);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}