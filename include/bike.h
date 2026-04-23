#ifndef BIKE_H
#define BIKE_H

#include "mesh.h"
#include "config.h"

typedef enum { DIR_NORTH=0, DIR_EAST=1, DIR_SOUTH=2, DIR_WEST=3 } Direction;

extern const float DDX[4];
extern const float DDZ[4];
extern const float DANGLE[4];

typedef struct {
    float     x, z;
    float     y;
    float     vy;
    Direction dir;
    int       alive;
    int       falling;
    float     fall_start_x;
    float     fall_start_z;
    float     trail_end_dist;
    float     wp_x   [MAX_WAYPOINTS];
    float     wp_z   [MAX_WAYPOINTS];
    float     wp_dist[MAX_WAYPOINTS];
    int       wp_count;
    float     total_dist;

    /* ── Dash ──────────────────────────────────────────────────────────── */
    float     speed_mult;    /* 1.0 = normal,  DASH_SPEED_MULT while dashing */
    float     dash_timer;    /* > 0 = currently dashing                       */
    float     dash_cooldown; /* > 0 = recharging, can't dash yet              */
} Bike;

extern float trail_w;

/* ── Lifecycle ─────────────────────────────────────────────────────────────── */
void bike_reset      (Bike *b);
void bike_turn_left  (Bike *b);
void bike_turn_right (Bike *b);
void bike_move       (Bike *b, float dt);
void bike_update     (Bike *b, float dt, Bike **all, int n);

/* Activate dash if able (no-op when cooling down or already dashing) */
void bike_activate_dash(Bike *b);

/* ── Collision ─────────────────────────────────────────────────────────────── */
int bike_trail_point_hits     (Bike *b, float px, float pz, float radius, int use_grace);
/* Box-based trail test — collision shape matches SHOW_HITBOX debug box */
int bike_trail_box_hits       (Bike *b, float px, float pz, Direction dir, int use_grace);
int bike_check_collision_multi(Bike *b, Bike **all, int n);

/* ── Trail geometry ─────────────────────────────────────────────────────────── */
int bike_build_trail(Bike *b, float *buf);

#endif