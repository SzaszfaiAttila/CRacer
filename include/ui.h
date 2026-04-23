#ifndef UI_H
#define UI_H

#include <glad/gl.h>

/* ── 2-D UI rendering context ─────────────────────────────────────────────── */
typedef struct {
    GLuint prog;
    GLuint vao, vbo;
    GLuint font_tex;   /* 1024×8 GL_R8 bitmap-font atlas */
    float  sw, sh;     /* current screen dimensions       */
} UI;

/* Create / destroy */
void  ui_init   (UI *u, int sw, int sh);
void  ui_free   (UI *u);
void  ui_resize (UI *u, int sw, int sh);

/* Call once before UI draws, once after */
void  ui_begin  (UI *u);
void  ui_end    (UI *u);

/* Draw a filled axis-aligned rectangle (pixels, top-left origin) */
void  ui_rect   (UI *u, float x, float y, float w, float h,
                 float r, float g, float b, float a);

/* Draw text string.  sz = character height in pixels (aspect is 1:1). */
void  ui_text   (UI *u, const char *s, float x, float y,
                 float sz, float r, float g, float b, float a);

/* Pixel width of a string at given character size */
float ui_textw  (const char *s, float sz);

#endif /* UI_H */