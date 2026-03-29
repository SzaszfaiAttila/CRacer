#ifndef BIKE_H
#define BIKE_H

#include "mesh.h"
#include "config.h"

typedef enum { DIR_NORTH=0, DIR_EAST=1, DIR_SOUTH=2, DIR_WEST=3 } Direction;

extern const float DDX[4];    // X delta per direction
extern const float DDZ[4];    // Z delta per direction
extern const float DANGLE[4]; // Y-rotation radians per direction (bike faces -Z at 0)

typedef struct {
    float     x, z;
    Direction dir;
    int       alive;
    float     wp_x   [MAX_WAYPOINTS];
    float     wp_z   [MAX_WAYPOINTS];
    float     wp_dist[MAX_WAYPOINTS];
    int       wp_count;
    float     total_dist;
} Bike;

// Display trail width — set by main each frame based on cam mode
extern float trail_w;

// ── Lifecycle ─────────────────────────────────────────────────────────────────
void bike_reset      (Bike *b);
void bike_turn_left  (Bike *b);
void bike_turn_right (Bike *b);

// Move the bike one frame (no collision check)
void bike_move (Bike *b, float dt);

// Move + check collision against ALL bikes in array; sets b->alive=0 on death
void bike_update(Bike *b, float dt, Bike **all, int n);

// ── Collision ─────────────────────────────────────────────────────────────────
// Returns 1 if point (px,pz) with given radius hits any visible trail segment
// of bike b.  use_grace=1 excludes the GRACE_DIST near the head (for self-checks).
int bike_trail_point_hits(Bike *b, float px, float pz, float radius, int use_grace);

// Full multi-bike collision check for bike b (arena boundary + all trails)
int bike_check_collision_multi(Bike *b, Bike **all, int n);

// ── Trail geometry ────────────────────────────────────────────────────────────
// Fills caller-provided buf with box geometry; returns float count written.
int bike_build_trail(Bike *b, float *buf);

#endif