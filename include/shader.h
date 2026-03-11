#ifndef SHADER_H
#define SHADER_H

#include <glad/gl.h>

GLuint shader_create(const char *vert_path, const char *frag_path);
void   shader_use(GLuint program);
void   shader_set_mat4(GLuint program, const char *name, float *mat);
void   shader_set_vec3(GLuint program, const char *name, float x, float y, float z);
void   shader_set_float(GLuint program, const char *name, float value);
void   shader_set_int(GLuint program, const char *name, int value);

#endif
