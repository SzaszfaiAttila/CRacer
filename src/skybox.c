/*  skybox.c — cubemap skybox using stb_image.h
 *
 *  Requires stb_image.h in your include path (public domain / MIT):
 *    https://github.com/nothings/stb/blob/master/stb_image.h
 *  Add ONE translation unit that defines STB_IMAGE_IMPLEMENTATION before
 *  including it — we do that here so you don't need to touch anything else. */

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "skybox.h"
#include "shader.h"     /* shader_create()                                    */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Unit cube (36 vertices, positions only) ─────────────────────────────── */
static const float CUBE_VERTS[] = {
    -1, 1,-1,  -1,-1,-1,   1,-1,-1,   1,-1,-1,   1, 1,-1,  -1, 1,-1,
    -1,-1, 1,  -1,-1,-1,  -1, 1,-1,  -1, 1,-1,  -1, 1, 1,  -1,-1, 1,
     1,-1,-1,   1,-1, 1,   1, 1, 1,   1, 1, 1,   1, 1,-1,   1,-1,-1,
    -1,-1, 1,  -1, 1, 1,   1, 1, 1,   1, 1, 1,   1,-1, 1,  -1,-1, 1,
    -1, 1,-1,   1, 1,-1,   1, 1, 1,   1, 1, 1,  -1, 1, 1,  -1, 1,-1,
    -1,-1,-1,  -1,-1, 1,   1,-1,-1,   1,-1,-1,  -1,-1, 1,   1,-1, 1,
};

/* ── Helpers ─────────────────────────────────────────────────────────────── */
static GLuint load_cubemap(const char *paths[6]) {
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_CUBE_MAP, tex);

    /* Flip disabled for cubemaps — faces are already oriented correctly     */
    stbi_set_flip_vertically_on_load(0);

    for (int i = 0; i < 6; i++) {
        int w, h, ch;
        unsigned char *data = stbi_load(paths[i], &w, &h, &ch, 3);
        if (!data) {
            fprintf(stderr, "Skybox: failed to load '%s': %s\n",
                    paths[i], stbi_failure_reason());
            glDeleteTextures(1, &tex);
            return 0;
        }
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                     0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        stbi_image_free(data);
        printf("Skybox: loaded face %d  '%s'  %dx%d\n", i, paths[i], w, h);
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    return tex;
}

/* ── Public API ──────────────────────────────────────────────────────────── */
int skybox_load(Skybox *sb,
                const char *face_paths[6],
                const char *vert_path,
                const char *frag_path) {
    memset(sb, 0, sizeof(*sb));

    /* Cubemap texture */
    sb->cubemap = load_cubemap(face_paths);
    if (!sb->cubemap) return 0;

    /* Shader */
    sb->shader = shader_create(vert_path, frag_path);
    if (!sb->shader) {
        fprintf(stderr, "Skybox: shader compile failed\n");
        glDeleteTextures(1, &sb->cubemap);
        return 0;
    }

    /* VAO / VBO */
    glGenVertexArrays(1, &sb->vao);
    glGenBuffers(1, &sb->vbo);
    glBindVertexArray(sb->vao);
    glBindBuffer(GL_ARRAY_BUFFER, sb->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(CUBE_VERTS), CUBE_VERTS, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    printf("Skybox: ready\n");
    return 1;
}

void skybox_draw(const Skybox *sb, mat4 view, mat4 proj, float brightness) {
    if (!sb->cubemap) return;

    /* Strip translation: copy view, zero out the 4th column/row translation */
    mat4 v;
    glm_mat4_copy(view, v);
    v[3][0] = 0.0f; v[3][1] = 0.0f; v[3][2] = 0.0f;
    v[0][3] = 0.0f; v[1][3] = 0.0f; v[2][3] = 0.0f;

    /* Draw at maximum depth with GL_LEQUAL so skybox sits behind everything */
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);

    glUseProgram(sb->shader);
    glUniformMatrix4fv(glGetUniformLocation(sb->shader,"uView"), 1,GL_FALSE,(float*)v);
    glUniformMatrix4fv(glGetUniformLocation(sb->shader,"uProj"), 1,GL_FALSE,(float*)proj);
    glUniform1f(glGetUniformLocation(sb->shader,"uBrightness"), brightness);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, sb->cubemap);
    glUniform1i(glGetUniformLocation(sb->shader,"uSkybox"), 0);

    glBindVertexArray(sb->vao);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);

    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

void skybox_free(Skybox *sb) {
    if (!sb) return;
    if (sb->cubemap) glDeleteTextures(1, &sb->cubemap);
    if (sb->shader)  glDeleteProgram(sb->shader);
    if (sb->vao)     glDeleteVertexArrays(1, &sb->vao);
    if (sb->vbo)     glDeleteBuffers(1, &sb->vbo);
    memset(sb, 0, sizeof(*sb));
}
