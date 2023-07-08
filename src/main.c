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
    #define SWARM_SIZE 50000
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

#define XSTR(a) STR(a)
#define STR(a) #a

#if !defined(FPS)
#define FPS 120
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

GLushort elements[SWARM_SIZE];

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

    GLint positionXAttr = glGetAttribLocation(particles, "posX");
    GLint positionYAttr = glGetAttribLocation(particles, "posY");

    GLuint glBuffers[3];
    glGenBuffers(sizeof(glBuffers) / sizeof(glBuffers[0]), glBuffers);

    glBindBuffer(GL_ARRAY_BUFFER, glBuffers[0]);
    glBufferData(GL_ARRAY_BUFFER, SWARM_SIZE * sizeof(GLfloat), NULL, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, glBuffers[1]);
    glBufferData(GL_ARRAY_BUFFER, SWARM_SIZE * sizeof(GLfloat), NULL, GL_DYNAMIC_DRAW);

    for(size_t i = 0; i < SWARM_SIZE; i++)
        elements[i] = (GLushort)i;

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glBuffers[2]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(elements), elements, GL_STATIC_DRAW);


// OpenCL init
    CGLContextObj cgl_current_context = CGLGetCurrentContext();
    CGLShareGroupObj cgl_share_group = CGLGetShareGroup(cgl_current_context);

    cl_context_properties properties[] = {
        CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE,
        (cl_context_properties) cgl_share_group,
        0
    };

    char* src = readFile("compute.cl", NULL);

    CHECK_ASSIGN(
        "Context creation",
        cl_context, ctx, 
        clCreateContext(properties, 0, NULL, NULL, NULL, NULL)
    );

    cl_device_id device;
    clGetContextInfo(ctx, CL_CONTEXT_DEVICES, sizeof(device), &device, NULL);


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
        clCreateFromGLBuffer(ctx, CL_MEM_READ_WRITE, glBuffers[0], NULL)
    );

    CHECK_ASSIGN(
        "Y buffer creation",
        cl_mem, dataY,
        clCreateFromGLBuffer(ctx, CL_MEM_READ_WRITE, glBuffers[1], NULL)
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
        "Enqueue X buffer acquire",
        clEnqueueAcquireGLObjects(queue, 1, &dataX, 0, NULL, NULL)
    );

    CHECK_OPERATION(
        "Enqueue Y buffer acquire",
        clEnqueueAcquireGLObjects(queue, 1, &dataY, 0, NULL, NULL)
    );

    CHECK_OPERATION(
        "Enqueue X buffer init",
        clEnqueueWriteBuffer(queue, dataX, CL_FALSE, 0, SWARM_SIZE * sizeof(float), Swarm.x, 0, NULL, NULL)
    );

    CHECK_OPERATION(
        "Enqueue Y buffer init",
        clEnqueueWriteBuffer(queue, dataY, CL_FALSE, 0, SWARM_SIZE * sizeof(float), Swarm.y, 0, NULL, NULL)
    );

    CHECK_OPERATION(
        "Enqueue X buffer acquire",
        clEnqueueReleaseGLObjects(queue, 1, &dataX, 0, NULL, NULL)
    );

    CHECK_OPERATION(
        "Enqueue Y buffer acquire",
        clEnqueueReleaseGLObjects(queue, 1, &dataY, 0, NULL, NULL)
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
            "Enqueue X buffer acquire in loop",
            clEnqueueAcquireGLObjects(queue, 1, &dataX, 0, NULL, NULL)
        );

        CHECK_OPERATION(
            "Enqueue Y buffer acquire in loop",
            clEnqueueAcquireGLObjects(queue, 1, &dataY, 0, NULL, NULL)
        );

        CHECK_OPERATION(
            "enqueue tick",
            clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global, NULL, 0, NULL, NULL)
        );

        CHECK_OPERATION(
            "Enqueue X buffer acquire",
            clEnqueueReleaseGLObjects(queue, 1, &dataX, 0, NULL, NULL)
        );

        CHECK_OPERATION(
            "Enqueue Y buffer acquire",
            clEnqueueReleaseGLObjects(queue, 1, &dataY, 0, NULL, NULL)
        );

        clFinish(queue);

        glClearColor(0.f, 0.f, 0.f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(particles);

        
        glBindBuffer(GL_ARRAY_BUFFER, glBuffers[0]);
        glVertexAttribPointer(
            positionXAttr,
            1,
            GL_FLOAT,
            GL_FALSE,
            0,
            NULL
        );
        glEnableVertexAttribArray(positionXAttr);


        glBindBuffer(GL_ARRAY_BUFFER, glBuffers[1]);
        glVertexAttribPointer(
            positionYAttr,
            1,
            GL_FLOAT,
            GL_FALSE,
            0,
            NULL
        );
        glEnableVertexAttribArray(positionYAttr);


        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glBuffers[2]);
        glDrawElements(
            GL_POINTS,
            SWARM_SIZE,
            GL_UNSIGNED_SHORT, 
            NULL
        );

        glDisableVertexAttribArray(positionXAttr);
        glDisableVertexAttribArray(positionYAttr);

        SDL_GL_SwapWindow(window);

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


    glDeleteBuffers(sizeof(glBuffers) / sizeof(glBuffers[0]), glBuffers);



    SDL_GL_DeleteContext(glCtx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}