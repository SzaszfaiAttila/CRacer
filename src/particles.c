#include "particles.h"
#include "config.h"
#include <math.h>
#include <GLFW/glfw3.h>

float part_buf[PART_BUF_FLOATS];

// Pillar top centres
static const float PTOP[4][2] = {
    {-PILLAR_DIST,-PILLAR_DIST},
    { PILLAR_DIST,-PILLAR_DIST},
    {-PILLAR_DIST, PILLAR_DIST},
    { PILLAR_DIST, PILLAR_DIST},
};

typedef struct {
    float x, y, z;
    float vx, vy, vz;
    float life, max_life, size;
    int   pillar;
} Particle;

static Particle particles[TOTAL_PARTS];

static float lcg_rand(unsigned int *s) {
    *s = (*s) * 1664525u + 1013904223u;
    return (float)(*s & 0x7FFFFFFF) / (float)0x7FFFFFFF;
}

static void particle_spawn(Particle *p, int pillar_idx, unsigned int *seed) {
    float r=lcg_rand(seed)*2.0f, theta=lcg_rand(seed)*6.2832f;
    p->x        = PTOP[pillar_idx][0] + r*cosf(theta);
    p->y        = PILLAR_HEIGHT + 3.5f;
    p->z        = PTOP[pillar_idx][1] + r*sinf(theta);
    p->vx       = (lcg_rand(seed)-0.5f)*1.8f;
    p->vy       = 3.5f + lcg_rand(seed)*7.0f;
    p->vz       = (lcg_rand(seed)-0.5f)*1.8f;
    p->max_life = 0.9f + lcg_rand(seed)*1.4f;
    p->life     = p->max_life * lcg_rand(seed);
    p->size     = 1.4f + lcg_rand(seed)*1.2f;
    p->pillar   = pillar_idx;
}

void particles_init(void) {
    static const unsigned int seeds[4] = {
        0xDEADBEEF, 0xCAFEBABE, 0x12345678, 0xABCDEF01
    };
    for (int i=0;i<TOTAL_PARTS;i++) {
        int pillar=i/PARTS_PER_PILLAR;
        unsigned int seed=seeds[pillar]+(unsigned int)(i%PARTS_PER_PILLAR)*6271u;
        particle_spawn(&particles[i], pillar, &seed);
    }
}

void particles_update(float dt) {
    unsigned int seed=0xCAFEBABE;
    float t=(float)glfwGetTime();
    for (int i=0;i<TOTAL_PARTS;i++) {
        Particle *p=&particles[i];
        p->life -= dt;
        if (p->life<=0.0f) { particle_spawn(p,p->pillar,&seed); seed+=i*6271u; continue; }
        float noise=sinf(t*3.1f+i*0.37f);
        p->vx += noise*0.9f*dt;
        p->vz += cosf(t*2.7f+i*0.53f)*0.9f*dt;
        p->vx *= (1.0f-1.5f*dt);
        p->vz *= (1.0f-1.5f*dt);
        p->vy -= 0.4f*dt;
        if (p->vy<0.5f) p->vy=0.5f;
        p->x += p->vx*dt;
        p->y += p->vy*dt;
        p->z += p->vz*dt;
    }
}

int particles_build(vec3 cam_right, vec3 cam_up) {
    int off=0;
    for (int i=0;i<TOTAL_PARTS;i++) {
        Particle *p=&particles[i];
        float ratio=p->life/p->max_life;
        float r,g,b,a;
        if (ratio>0.65f) {
            float t=(ratio-0.65f)/0.35f;
            r=0.5f+t*0.5f; g=0.7f+t*0.3f; b=1.0f;
        } else if (ratio>0.25f) {
            float t=(ratio-0.25f)/0.40f;
            r=0.05f+t*0.45f; g=0.25f+t*0.45f; b=1.0f;
        } else {
            float t=ratio/0.25f;
            r=0.0f; g=t*0.1f; b=0.4f+t*0.6f;
        }
        a=ratio*0.85f;
        float hs=p->size*(0.4f+ratio*0.6f);
        float rx=cam_right[0]*hs, ry=cam_right[1]*hs, rz=cam_right[2]*hs;
        float ux=cam_up[0]*hs,    uy=cam_up[1]*hs,    uz=cam_up[2]*hs;
        float cx=p->x, cy=p->y, cz=p->z;
        float v[4][3]={
            {cx-rx-ux,cy-ry-uy,cz-rz-uz},
            {cx+rx-ux,cy+ry-uy,cz+rz-uz},
            {cx+rx+ux,cy+ry+uy,cz+rz+uz},
            {cx-rx+ux,cy-ry+uy,cz-rz+uz},
        };
        int idx[6]={0,1,2,0,2,3};
        for (int k=0;k<6;k++){
            float *vv=v[idx[k]];
            part_buf[off++]=vv[0]; part_buf[off++]=vv[1]; part_buf[off++]=vv[2];
            part_buf[off++]=r; part_buf[off++]=g; part_buf[off++]=b; part_buf[off++]=a;
        }
    }
    return off;
}