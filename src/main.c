#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <cglm/cglm.h>
#include <stdio.h>

#include "config.h"
#include "shader.h"
#include "camera.h"
#include "mesh.h"
#include "scene.h"
#include "bike.h"
#include "particles.h"
#include "ai.h"

// ── Number of AI opponents (1–6) ─────────────────────────────────────────────
#define N_AI 6

// ── Camera modes ──────────────────────────────────────────────────────────────
typedef enum { CAM_FOLLOW=0, CAM_TOPDOWN=1, CAM_FREE=2 } CamMode;

// ── Trail buffers ─────────────────────────────────────────────────────────────
static float player_trail_buf         [TRAIL_BUF_FLOATS];
static float ai_trail_bufs[MAX_AI]    [TRAIL_BUF_FLOATS];

// ── Globals ───────────────────────────────────────────────────────────────────
static Camera  camera;
static float   last_x, last_y;
static int     first_mouse = 1;
static float   delta_time  = 0.0f, last_frame = 0.0f;
static CamMode cam_mode    = CAM_FOLLOW;
static int     show_help   = 0;
static float   follow_zoom = 0.5f;
static float   cam_px=0, cam_py=70, cam_pz=130;

// Death particle tracking (so we only spawn the explosion ONCE when a bike dies)
static int prev_bike_alive[1 + MAX_AI];

// ── Shader helpers ────────────────────────────────────────────────────────────
static void draw_obj(GLuint sh, float r, float g, float b) {
    shader_set_vec3(sh,"objectColor",r,g,b);
    shader_set_vec3(sh,"emissive",0,0,0);
    shader_set_float(sh,"ambientStr",0.12f);
}
static void draw_emissive(GLuint sh,
                           float or, float og, float ob,
                           float er, float eg, float eb) {
    shader_set_vec3(sh,"objectColor",or,og,ob);
    shader_set_vec3(sh,"emissive",er,eg,eb);
    shader_set_float(sh,"ambientStr",0.05f);
}

// ── Callbacks ─────────────────────────────────────────────────────────────────
static void framebuffer_size_cb(GLFWwindow *w, int W, int H) {
    (void)w; glViewport(0,0,W,H);
}
static void scroll_cb(GLFWwindow *w, double xoff, double yoff) {
    (void)w;(void)xoff;
    if (cam_mode!=CAM_FOLLOW) return;
    follow_zoom -= (float)yoff*0.08f;
    if (follow_zoom<0.0f) follow_zoom=0.0f;
    if (follow_zoom>1.0f) follow_zoom=1.0f;
}
static void mouse_cb(GLFWwindow *w, double xpos, double ypos) {
    (void)w;
    if (first_mouse){last_x=xpos;last_y=ypos;first_mouse=0;}
    float xo=(float)(xpos-last_x), yo=-(float)(ypos-last_y);
    last_x=xpos; last_y=ypos;
    if (cam_mode==CAM_FREE) camera_process_mouse(&camera,xo,yo);
}

// ── Main ──────────────────────────────────────────────────────────────────────
int main(void) {
    if (!glfwInit()) {
        fprintf(stderr, "GLFW fail\n");
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // ── Select monitor from config ─────────────────────────────────────────────
    int monitorCount;
    GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);

    GLFWmonitor* targetMonitor;
    if (MONITOR_INDEX >= 0 && MONITOR_INDEX < monitorCount) {
        targetMonitor = monitors[MONITOR_INDEX];
    } else {
        targetMonitor = glfwGetPrimaryMonitor();   // safety fallback
    }

    // Debug info so you know exactly which monitor is being used
    const char* monitorName = glfwGetMonitorName(targetMonitor);
    printf("Using monitor %d/%d → %s\n",
           MONITOR_INDEX, monitorCount,
           monitorName ? monitorName : "Unknown");

    const GLFWvidmode* vmode = glfwGetVideoMode(targetMonitor);

    GLFWwindow* win = glfwCreateWindow(vmode->width, vmode->height,
                                       WINDOW_TITLE,
                                       targetMonitor,   // real exclusive fullscreen
                                       NULL);

    if (!win) {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(win);
    glfwSetFramebufferSizeCallback(win,framebuffer_size_cb);
    glfwSetCursorPosCallback(win,mouse_cb);
    glfwSetScrollCallback(win,scroll_cb);
    glfwSetInputMode(win,GLFW_CURSOR,GLFW_CURSOR_DISABLED);

    if (!gladLoadGL(glfwGetProcAddress)){fprintf(stderr,"GLAD fail\n");return -1;}
    glEnable(GL_DEPTH_TEST);
    printf("OpenGL %s  |  %d AI opponent%s\n",
           glGetString(GL_VERSION), N_AI, N_AI==1?"":"s");
    printf("Press F1 for controls\n");

    camera_init(&camera,0.0f,70.0f,130.0f);

    GLuint sh  = shader_create("../shaders/vertex.glsl","../shaders/fragment.glsl");
    GLuint psh = shader_create("../shaders/particle_vert.glsl","../shaders/particle_frag.glsl");
    GLuint wsh = shader_create("../shaders/water_vert.glsl","../shaders/water_frag.glsl");

    particles_init();
    Scene   scene        = scene_build();
    Bike    bike;        bike_reset(&bike);
    DynMesh player_trail = dynmesh_create(TRAIL_BUF_FLOATS);

    AIBike  ai_bikes[MAX_AI];
    DynMesh ai_trail_meshes[MAX_AI];
    for (int i=0;i<N_AI;i++){
        ai_init(&ai_bikes[i],i);
        ai_trail_meshes[i] = dynmesh_create(TRAIL_BUF_FLOATS);
    }

    PMesh part_mesh = pmesh_create(PART_BUF_FLOATS);

    // All-bikes pointer array: [0]=player, [1..N_AI]=AIs
    Bike *all_bikes[1+MAX_AI];
    all_bikes[0] = &bike;
    for (int i=0;i<N_AI;i++) all_bikes[1+i] = &ai_bikes[i].bike;
    int n_all = 1+N_AI;

    int prev_a=0,prev_d=0,prev_tab=0,prev_c=0,prev_r=0,prev_f1=0;
    int player_dead_announced=0;

    // Initialize death tracking
    prev_bike_alive[0] = 1;
    for (int i = 0; i < N_AI; i++) prev_bike_alive[i + 1] = 1;

    while (!glfwWindowShouldClose(win)){
        float now  = (float)glfwGetTime();
        delta_time = now-last_frame; last_frame=now;

        // ── Input ─────────────────────────────────────────────────────────────
        if (glfwGetKey(win,GLFW_KEY_ESCAPE)==GLFW_PRESS) glfwSetWindowShouldClose(win,1);

        int cur_a  =glfwGetKey(win,GLFW_KEY_A)  ==GLFW_PRESS;
        int cur_d  =glfwGetKey(win,GLFW_KEY_D)  ==GLFW_PRESS;
        int cur_tab=glfwGetKey(win,GLFW_KEY_TAB) ==GLFW_PRESS;
        int cur_c  =glfwGetKey(win,GLFW_KEY_C)  ==GLFW_PRESS;
        int cur_r  =glfwGetKey(win,GLFW_KEY_R)  ==GLFW_PRESS;
        int cur_f1 =glfwGetKey(win,GLFW_KEY_F1) ==GLFW_PRESS;

        if (cam_mode!=CAM_FREE){
            if (cur_a&&!prev_a) bike_turn_left (&bike);
            if (cur_d&&!prev_d) bike_turn_right(&bike);
        }
        if (cur_tab&&!prev_tab){cam_mode=(cam_mode==CAM_FREE)?CAM_FOLLOW:CAM_FREE;first_mouse=1;}
        if (cur_c  &&!prev_c  ){cam_mode=(cam_mode==CAM_TOPDOWN)?CAM_FOLLOW:CAM_TOPDOWN;}
        if (cur_r  &&!prev_r  ){
            bike_reset(&bike);
            for (int i=0;i<N_AI;i++) ai_reset(&ai_bikes[i],i);
            player_dead_announced=0;

            // Reset death tracking on full restart
            prev_bike_alive[0] = 1;
            for (int i = 0; i < N_AI; i++) prev_bike_alive[i + 1] = 1;
        }
        if (cur_f1&&!prev_f1){
            show_help=!show_help;
            if(show_help){
                printf("\n╔═══════════════════════════════╗\n");
                printf("║       CRACER  CONTROLS        ║\n");
                printf("╠═══════════════════════════════╣\n");
                printf("║  A / D        Turn bike       ║\n");
                printf("║  R            Restart         ║\n");
                printf("║  C            Top-down view   ║\n");
                printf("║  Tab          Free camera     ║\n");
                printf("║  (Free cam)   WASD + mouse    ║\n");
                printf("║  Space/Ctrl   Free cam up/dn  ║\n");
                printf("║  Scroll       Zoom follow cam ║\n");
                printf("║  F1           Toggle this help║\n");
                printf("║  Esc          Quit            ║\n");
                printf("╚═══════════════════════════════╝\n\n");
            }
        }
        prev_a=cur_a;prev_d=cur_d;prev_tab=cur_tab;
        prev_c=cur_c;prev_r=cur_r;prev_f1=cur_f1;

        if (cam_mode==CAM_FREE){
            if(glfwGetKey(win,GLFW_KEY_W)           ==GLFW_PRESS) camera_process_keyboard(&camera,CAM_FORWARD, delta_time);
            if(glfwGetKey(win,GLFW_KEY_S)           ==GLFW_PRESS) camera_process_keyboard(&camera,CAM_BACKWARD,delta_time);
            if(glfwGetKey(win,GLFW_KEY_A)           ==GLFW_PRESS) camera_process_keyboard(&camera,CAM_LEFT,    delta_time);
            if(glfwGetKey(win,GLFW_KEY_D)           ==GLFW_PRESS) camera_process_keyboard(&camera,CAM_RIGHT,   delta_time);
            if(glfwGetKey(win,GLFW_KEY_SPACE)       ==GLFW_PRESS) camera_process_keyboard(&camera,CAM_UP,      delta_time);
            if(glfwGetKey(win,GLFW_KEY_LEFT_CONTROL)==GLFW_PRESS) camera_process_keyboard(&camera,CAM_DOWN,    delta_time);
        }

        // ── Count alive AIs ───────────────────────────────────────────────────
        int n_ai_alive = 0;
        for (int i=0;i<N_AI;i++) if (ai_bikes[i].bike.alive) n_ai_alive++;

        // ── Pack coordination (before individual updates) ─────────────────────
        ai_pack_coordinator(ai_bikes, N_AI, n_ai_alive, &bike, delta_time);

        // ── Update bikes ──────────────────────────────────────────────────────
        bike_update(&bike, delta_time, all_bikes, n_all);
        if (!bike.alive && !player_dead_announced){
            printf("You died! Press R to restart.\n");
            player_dead_announced=1;
        }

        for (int i=0;i<N_AI;i++)
            ai_update(&ai_bikes[i], delta_time, all_bikes, n_all, &bike, n_ai_alive);

        // Spawn death explosion when a bike dies
        if (!bike.alive && prev_bike_alive[0]) {
            float death_y = bike.falling ? ARENA_BASE : ARENA_TOP;
            particles_spawn_death_cloud(bike.x, bike.z, bike.dir, BIKE_SPEED,
                                        0.6f, 1.0f, 1.0f, death_y);
            prev_bike_alive[0] = 0;
        }

        for (int i = 0; i < N_AI; i++) {
            Bike *ab = &ai_bikes[i].bike;
            if (!ab->alive && prev_bike_alive[i + 1]) {
                float death_y = ab->falling ? ARENA_BASE : ARENA_TOP;
                particles_spawn_death_cloud(ab->x, ab->z, ab->dir, BIKE_SPEED,
                                            1.0f, 0.5f, 0.05f, death_y);
                prev_bike_alive[i + 1] = 0;
            }
        }

        // ── Trail geometry ────────────────────────────────────────────────────
        trail_w = (cam_mode==CAM_TOPDOWN) ? 0.3f : 0.1f;
        dynmesh_update(&player_trail, player_trail_buf,
                       bike_build_trail(&bike, player_trail_buf));
        for (int i=0;i<N_AI;i++)
            dynmesh_update(&ai_trail_meshes[i], ai_trail_bufs[i],
                           bike_build_trail(&ai_bikes[i].bike, ai_trail_bufs[i]));

        particles_update(delta_time);
        particles_update_death(delta_time);        // ← NEW: death particles

        // ── Camera ────────────────────────────────────────────────────────────
        if (cam_mode==CAM_FOLLOW){
            float dist  =10.0f+follow_zoom*28.0f;
            float height= 5.0f+follow_zoom*17.0f;
            float ahead = 4.0f+follow_zoom* 4.0f;
            float tx=bike.x-DDX[bike.dir]*dist, ty=ARENA_TOP+height, tz=bike.z-DDZ[bike.dir]*dist;
            float lp=1.0f-expf(-8.0f*delta_time);
            cam_px+=(tx-cam_px)*lp; cam_py+=(ty-cam_py)*lp; cam_pz+=(tz-cam_pz)*lp;
            camera.position[0]=cam_px;camera.position[1]=cam_py;camera.position[2]=cam_pz;

            vec3 look={bike.x+DDX[bike.dir]*ahead,ARENA_TOP+1.5f,bike.z+DDZ[bike.dir]*ahead};
            glm_vec3_sub(look,camera.position,camera.front);
            glm_normalize(camera.front);

            // Proper camera right + up vectors (this fixes the particle lines)
            vec3 world_up = {0.0f, 1.0f, 0.0f};
            glm_vec3_cross(camera.front, world_up, camera.right);
            glm_normalize(camera.right);
            glm_vec3_cross(camera.right, camera.front, camera.up);
            glm_normalize(camera.up);
        } else if (cam_mode==CAM_TOPDOWN){
            camera.position[0]=0;camera.position[1]=150;camera.position[2]=0;
            camera.front[0]=0;camera.front[1]=-1;camera.front[2]=-0.001f;
            glm_normalize(camera.front);
            camera.up[0]=0;camera.up[1]=0;camera.up[2]=-1;
        }

        // ── Render ────────────────────────────────────────────────────────────
        glClearColor(0.01f,0.01f,0.04f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        shader_use(sh);

        mat4 view,proj,model;
        camera_get_view(&camera,view);
        if (cam_mode==CAM_TOPDOWN){
            float aspect=(float)vmode->width/vmode->height;
            float hs=ARENA_HALF+10.0f;
            glm_ortho(-hs*aspect,hs*aspect,-hs,hs,0.1f,500.0f,proj);
        } else {
            camera_get_projection(&camera,proj,(float)vmode->width/vmode->height);
        }

        // ── Shared constants ──────────────────────────────────────────────────
        static const float MOON_X =  150.0f;
        static const float MOON_Y =   48.0f;
        static const float MOON_Z = -560.0f;

        static const float FOG_R = 0.01f, FOG_G = 0.01f, FOG_B = 0.04f;
        static const float FOG_DENSITY = 0.0022f;

        // ── Main scene shader setup ───────────────────────────────────────────
        glm_mat4_identity(model);
        shader_use(sh);
        shader_set_mat4(sh,"view",      (float*)view);
        shader_set_mat4(sh,"projection",(float*)proj);
        shader_set_mat4(sh,"model",     (float*)model);
        shader_set_vec3(sh,"lightPos",  MOON_X, MOON_Y+200.0f, MOON_Z);
        shader_set_vec3(sh,"lightColor",0.55f,0.65f,0.90f);
        shader_set_vec3(sh,"viewPos",
            camera.position[0],camera.position[1],camera.position[2]);
        shader_set_vec3(sh,"fogColor",   FOG_R,FOG_G,FOG_B);
        shader_set_float(sh,"fogDensity",FOG_DENSITY);

        // ── Moon disc ─────────────────────────────────────────────────────────
        {
            glm_mat4_identity(model);
            vec3 mpos = {MOON_X, MOON_Y, MOON_Z};
            glm_translate(model, mpos);
            shader_set_mat4(sh,"model",(float*)model);
            draw_emissive(sh, 0.96f,0.98f,0.88f, 2.8f,2.9f,2.4f);
            mesh_draw(&scene.moon);
            glm_mat4_identity(model);
            shader_set_mat4(sh,"model",(float*)model);
        }

        // ── Scene ─────────────────────────────────────────────────────────────
        draw_obj     (sh,0.07f,0.07f,0.10f);                 mesh_draw(&scene.arena);
        draw_emissive(sh,0.2f,0.5f,0.5f,0.0f,0.10f,0.15f);  mesh_draw(&scene.arena_acc);
        draw_obj     (sh,0.10f,0.10f,0.14f);                 mesh_draw(&scene.pillars);
        draw_emissive(sh,0.9f,0.95f,1.0f,0.2f,0.25f,0.35f); mesh_draw(&scene.pillar_acc);

        // ── Player trail + bike ───────────────────────────────────────────────
        draw_emissive(sh,1.0f,1.0f,1.0f,0.3f,0.3f,0.3f);
        dynmesh_draw(&player_trail);

        vec3 yaxis = {0,1,0};
        if (bike.alive || (bike.falling && bike.y > ARENA_BASE + 4.0f)) {
            glm_mat4_identity(model);
            vec3 bpos = {bike.x, bike.y, bike.z};
            glm_translate(model, bpos);
            glm_rotate(model, DANGLE[bike.dir], yaxis);
            shader_set_mat4(sh,"model",(float*)model);

            if (bike.alive)
                draw_emissive(sh,0.6f,1.0f,1.0f,0.1f,0.6f,0.7f);
            else
                draw_emissive(sh,0.5f,0.1f,0.1f,0.3f,0.0f,0.0f);

            mesh_draw(&scene.bike_mesh);
        }

        // ── AI trails + bikes (bikes only drawn while alive) ─────────────────
        // ── AI trails + bikes ─────────────────────────────────────────────────
        for (int i=0;i<N_AI;i++){
            glm_mat4_identity(model);
            shader_set_mat4(sh,"model",(float*)model);
            draw_emissive(sh,1.0f,0.95f,0.2f,0.35f,0.28f,0.0f);
            dynmesh_draw(&ai_trail_meshes[i]);

            if (ai_bikes[i].bike.alive ||
                (ai_bikes[i].bike.falling && ai_bikes[i].bike.y > ARENA_BASE + 4.0f)) {
                glm_mat4_identity(model);
                vec3 apos = {ai_bikes[i].bike.x, ai_bikes[i].bike.y, ai_bikes[i].bike.z};
                glm_translate(model, apos);
                glm_rotate(model, DANGLE[ai_bikes[i].bike.dir], yaxis);
                shader_set_mat4(sh,"model",(float*)model);

                if (ai_bikes[i].bike.alive)
                    draw_emissive(sh,1.0f,0.5f,0.05f,0.5f,0.18f,0.0f);
                else
                    draw_emissive(sh,0.3f,0.15f,0.05f,0.15f,0.05f,0.0f);

                mesh_draw(&scene.bike_mesh);
            }
        }

        // ── Water (after opaque scene, before particles) ──────────────────────
        {
            glm_mat4_identity(model);
            shader_use(wsh);
            shader_set_mat4(wsh,"view",       (float*)view);
            shader_set_mat4(wsh,"projection", (float*)proj);
            shader_set_float(wsh,"time",      (float)glfwGetTime());
            shader_set_vec3(wsh,"viewPos",
                camera.position[0],camera.position[1],camera.position[2]);
            shader_set_vec3(wsh,"moonPos",    MOON_X, MOON_Y, MOON_Z);
            shader_set_vec3(wsh,"fogColor",   FOG_R, FOG_G, FOG_B);
            shader_set_float(wsh,"fogDensity",FOG_DENSITY);
            mesh_draw(&scene.sea);
        }

        // ── Particles ─────────────────────────────────────────────────────────
        {
            glm_mat4_identity(model);
            shader_set_mat4(sh,"model",(float*)model);
            vec3 cr={camera.right[0],camera.right[1],camera.right[2]};
            vec3 cu={camera.up[0],camera.up[1],camera.up[2]};
            pmesh_update(&part_mesh,part_buf,particles_build(cr,cu));
            glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE); glDepthMask(GL_FALSE);
            shader_use(psh);
            shader_set_mat4(psh,"view",      (float*)view);
            shader_set_mat4(psh,"projection",(float*)proj);
            pmesh_draw(&part_mesh);
            glDepthMask(GL_TRUE); glDisable(GL_BLEND);
        }

        glfwSwapBuffers(win); glfwPollEvents();
    }

    scene_free(&scene);
    dynmesh_free(&player_trail);
    for (int i=0;i<N_AI;i++) dynmesh_free(&ai_trail_meshes[i]);
    pmesh_free(&part_mesh);
    glDeleteProgram(sh); glDeleteProgram(psh); glDeleteProgram(wsh);
    glfwTerminate();
    return 0;
}
