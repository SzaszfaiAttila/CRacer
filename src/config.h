#ifndef CONFIG_H
#define CONFIG_H

#define WINDOW_TITLE   "TRON CRacer"

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
#define TRAIL_HEIGHT     1.35f
#define TRAIL_LENGTH    150.0f
#define TRAIL_PHYS_HW   0.05f
#define COLLISION_R      0.8f
#define GRACE_DIST       5.0f
#define MAX_WAYPOINTS   4096
#define TRAIL_BUF_FLOATS (MAX_WAYPOINTS * 216)

// ── Particles ─────────────────────────────────────────────────────────────────
#define NUM_PILLARS       4
#define PARTS_PER_PILLAR  140
#define TOTAL_PARTS       (NUM_PILLARS * PARTS_PER_PILLAR)
#define PART_FLOATS_PER   (6 * 7)
#define PART_BUF_FLOATS   (TOTAL_PARTS * PART_FLOATS_PER)

// ── AI ────────────────────────────────────────────────────────────────────────
#define MAX_AI  6   // max supported; set N_AI in main.c (1–6)

#endif