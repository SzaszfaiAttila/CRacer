#ifndef CONFIG_H
#define CONFIG_H

#define WINDOW_TITLE   "TRON CRacer"

// ── Monitor selection (for Hyprland / multi-monitor setups) ─────────────────
#define MONITOR_INDEX  1

// ── Arena ─────────────────────────────────────────────────────────────────────
#define ARENA_HALF    72.0f
#define ARENA_TOP      3.5f
#define ARENA_BASE    -4.0f
#define PILLAR_DIST  100.0f
#define PILLAR_BH      5.0f
#define PILLAR_TH      2.2f
#define PILLAR_HEIGHT 28.0f

// ── Bike / Trail ──────────────────────────────────────────────────────────────
#define BIKE_SPEED      20.0f
#define TRAIL_HEIGHT     1.3f
#define TRAIL_LENGTH    125.0f
#define TRAIL_PHYS_HW   0.05f
#define COLLISION_R      0.8f
#define GRACE_DIST       5.0f
#define MAX_WAYPOINTS   4096
#define TRAIL_BUF_FLOATS (MAX_WAYPOINTS * 216)

// ── Dash ──────────────────────────────────────────────────────────────────────
#define DASH_DURATION      3.0f   /* seconds of 1.5× speed                  */
#define DASH_COOLDOWN_DUR  6.0f   /* recharge time after dash expires        */
#define DASH_SPEED_MULT    1.5f   /* speed multiplier while dashing          */

// ── Particles ─────────────────────────────────────────────────────────────────
#define NUM_PILLARS       4
#define PARTS_PER_PILLAR  140
#define TOTAL_PARTS       (NUM_PILLARS * PARTS_PER_PILLAR)

// ── Death explosion particles ─────────────────────────────────────────────────
#define DEATH_PARTS_PER_BIKE  140
#define MAX_DEATH_PARTICLES   (8 * DEATH_PARTS_PER_BIKE)

// ── Dash spark particles ──────────────────────────────────────────────────────
#define MAX_DASH_SPARKS  640

#define PART_FLOATS_PER   (6 * 7)
#define PART_BUF_FLOATS   ((TOTAL_PARTS + MAX_DEATH_PARTICLES + MAX_DASH_SPARKS) * PART_FLOATS_PER)

// ── AI ────────────────────────────────────────────────────────────────────────
#define MAX_AI  6

// ── Debug hitbox ──────────────────────────────────────────────────────────────
// Set SHOW_HITBOX to 1 to render a red wireframe box around every bike.
// The box is drawn over all geometry (depth-always) so it is always visible.
// Tweak the values below to align the box with the visible OBJ model.
#define SHOW_HITBOX         0       /* 0 = off, 1 = on                        */

// Half-extents of the box (full box = 2x each value in that axis).
// Defaults mirror the actual collision radius used by bike_trail_point_hits.
#define HITBOX_HALF_W       0.2f   /* left / right                           */
#define HITBOX_HALF_L       0.2f   /* front / back  (bikes are longer)       */
#define HITBOX_HALF_H       0.5f   /* vertical, measured upward from pivot   */

// Local-space position offset (applied before direction rotation).
// Positive X = bike's right, positive Y = up, positive Z = backward (into trail).
#define HITBOX_OFFSET_X     0.0f
#define HITBOX_OFFSET_Y     0.0f    /* raise/lower to match BIKE_MODEL_HEIGHT_OFFSET */
#define HITBOX_OFFSET_Z     -0.2f   /* forward(negative) / backward(positive) shift  */

#endif
