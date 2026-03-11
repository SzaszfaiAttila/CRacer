#include "shader.h"
#include <stdio.h>
#include <stdlib.h>

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "Cannot open shader: %s\n", path); return NULL; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    char *buf = malloc(size + 1);
    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);
    return buf;
}

static GLuint compile(const char *src, GLenum type) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    int ok; char log[512];
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { glGetShaderInfoLog(s, 512, NULL, log); fprintf(stderr, "Shader error:\n%s\n", log); }
    return s;
}

GLuint shader_create(const char *vert_path, const char *frag_path) {
    char *vs = read_file(vert_path);
    char *fs = read_file(frag_path);
    GLuint v = compile(vs, GL_VERTEX_SHADER);
    GLuint f = compile(fs, GL_FRAGMENT_SHADER);
    GLuint p = glCreateProgram();
    glAttachShader(p, v); glAttachShader(p, f);
    glLinkProgram(p);
    int ok; char log[512];
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) { glGetProgramInfoLog(p, 512, NULL, log); fprintf(stderr, "Link error:\n%s\n", log); }
    glDeleteShader(v); glDeleteShader(f);
    free(vs); free(fs);
    return p;
}

void shader_use(GLuint p)                                          { glUseProgram(p); }
void shader_set_mat4(GLuint p, const char *n, float *m)           { glUniformMatrix4fv(glGetUniformLocation(p,n),1,GL_FALSE,m); }
void shader_set_vec3(GLuint p, const char *n, float x,float y,float z) { glUniform3f(glGetUniformLocation(p,n),x,y,z); }
void shader_set_float(GLuint p, const char *n, float v)           { glUniform1f(glGetUniformLocation(p,n),v); }
void shader_set_int(GLuint p, const char *n, int v)               { glUniform1i(glGetUniformLocation(p,n),v); }
