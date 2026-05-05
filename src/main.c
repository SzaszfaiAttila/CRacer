#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <cglm/cglm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "config.h"
#include "shader.h"
#include "camera.h"
#include "mesh.h"
#include "scene.h"
#include "bike.h"
#include "particles.h"
#include "ai.h"
#include "ui.h"
#include "glbloader.h"
#include "skybox.h"

/* ══════════════════════════════════════════════════════════════════════════════
   GAME STATE
   ══════════════════════════════════════════════════════════════════════════ */
typedef enum {
    STATE_MAIN_MENU = 0,
    STATE_PLAY_SELECT,
    STATE_COUNTDOWN,
    STATE_PLAYING,
    STATE_PAUSED,
    STATE_SETTINGS,
    STATE_GAME_OVER,
    STATE_WIN,
    STATE_TO_MENU,
} GameState;

typedef enum { CAM_FOLLOW=0, CAM_TOPDOWN=1, CAM_FREE=2 } CamMode;

/* ── Camera constants ──────────────────────────────────────────────────────── */
#define MENU_CAM_PX  -50.0f
#define MENU_CAM_PY   30.0f
#define MENU_CAM_PZ  100.0f
#define MENU_CAM_FX    0.1f
#define MENU_CAM_FY  -0.05f
#define MENU_CAM_FZ   -0.5f

#define FOLLOW_START_PX  0.0f
#define FOLLOW_START_PY  17.0f
#define FOLLOW_START_PZ  72.0f
#define FOLLOW_START_FX  0.0f
#define FOLLOW_START_FY  -0.3714f
#define FOLLOW_START_FZ  -0.9285f

#define COUNTDOWN_DUR       3.0f
#define TO_MENU_DUR         2.2f
#define GAME_OVER_DELAY     3.0f
#define WIN_DELAY           6.0f
#define MENU_RESPAWN_DELAY 10.0f

static const float MOON_X =  150.0f;
static const float MOON_Y =   48.0f;
static const float MOON_Z = -560.0f;
static const float FOG_R = 0.01f, FOG_G = 0.01f, FOG_B = 0.04f;
static const float FOG_DENSITY = 0.0022f;

/* ── Bike model transform constants — tune here ────────────────────────────── */
#define BIKE_MODEL_SCALE           0.025f
#define BIKE_MODEL_ROT_Y           ((float)M_PI * 0.5f)   /* 90 degrees right */
#define BIKE_MODEL_FORWARD         1.5f   /* forward offset along travel dir   */
#define BIKE_MODEL_HEIGHT_OFFSET   0.7f   /* push up so wheels clear the floor */

/* ══════════════════════════════════════════════════════════════════════════════
   GLOBALS
   ══════════════════════════════════════════════════════════════════════════ */
static Camera      camera;
static float       last_x=0, last_y=0;
static int         first_mouse=1;
static float       real_dt=0.0f, last_wall=0.0f;
static CamMode     cam_mode = CAM_FOLLOW;
static float       follow_zoom = 0.5f;
static float       cam_px=0, cam_py=FOLLOW_START_PY, cam_pz=FOLLOW_START_PZ;

static GameState   g_state         = STATE_MAIN_MENU;
static GameState   settings_return = STATE_MAIN_MENU;

static float       g_brightness    = 1.3f;
static float       g_volume        = 1.0f;
static int         g_n_ai          = 6;

static float       countdown_timer = 0.0f;
static float       end_timer       = 0.0f;
static float       to_menu_timer   = 0.0f;
static float       menu_fade       = 0.0f;

static float       trans_from_pos  [3];
static float       trans_from_front[3];

static float       g_mx=0, g_my=0;
static int         g_lmb_down=0, g_lmb_clicked=0;
static int         slider_drag=-1;

static float       SCR_W=1920, SCR_H=1080;
static float       SZ_HUGE, SZ_LARGE, SZ_MEDIUM, SZ_SMALL;

static int         main_sel=0;
static int         pause_sel=0;
static int         play_sel=0;

static int         prev_bike_alive[1+MAX_AI];
static int         player_was_alive=1;
static int         win_announced=0;
static float       menu_respawn_timer[MAX_AI];

static float player_trail_buf      [TRAIL_BUF_FLOATS];
static float ai_trail_bufs[MAX_AI] [TRAIL_BUF_FLOATS];

static UI          ui;
static GLFWwindow *g_win = NULL;

static Bike        bike;
static AIBike      ai_bikes[MAX_AI];
static DynMesh     player_trail;
static DynMesh     ai_trail_meshes[MAX_AI];
static Bike       *all_bikes_game[1+MAX_AI];
static Bike       *all_bikes_menu[MAX_AI];

static GLuint sh, psh, wsh;
static PMesh  part_mesh;
static Scene  g_scene;
static Mesh   g_hitbox_mesh;
static Skybox g_skybox;

/* ── OBJ bike models ─────────────────────────────────────────────────────── */
static ObjModel g_bike_player;
static ObjModel g_bike_ai;
static ObjModel g_bike_player_refl;
static ObjModel g_bike_ai_refl;

static int has_player     (void){ return g_bike_player.prim_count      > 0; }
static int has_ai         (void){ return g_bike_ai.prim_count          > 0; }
static int has_player_refl(void){ return g_bike_player_refl.prim_count > 0; }
static int has_ai_refl    (void){ return g_bike_ai_refl.prim_count     > 0; }

static Mesh *bike_mesh_player     (void){ return has_player()      ? &g_bike_player.prims[0].mesh      : &g_scene.bike_mesh; }
static Mesh *bike_mesh_ai         (void){ return has_ai()          ? &g_bike_ai.prims[0].mesh          : &g_scene.bike_mesh; }
static Mesh *bike_mesh_player_refl(void){ return has_player_refl() ? &g_bike_player_refl.prims[0].mesh : bike_mesh_player(); }
static Mesh *bike_mesh_ai_refl    (void){ return has_ai_refl()     ? &g_bike_ai_refl.prims[0].mesh     : bike_mesh_ai();     }

/* ══════════════════════════════════════════════════════════════════════════════
   DRAW HELPERS
   ══════════════════════════════════════════════════════════════════════════ */
static void draw_obj(float r, float g, float b) {
    shader_set_vec3(sh,"objectColor",r,g,b);
    shader_set_vec3(sh,"emissive",0,0,0);
    shader_set_float(sh,"ambientStr",0.12f*g_brightness);
}
static void draw_emissive(float or, float og, float ob,
                           float er, float eg, float eb) {
    shader_set_vec3(sh,"objectColor",or,og,ob);
    shader_set_vec3(sh,"emissive",er*g_brightness,eg*g_brightness,eb*g_brightness);
    shader_set_float(sh,"ambientStr",0.05f*g_brightness);
}
static void sh_opaque(void){
    shader_set_float(sh,"uAlphaTop", 1.0f);
    shader_set_float(sh,"uAlphaSide",1.0f);
}
static void sh_specular(float str, float shine){
    shader_set_float(sh,"uSpecularStr",str);
    shader_set_float(sh,"uShininess",  shine);
}
static void sh_defaults(void){
    shader_set_float(sh,"uAlphaTop",   1.0f);
    shader_set_float(sh,"uAlphaSide",  1.0f);
    shader_set_float(sh,"uAlphaMult",  1.0f);
    shader_set_float(sh,"uNormalYFlip",1.0f);
    shader_set_float(sh,"uSpecularStr",0.0f);
    shader_set_float(sh,"uShininess",  32.0f);
    shader_set_float(sh,"uTwoSided",   0.0f);
    /* Hemisphere GI — dark teal sky, near-black ground */
    shader_set_float(sh,"uHemiStr",    0.55f);
    shader_set_vec3 (sh,"uSkyColor",   0.04f, 0.07f, 0.14f);
    shader_set_vec3 (sh,"uGroundColor",0.01f, 0.01f, 0.02f);
}

/* ══════════════════════════════════════════════════════════════════════════════
   GLFW CALLBACKS
   ══════════════════════════════════════════════════════════════════════════ */
static void framebuffer_size_cb(GLFWwindow *w, int W, int H) {
    (void)w; glViewport(0,0,W,H);
    SCR_W=(float)W; SCR_H=(float)H;
    SZ_HUGE  =SCR_H*0.055f; SZ_LARGE =SCR_H*0.040f;
    SZ_MEDIUM=SCR_H*0.028f; SZ_SMALL =SCR_H*0.021f;
    ui_resize(&ui,W,H);
}
static void scroll_cb(GLFWwindow *w, double xoff, double yoff) {
    (void)w;(void)xoff;
    if(g_state!=STATE_PLAYING||cam_mode!=CAM_FOLLOW) return;
    follow_zoom-=(float)yoff*0.08f;
    if(follow_zoom<0.0f) follow_zoom=0.0f;
    if(follow_zoom>1.0f) follow_zoom=1.0f;
}
static void mouse_cb(GLFWwindow *w, double xpos, double ypos) {
    (void)w;
    g_mx=(float)xpos; g_my=(float)ypos;
    int game_running=(g_state==STATE_PLAYING||g_state==STATE_GAME_OVER||g_state==STATE_WIN);
    if(game_running && cam_mode==CAM_FREE){
        if(first_mouse){last_x=xpos;last_y=ypos;first_mouse=0;}
        float xo=(float)(xpos-last_x), yo=-(float)(ypos-last_y);
        last_x=xpos; last_y=ypos;
        camera_process_mouse(&camera,xo,yo);
    } else {
        last_x=xpos; last_y=ypos; first_mouse=0;
    }
}

/* ══════════════════════════════════════════════════════════════════════════════
   CAMERA HELPERS
   ══════════════════════════════════════════════════════════════════════════ */
static void apply_camera_vecs(void){
    vec3 wu={0,1,0};
    glm_cross(camera.front,wu,camera.right); glm_normalize(camera.right);
    glm_cross(camera.right,camera.front,camera.up); glm_normalize(camera.up);
}
static void set_menu_camera(void){
    camera.position[0]=MENU_CAM_PX; camera.position[1]=MENU_CAM_PY; camera.position[2]=MENU_CAM_PZ;
    camera.front[0]=MENU_CAM_FX; camera.front[1]=MENU_CAM_FY; camera.front[2]=MENU_CAM_FZ;
    apply_camera_vecs();
    cam_px=MENU_CAM_PX; cam_py=MENU_CAM_PY; cam_pz=MENU_CAM_PZ;
}
static void lerp3(float *out,const float *a,const float *b,float t){
    out[0]=a[0]+(b[0]-a[0])*t; out[1]=a[1]+(b[1]-a[1])*t; out[2]=a[2]+(b[2]-a[2])*t;
}
static float smoothstep(float t){ return t*t*(3.0f-2.0f*t); }

static void update_follow_camera(float dt){
    if(cam_mode==CAM_FOLLOW){
        float dist  =10.0f+follow_zoom*28.0f;
        float height= 5.0f+follow_zoom*17.0f;
        float ahead = 4.0f+follow_zoom* 4.0f;
        float tx=bike.x-DDX[bike.dir]*dist;
        float ty=ARENA_TOP+height;
        float tz=bike.z-DDZ[bike.dir]*dist;
        if(dt>0.0f){
            float lp=1.0f-expf(-8.0f*dt);
            cam_px+=(tx-cam_px)*lp; cam_py+=(ty-cam_py)*lp; cam_pz+=(tz-cam_pz)*lp;
        }
        camera.position[0]=cam_px; camera.position[1]=cam_py; camera.position[2]=cam_pz;
        vec3 look={bike.x+DDX[bike.dir]*ahead, ARENA_TOP+1.5f, bike.z+DDZ[bike.dir]*ahead};
        glm_vec3_sub(look,camera.position,camera.front); glm_normalize(camera.front);
        vec3 wu={0,1,0};
        glm_cross(camera.front,wu,camera.right); glm_normalize(camera.right);
        glm_cross(camera.right,camera.front,camera.up); glm_normalize(camera.up);
    } else if(cam_mode==CAM_TOPDOWN){
        camera.position[0]=0; camera.position[1]=150; camera.position[2]=0;
        camera.front[0]=0; camera.front[1]=-1; camera.front[2]=-0.001f;
        glm_normalize(camera.front); camera.up[0]=0; camera.up[1]=0; camera.up[2]=-1;
        vec3 wu={0,1,0};
        glm_cross(camera.front,wu,camera.right); glm_normalize(camera.right);
    }
}

/* Build the model matrix for a bike, including OBJ scale/rotation/offset */
static void bike_model_mat(mat4 out, float wx, float wy, float wz,
                            float dir_angle, int use_obj, float reflect_y) {
    glm_mat4_identity(out);
    float fwd = (use_obj) ? BIKE_MODEL_FORWARD : 0.0f;
    float fx = -sinf(dir_angle) * fwd;
    float fz = -cosf(dir_angle) * fwd;
    /* reflect_y is +1 for normal bikes, -1 for reflections.
       Applying it to the height offset mirrors the visual model correctly
       across ARENA_TOP: bike at 3.5+0.7=4.2 → reflection at 3.5-0.7=2.8 */
    float oy = (use_obj) ? BIKE_MODEL_HEIGHT_OFFSET * reflect_y : 0.0f;
    vec3 pos = {wx+fx, wy+oy, wz+fz}; glm_translate(out, pos);
    vec3 yaxis = {0,1,0};
    glm_rotate(out, dir_angle, yaxis);
    if (use_obj) {
        glm_rotate(out, BIKE_MODEL_ROT_Y, yaxis);
        vec3 sc = {BIKE_MODEL_SCALE, BIKE_MODEL_SCALE*reflect_y, BIKE_MODEL_SCALE};
        glm_scale(out, sc);
    } else {
        if (reflect_y < 0.0f) { vec3 sc={1.0f,-1.0f,1.0f}; glm_scale(out,sc); }
    }
}

/* ══════════════════════════════════════════════════════════════════════════════
   ENTER STATE
   ══════════════════════════════════════════════════════════════════════════ */
static void enter_state(GameState ns){
    switch(ns){
    case STATE_MAIN_MENU:
        for(int i=0;i<MAX_AI;i++){ai_init(&ai_bikes[i],i);menu_respawn_timer[i]=0.0f;}
        bike_reset(&bike); bike.alive=0;
        for(int i=0;i<1+MAX_AI;i++) prev_bike_alive[i]=0;
        for(int i=0;i<MAX_AI;i++) prev_bike_alive[i+1]=1;
        win_announced=0; player_was_alive=0;
        set_menu_camera(); cam_mode=CAM_FOLLOW;
        menu_fade=0.0f;
        glfwSetInputMode(g_win,GLFW_CURSOR,GLFW_CURSOR_NORMAL);
        main_sel=0;
        break;
    case STATE_PLAY_SELECT:
        glfwSetInputMode(g_win,GLFW_CURSOR,GLFW_CURSOR_NORMAL);
        play_sel=0;
        break;
    case STATE_COUNTDOWN:
        bike_reset(&bike);
        for(int i=0;i<MAX_AI;i++){
            ai_reset(&ai_bikes[i],i);
            if(i>=g_n_ai) ai_bikes[i].bike.alive=0;
        }
        prev_bike_alive[0]=1;
        for(int i=0;i<MAX_AI;i++) prev_bike_alive[i+1]=(i<g_n_ai)?1:0;
        player_was_alive=1; win_announced=0;
        trans_from_pos[0]=camera.position[0]; trans_from_pos[1]=camera.position[1]; trans_from_pos[2]=camera.position[2];
        trans_from_front[0]=camera.front[0]; trans_from_front[1]=camera.front[1]; trans_from_front[2]=camera.front[2];
        countdown_timer=0.0f; cam_mode=CAM_FOLLOW; follow_zoom=0.5f;
        cam_px=FOLLOW_START_PX; cam_py=FOLLOW_START_PY; cam_pz=FOLLOW_START_PZ;
        glfwSetInputMode(g_win,GLFW_CURSOR,GLFW_CURSOR_DISABLED);
        first_mouse=1;
        break;
    case STATE_PLAYING:
        glfwSetInputMode(g_win,GLFW_CURSOR,GLFW_CURSOR_DISABLED);
        first_mouse=1; pause_sel=0;
        break;
    case STATE_PAUSED:
        glfwSetInputMode(g_win,GLFW_CURSOR,GLFW_CURSOR_NORMAL);
        pause_sel=0; slider_drag=-1;
        break;
    case STATE_SETTINGS:
        glfwSetInputMode(g_win,GLFW_CURSOR,GLFW_CURSOR_NORMAL);
        slider_drag=-1;
        break;
    case STATE_GAME_OVER:
        end_timer=GAME_OVER_DELAY;
        break;
    case STATE_WIN:
        end_timer=WIN_DELAY;
        break;
    case STATE_TO_MENU:
        trans_from_pos[0]=camera.position[0]; trans_from_pos[1]=camera.position[1]; trans_from_pos[2]=camera.position[2];
        trans_from_front[0]=camera.front[0]; trans_from_front[1]=camera.front[1]; trans_from_front[2]=camera.front[2];
        to_menu_timer=0.0f;
        glfwSetInputMode(g_win,GLFW_CURSOR,GLFW_CURSOR_DISABLED);
        break;
    default: break;
    }
    g_state=ns;
}

/* ══════════════════════════════════════════════════════════════════════════════
   UI WIDGETS
   ══════════════════════════════════════════════════════════════════════════ */
static int do_button(float x, float y, float w, float h,
                     const char *label, float fsz, float alpha, int selected){
    int hov = selected || (g_mx>=x && g_mx<=x+w && g_my>=y && g_my<=y+h);
    ui_rect(&ui, x+2, y+2, w-4, h-4, 0.0f, 0.03f, 0.09f, alpha*0.58f);
    float bdr=hov?1.0f:0.38f, gc=hov?0.60f:0.0f;
    ui_rect(&ui, x,      y,      w,    2.0f, 0.0f,gc*0.5f,bdr,alpha);
    ui_rect(&ui, x,      y+h-2,  w,    2.0f, 0.0f,gc*0.5f,bdr,alpha);
    ui_rect(&ui, x,      y,      2.0f, h,    0.0f,gc*0.5f,bdr,alpha);
    ui_rect(&ui, x+w-2,  y,      2.0f, h,    0.0f,gc*0.5f,bdr,alpha);
    float tw=ui_textw(label,fsz);
    float tx=x+(w-tw)*0.5f, ty=y+(h-fsz)*0.5f;
    float tr=hov?0.10f:0.0f, tg=hov?1.0f:0.65f, tb=hov?1.0f:0.80f;
    ui_text(&ui,label,tx,ty,fsz,tr,tg,tb,alpha);
    return hov && g_lmb_clicked;
}

static float do_slider(float x, float y, float sw, float sh2,
                       float val, float val_max,
                       const char *label, float fsz, float alpha, int drag_id){
    ui_text(&ui,label,x,y,fsz,0.32f,0.82f,1.0f,alpha);
    float sy=y+fsz*1.4f;
    float fill=(val_max>0.0f)?val/val_max:0.0f;
    if(fill>1.0f) fill=1.0f; if(fill<0.0f) fill=0.0f;
    ui_rect(&ui,x,sy,sw,sh2,0.02f,0.08f,0.12f,alpha*0.85f);
    ui_rect(&ui,x,sy,sw*fill,sh2,0.0f,0.55f,1.0f,alpha*0.78f);
    ui_rect(&ui,x,      sy,          sw,   2.0f,0.0f,0.35f,0.65f,alpha);
    ui_rect(&ui,x,      sy+sh2-2.0f, sw,   2.0f,0.0f,0.35f,0.65f,alpha);
    ui_rect(&ui,x,      sy,          2.0f, sh2, 0.0f,0.35f,0.65f,alpha);
    ui_rect(&ui,x+sw-2, sy,          2.0f, sh2, 0.0f,0.35f,0.65f,alpha);
    float hx=x+sw*fill-sh2*0.5f;
    ui_rect(&ui,hx,sy-sh2*0.3f,sh2,sh2*1.6f,0.0f,0.9f,1.0f,alpha);
    char buf[8]; snprintf(buf,sizeof(buf),"%d%%",(int)(val*100.0f+0.5f));
    ui_text(&ui,buf,x+sw+fsz*0.6f,sy+(sh2-fsz)*0.5f,fsz*0.9f,0.45f,0.85f,1.0f,alpha);
    int hov=g_mx>=x&&g_mx<=x+sw&&g_my>=sy-sh2&&g_my<=sy+sh2*2.0f;
    if(hov&&g_lmb_clicked) slider_drag=drag_id;
    if(slider_drag==drag_id&&g_lmb_down){
        val=((g_mx-x)/sw)*val_max;
        if(val<0.0f) val=0.0f; if(val>val_max) val=val_max;
    }
    return val;
}

static void draw_title(const char *s, float x, float y, float fsz, float alpha){
    float off=fsz*0.09f;
    ui_text(&ui,s,x-off,y,    fsz,0.0f,0.38f,1.0f,alpha*0.28f);
    ui_text(&ui,s,x+off,y,    fsz,0.0f,0.38f,1.0f,alpha*0.28f);
    ui_text(&ui,s,x,    y-off,fsz,0.0f,0.38f,1.0f,alpha*0.28f);
    ui_text(&ui,s,x,    y+off,fsz,0.0f,0.38f,1.0f,alpha*0.28f);
    ui_text(&ui,s,x,y,fsz,0.2f,0.95f,1.0f,alpha);
}

/* ══════════════════════════════════════════════════════════════════════════════
   MENU RENDER FUNCTIONS
   ══════════════════════════════════════════════════════════════════════════ */
static void render_main_menu(void){
    float alpha=menu_fade;
    float lx=SCR_W*0.05f, by=SCR_H*0.10f;
    float bw=SCR_W*0.28f, bh=SZ_LARGE*1.75f, gap=bh*1.40f;
    draw_title("TRON C-RACER",lx,by,SZ_HUGE,alpha);
    ui_text(&ui,"LIGHT CYCLE ARENA",lx,by+SZ_HUGE*1.3f,SZ_SMALL,0.18f,0.52f,0.78f,alpha*0.65f);
    float iy=by+SZ_HUGE*2.5f;
    const char *labels[3]={"PLAY GAME","SETTINGS","EXIT"};
    for(int i=0;i<3;i++){
        float fx=lx, fy=iy+gap*i;
        if(g_mx>=fx&&g_mx<=fx+bw&&g_my>=fy&&g_my<=fy+bh) main_sel=i;
        if(do_button(fx,fy,bw,bh,labels[i],SZ_MEDIUM,alpha,(main_sel==i))){
            if(i==0) enter_state(STATE_PLAY_SELECT);
            else if(i==1){settings_return=STATE_MAIN_MENU; enter_state(STATE_SETTINGS);}
            else glfwSetWindowShouldClose(g_win,1);
        }
    }
}

static void render_play_select(void){
    float lx=SCR_W*0.05f, by=SCR_H*0.10f;
    float bw=SCR_W*0.28f, bh=SZ_LARGE*1.75f;
    draw_title("PLAY GAME",lx,by,SZ_LARGE,1.0f);
    float oy=by+SZ_LARGE*2.2f;
    ui_text(&ui,"AI OPPONENTS:",lx,oy,SZ_MEDIUM,0.28f,0.82f,1.0f,1.0f);
    float sy=oy+SZ_MEDIUM*2.0f, aw=SZ_LARGE*1.75f;
    if(do_button(lx,sy,aw,aw,"<",SZ_MEDIUM,1.0f,0)){g_n_ai--;if(g_n_ai<1)g_n_ai=1;}
    char nb[4]; snprintf(nb,sizeof(nb),"%d",g_n_ai);
    float nw=aw*2.2f, ntw=ui_textw(nb,SZ_LARGE);
    ui_rect(&ui,lx+aw+4,sy,nw,aw,0,0.07f,0.13f,0.82f);
    ui_text(&ui,nb,lx+aw+4+(nw-ntw)*0.5f,sy+(aw-SZ_LARGE)*0.5f,SZ_LARGE,0.1f,1.0f,1.0f,1.0f);
    if(do_button(lx+aw+4+nw+4,sy,aw,aw,">",SZ_MEDIUM,1.0f,0)){g_n_ai++;if(g_n_ai>MAX_AI)g_n_ai=MAX_AI;}
    float sy2=sy+aw+SZ_MEDIUM*1.6f;
    if(do_button(lx,sy2,bw,bh,"START",SZ_MEDIUM,1.0f,(play_sel==0))) enter_state(STATE_COUNTDOWN);
    if(do_button(lx,sy2+bh*1.5f,bw,bh,"BACK",SZ_MEDIUM,1.0f,(play_sel==1))) enter_state(STATE_MAIN_MENU);
    if(g_mx>=lx&&g_mx<=lx+bw&&g_my>=sy2&&g_my<=sy2+bh) play_sel=0;
    if(g_mx>=lx&&g_mx<=lx+bw&&g_my>=sy2+bh*1.5f&&g_my<=sy2+bh*2.5f) play_sel=1;
}

static void render_countdown_overlay(void){
    float t=countdown_timer;
    int num=3-(int)t;
    char buf[8];
    if(num>0) snprintf(buf,sizeof(buf),"%d",num);
    else       snprintf(buf,sizeof(buf),"GO!");
    float fsz=SZ_HUGE*2.4f;
    float tw=ui_textw(buf,fsz);
    float tx=(SCR_W-tw)*0.5f, ty=(SCR_H-fsz)*0.5f;
    float phase=t-floorf(t);
    float a=0.55f+0.45f*cosf(phase*6.28318530f);
    if(num<=0) a=1.0f;
    draw_title(buf,tx,ty,fsz,a);
}

static void render_pause_menu(void){
    float cx=SCR_W*0.5f, cy=SCR_H*0.26f;
    float bw=SCR_W*0.32f, bh=SZ_LARGE*1.8f, gap=bh*1.46f;
    float bx=cx-bw*0.5f;
    draw_title("PAUSED",cx-ui_textw("PAUSED",SZ_LARGE)*0.5f,cy,SZ_LARGE,1.0f);
    float iy=cy+SZ_LARGE*2.3f;
    const char *labels[3]={"RESUME","SETTINGS","EXIT TO MAIN MENU"};
    for(int i=0;i<3;i++){
        if(g_mx>=bx&&g_mx<=bx+bw&&g_my>=iy+gap*i&&g_my<=iy+gap*i+bh) pause_sel=i;
        if(do_button(bx,iy+gap*i,bw,bh,labels[i],SZ_MEDIUM,1.0f,(pause_sel==i))){
            if(i==0) enter_state(STATE_PLAYING);
            else if(i==1){settings_return=STATE_PAUSED; enter_state(STATE_SETTINGS);}
            else enter_state(STATE_TO_MENU);
        }
    }
}

static void render_settings(void){
    float lx=SCR_W*0.05f, sw2=SCR_W*0.44f, sh2=SCR_H*0.028f;
    float ly=SCR_H*0.07f;
    draw_title("SETTINGS",lx,ly,SZ_LARGE,1.0f);
    float ry=ly+SZ_LARGE*1.7f;
    ui_rect(&ui,lx,ry,sw2,2.0f,0,0.30f,0.50f,0.45f); ry+=SZ_SMALL*0.8f;
    g_volume     = do_slider(lx,ry, sw2,sh2, g_volume,    1.0f, "VOLUME",     SZ_MEDIUM,1.0f,0);
    ry+=SZ_MEDIUM+sh2+SZ_MEDIUM*2.4f;
    g_brightness = do_slider(lx,ry, sw2,sh2, g_brightness,1.5f, "BRIGHTNESS", SZ_MEDIUM,1.0f,1);
    ry+=SZ_MEDIUM+sh2+SZ_MEDIUM*1.6f;
    ui_rect(&ui,lx,ry,sw2,2.0f,0,0.30f,0.50f,0.45f); ry+=SZ_SMALL*0.8f;
    ui_text(&ui,"CONTROLS",lx,ry,SZ_MEDIUM,0.28f,0.82f,1.0f,1.0f);
    ry+=SZ_MEDIUM*1.9f;
    static const char *ctrl[][2]={
        {"A / D",             "Turn bike left / right"},
        {"Left Shift",        "Dash  (1.5x speed, 3s, 6s cooldown)"},
        {"ESC",               "Pause game  /  Back in menus"},
        {"F1",                "Open controls / settings overlay"},
        {"F3 / F4",           "Brightness -/+  (works anywhere)"},
        {"TAB",               "Toggle free camera"},
        {"WASD + Mouse",      "Free camera movement / look"},
        {"Space / Ctrl",      "Free cam up / down"},
        {"Scroll Wheel",      "Zoom follow camera in/out"},
        {"C",                 "Toggle top-down view"},
        {"R",                 "Quick restart round"},
        {"W / S or Arrows",   "Navigate menus up / down"},
        {"Enter / Space",     "Confirm menu selection"},
    };
    int nc=(int)(sizeof(ctrl)/sizeof(ctrl[0]));
    float lh=SZ_SMALL*1.62f, col2=lx+SCR_W*0.19f;
    for(int i=0;i<nc;i++){
        ui_text(&ui,ctrl[i][0],lx,   ry+lh*i, SZ_SMALL, 0.0f,0.88f,1.0f,1.0f);
        ui_text(&ui,ctrl[i][1],col2, ry+lh*i, SZ_SMALL, 0.50f,0.70f,0.80f,0.85f);
    }
    ry+=lh*nc+SZ_MEDIUM;
    float bw=SCR_W*0.20f, bh=SZ_LARGE*1.5f;
    if(do_button(lx,ry,bw,bh,"BACK",SZ_MEDIUM,1.0f,0)) enter_state(settings_return);
}

static void render_big_text_overlay(const char *msg, float r, float g, float b){
    float fsz=SZ_HUGE*1.9f;
    float tw=ui_textw(msg,fsz);
    float tx=(SCR_W-tw)*0.5f, ty=(SCR_H-fsz)*0.42f;
    float off=fsz*0.07f;
    ui_text(&ui,msg,tx-off,ty,fsz,r,g,b,0.26f);
    ui_text(&ui,msg,tx+off,ty,fsz,r,g,b,0.26f);
    ui_text(&ui,msg,tx,ty-off,fsz,r,g,b,0.26f);
    ui_text(&ui,msg,tx,ty+off,fsz,r,g,b,0.26f);
    ui_text(&ui,msg,tx,ty,fsz,r,g,b,1.0f);
    const char *hint="Press R to restart";
    float hw=ui_textw(hint,SZ_SMALL);
    ui_text(&ui,hint,(SCR_W-hw)*0.5f,SCR_H*0.80f,SZ_SMALL,0.38f,0.65f,0.75f,0.36f);
}

/* ══════════════════════════════════════════════════════════════════════════════
   3-D RENDER
   ══════════════════════════════════════════════════════════════════════════ */
static void render_3d(int W, int H){
    glClearColor(0.01f,0.01f,0.04f,1.0f);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    shader_use(sh);

    mat4 view,proj,model;
    camera_get_view(&camera,view);

    int is_topdown=(cam_mode==CAM_TOPDOWN&&
                   (g_state==STATE_PLAYING||g_state==STATE_GAME_OVER||g_state==STATE_WIN));
    if(is_topdown){
        float aspect=(float)W/H, hs=ARENA_HALF+10.0f;
        glm_ortho(-hs*aspect,hs*aspect,-hs,hs,0.1f,500.0f,proj);
    } else {
        camera_get_projection(&camera,proj,(float)W/H);
    }

    glm_mat4_identity(model);
    shader_set_mat4(sh,"view",      (float*)view);
    shader_set_mat4(sh,"projection",(float*)proj);
    shader_set_mat4(sh,"model",     (float*)model);
    shader_set_vec3(sh,"lightPos",  MOON_X,MOON_Y+200.0f,MOON_Z);
    shader_set_vec3(sh,"lightColor",0.80f*g_brightness,0.90f*g_brightness,1.15f*g_brightness);
    shader_set_vec3(sh,"viewPos",   camera.position[0],camera.position[1],camera.position[2]);
    shader_set_vec3(sh,"fogColor",  FOG_R,FOG_G,FOG_B);
    shader_set_float(sh,"fogDensity",FOG_DENSITY);
    sh_defaults();

    /* ── Skybox (drawn first so everything else sits in front of it) ───── */
    skybox_draw(&g_skybox, view, proj, g_brightness * 0.8f);
    /* Restore scene shader uniforms after skybox */
    shader_use(sh);
    shader_set_mat4(sh,"view",      (float*)view);
    shader_set_mat4(sh,"projection",(float*)proj);
    glm_mat4_identity(model); shader_set_mat4(sh,"model",(float*)model);
    shader_set_vec3(sh,"lightPos",  MOON_X,MOON_Y+200.0f,MOON_Z);
    shader_set_vec3(sh,"lightColor",0.80f*g_brightness,0.90f*g_brightness,1.15f*g_brightness);
    shader_set_vec3(sh,"viewPos",   camera.position[0],camera.position[1],camera.position[2]);
    shader_set_vec3(sh,"fogColor",  FOG_R,FOG_G,FOG_B);
    shader_set_float(sh,"fogDensity",FOG_DENSITY);
    sh_defaults();

    int in_game=(g_state==STATE_PLAYING||g_state==STATE_GAME_OVER||
                 g_state==STATE_WIN||g_state==STATE_COUNTDOWN||
                 g_state==STATE_PAUSED||
                 (g_state==STATE_SETTINGS&&settings_return!=STATE_MAIN_MENU));
    int ai_limit=in_game?g_n_ai:MAX_AI;


    /* ── Reflections: blurred Y-mirror, drawn BEFORE the arena so the
       arena quad naturally occludes them and masks water-shine ────────── */
#define BLUR_SAMPLES 7
#define BLUR_R       0.55f
    static const float JITTER_X[BLUR_SAMPLES]={0.0f,BLUR_R,0.0f,-BLUR_R,0.0f,BLUR_R*0.71f,-BLUR_R*0.71f};
    static const float JITTER_Z[BLUR_SAMPLES]={0.0f,0.0f,BLUR_R,0.0f,-BLUR_R,BLUR_R*0.71f,BLUR_R*0.71f};

    glm_mat4_identity(model); shader_set_mat4(sh,"model",(float*)model);
    shader_set_float(sh,"uNormalYFlip",-1.0f);
    shader_set_float(sh,"uAlphaTop",   1.0f);
    shader_set_float(sh,"uAlphaSide",  1.0f);
    shader_set_float(sh,"uTwoSided",   1.0f);

    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE);
    /* GL_LEQUAL: reflections only visible where no closer opaque fragment
       has been written yet (i.e. below/through the not-yet-drawn arena).  */
    glDepthMask(GL_FALSE); glDepthFunc(GL_LEQUAL);

    float base_refl_alpha=0.52f;
    float samp_centre=base_refl_alpha*2.0f/(float)(BLUR_SAMPLES+1);
    float samp_outer =base_refl_alpha*1.0f/(float)(BLUR_SAMPLES+1);

    for(int samp=0;samp<BLUR_SAMPLES;samp++){
        float jx=JITTER_X[samp], jz=JITTER_Z[samp];
        float sa=(samp==0)?samp_centre:samp_outer;
        shader_set_float(sh,"uAlphaMult",sa);

        /* Reflected player — use glb_draw_model so ALL primitives render */
        if(bike.alive||(bike.falling&&bike.y>ARENA_BASE+4.0f)){
            float ry=2.0f*ARENA_TOP-bike.y;
            mat4 rm; bike_model_mat(rm,bike.x+jx,ry,bike.z+jz,DANGLE[bike.dir],has_player_refl(),-1.0f);
            shader_set_mat4(sh,"model",(float*)rm);
            if(has_player_refl()){
                /* uNormalYFlip and uAlphaMult already set; glb_draw_model
                   only touches objectColor/emissive/ambient/specular       */
                glb_draw_model(&g_bike_player_refl,sh,g_brightness*0.6f);
                /* Restore reflection-pass uniforms glb_draw_model may reset */
                shader_set_float(sh,"uNormalYFlip",-1.0f);
                shader_set_float(sh,"uAlphaMult",sa);
                shader_set_float(sh,"uTwoSided",1.0f);
            } else {
                draw_emissive(0.6f,1.0f,1.0f,0.15f,0.80f,0.90f);
                mesh_draw(bike_mesh_player_refl());
            }
        }

        /* Reflected AIs */
        for(int i=0;i<ai_limit;i++){
            Bike *ab=&ai_bikes[i].bike;
            if(ab->alive||(ab->falling&&ab->y>ARENA_BASE+4.0f)){
                float ry=2.0f*ARENA_TOP-ab->y;
                mat4 rm; bike_model_mat(rm,ab->x+jx,ry,ab->z+jz,DANGLE[ab->dir],has_ai_refl(),-1.0f);
                shader_set_mat4(sh,"model",(float*)rm);
                if(has_ai_refl()){
                    glb_draw_model(&g_bike_ai_refl,sh,g_brightness*0.6f);
                    shader_set_float(sh,"uNormalYFlip",-1.0f);
                    shader_set_float(sh,"uAlphaMult",sa);
                    shader_set_float(sh,"uTwoSided",1.0f);
                } else {
                    draw_emissive(1.0f,0.5f,0.05f,0.60f,0.22f,0.0f);
                    mesh_draw(bike_mesh_ai_refl());
                }
            }
        }

        /* Reflected trails */
        {
            mat4 tm; glm_mat4_identity(tm);
            vec3 tt={jx,2.0f*ARENA_TOP,jz}; glm_translate(tm,tt);
            vec3 ts={1.0f,-1.0f,1.0f}; glm_scale(tm,ts);
            shader_set_mat4(sh,"model",(float*)tm);
            float trail_samp=sa*0.60f;
            shader_set_float(sh,"uAlphaMult",trail_samp);
            shader_set_float(sh,"ambientStr",0.0f);
            shader_set_vec3(sh,"objectColor",0,0,0);
            shader_set_vec3(sh,"emissive",0.28f*g_brightness,0.28f*g_brightness,0.28f*g_brightness);
            dynmesh_draw(&player_trail);
            for(int i=0;i<ai_limit;i++){
                shader_set_vec3(sh,"emissive",0.30f*g_brightness,0.24f*g_brightness,0.0f);
                dynmesh_draw(&ai_trail_meshes[i]);
            }
            shader_set_float(sh,"uAlphaMult",sa);
        }
    }

    glDepthFunc(GL_LESS); glDepthMask(GL_TRUE); glDisable(GL_BLEND);
    shader_set_float(sh,"uNormalYFlip",1.0f);
    shader_set_float(sh,"uAlphaMult",  1.0f);
    shader_set_float(sh,"uTwoSided",   0.0f);
    glm_mat4_identity(model); shader_set_mat4(sh,"model",(float*)model);

    /* ── Moon ────────────────────────────────────────────────────────────── */
    glm_mat4_identity(model);
    {vec3 mp={MOON_X,MOON_Y,MOON_Z}; glm_translate(model,mp);}
    shader_set_mat4(sh,"model",(float*)model);
    draw_emissive(0.98f,0.99f,0.95f,6.0f,6.2f,5.5f);
    mesh_draw(&g_scene.moon);
    glm_mat4_identity(model); shader_set_mat4(sh,"model",(float*)model);

    /* ── Arena floor — semi-transparent so reflections show through ──────── */
    /* Arena is drawn first of the scene geometry so its depth blocks the
       reflected models that were drawn earlier at y < ARENA_TOP.
       uAlphaTop=0.82 lets ~18% of the reflection colour show through.       */
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    shader_set_float(sh,"uAlphaTop",  0.82f);
    shader_set_float(sh,"uAlphaSide", 1.0f);
    sh_specular(0.88f,220.0f);
    draw_obj(0.06f,0.07f,0.11f);             mesh_draw(&g_scene.arena);
    glDisable(GL_BLEND);
    sh_defaults();

    /* ── Pillars and accents (fully opaque) ─────────────────────────────── */
    sh_specular(0.45f,64.0f);
    draw_emissive(0.22f,0.55f,0.55f,0.0f,0.22f,0.28f); mesh_draw(&g_scene.arena_acc);
    sh_specular(0.22f,36.0f);
    draw_obj(0.10f,0.10f,0.14f);             mesh_draw(&g_scene.pillars);
    sh_specular(0.32f,44.0f);
    draw_emissive(0.9f,0.95f,1.0f,0.45f,0.55f,0.75f); mesh_draw(&g_scene.pillar_acc);

    /* ── Opaque bikes ────────────────────────────────────────────────────── */
    sh_specular(0.0f,32.0f);
    shader_set_float(sh,"uTwoSided",1.0f);



    vec3 yaxis={0,1,0};

    if(bike.alive||(bike.falling&&bike.y>ARENA_BASE+4.0f)){
        mat4 bm; bike_model_mat(bm,bike.x,bike.y,bike.z,DANGLE[bike.dir],has_player(),1.0f);
        shader_set_mat4(sh,"model",(float*)bm);
        if(has_player()){
            glb_draw_model(&g_bike_player,sh,g_brightness);
        } else {
            shader_set_float(sh,"ambientStr",0.12f*g_brightness);
            if(bike.alive) draw_emissive(0.6f,1.0f,1.0f,0.1f,0.6f,0.7f);
            else            draw_emissive(0.5f,0.1f,0.1f,0.3f,0.0f,0.0f);
            mesh_draw(bike_mesh_player());
        }
    }

    for(int i=0;i<MAX_AI;i++){
        Bike *ab=&ai_bikes[i].bike;
        if(i<ai_limit&&(ab->alive||(ab->falling&&ab->y>ARENA_BASE+4.0f))){
            mat4 am; bike_model_mat(am,ab->x,ab->y,ab->z,DANGLE[ab->dir],has_ai(),1.0f);
            shader_set_mat4(sh,"model",(float*)am);
            if(has_ai()){
                glb_draw_model(&g_bike_ai,sh,g_brightness);
            } else {
                shader_set_float(sh,"ambientStr",0.12f*g_brightness);
                if(ab->alive) draw_emissive(1.0f,0.5f,0.05f,0.5f,0.18f,0.0f);
                else           draw_emissive(0.3f,0.15f,0.05f,0.15f,0.05f,0.0f);
                mesh_draw(bike_mesh_ai());
            }
        }
    }
    sh_specular(0.0f,32.0f);
    shader_set_float(sh,"uTwoSided",0.0f);

    /* ── Water ──────────────────────────────────────────────────────────── */
    /* Rendered after opaque geometry. Polygon offset lets wave peaks
       composite over arena edges without depth-fighting stripes.            */
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    shader_use(wsh);
    shader_set_mat4(wsh,"view",       (float*)view);
    shader_set_mat4(wsh,"projection", (float*)proj);
    shader_set_float(wsh,"time",      (float)glfwGetTime());
    shader_set_vec3(wsh,"viewPos",    camera.position[0],camera.position[1],camera.position[2]);
    shader_set_vec3(wsh,"moonPos",    MOON_X,MOON_Y,MOON_Z);
    shader_set_vec3(wsh,"fogColor",   FOG_R,FOG_G,FOG_B);
    shader_set_float(wsh,"fogDensity",FOG_DENSITY);
    mesh_draw(&g_scene.sea);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDepthFunc(GL_LESS);
    /* Restore scene shader */
    shader_use(sh);
    shader_set_mat4(sh,"view",      (float*)view);
    shader_set_mat4(sh,"projection",(float*)proj);
    glm_mat4_identity(model); shader_set_mat4(sh,"model",(float*)model);
    sh_defaults();


    /* ── Transparent trails ──────────────────────────────────────────────── */

    sh_specular(0.0f,32.0f);
    shader_set_float(sh,"uAlphaTop",  0.80f);
    shader_set_float(sh,"uAlphaSide", 0.60f);
    shader_set_float(sh,"uAlphaMult", 1.0f);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    draw_emissive(1.0f,1.0f,1.0f,0.55f,0.55f,0.55f); dynmesh_draw(&player_trail);
    for(int i=0;i<MAX_AI;i++){
        if(i<ai_limit){
            draw_emissive(1.0f,0.95f,0.2f,0.60f,0.48f,0.0f);
            dynmesh_draw(&ai_trail_meshes[i]);
        }
    }
    glDepthMask(GL_TRUE); glDisable(GL_BLEND);
    sh_defaults();

#if SHOW_HITBOX
    /* ── Debug hitbox wireframe ──────────────────────────────────────────── */
    shader_set_vec3(sh,"objectColor",0.0f,0.0f,0.0f);
    shader_set_vec3(sh,"emissive",   1.0f,0.08f,0.08f);
    shader_set_float(sh,"ambientStr",0.0f);
    shader_set_float(sh,"fogDensity",0.0f);
    shader_set_float(sh,"uAlphaMult",0.55f);
    glPolygonMode(GL_FRONT_AND_BACK,GL_LINE); glLineWidth(2.0f);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glDepthFunc(GL_ALWAYS); glDepthMask(GL_FALSE);
    if(bike.alive||(bike.falling&&bike.y>ARENA_BASE+4.0f)){
        mat4 hm; glm_mat4_identity(hm);
        vec3 hp={bike.x,bike.y,bike.z}; glm_translate(hm,hp);
        glm_rotate(hm,DANGLE[bike.dir],yaxis);
        shader_set_mat4(sh,"model",(float*)hm); mesh_draw(&g_hitbox_mesh);
    }
    for(int i=0;i<ai_limit;i++){
        Bike *ab=&ai_bikes[i].bike;
        if(ab->alive||(ab->falling&&ab->y>ARENA_BASE+4.0f)){
            mat4 hm; glm_mat4_identity(hm);
            vec3 hp={ab->x,ab->y,ab->z}; glm_translate(hm,hp);
            glm_rotate(hm,DANGLE[ab->dir],yaxis);
            shader_set_mat4(sh,"model",(float*)hm); mesh_draw(&g_hitbox_mesh);
        }
    }
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    glDepthFunc(GL_LEQUAL); glDepthMask(GL_TRUE); glDisable(GL_BLEND);
    shader_set_float(sh,"fogDensity",FOG_DENSITY);
    sh_defaults();
    glm_mat4_identity(model); shader_set_mat4(sh,"model",(float*)model);
#endif

    /* ── Particles (additive) ────────────────────────────────────────────── */
    glm_mat4_identity(model); shader_use(sh); shader_set_mat4(sh,"model",(float*)model);
    vec3 cr={camera.right[0],camera.right[1],camera.right[2]};
    vec3 cu={camera.up[0],   camera.up[1],   camera.up[2]};
    pmesh_update(&part_mesh,part_buf,particles_build(cr,cu));
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE); glDepthMask(GL_FALSE);
    shader_use(psh);
    shader_set_mat4(psh,"view",      (float*)view);
    shader_set_mat4(psh,"projection",(float*)proj);
    pmesh_draw(&part_mesh);
    glDepthMask(GL_TRUE); glDisable(GL_BLEND);
}

/* ══════════════════════════════════════════════════════════════════════════════
   MAIN
   ══════════════════════════════════════════════════════════════════════════ */
int main(void){
    if(!glfwInit()){fprintf(stderr,"GLFW fail\n");return -1;}
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,6);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);

    int mc; GLFWmonitor **monitors=glfwGetMonitors(&mc);
    GLFWmonitor *mon=(MONITOR_INDEX>=0&&MONITOR_INDEX<mc)?monitors[MONITOR_INDEX]:glfwGetPrimaryMonitor();
    printf("Monitor: %s\n",glfwGetMonitorName(mon));
    const GLFWvidmode *vm=glfwGetVideoMode(mon);

    g_win=glfwCreateWindow(vm->width,vm->height,WINDOW_TITLE,mon,NULL);
    if(!g_win){glfwTerminate();return -1;}
    glfwMakeContextCurrent(g_win);
    glfwSetFramebufferSizeCallback(g_win,framebuffer_size_cb);
    glfwSetCursorPosCallback(g_win,mouse_cb);
    glfwSetScrollCallback(g_win,scroll_cb);

    if(!gladLoadGL(glfwGetProcAddress)){fprintf(stderr,"GLAD fail\n");return -1;}
    glEnable(GL_DEPTH_TEST);
    /* Scene geometry (add_box/add_frustum) uses CW winding viewed from outside,
       so we leave cull-face globally OFF. glb_draw_model manages it per-draw
       for the imported bike models only.                                     */

    SCR_W=(float)vm->width; SCR_H=(float)vm->height;
    SZ_HUGE  =SCR_H*0.055f; SZ_LARGE =SCR_H*0.040f;
    SZ_MEDIUM=SCR_H*0.028f; SZ_SMALL =SCR_H*0.021f;

    sh  =shader_create("../shaders/vertex.glsl",        "../shaders/fragment.glsl");
    psh =shader_create("../shaders/particle_vert.glsl", "../shaders/particle_frag.glsl");
    wsh =shader_create("../shaders/water_vert.glsl",    "../shaders/water_frag.glsl");
    ui_init(&ui,(int)SCR_W,(int)SCR_H);

    camera_init(&camera,0,70,130);
    particles_init();
    g_scene=scene_build();

    /* Skybox */
    {
        const char *faces[6] = {
            "../assets/textures/skybox/px.png",
            "../assets/textures/skybox/nx.png",
            "../assets/textures/skybox/py.png",
            "../assets/textures/skybox/ny.png",
            "../assets/textures/skybox/pz.png",
            "../assets/textures/skybox/nz.png",
        };
        if (!skybox_load(&g_skybox, faces,
                         "../shaders/skybox_vert.glsl",
                         "../shaders/skybox_frag.glsl"))
            fprintf(stderr, "Skybox load failed — continuing without it\n");
    }

    /* Debug hitbox mesh */
    {
        float hb[216];
        add_box(hb,0,
                -HITBOX_HALF_W+HITBOX_OFFSET_X,  HITBOX_OFFSET_Y,
                -HITBOX_HALF_L+HITBOX_OFFSET_Z,
                 HITBOX_HALF_W+HITBOX_OFFSET_X,   HITBOX_HALF_H*2.0f+HITBOX_OFFSET_Y,
                 HITBOX_HALF_L+HITBOX_OFFSET_Z);
        g_hitbox_mesh=mesh_create(hb,216);
    }

    /* Load GLB bike models */
    g_bike_player      = obj_load("../assets/models/bikes/bike-white.glb");
    g_bike_ai          = obj_load("../assets/models/bikes/bike-orange.glb");
    g_bike_player_refl = obj_load("../assets/models/bikes/bike-white-reflection.glb");
    g_bike_ai_refl     = obj_load("../assets/models/bikes/bike-orange-reflection.glb");

    bike_reset(&bike);
    player_trail=dynmesh_create(TRAIL_BUF_FLOATS);
    for(int i=0;i<MAX_AI;i++){
        ai_init(&ai_bikes[i],i);
        ai_trail_meshes[i]=dynmesh_create(TRAIL_BUF_FLOATS);
        menu_respawn_timer[i]=0.0f;
    }
    part_mesh=pmesh_create(PART_BUF_FLOATS);

    all_bikes_game[0]=&bike;
    for(int i=0;i<MAX_AI;i++){
        all_bikes_game[1+i]=&ai_bikes[i].bike;
        all_bikes_menu[i]  =&ai_bikes[i].bike;
    }

    enter_state(STATE_MAIN_MENU);

    int prev_a=0,prev_d=0,prev_w=0,prev_s=0,
        prev_tab=0,prev_c=0,prev_r=0,
        prev_f1=0,prev_f3=0,prev_f4=0,
        prev_esc=0,prev_up=0,prev_dn=0,prev_lt=0,prev_rt=0,
        prev_ret=0,prev_sp=0,prev_lmb=0,prev_sh=0;

    /* ════════════════════════════════════════════════════════════════════════
       MAIN LOOP
       ═════════════════════════════════════════════════════════════════════ */
    while(!glfwWindowShouldClose(g_win)){
        float now=(float)glfwGetTime();
        real_dt=now-last_wall; if(real_dt>0.1f) real_dt=0.1f;
        last_wall=now;

        int game_active=(g_state==STATE_PLAYING||g_state==STATE_GAME_OVER||g_state==STATE_WIN);
        float game_dt=game_active?real_dt:0.0f;

        int cur_lmb=glfwGetMouseButton(g_win,GLFW_MOUSE_BUTTON_LEFT)==GLFW_PRESS;
        g_lmb_clicked=cur_lmb&&!prev_lmb; g_lmb_down=cur_lmb;
        if(!cur_lmb) slider_drag=-1;
        prev_lmb=cur_lmb;

        int cur_a  =glfwGetKey(g_win,GLFW_KEY_A)         ==GLFW_PRESS;
        int cur_d  =glfwGetKey(g_win,GLFW_KEY_D)         ==GLFW_PRESS;
        int cur_w  =glfwGetKey(g_win,GLFW_KEY_W)         ==GLFW_PRESS;
        int cur_s  =glfwGetKey(g_win,GLFW_KEY_S)         ==GLFW_PRESS;
        int cur_tab=glfwGetKey(g_win,GLFW_KEY_TAB)       ==GLFW_PRESS;
        int cur_c  =glfwGetKey(g_win,GLFW_KEY_C)         ==GLFW_PRESS;
        int cur_r  =glfwGetKey(g_win,GLFW_KEY_R)         ==GLFW_PRESS;
        int cur_f1 =glfwGetKey(g_win,GLFW_KEY_F1)        ==GLFW_PRESS;
        int cur_f3 =glfwGetKey(g_win,GLFW_KEY_F3)        ==GLFW_PRESS;
        int cur_f4 =glfwGetKey(g_win,GLFW_KEY_F4)        ==GLFW_PRESS;
        int cur_esc=glfwGetKey(g_win,GLFW_KEY_ESCAPE)    ==GLFW_PRESS;
        int cur_up =glfwGetKey(g_win,GLFW_KEY_UP)        ==GLFW_PRESS;
        int cur_dn =glfwGetKey(g_win,GLFW_KEY_DOWN)      ==GLFW_PRESS;
        int cur_lt =glfwGetKey(g_win,GLFW_KEY_LEFT)      ==GLFW_PRESS;
        int cur_rt =glfwGetKey(g_win,GLFW_KEY_RIGHT)     ==GLFW_PRESS;
        int cur_ret=glfwGetKey(g_win,GLFW_KEY_ENTER)     ==GLFW_PRESS;
        int cur_sp =glfwGetKey(g_win,GLFW_KEY_SPACE)     ==GLFW_PRESS;
        int cur_sh =glfwGetKey(g_win,GLFW_KEY_LEFT_SHIFT)==GLFW_PRESS;

        if(cur_f3&&!prev_f3){g_brightness-=0.1f;if(g_brightness<0.0f)g_brightness=0.0f;}
        if(cur_f4&&!prev_f4){g_brightness+=0.1f;if(g_brightness>1.5f)g_brightness=1.5f;}

        int nav_up =(cur_up&&!prev_up)||(cur_w&&!prev_w);
        int nav_dn =(cur_dn&&!prev_dn)||(cur_s&&!prev_s);
        int nav_lt =(cur_lt&&!prev_lt)||(cur_a&&!prev_a);
        int nav_rt =(cur_rt&&!prev_rt)||(cur_d&&!prev_d);
        int nav_sel=(cur_ret&&!prev_ret)||(cur_sp&&!prev_sp);

        switch(g_state){
        case STATE_MAIN_MENU:
            if(cur_esc&&!prev_esc) glfwSetWindowShouldClose(g_win,1);
            if(nav_up){main_sel--;if(main_sel<0)main_sel=2;}
            if(nav_dn){main_sel++;if(main_sel>2)main_sel=0;}
            if(nav_sel){
                if(main_sel==0) enter_state(STATE_PLAY_SELECT);
                else if(main_sel==1){settings_return=STATE_MAIN_MENU;enter_state(STATE_SETTINGS);}
                else glfwSetWindowShouldClose(g_win,1);
            }
            break;
        case STATE_PLAY_SELECT:
            if(cur_esc&&!prev_esc) enter_state(STATE_MAIN_MENU);
            if(nav_lt){g_n_ai--;if(g_n_ai<1)g_n_ai=1;}
            if(nav_rt){g_n_ai++;if(g_n_ai>MAX_AI)g_n_ai=MAX_AI;}
            if(nav_up||nav_dn) play_sel=1-play_sel;
            if(nav_sel){if(play_sel==0) enter_state(STATE_COUNTDOWN); else enter_state(STATE_MAIN_MENU);}
            break;
        case STATE_COUNTDOWN:
            break;
        case STATE_PLAYING:
            if(cam_mode!=CAM_FREE){
                if(cur_a&&!prev_a) bike_turn_left (&bike);
                if(cur_d&&!prev_d) bike_turn_right(&bike);
            }
            if(cur_tab&&!prev_tab){cam_mode=(cam_mode==CAM_FREE)?CAM_FOLLOW:CAM_FREE;first_mouse=1;}
            if(cur_c  &&!prev_c  ){cam_mode=(cam_mode==CAM_TOPDOWN)?CAM_FOLLOW:CAM_TOPDOWN;}
            if(cur_r  &&!prev_r  ) enter_state(STATE_COUNTDOWN);
            if(cur_esc&&!prev_esc) enter_state(STATE_PAUSED);
            if(cur_f1 &&!prev_f1 ){settings_return=STATE_PLAYING;enter_state(STATE_SETTINGS);}
            if(cur_sh &&!prev_sh )  bike_activate_dash(&bike);
            if(cam_mode==CAM_FREE){
                if(glfwGetKey(g_win,GLFW_KEY_W)           ==GLFW_PRESS) camera_process_keyboard(&camera,CAM_FORWARD, game_dt);
                if(glfwGetKey(g_win,GLFW_KEY_S)           ==GLFW_PRESS) camera_process_keyboard(&camera,CAM_BACKWARD,game_dt);
                if(glfwGetKey(g_win,GLFW_KEY_A)           ==GLFW_PRESS) camera_process_keyboard(&camera,CAM_LEFT,    game_dt);
                if(glfwGetKey(g_win,GLFW_KEY_D)           ==GLFW_PRESS) camera_process_keyboard(&camera,CAM_RIGHT,   game_dt);
                if(glfwGetKey(g_win,GLFW_KEY_SPACE)       ==GLFW_PRESS) camera_process_keyboard(&camera,CAM_UP,      game_dt);
                if(glfwGetKey(g_win,GLFW_KEY_LEFT_CONTROL)==GLFW_PRESS) camera_process_keyboard(&camera,CAM_DOWN,    game_dt);
            }
            break;
        case STATE_PAUSED:
            if(cur_esc&&!prev_esc) enter_state(STATE_PLAYING);
            if(cur_f1 &&!prev_f1 ) enter_state(STATE_PLAYING);
            if(nav_up){pause_sel--;if(pause_sel<0)pause_sel=2;}
            if(nav_dn){pause_sel++;if(pause_sel>2)pause_sel=0;}
            if(nav_sel){
                if(pause_sel==0) enter_state(STATE_PLAYING);
                else if(pause_sel==1){settings_return=STATE_PAUSED;enter_state(STATE_SETTINGS);}
                else enter_state(STATE_TO_MENU);
            }
            break;
        case STATE_SETTINGS:
            if(cur_esc&&!prev_esc) enter_state(settings_return);
            break;
        case STATE_GAME_OVER:
        case STATE_WIN:
            if(cam_mode!=CAM_FREE){
                if(cur_a&&!prev_a) bike_turn_left (&bike);
                if(cur_d&&!prev_d) bike_turn_right(&bike);
            }
            if(cur_sh &&!prev_sh ) bike_activate_dash(&bike);
            if(cur_tab&&!prev_tab){cam_mode=(cam_mode==CAM_FREE)?CAM_FOLLOW:CAM_FREE;first_mouse=1;}
            if(cur_r  &&!prev_r  ) enter_state(STATE_COUNTDOWN);
            if(cam_mode==CAM_FREE){
                if(glfwGetKey(g_win,GLFW_KEY_W)           ==GLFW_PRESS) camera_process_keyboard(&camera,CAM_FORWARD, game_dt);
                if(glfwGetKey(g_win,GLFW_KEY_S)           ==GLFW_PRESS) camera_process_keyboard(&camera,CAM_BACKWARD,game_dt);
                if(glfwGetKey(g_win,GLFW_KEY_A)           ==GLFW_PRESS) camera_process_keyboard(&camera,CAM_LEFT,    game_dt);
                if(glfwGetKey(g_win,GLFW_KEY_D)           ==GLFW_PRESS) camera_process_keyboard(&camera,CAM_RIGHT,   game_dt);
                if(glfwGetKey(g_win,GLFW_KEY_SPACE)       ==GLFW_PRESS) camera_process_keyboard(&camera,CAM_UP,      game_dt);
                if(glfwGetKey(g_win,GLFW_KEY_LEFT_CONTROL)==GLFW_PRESS) camera_process_keyboard(&camera,CAM_DOWN,    game_dt);
            }
            break;
        case STATE_TO_MENU:
            break;
        }

        /* ── Menu AI background ─────────────────────────────────────────── */
        int menu_ctx=(g_state==STATE_MAIN_MENU||g_state==STATE_PLAY_SELECT||
                      (g_state==STATE_SETTINGS&&settings_return==STATE_MAIN_MENU));
        if(menu_ctx){
            int nv=0;
            for(int i=0;i<MAX_AI;i++) if(ai_bikes[i].bike.alive) nv++;
            ai_pack_coordinator(ai_bikes,MAX_AI,nv,NULL,real_dt);
            for(int i=0;i<MAX_AI;i++)
                ai_update(&ai_bikes[i],real_dt,all_bikes_menu,MAX_AI,NULL,nv);
            for(int i=0;i<MAX_AI;i++){
                if(!ai_bikes[i].bike.alive){
                    if(prev_bike_alive[i+1]){
                        float dy=ai_bikes[i].bike.falling?ARENA_BASE:ARENA_TOP;
                        particles_spawn_death_cloud(ai_bikes[i].bike.x,ai_bikes[i].bike.z,
                            ai_bikes[i].bike.dir,
                            BIKE_SPEED*ai_bikes[i].bike.speed_mult,
                            1.0f,0.5f,0.05f,dy);
                        prev_bike_alive[i+1]=0;
                        menu_respawn_timer[i]=MENU_RESPAWN_DELAY;
                    } else {
                        menu_respawn_timer[i]-=real_dt;
                        if(menu_respawn_timer[i]<=0.0f){
                            ai_reset(&ai_bikes[i],i);
                            ai_bikes[i].bike.alive=1;
                            prev_bike_alive[i+1]=1;
                        }
                    }
                }
            }
            if(g_state==STATE_MAIN_MENU){
                menu_fade+=real_dt*2.0f;
                if(menu_fade>1.0f) menu_fade=1.0f;
            }
        }

        /* ── Countdown camera transition ────────────────────────────────── */
        if(g_state==STATE_COUNTDOWN){
            countdown_timer+=real_dt;
            float t=countdown_timer/COUNTDOWN_DUR; if(t>1.0f) t=1.0f;
            float sv=smoothstep(t);
            float ep[3]={FOLLOW_START_PX,FOLLOW_START_PY,FOLLOW_START_PZ};
            float ef[3]={FOLLOW_START_FX,FOLLOW_START_FY,FOLLOW_START_FZ};
            float np[3],nf[3];
            lerp3(np,trans_from_pos,ep,sv); lerp3(nf,trans_from_front,ef,sv);
            camera.position[0]=np[0]; camera.position[1]=np[1]; camera.position[2]=np[2];
            float fl=sqrtf(nf[0]*nf[0]+nf[1]*nf[1]+nf[2]*nf[2]);
            if(fl>0.0001f){camera.front[0]=nf[0]/fl;camera.front[1]=nf[1]/fl;camera.front[2]=nf[2]/fl;}
            apply_camera_vecs();
            if(countdown_timer>=COUNTDOWN_DUR){
                cam_px=FOLLOW_START_PX; cam_py=FOLLOW_START_PY; cam_pz=FOLLOW_START_PZ;
                enter_state(STATE_PLAYING);
            }
        }

        /* ── Game physics ───────────────────────────────────────────────── */
        if(game_active){
            int nai=0;
            for(int i=0;i<g_n_ai;i++) if(ai_bikes[i].bike.alive) nai++;
            ai_pack_coordinator(ai_bikes,g_n_ai,nai,&bike,game_dt);
            bike_update(&bike,game_dt,all_bikes_game,1+g_n_ai);
            for(int i=0;i<g_n_ai;i++)
                ai_update(&ai_bikes[i],game_dt,all_bikes_game,1+g_n_ai,&bike,nai);

            if(!bike.alive&&prev_bike_alive[0]){
                float dy=bike.falling?ARENA_BASE:ARENA_TOP;
                particles_spawn_death_cloud(bike.x,bike.z,bike.dir,
                    BIKE_SPEED*bike.speed_mult,0.6f,1.0f,1.0f,dy);
                prev_bike_alive[0]=0;
            }
            for(int i=0;i<g_n_ai;i++){
                if(!ai_bikes[i].bike.alive&&prev_bike_alive[i+1]){
                    float dy=ai_bikes[i].bike.falling?ARENA_BASE:ARENA_TOP;
                    particles_spawn_death_cloud(ai_bikes[i].bike.x,ai_bikes[i].bike.z,
                        ai_bikes[i].bike.dir,
                        BIKE_SPEED*ai_bikes[i].bike.speed_mult,1.0f,0.5f,0.05f,dy);
                    prev_bike_alive[i+1]=0;
                }
            }

            /* Dash sparks — player */
            if(bike.alive&&!bike.falling){
                int rl=(bike.dash_timer>0.0f)?2:(bike.dash_cooldown<=0.0f?1:0);
                particles_emit_dash(bike.x,bike.y,bike.z,bike.dir,0.2f,1.0f,1.0f,rl,game_dt);
            }
            /* Dash sparks — AIs */
            for(int i=0;i<g_n_ai;i++){
                Bike *ab=&ai_bikes[i].bike;
                if(ab->alive&&!ab->falling){
                    int rl=(ab->dash_timer>0.0f)?2:(ab->dash_cooldown<=0.0f?1:0);
                    particles_emit_dash(ab->x,ab->y,ab->z,ab->dir,1.0f,0.6f,0.05f,rl,game_dt);
                }
            }

            if(g_state==STATE_PLAYING){
                if(player_was_alive&&!bike.alive&&cam_mode!=CAM_FREE)
                    enter_state(STATE_GAME_OVER);
                player_was_alive=bike.alive;
                if(!win_announced&&bike.alive&&cam_mode!=CAM_FREE){
                    int na=0;
                    for(int i=0;i<g_n_ai;i++) if(ai_bikes[i].bike.alive||ai_bikes[i].bike.falling) na++;
                    if(na==0){win_announced=1;enter_state(STATE_WIN);}
                }
            }

            update_follow_camera(game_dt);
        }

        if(g_state==STATE_GAME_OVER||g_state==STATE_WIN){
            end_timer-=real_dt;
            if(end_timer<=0.0f) enter_state(STATE_TO_MENU);
        }

        if(g_state==STATE_TO_MENU){
            to_menu_timer+=real_dt;
            float t=to_menu_timer/TO_MENU_DUR; if(t>1.0f) t=1.0f;
            float sv=smoothstep(t);
            float ep[3]={MENU_CAM_PX,MENU_CAM_PY,MENU_CAM_PZ};
            float ef[3]={MENU_CAM_FX,MENU_CAM_FY,MENU_CAM_FZ};
            float np[3],nf[3];
            lerp3(np,trans_from_pos,ep,sv); lerp3(nf,trans_from_front,ef,sv);
            camera.position[0]=np[0]; camera.position[1]=np[1]; camera.position[2]=np[2];
            float fl=sqrtf(nf[0]*nf[0]+nf[1]*nf[1]+nf[2]*nf[2]);
            if(fl>0.0001f){camera.front[0]=nf[0]/fl;camera.front[1]=nf[1]/fl;camera.front[2]=nf[2]/fl;}
            apply_camera_vecs();
            if(to_menu_timer>=TO_MENU_DUR) enter_state(STATE_MAIN_MENU);
        }

        particles_update(real_dt);
        particles_update_death(real_dt);
        particles_update_dash(real_dt);

        /* ── Trail geometry ─────────────────────────────────────────────── */
        trail_w=(cam_mode==CAM_TOPDOWN)?0.3f:0.1f;
        if(menu_ctx||g_state==STATE_TO_MENU)
            dynmesh_update(&player_trail,player_trail_buf,0);
        else
            dynmesh_update(&player_trail,player_trail_buf,bike_build_trail(&bike,player_trail_buf));

        int in_game_t=(g_state==STATE_PLAYING||g_state==STATE_GAME_OVER||
                       g_state==STATE_WIN||g_state==STATE_COUNTDOWN||
                       g_state==STATE_PAUSED||
                       (g_state==STATE_SETTINGS&&settings_return!=STATE_MAIN_MENU));
        int ai_tl=in_game_t?g_n_ai:MAX_AI;
        for(int i=0;i<MAX_AI;i++){
            if(i<ai_tl)
                dynmesh_update(&ai_trail_meshes[i],ai_trail_bufs[i],
                               bike_build_trail(&ai_bikes[i].bike,ai_trail_bufs[i]));
            else
                dynmesh_update(&ai_trail_meshes[i],ai_trail_bufs[i],0);
        }

        render_3d(vm->width,vm->height);

        ui_begin(&ui);
        switch(g_state){
            case STATE_MAIN_MENU:   render_main_menu();         break;
            case STATE_PLAY_SELECT: render_play_select();       break;
            case STATE_COUNTDOWN:   render_countdown_overlay(); break;
            case STATE_PAUSED:      render_pause_menu();        break;
            case STATE_SETTINGS:    render_settings();          break;
            case STATE_GAME_OVER:   render_big_text_overlay("GAME OVER", 1.0f,0.12f,0.12f); break;
            case STATE_WIN:         render_big_text_overlay("USER LIVES!",0.12f,1.0f,0.55f); break;
            default: break;
        }
        ui_end(&ui);

        prev_a=cur_a;   prev_d=cur_d;   prev_w=cur_w;   prev_s=cur_s;
        prev_tab=cur_tab; prev_c=cur_c; prev_r=cur_r;
        prev_f1=cur_f1; prev_f3=cur_f3; prev_f4=cur_f4;
        prev_esc=cur_esc; prev_up=cur_up; prev_dn=cur_dn;
        prev_lt=cur_lt;   prev_rt=cur_rt;
        prev_ret=cur_ret; prev_sp=cur_sp; prev_sh=cur_sh;

        glfwSwapBuffers(g_win); glfwPollEvents();
    }

    /* ── Cleanup ──────────────────────────────────────────────────────────── */
    skybox_free(&g_skybox);
    mesh_free(&g_hitbox_mesh);
    obj_free(&g_bike_player);
    obj_free(&g_bike_ai);
    obj_free(&g_bike_player_refl);
    obj_free(&g_bike_ai_refl);
    ui_free(&ui);
    scene_free(&g_scene);
    dynmesh_free(&player_trail);
    for(int i=0;i<MAX_AI;i++) dynmesh_free(&ai_trail_meshes[i]);
    pmesh_free(&part_mesh);
    glDeleteProgram(sh); glDeleteProgram(psh); glDeleteProgram(wsh);
    glfwTerminate();
    return 0;
}