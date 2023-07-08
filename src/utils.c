#include "main.h"

#include <stdlib.h>
#include <stdio.h>

char* readFile(char* path, size_t* strLength) {
    FILE* file = fopen(path, "r");

    fseek(file, 0, SEEK_END);

    fpos_t length;

    fgetpos(file, &length);

    char* result = (char*)malloc(length + 1);

    fseek(file, 0, SEEK_SET);

    fread(result, 1, length, file);

    fclose(file);

    result[length] = '\0';

    if(strLength) {
        *strLength = (size_t)(length + 1);
    }

    return result;
}

GLuint createShader(GLenum type, char* fileSrc) {
    char* src = readFile(fileSrc, NULL);

    GLuint shader = glCreateShader(type);

    glShaderSource(shader, 1, (const char**)&src, NULL);
    free(src);

    glCompileShader(shader);

    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

    if(!compiled) {
        printf("Failed to compile shader %s\n", fileSrc);
        glDeleteShader(shader);

        return 0;
    }

    return shader;
}

GLuint createProgram(GLuint vertex, GLuint fragment) {
    GLuint program = glCreateProgram();

    glAttachShader(program, vertex);
    glAttachShader(program, fragment);

    glLinkProgram(program);

    GLint linked;

    glGetProgramiv(program, GL_LINK_STATUS, &linked);

    if(!linked) {
        printf("Failed to link program\n");
        glDeleteProgram(program);

        return 0;
    }

    return program;
}