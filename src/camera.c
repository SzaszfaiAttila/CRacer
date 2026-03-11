#include "camera.h"

static void update_vectors(Camera *c) {
    vec3 front = {
        cosf(glm_rad(c->yaw)) * cosf(glm_rad(c->pitch)),
        sinf(glm_rad(c->pitch)),
        sinf(glm_rad(c->yaw)) * cosf(glm_rad(c->pitch))
    };
    glm_normalize_to(front, c->front);

    vec3 world_up = {0.0f, 1.0f, 0.0f};
    glm_cross(c->front, world_up, c->right);
    glm_normalize(c->right);
    glm_cross(c->right, c->front, c->up);
    glm_normalize(c->up);
}

void camera_init(Camera *c, float x, float y, float z) {
    c->position[0] = x; c->position[1] = y; c->position[2] = z;
    c->yaw         = -90.0f;
    c->pitch       = -25.0f;
    c->speed       = 20.0f;
    c->sensitivity = 0.1f;
    c->fov         = 60.0f;
    update_vectors(c);
}

void camera_get_view(Camera *c, mat4 out) {
    vec3 target;
    glm_vec3_add(c->position, c->front, target);
    glm_lookat(c->position, target, c->up, out);
}

void camera_get_projection(Camera *c, mat4 out, float aspect) {
    glm_perspective(glm_rad(c->fov), aspect, 0.1f, 1000.0f, out);
}

void camera_process_keyboard(Camera *c, int dir, float dt) {
    float v = c->speed * dt;
    vec3 tmp;
    if (dir == CAM_FORWARD)  { glm_vec3_scale(c->front, v, tmp); glm_vec3_add(c->position, tmp, c->position); }
    if (dir == CAM_BACKWARD) { glm_vec3_scale(c->front, v, tmp); glm_vec3_sub(c->position, tmp, c->position); }
    if (dir == CAM_RIGHT)    { glm_vec3_scale(c->right, v, tmp); glm_vec3_add(c->position, tmp, c->position); }
    if (dir == CAM_LEFT)     { glm_vec3_scale(c->right, v, tmp); glm_vec3_sub(c->position, tmp, c->position); }
    if (dir == CAM_UP)       { c->position[1] += v; }
    if (dir == CAM_DOWN)     { c->position[1] -= v; }
}

void camera_process_mouse(Camera *c, float xoff, float yoff) {
    c->yaw   += xoff * c->sensitivity;
    c->pitch += yoff * c->sensitivity;
    if (c->pitch >  89.0f) c->pitch =  89.0f;
    if (c->pitch < -89.0f) c->pitch = -89.0f;
    update_vectors(c);
}
