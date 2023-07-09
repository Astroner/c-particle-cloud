#include <stddef.h>
#include <OpenGL/gl.h>

#if !defined(MAIN_H)
#define MAIN_H

char* readFile(char* path, size_t* strLength);

GLuint createShader(GLenum type, char* fileSrc);
GLuint createProgram(GLuint vertex, GLuint fragment);

#endif // MAIN_H
