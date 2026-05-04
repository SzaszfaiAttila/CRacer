#ifndef SKYBOX_H
#define SKYBOX_H

#include <glad/gl.h>
#include <cglm/cglm.h>

/* ── Skybox ──────────────────────────────────────────────────────────────── */
typedef struct {
    GLuint vao, vbo;
    GLuint cubemap;   /* GL_TEXTURE_CUBE_MAP                                  */
    GLuint shader;    /* skybox_vert + skybox_frag                            */
} Skybox;

/* Load the 6 face images and compile shaders.
   face_paths: array of 6 paths in order:
     [0]=+X  [1]=-X  [2]=+Y  [3]=-Y  [4]=+Z  [5]=-Z
   vert_path / frag_path: GLSL shader files.
   Returns 1 on success, 0 on failure.                                       */
int  skybox_load(Skybox *sb,
                 const char *face_paths[6],
                 const char *vert_path,
                 const char *frag_path);

/* Draw the skybox.  Call AFTER opaque geometry with depth func GL_LEQUAL.
   view must have translation stripped (pass the 3×3 rotation part only).   */
void skybox_draw(const Skybox *sb, mat4 view, mat4 proj, float brightness);

void skybox_free(Skybox *sb);

#endif /* SKYBOX_H */
