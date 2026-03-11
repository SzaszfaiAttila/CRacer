#ifndef CAMERA_H
#define CAMERA_H

#include <cglm/cglm.h>

#define CAM_FORWARD  0
#define CAM_BACKWARD 1
#define CAM_LEFT     2
#define CAM_RIGHT    3
#define CAM_UP       4
#define CAM_DOWN     5

typedef struct {
    vec3  position;
    vec3  front;
    vec3  up;
    vec3  right;
    float yaw;
    float pitch;
    float speed;
    float sensitivity;
    float fov;
} Camera;

void camera_init(Camera *cam, float x, float y, float z);
void camera_get_view(Camera *cam, mat4 out);
void camera_get_projection(Camera *cam, mat4 out, float aspect);
void camera_process_keyboard(Camera *cam, int dir, float dt);
void camera_process_mouse(Camera *cam, float xoff, float yoff);

#endif
