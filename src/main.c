#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <cglm/cglm.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "shader.h"
#include "camera.h"

#define WINDOW_WIDTH  1280
#define WINDOW_HEIGHT 720
#define WINDOW_TITLE  "TRON CRacer"

// ------------------------------------------------------------------ globals
Camera camera;
float  last_x      = WINDOW_WIDTH  / 2.0f;
float  last_y      = WINDOW_HEIGHT / 2.0f;
int    first_mouse = 1;
float  delta_time  = 0.0f;
float  last_frame  = 0.0f;

// ---------------------------------------------------------------- callbacks
void framebuffer_size_callback(GLFWwindow *w, int width, int height) {
    (void)w;
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow *w, double xpos, double ypos) {
    (void)w;
    if (first_mouse) { last_x = xpos; last_y = ypos; first_mouse = 0; }
    float xoff =  (float)(xpos - last_x);
    float yoff = -(float)(ypos - last_y);
    last_x = xpos; last_y = ypos;
    camera_process_mouse(&camera, xoff, yoff);
}

void process_input(GLFWwindow *w) {
    if (glfwGetKey(w, GLFW_KEY_ESCAPE)       == GLFW_PRESS) glfwSetWindowShouldClose(w, 1);
    if (glfwGetKey(w, GLFW_KEY_W)            == GLFW_PRESS) camera_process_keyboard(&camera, CAM_FORWARD,  delta_time);
    if (glfwGetKey(w, GLFW_KEY_S)            == GLFW_PRESS) camera_process_keyboard(&camera, CAM_BACKWARD, delta_time);
    if (glfwGetKey(w, GLFW_KEY_A)            == GLFW_PRESS) camera_process_keyboard(&camera, CAM_LEFT,     delta_time);
    if (glfwGetKey(w, GLFW_KEY_D)            == GLFW_PRESS) camera_process_keyboard(&camera, CAM_RIGHT,    delta_time);
    if (glfwGetKey(w, GLFW_KEY_SPACE)        == GLFW_PRESS) camera_process_keyboard(&camera, CAM_UP,       delta_time);
    if (glfwGetKey(w, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) camera_process_keyboard(&camera, CAM_DOWN,     delta_time);
}

// ===================================================================== MESH
typedef struct { GLuint vao, vbo; int vert_count; } Mesh;

static Mesh mesh_create(float *verts, int float_count) {
    Mesh m;
    m.vert_count = float_count / 6;
    glGenVertexArrays(1, &m.vao);
    glGenBuffers(1, &m.vbo);
    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, float_count * sizeof(float), verts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    return m;
}

static void mesh_draw(Mesh *m) {
    glBindVertexArray(m->vao);
    glDrawArrays(GL_TRIANGLES, 0, m->vert_count);
    glBindVertexArray(0);
}

static void mesh_free(Mesh *m) {
    glDeleteVertexArrays(1, &m->vao);
    glDeleteBuffers(1, &m->vbo);
}

// ==================================================================== BUILDERS
// Each vertex: x y z  nx ny nz

// Flat quad, normal up
static int add_quad_y(float *b, int off,
                      float x0, float z0, float x1, float z1, float y) {
    float q[6][6] = {
        {x0,y,z0,0,1,0},{x1,y,z0,0,1,0},{x0,y,z1,0,1,0},
        {x1,y,z0,0,1,0},{x1,y,z1,0,1,0},{x0,y,z1,0,1,0},
    };
    for (int i=0;i<6;i++) for(int j=0;j<6;j++) b[off++]=q[i][j];
    return off;
}

// Full solid box — 6 faces, 2 tris each
static int add_box(float *b, int off,
                   float x0, float y0, float z0,
                   float x1, float y1, float z1) {
    typedef struct { float n[3]; float v[4][3]; } Face;
    Face faces[6] = {
        {{ 0, 1, 0}, {{x0,y1,z0},{x1,y1,z0},{x1,y1,z1},{x0,y1,z1}}},
        {{ 0,-1, 0}, {{x0,y0,z1},{x1,y0,z1},{x1,y0,z0},{x0,y0,z0}}},
        {{ 1, 0, 0}, {{x1,y0,z0},{x1,y0,z1},{x1,y1,z1},{x1,y1,z0}}},
        {{-1, 0, 0}, {{x0,y0,z1},{x0,y0,z0},{x0,y1,z0},{x0,y1,z1}}},
        {{ 0, 0,-1}, {{x1,y0,z0},{x0,y0,z0},{x0,y1,z0},{x1,y1,z0}}},
        {{ 0, 0, 1}, {{x0,y0,z1},{x1,y0,z1},{x1,y1,z1},{x0,y1,z1}}},
    };
    int tris[2][3] = {{0,1,2},{0,2,3}};
    for (int f=0;f<6;f++) {
        for (int t=0;t<2;t++) {
            for (int k=0;k<3;k++) {
                float *vv = faces[f].v[tris[t][k]];
                b[off++]=vv[0]; b[off++]=vv[1]; b[off++]=vv[2];
                b[off++]=faces[f].n[0]; b[off++]=faces[f].n[1]; b[off++]=faces[f].n[2];
            }
        }
    }
    return off;
}

// Tapered pillar (square frustum): wider at bottom, narrower at top
// cx,cz = center; bh = bottom half-size; th = top half-size; y0,y1 = bottom/top Y
static int add_frustum(float *b, int off,
                       float cx, float cz,
                       float bh, float th,
                       float y0, float y1) {
    // 4 side faces + top + bottom
    // Side face vertices (from outside, CCW):
    //   BL_bottom, BR_bottom, BR_top, BL_top
    typedef struct { float v[4][3]; } SFace;
    SFace sides[4] = {
        // +X face
        {{{cx+bh,y0,cz-bh},{cx+bh,y0,cz+bh},{cx+th,y1,cz+th},{cx+th,y1,cz-th}}},
        // -X face
        {{{cx-bh,y0,cz+bh},{cx-bh,y0,cz-bh},{cx-th,y1,cz-th},{cx-th,y1,cz+th}}},
        // +Z face
        {{{cx+bh,y0,cz+bh},{cx-bh,y0,cz+bh},{cx-th,y1,cz+th},{cx+th,y1,cz+th}}},
        // -Z face
        {{{cx-bh,y0,cz-bh},{cx+bh,y0,cz-bh},{cx+th,y1,cz-th},{cx-th,y1,cz-th}}},
    };
    int tris[2][3] = {{0,1,2},{0,2,3}};
    for (int f=0;f<4;f++) {
        // Compute face normal from cross product
        float *v0 = sides[f].v[0], *v1 = sides[f].v[1], *v2 = sides[f].v[2];
        float e1[3] = {v1[0]-v0[0], v1[1]-v0[1], v1[2]-v0[2]};
        float e2[3] = {v2[0]-v0[0], v2[1]-v0[1], v2[2]-v0[2]};
        float n[3]  = {
            e1[1]*e2[2] - e1[2]*e2[1],
            e1[2]*e2[0] - e1[0]*e2[2],
            e1[0]*e2[1] - e1[1]*e2[0],
        };
        float len = sqrtf(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
        if (len > 0.0001f) { n[0]/=len; n[1]/=len; n[2]/=len; }
        for (int t=0;t<2;t++) {
            for (int k=0;k<3;k++) {
                float *vv = sides[f].v[tris[t][k]];
                b[off++]=vv[0]; b[off++]=vv[1]; b[off++]=vv[2];
                b[off++]=n[0];  b[off++]=n[1];  b[off++]=n[2];
            }
        }
    }
    // Top cap
    off = add_box(b, off, cx-th, y1-0.01f, cz-th, cx+th, y1, cz+th);
    // Bottom cap (optional but good for completeness)
    off = add_box(b, off, cx-bh, y0, cz-bh, cx+bh, y0+0.01f, cz+bh);
    return off;
}

// ==================================================================== SCENE

// 1. The infinite dark sea — large flat plane at y = 0
static Mesh build_sea(void) {
    float v[6*6];
    add_quad_y(v, 0, -600.0f, -600.0f, 600.0f, 600.0f, 0.0f);
    return mesh_create(v, 6*6);
}

// 2. Arena platform — square slab rising from the sea
//    Top at y = ARENA_TOP, base extends down below sea
#define ARENA_HALF  48.0f
#define ARENA_TOP    3.5f
#define ARENA_BASE  -4.0f

static Mesh build_arena(void) {
    // 216 floats per box (6 faces * 2 tris * 3 verts * 6 floats)
    float v[216];
    add_box(v, 0,
            -ARENA_HALF, ARENA_BASE, -ARENA_HALF,
             ARENA_HALF, ARENA_TOP,   ARENA_HALF);
    return mesh_create(v, 216);
}

// 3. Accent lines — thin glowing strips along all 4 top edges + grid
//    These will be drawn with emissive = cyan/white
static Mesh build_arena_accents(void) {
    // Edge strips: 4 edges * 216 floats + grid lines
    // Edge half-width
    float aw = 0.4f;
    float ay = ARENA_TOP;
    float ah = 0.25f; // height of accent strip
    float hs = ARENA_HALF;

    // Grid lines on the arena floor: every 8 units, thin raised strips
    int grid_spacing = 8;
    int grid_lines   = (int)(2 * hs / grid_spacing) - 1; // interior lines only
    int total_boxes  = 4 + grid_lines * 2;                // 4 edges + X and Z grid lines
    float *v = malloc(total_boxes * 216 * sizeof(float));
    int off  = 0;

    // Top edge strips (sitting on top surface)
    off = add_box(v, off, -hs,      ay, -hs,      hs,      ay+ah, -hs+aw); // -Z edge
    off = add_box(v, off, -hs,      ay,  hs-aw,   hs,      ay+ah,  hs   ); // +Z edge
    off = add_box(v, off, -hs,      ay, -hs,      -hs+aw,  ay+ah,  hs   ); // -X edge
    off = add_box(v, off,  hs-aw,   ay, -hs,       hs,     ay+ah,  hs   ); // +X edge

    // Interior grid lines (very thin, barely raised)
    float gw = 0.12f;
    float gh = 0.05f;
    for (int i = 1; i * grid_spacing < (int)(2 * hs); i++) {
        float pos = -hs + i * grid_spacing;
        // Line parallel to X axis
        off = add_box(v, off, -hs, ay, pos-gw, hs, ay+gh, pos+gw);
        // Line parallel to Z axis
        off = add_box(v, off, pos-gw, ay, -hs, pos+gw, ay+gh, hs);
    }

    Mesh m = mesh_create(v, off);
    free(v);
    return m;
}

// 4. Four corner pillars — tapered, centered beyond arena corners
//    fire sim placeholder on top later
#define PILLAR_DIST  80.0f   // distance from center to pillar center
#define PILLAR_BH     5.0f   // bottom half-size
#define PILLAR_TH     2.2f   // top half-size
#define PILLAR_HEIGHT 28.0f

static const float PILLAR_POS[4][2] = {
    {-PILLAR_DIST, -PILLAR_DIST},
    { PILLAR_DIST, -PILLAR_DIST},
    {-PILLAR_DIST,  PILLAR_DIST},
    { PILLAR_DIST,  PILLAR_DIST},
};

static Mesh build_pillars(void) {
    // Each frustum: ~4 side face pairs + 2 caps = lots of floats; budget 2000 per pillar
    float *v = malloc(4 * 4000 * sizeof(float));
    int off  = 0;
    for (int i = 0; i < 4; i++) {
        float cx = PILLAR_POS[i][0], cz = PILLAR_POS[i][1];
        off = add_frustum(v, off, cx, cz,
                          PILLAR_BH, PILLAR_TH,
                          ARENA_BASE, PILLAR_HEIGHT);
    }
    Mesh m = mesh_create(v, off);
    free(v);
    return m;
}

// Pillar accent rings — thin bright boxes around top of each pillar
static Mesh build_pillar_accents(void) {
    float *v = malloc(4 * 8 * 216 * sizeof(float));
    int off  = 0;
    for (int i = 0; i < 4; i++) {
        float cx = PILLAR_POS[i][0], cz = PILLAR_POS[i][1];
        float th = PILLAR_TH;
        float yt = PILLAR_HEIGHT;
        float aw = 0.25f, ah = 0.4f;
        // Ring of 4 thin boxes around the top perimeter
        off = add_box(v,off, cx-th-aw, yt-ah, cz-th-aw, cx+th+aw, yt, cz-th);       // -Z
        off = add_box(v,off, cx-th-aw, yt-ah, cz+th,    cx+th+aw, yt, cz+th+aw);    // +Z
        off = add_box(v,off, cx-th-aw, yt-ah, cz-th,    cx-th,    yt, cz+th);        // -X
        off = add_box(v,off, cx+th,    yt-ah, cz-th,    cx+th+aw, yt, cz+th);        // +X
        // Mid-height accent band
        float ym = PILLAR_HEIGHT * 0.5f;
        float bh_at_mid = PILLAR_BH + (PILLAR_TH - PILLAR_BH) * 0.5f;
        off = add_box(v,off, cx-bh_at_mid-aw, ym-ah*0.5f, cz-bh_at_mid-aw,
                             cx+bh_at_mid+aw, ym,          cz-bh_at_mid);
        off = add_box(v,off, cx-bh_at_mid-aw, ym-ah*0.5f, cz+bh_at_mid,
                             cx+bh_at_mid+aw, ym,          cz+bh_at_mid+aw);
        off = add_box(v,off, cx-bh_at_mid-aw, ym-ah*0.5f, cz-bh_at_mid,
                             cx-bh_at_mid,    ym,          cz+bh_at_mid);
        off = add_box(v,off, cx+bh_at_mid,    ym-ah*0.5f, cz-bh_at_mid,
                             cx+bh_at_mid+aw, ym,          cz+bh_at_mid);
    }
    Mesh m = mesh_create(v, off);
    free(v);
    return m;
}

// ==================================================================== HELPERS

static void set_normal_object(GLuint sh, float r, float g, float b) {
    shader_set_vec3(sh, "objectColor", r, g, b);
    shader_set_vec3(sh, "emissive",    0.0f, 0.0f, 0.0f);
    shader_set_float(sh, "ambientStr", 0.12f);
}

static void set_emissive_object(GLuint sh,
                                 float or, float og, float ob,
                                 float er, float eg, float eb) {
    shader_set_vec3(sh, "objectColor", or, og, ob);
    shader_set_vec3(sh, "emissive",    er, eg, eb);
    shader_set_float(sh, "ambientStr", 0.05f);
}

// ==================================================================== MAIN
int main(void) {
    if (!glfwInit()) { fprintf(stderr, "GLFW init failed\n"); return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE, NULL, NULL);
    if (!window) { fprintf(stderr, "Window creation failed\n"); glfwTerminate(); return -1; }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGL(glfwGetProcAddress)) { fprintf(stderr, "GLAD init failed\n"); return -1; }

    glEnable(GL_DEPTH_TEST);
    printf("OpenGL %s\n", glGetString(GL_VERSION));

    // Start above and behind, looking at the arena
    camera_init(&camera, 0.0f, 70.0f, 130.0f);

    GLuint shader = shader_create("../shaders/vertex.glsl", "../shaders/fragment.glsl");

    // Build scene
    Mesh sea             = build_sea();
    Mesh arena           = build_arena();
    Mesh arena_accents   = build_arena_accents();
    Mesh pillars         = build_pillars();
    Mesh pillar_accents  = build_pillar_accents();

    while (!glfwWindowShouldClose(window)) {
        float now  = (float)glfwGetTime();
        delta_time = now - last_frame;
        last_frame = now;

        process_input(window);

        // Near-black sky with a very slight blue tint
        glClearColor(0.01f, 0.01f, 0.04f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader_use(shader);

        mat4 view, proj, model;
        camera_get_view(&camera, view);
        camera_get_projection(&camera, proj, (float)WINDOW_WIDTH / WINDOW_HEIGHT);
        glm_mat4_identity(model);

        shader_set_mat4(shader, "view",       (float*)view);
        shader_set_mat4(shader, "projection", (float*)proj);
        shader_set_mat4(shader, "model",      (float*)model);

        // Overhead bluish-white light — cold and distant like the Grid
        shader_set_vec3(shader, "lightPos",   0.0f, 300.0f, 0.0f);
        shader_set_vec3(shader, "lightColor", 0.6f, 0.75f,  1.0f);
        shader_set_vec3(shader, "viewPos",
            camera.position[0], camera.position[1], camera.position[2]);

        // Sea — very dark navy, slightly reflective
        set_normal_object(shader, 0.02f, 0.04f, 0.12f);
        mesh_draw(&sea);

        // Arena slab — dark charcoal
        set_normal_object(shader, 0.07f, 0.07f, 0.10f);
        mesh_draw(&arena);

        // Arena accent lines — bright cyan glow
        set_emissive_object(shader,
            0.7f, 1.0f, 1.0f,   // object color (white-cyan base)
            0.0f, 0.35f, 0.45f  // emissive (cyan glow additive)
        );
        mesh_draw(&arena_accents);

        // Pillars — dark, slightly lighter than arena
        set_normal_object(shader, 0.10f, 0.10f, 0.14f);
        mesh_draw(&pillars);

        // Pillar accent rings — bright white glow
        set_emissive_object(shader,
            0.9f, 0.95f, 1.0f,  // object color
            0.2f, 0.25f, 0.35f  // emissive
        );
        mesh_draw(&pillar_accents);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    mesh_free(&sea);
    mesh_free(&arena);
    mesh_free(&arena_accents);
    mesh_free(&pillars);
    mesh_free(&pillar_accents);
    glDeleteProgram(shader);
    glfwTerminate();
    return 0;
}
