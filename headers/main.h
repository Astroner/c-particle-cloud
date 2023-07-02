#include <stddef.h>
#include <OpenCL/opencl.h>
#include <OpenGL/gl.h>

#if !defined(MAIN_H)
#define MAIN_H

#define CHECK_ASSIGN(operation, type, name, code) \
    type name;\
    if(!(name = (code))) {\
        printf("Operation failed: '"operation"'\n");\
        return 1;\
    }\

#define CHECK_OPERATION(operation, code)\
    if((code) < 0) {\
        printf("Operation failed: '"operation"'\n");\
        return 1;\
    }\

char* readFile(char* path, size_t* strLength);
cl_device_id getDevice();

GLuint createShader(GLenum type, char* fileSrc);
GLuint createProgram(GLuint vertex, GLuint fragment);

#endif // MAIN_H
