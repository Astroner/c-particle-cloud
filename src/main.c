#include <stdio.h>

#include <Accelerate/Accelerate.h>

#include <SDL2/SDL.h>
#include <OpenGL/CGLDevice.h>
#include <OpenGL/CGLCurrent.h>
#include <OpenGL/gl.h>

#include <OpenCL/opencl.h>
#include <OpenCL/cl_gl_ext.h>

#include "main.h"

#define WIDTH 1280
#define HEIGHT 720

#if !defined(SWARM_SIZE)
    #define SWARM_SIZE 500000
#endif
#define UNIT_SIZE 1
#define SPEED_RATIO 0.001

#define CLICK_RADIUS 100

float clickRadius = (float)CLICK_RADIUS / (float)WIDTH;

int swarmSize = SWARM_SIZE;
GLushort elements[SWARM_SIZE];
float ratio = (float)WIDTH / (float)HEIGHT;
const size_t global = SWARM_SIZE;

struct Swarm {
    float pos[SWARM_SIZE * 2];
    float speed[SWARM_SIZE * 2];
} Swarm;


#define XSTR(a) STR(a)
#define STR(a) #a

#if !defined(FPS)
#define FPS 120
#endif

float msPerFrame = 1000.0f / (float)FPS;


void initSwarm() {
    // Randomize swarm positions and speed
   
    int idist = 2; // result type. "2" means range from -1 to 1
    int seed[] = { 0, 1, 7, 11 }; // random seed
    int size = SWARM_SIZE * 4;
    slarnv_(&idist, seed, &size, Swarm.pos); // LAPACK random vector function

    cblas_sscal(SWARM_SIZE * 2, SPEED_RATIO, Swarm.speed, 1); // BLAS multiply vector by scalar
}

int main(void) {

// SDL/OpenGL init
    if(SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("Failed to init SDL\n");

        return 1;
    }
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);

    SDL_Window* window = SDL_CreateWindow(
        "The Swarm",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WIDTH,
        HEIGHT,
        SDL_WINDOW_OPENGL
    );    

    SDL_GLContext glCtx = SDL_GL_CreateContext(window);


    GLuint vertex = createShader(GL_VERTEX_SHADER, "vertex.glsl");
    GLuint fragment = createShader(GL_FRAGMENT_SHADER, "fragment.glsl");
    GLuint particles = createProgram(vertex, fragment);
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    GLint positionAttr = glGetAttribLocation(particles, "pos");

    GLuint glBuffers[2];
    glGenBuffers(sizeof(glBuffers) / sizeof(glBuffers[0]), glBuffers);

    glBindBuffer(GL_ARRAY_BUFFER, glBuffers[0]);
    glBufferData(GL_ARRAY_BUFFER, SWARM_SIZE * sizeof(GLfloat) * 2, NULL, GL_DYNAMIC_DRAW);

    for(size_t i = 0; i < SWARM_SIZE; i++)
        elements[i] = (GLushort)i;

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glBuffers[1]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(elements), elements, GL_STATIC_DRAW);


// OpenCL init
    CGLContextObj cgl_current_context = CGLGetCurrentContext();
    CGLShareGroupObj cgl_share_group = CGLGetShareGroup(cgl_current_context);

    cl_context_properties properties[] = {
        CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE,
        (cl_context_properties) cgl_share_group,
        0
    };


    cl_context ctx = clCreateContext(properties, 0, NULL, NULL, NULL, NULL);

    cl_device_id device;
    clGetContextInfo(ctx, CL_CONTEXT_DEVICES, sizeof(device), &device, NULL);

    cl_command_queue queue = clCreateCommandQueue(ctx, device, 0, NULL);

    char* src = readFile("compute.cl", NULL);
    cl_program program = clCreateProgramWithSource(ctx, 1, (const char**)&src, NULL, NULL);
    clBuildProgram(program, 1, &device, NULL, NULL, NULL);
    free(src);


    cl_kernel kernel = clCreateKernel(program, "compute", NULL);

    cl_mem dataPos = clCreateFromGLBuffer(ctx, CL_MEM_READ_WRITE, glBuffers[0], NULL);
    cl_mem dataSpeed = clCreateBuffer(ctx, CL_MEM_READ_WRITE, SWARM_SIZE * sizeof(float) * 2, NULL, NULL);

    initSwarm();

    clEnqueueAcquireGLObjects(queue, 1, &dataPos, 0, NULL, NULL);
    clEnqueueWriteBuffer(queue, dataPos, CL_FALSE, 0, sizeof(Swarm.pos), Swarm.pos, 0, NULL, NULL);
    clEnqueueReleaseGLObjects(queue, 1, &dataPos, 0, NULL, NULL);

    clEnqueueWriteBuffer(queue, dataSpeed, CL_FALSE, 0, sizeof(Swarm.speed), Swarm.speed, 0, NULL, NULL);
    clFinish(queue);



    float mousePosition[2] = { -2, -2 };
    
    clSetKernelArg(kernel, 0, sizeof(dataPos), &dataPos);
    clSetKernelArg(kernel, 1, sizeof(dataSpeed), &dataSpeed);
    clSetKernelArg(kernel, 2, sizeof(swarmSize), &swarmSize);
    clSetKernelArg(kernel, 3, sizeof(ratio), &ratio);
    clSetKernelArg(kernel, 4, sizeof(clickRadius), &clickRadius);
    clSetKernelArg(kernel, 5, sizeof(mousePosition), &mousePosition);


    // Main loop
    int isMouseDown = 0;


    SDL_Event e;
    int quit = 0;

    while(!quit) {
        Uint64 start = SDL_GetPerformanceCounter();

        while(SDL_PollEvent(&e)) {
            switch(e.type) {
                case SDL_QUIT:
                    quit = 1;
                    break;

                case SDL_MOUSEBUTTONDOWN:
                    isMouseDown = 1;
                    mousePosition[0] = e.button.x * 2 / (float)WIDTH - 1;
                    mousePosition[1] = e.button.y * -2 / (float)HEIGHT + 1;

                    clSetKernelArg(kernel, 5, sizeof(mousePosition), &mousePosition);
                    break;

                case SDL_MOUSEBUTTONUP:
                    isMouseDown = 0;
                    mousePosition[0] = -2;
                    clSetKernelArg(kernel, 5, sizeof(float), &mousePosition);
                    break;

                case SDL_MOUSEMOTION:
                    if(!isMouseDown) break;

                    mousePosition[0] = e.button.x * 2 / (float)WIDTH - 1;
                    mousePosition[1] = e.button.y * -2 / (float)HEIGHT + 1;

                    clSetKernelArg(kernel, 5, sizeof(mousePosition), &mousePosition);
                    break;
            }
        }

        clEnqueueAcquireGLObjects(queue, 1, &dataPos, 0, NULL, NULL);
        clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global, NULL, 0, NULL, NULL);
        clEnqueueReleaseGLObjects(queue, 1, &dataPos, 0, NULL, NULL);
        clFinish(queue);

        glClearColor(0.f, 0.f, 0.f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(particles);
        
        glBindBuffer(GL_ARRAY_BUFFER, glBuffers[0]);
        glVertexAttribPointer(
            positionAttr,
            2,
            GL_FLOAT,
            GL_FALSE,
            0,
            NULL
        );
        glEnableVertexAttribArray(positionAttr);


        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glBuffers[1]);
        glDrawElements(
            GL_POINTS,
            SWARM_SIZE,
            GL_UNSIGNED_SHORT, 
            NULL
        );

        glDisableVertexAttribArray(positionAttr);

        SDL_GL_SwapWindow(window);

        Uint64 end = SDL_GetPerformanceCounter();

        float timeForFrame = (end - start) * 1000.0f / (float)SDL_GetPerformanceFrequency();

        if(timeForFrame < msPerFrame) {
            SDL_Delay(msPerFrame - timeForFrame);
        }
    }

    clReleaseMemObject(dataPos);
    clReleaseMemObject(dataSpeed);
    clReleaseKernel(kernel);
    clReleaseProgram(program);
    clReleaseCommandQueue(queue);
    clReleaseContext(ctx);


    glDeleteBuffers(sizeof(glBuffers) / sizeof(glBuffers[0]), glBuffers);
    glDeleteProgram(particles);


    SDL_GL_DeleteContext(glCtx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}