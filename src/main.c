#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <cglm/cglm.h>
#include <stdio.h>
#include <stdlib.h>

#include "shader.h"
#include "camera.h"

#define WINDOW_WIDTH  1280
#define WINDOW_HEIGHT 720
#define WINDOW_TITLE  "CRacer"

// ------------------------------------------------------------------ globals
Camera camera;
float  last_x      = WINDOW_WIDTH  / 2.0f;
float  last_y      = WINDOW_HEIGHT / 2.0f;
int    first_mouse = 1;
float  delta_time  = 0.0f;
float  last_frame  = 0.0f;

// ---------------------------------------------------------------- callbacks
void framebuffer_size_callback(GLFWwindow *w, int width, int height) {
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow *w, double xpos, double ypos) {
    (void)w;
    if (first_mouse) { last_x = xpos; last_y = ypos; first_mouse = 0; }
    float xoff =  (float)(xpos - last_x);
    float yoff = -(float)(ypos - last_y); // inverted y
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

// ------------------------------------------------------------------- meshes
typedef struct { GLuint vao, vbo; int vert_count; } Mesh;

Mesh mesh_create(float *verts, int float_count) {
    Mesh m;
    m.vert_count = float_count / 6;
    glGenVertexArrays(1, &m.vao);
    glGenBuffers(1, &m.vbo);
    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, float_count * sizeof(float), verts, GL_STATIC_DRAW);
    // position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    return m;
}

void mesh_draw(Mesh *m) {
    glBindVertexArray(m->vao);
    glDrawArrays(GL_TRIANGLES, 0, m->vert_count);
    glBindVertexArray(0);
}

void mesh_free(Mesh *m) {
    glDeleteVertexArrays(1, &m->vao);
    glDeleteBuffers(1, &m->vbo);
}

// Append a flat horizontal quad (y = level) into buf. Returns new offset.
static int add_quad(float *buf, int off,
                    float x0, float z0, float x1, float z1, float y) {
    float q[6][6] = {
        {x0,y,z0,0,1,0}, {x1,y,z0,0,1,0}, {x0,y,z1,0,1,0},
        {x1,y,z0,0,1,0}, {x1,y,z1,0,1,0}, {x0,y,z1,0,1,0},
    };
    for (int i = 0; i < 6; i++)
        for (int j = 0; j < 6; j++)
            buf[off++] = q[i][j];
    return off;
}

// Append a box (6 faces, 2 tris each) into buf. Returns new offset.
static int add_box(float *buf, int off,
                   float x0, float y0, float z0,
                   float x1, float y1, float z1) {
    typedef struct { float n[3]; float v[4][3]; } Face;
    Face faces[6] = {
        {{ 0, 1, 0}, {{x0,y1,z0},{x1,y1,z0},{x1,y1,z1},{x0,y1,z1}}},
        {{ 0,-1, 0}, {{x0,y0,z1},{x1,y0,z1},{x1,y0,z0},{x0,y0,z0}}},
        {{ 1, 0, 0}, {{x1,y0,z0},{x1,y0,z1},{x1,y1,z1},{x1,y1,z0}}},
        {{-1, 0, 0}, {{x0,y0,z1},{x0,y0,z0},{x0,y1,z0},{x0,y1,z1}}},
        {{ 0, 0,-1}, {{x0,y0,z0},{x1,y0,z0},{x1,y1,z0},{x0,y1,z0}}},
        {{ 0, 0, 1}, {{x1,y0,z1},{x0,y0,z1},{x0,y1,z1},{x1,y1,z1}}},
    };
    for (int f = 0; f < 6; f++) {
        float nx = faces[f].n[0], ny = faces[f].n[1], nz = faces[f].n[2];
        int tri[2][3] = {{0,1,2},{0,2,3}};
        for (int t = 0; t < 2; t++) {
            for (int k = 0; k < 3; k++) {
                float *vv = faces[f].v[tri[t][k]];
                buf[off++]=vv[0]; buf[off++]=vv[1]; buf[off++]=vv[2];
                buf[off++]=nx;    buf[off++]=ny;    buf[off++]=nz;
            }
        }
    }
    return off;
}

// -------------------------------------------------------------------- scene
static Mesh build_ground(void) {
    float v[6*6];
    add_quad(v, 0, -200.0f, -200.0f, 200.0f, 200.0f, 0.0f);
    return mesh_create(v, 6*6);
}

static Mesh build_lake(float ix, float iz) {
    float v[6*6];
    add_quad(v, 0, -ix, -iz, ix, iz, 0.005f);
    return mesh_create(v, 6*6);
}

// Track layout (XZ top-down):
//   Outer: x[-ox,ox], z[-oz,oz]
//   Inner: x[-ix,ix], z[-iz,iz]   <-- lake/grass inside
static Mesh build_track(float ox, float oz, float ix, float iz) {
    float v[8 * 6 * 6];
    int off = 0;
    float y = 0.01f;
    off = add_quad(v, off, -ix, -oz,  ix, -iz, y); // top straight
    off = add_quad(v, off, -ix,  iz,  ix,  oz, y); // bottom straight
    off = add_quad(v, off, -ox, -iz, -ix,  iz, y); // left straight
    off = add_quad(v, off,  ix, -iz,  ox,  iz, y); // right straight
    off = add_quad(v, off, -ox, -oz, -ix, -iz, y); // top-left corner
    off = add_quad(v, off,  ix, -oz,  ox, -iz, y); // top-right corner
    off = add_quad(v, off, -ox,  iz, -ix,  oz, y); // bottom-left corner
    off = add_quad(v, off,  ix,  iz,  ox,  oz, y); // bottom-right corner
    return mesh_create(v, 8 * 6 * 6);
}

static const float TREE_POS[][2] = {
    {-60,-30},{-50,-30},{-38,-30},{-25,-30},{-10,-30},
    {  5,-30},{ 18,-30},{ 32,-30},{ 45,-30},{ 58,-30},
    {-60, 30},{-50, 30},{-38, 30},{-25, 30},{-10, 30},
    {  5, 30},{ 18, 30},{ 32, 30},{ 45, 30},{ 58, 30},
    {-60,-15},{-60,  0},{-60, 15},
    { 60,-15},{ 60,  0},{ 60, 15},
};
#define TREE_COUNT (int)(sizeof(TREE_POS)/sizeof(TREE_POS[0]))
// 6 faces * 2 tris * 3 verts * 6 floats = 216 floats per box
#define FLOATS_PER_BOX 216

static Mesh build_tree_trunks(void) {
    float *v = malloc(TREE_COUNT * FLOATS_PER_BOX * sizeof(float));
    int off = 0;
    for (int i = 0; i < TREE_COUNT; i++) {
        float x = TREE_POS[i][0], z = TREE_POS[i][1];
        off = add_box(v, off, x-0.5f,0.0f,z-0.5f, x+0.5f,4.0f,z+0.5f);
    }
    Mesh m = mesh_create(v, off);
    free(v);
    return m;
}

static Mesh build_tree_canopy(void) {
    float *v = malloc(TREE_COUNT * FLOATS_PER_BOX * sizeof(float));
    int off = 0;
    for (int i = 0; i < TREE_COUNT; i++) {
        float x = TREE_POS[i][0], z = TREE_POS[i][1];
        off = add_box(v, off, x-2.5f,4.0f,z-2.5f, x+2.5f,10.0f,z+2.5f);
    }
    Mesh m = mesh_create(v, off);
    free(v);
    return m;
}

// --------------------------------------------------------------------- main
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

    camera_init(&camera, 0.0f, 50.0f, 90.0f);

    GLuint shader = shader_create("../shaders/vertex.glsl", "../shaders/fragment.glsl");

    float ox=45.0f, oz=22.0f, ix=22.0f, iz=8.0f;

    Mesh ground      = build_ground();
    Mesh lake        = build_lake(ix, iz);
    Mesh track       = build_track(ox, oz, ix, iz);
    Mesh trunks      = build_tree_trunks();
    Mesh canopy      = build_tree_canopy();

    while (!glfwWindowShouldClose(window)) {
        float now  = (float)glfwGetTime();
        delta_time = now - last_frame;
        last_frame = now;

        process_input(window);

        glClearColor(0.52f, 0.80f, 0.92f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader_use(shader);

        mat4 view, proj, model;
        camera_get_view(&camera, view);
        camera_get_projection(&camera, proj, (float)WINDOW_WIDTH / WINDOW_HEIGHT);
        glm_mat4_identity(model);

        shader_set_mat4(shader, "view",       (float*)view);
        shader_set_mat4(shader, "projection", (float*)proj);
        shader_set_mat4(shader, "model",      (float*)model);

        shader_set_vec3(shader, "lightPos",    60.0f, 120.0f,  60.0f);
        shader_set_vec3(shader, "lightColor",   1.0f,   0.98f,  0.90f);
        shader_set_vec3(shader, "viewPos",
            camera.position[0], camera.position[1], camera.position[2]);

        shader_set_vec3(shader, "objectColor", 0.22f, 0.55f, 0.12f);
        mesh_draw(&ground);

        shader_set_vec3(shader, "objectColor", 0.28f, 0.28f, 0.30f);
        mesh_draw(&track);

        shader_set_vec3(shader, "objectColor", 0.10f, 0.40f, 0.75f);
        mesh_draw(&lake);

        shader_set_vec3(shader, "objectColor", 0.42f, 0.26f, 0.08f);
        mesh_draw(&trunks);

        shader_set_vec3(shader, "objectColor", 0.10f, 0.42f, 0.10f);
        mesh_draw(&canopy);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    mesh_free(&ground); mesh_free(&lake); mesh_free(&track);
    mesh_free(&trunks); mesh_free(&canopy);
    glDeleteProgram(shader);
    glfwTerminate();
    return 0;
}
