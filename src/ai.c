#include "ai.h"
#include <math.h>
#include <string.h>

// ── Tuning ────────────────────────────────────────────────────────────────────
#define LOOKAHEAD_STEP      1.0f
#define LOOKAHEAD_N         80      // INSANE lookahead (~80 units)
#define DANGER_STEPS        10
#define TURN_COOLDOWN       0.4f
#define OTHER_TRAIL_DANGER  6       // ultra-fast reaction when about to hit ANY other trail
#define TEAM_AVOID_PENALTY  35.0f   // heavy penalty if teammate trail is <12 steps away
#define LOOKAHEAD_R         (COLLISION_R + 0.5f)

// Aggression scaling (your latest high values)
static float aggression_scale(int n_alive) {
    if (n_alive <= 1) return 9.0f;
    if (n_alive == 2) return 8.0f;
    if (n_alive == 3) return 7.0f;
    if (n_alive == 4) return 6.0f;
    if (n_alive == 5) return 5.0f;
    return 4.0f;
}

// Starting positions for up to 6 AIs
static const float AI_START[MAX_AI][3] = {
    {  10.0f,  -64.0f, (float)DIR_SOUTH },
    {  30.0f,  -64.0f, (float)DIR_SOUTH },
    {  50.0f,  -64.0f, (float)DIR_SOUTH },
    { -10.0f,  -64.0f, (float)DIR_SOUTH },
    { -30.0f,  -64.0f, (float)DIR_SOUTH },
    { -50.0f,  -64.0f, (float)DIR_SOUTH },
};

// ── LCG ──────────────────────────────────────────────────────────────────────
static float ai_lcg(unsigned int *s) {
    *s = *s * 1664525u + 1013904223u;
    return (float)(*s & 0x7FFFFFFFu) / 2147483647.0f;
}

// ── Lookahead ─────────────────────────────────────────────────────────────────
static int score_dir(float px, float pz, Direction dir,
                     Bike *self, Bike **all, int n) {
    for (int i = 1; i <= LOOKAHEAD_N; i++) {
        float nx = px + DDX[dir] * LOOKAHEAD_STEP * i;
        float nz = pz + DDZ[dir] * LOOKAHEAD_STEP * i;
        if (fabsf(nx) > ARENA_HALF - 2.0f || fabsf(nz) > ARENA_HALF - 2.0f)
            return i;
        for (int b = 0; b < n; b++) {
            int use_grace = (all[b] == self);
            if (bike_trail_point_hits(all[b], nx, nz, LOOKAHEAD_R, use_grace))
                return i;
        }
    }
    return LOOKAHEAD_N;
}

// Teammate-specific clear distance (used for active mutual avoidance)
static int teammate_clear_steps(float px, float pz, Direction dir,
                                Bike *self, Bike **all, int n) {
    for (int i = 1; i <= LOOKAHEAD_N; i++) {
        float nx = px + DDX[dir] * LOOKAHEAD_STEP * i;
        float nz = pz + DDZ[dir] * LOOKAHEAD_STEP * i;
        if (fabsf(nx) > ARENA_HALF - 2.0f || fabsf(nz) > ARENA_HALF - 2.0f)
            return i;                     // wall stops the count (treated as "blocked")
        for (int b = 0; b < n; b++) {
            if (all[b] == self) continue;
            if (bike_trail_point_hits(all[b], nx, nz, LOOKAHEAD_R, 0))
                return i;
        }
    }
    return LOOKAHEAD_N;
}

static int score_after_turn(AIBike *ai, int turn_dir,
                             Bike **all, int n) {
    Direction new_dir = (Direction)(((int)ai->bike.dir + turn_dir + 4) % 4);
    float nx = ai->bike.x + DDX[new_dir] * LOOKAHEAD_STEP;
    float nz = ai->bike.z + DDZ[new_dir] * LOOKAHEAD_STEP;
    return score_dir(nx, nz, new_dir, &ai->bike, all, n);
}

// ── Decision ──────────────────────────────────────────────────────────────────
static void ai_decide(AIBike *ai, float dt,
                      Bike **all, int n,
                      Bike *player, int n_ai_alive) {
    (void)dt;
    Bike      *b     = &ai->bike;
    Direction  cur   = b->dir;
    Direction  left  = (Direction)((cur + 3) % 4);
    Direction  right = (Direction)((cur + 1) % 4);

    int sa = score_dir(b->x, b->z, cur,   b, all, n);
    int sl = score_dir(b->x, b->z, left,  b, all, n);
    int sr = score_dir(b->x, b->z, right, b, all, n);

    // Teammate avoidance distances (insane mutual avoidance)
    int tl = teammate_clear_steps(b->x, b->z, left,  b, all, n);
    int ts = teammate_clear_steps(b->x, b->z, cur,   b, all, n);
    int tr = teammate_clear_steps(b->x, b->z, right, b, all, n);

    int emergency = (sa < DANGER_STEPS);
    int turn      = 0;

    // Other-trail ultra-fast reaction (already on straight, now extended via penalty below)
    int other_steps = 999;
    for (int i = 1; i <= OTHER_TRAIL_DANGER + 4; ++i) {
        float nx = b->x + DDX[cur] * LOOKAHEAD_STEP * i;
        float nz = b->z + DDZ[cur] * LOOKAHEAD_STEP * i;
        int hit_other = 0;
        for (int bb = 0; bb < n; ++bb) {
            if (all[bb] == b) continue;
            if (bike_trail_point_hits(all[bb], nx, nz, LOOKAHEAD_R, 0)) {
                hit_other = 1;
                break;
            }
        }
        if (hit_other) {
            other_steps = i;
            break;
        }
    }
    int other_emergency = (other_steps <= OTHER_TRAIL_DANGER);
    if (other_emergency) emergency = 1;

    // ── Unified scoring: survival + huge straight bias + alignment + teammate penalty ──
    float straight_bias = emergency ? 0.0f : 18.0f;   // stronger bias = cleaner lines
    float align_scale   = 32.0f;                      // higher pull when safe

    float scores[3] = { (float)sl, (float)sa, (float)sr };
    scores[1] += straight_bias;

    // ── Determine target (roam > smart intercept prediction > hunt role) ──
    float tx = 0.0f, tz = 0.0f;
    int has_target = 0;

    if (ai->roam_timer > 0.0f) {
        tx = ai->roam_tx;
        tz = ai->roam_tz;
        has_target = 1;
    } else if (player && player->alive) {
        has_target = 1;
        tx = player->x;
        tz = player->z;

        float pdx = DDX[player->dir];
        float pdz = DDZ[player->dir];
        float ax = b->x - player->x;
        float az = b->z - player->z;
        float proj = ax * pdx + az * pdz;
        float adx = DDX[b->dir];
        float adz = DDZ[b->dir];
        float dir_dot = adx * pdx + adz * pdz;

        // SMART movement prediction: dynamic lead distance
        float lead = 30.0f;
        if (proj > 20.0f && dir_dot > 0.65f) {
            lead = 48.0f;                     // cut-off when nicely ahead & parallel
        } else if (ai->hunt_role == HUNT_INTERCEPT) {
            lead = 38.0f;
        } else if (proj < 0.0f) {
            lead = 18.0f;                     // shorter lead when behind
        }

        if (proj > 18.0f && dir_dot > 0.60f) {
            tx = player->x + pdx * lead;
            tz = player->z + pdz * lead;
        } else if (ai->hunt_role == HUNT_INTERCEPT) {
            tx = player->x + pdx * lead;
            tz = player->z + pdz * lead;
        }
    }

    // Alignment + space-aware aggression (prevents box traps even at max aggression)
    if (!emergency && has_target) {
        float eff_agg = ai->base_aggression * aggression_scale(n_ai_alive);
        if (eff_agg > 1.0f) eff_agg = 1.0f;

        float safe_threshold = (float)DANGER_STEPS * 4.5f;   // very strict space requirement

        for (int i = 0; i < 3; ++i) {
            Direction d = (i == 0 ? left : (i == 1 ? cur : right));
            float dx = tx - b->x;
            float dz = tz - b->z;
            float dist = hypotf(dx, dz);
            if (dist > 1.0f) {
                dx /= dist;
                dz /= dist;
                float dirx = DDX[d];
                float dirz = DDZ[d];
                float align = dx * dirx + dz * dirz;

                float space_factor = scores[i] / safe_threshold;
                if (space_factor > 1.0f) space_factor = 1.0f;
                scores[i] += align * align_scale * eff_agg * (space_factor * space_factor);
            }
        }
    }

    // ── Active teammate avoidance (this is the "no more offing each other" part) ──
    float t_scores[3] = { (float)tl, (float)ts, (float)tr };
    for (int i = 0; i < 3; ++i) {
        if (t_scores[i] < 12.0f) {
            float penalty = TEAM_AVOID_PENALTY * (1.0f - t_scores[i] / 12.0f);
            scores[i] -= penalty;
        }
    }

    // Pick best direction
    int best = 1;
    if (scores[0] > scores[best]) best = 0;
    if (scores[2] > scores[best]) best = 2;
    turn = (best == 0 ? -1 : (best == 2 ? 1 : 0));

    // Occasional human-like miss
    if (!emergency && turn != 0 && ai_lcg(&ai->rng) < 0.05f)
        turn = 0;

    // Queue turn
    if (turn != 0) {
        int post = score_after_turn(ai, (turn == -1) ? 3 : 1, all, n);
        if (post < DANGER_STEPS && sa >= DANGER_STEPS) {
            ai->cooldown = 0.05f;
            return;
        }

        ai->pending_turn      = turn;
        ai->pending_emergency = other_emergency;

        float delay;
        if (emergency) {
            if (other_emergency)
                delay = 0.005f + ai_lcg(&ai->rng) * 0.02f;
            else
                delay = 0.04f + ai_lcg(&ai->rng) * 0.05f;
        } else {
            delay = ai->react_delay + (ai_lcg(&ai->rng) - 0.5f) * 0.06f;
        }
        ai->react_timer = delay > 0.005f ? delay : 0.005f;
    } else {
        ai->cooldown = 0.05f;
    }
}

// Pack coordinator + token system (unchanged, still gives perfect coordination)
static float pack_cooldown = 0.0f;

void ai_pack_coordinator(AIBike *ai_arr, int n_ai, int n_alive, Bike *player, float dt) {
    if (n_alive < 1 || !player || !player->alive) return;

    // Doom-Eternal token (closest eligible AI is always intercept)
    float min_dist = 1e9f;
    int token_idx = -1;

    for (int i = 0; i < n_ai; i++) {
        AIBike *ai = &ai_arr[i];
        if (!ai->bike.alive) continue;
        int in_pack = (ai->hunt_timer > 0.0f);
        int in_roam = (ai->roam_timer > 0.0f);
        if (in_pack || in_roam) continue;

        float dx = ai->bike.x - player->x;
        float dz = ai->bike.z - player->z;
        float dist = hypotf(dx, dz);

        if (dist < min_dist) {
            min_dist = dist;
            token_idx = i;
        }
    }

    if (token_idx != -1) {
        ai_arr[token_idx].hunt_role  = HUNT_INTERCEPT;
        ai_arr[token_idx].hunt_timer = 1.0f;
    }

    pack_cooldown -= dt;
    if (pack_cooldown > 0.0f) return;

    pack_cooldown = 5.0f + (float)n_alive * 0.8f;

    int alive[MAX_AI], na = 0;
    for (int i = 0; i < n_ai; i++)
        if (ai_arr[i].bike.alive) alive[na++] = i;
    if (na == 0) return;

    unsigned int *rng = &ai_arr[alive[0]].rng;

    if (na >= 1) {
        int pa = (int)(ai_lcg(rng) * na) % na;
        float direct_duration = 7.0f + 4.0f * ai_lcg(rng);
        ai_arr[alive[pa]].hunt_role  = HUNT_DIRECT;
        ai_arr[alive[pa]].hunt_timer = direct_duration;

        if (na >= 2 && ai_lcg(rng) < 0.5f) {
            int pb;
            do { pb = (int)(ai_lcg(rng) * na) % na; } while (pb == pa);
            ai_arr[alive[pb]].hunt_role  = HUNT_DIRECT;
            ai_arr[alive[pb]].hunt_timer = direct_duration * (0.7f + 0.4f * ai_lcg(rng));
        }
    }

    float roam_chance = 0.25f + (float)n_alive * 0.035f;
    if (ai_lcg(rng) < roam_chance) {
        int r = (int)(ai_lcg(rng) * na) % na;
        int idx = alive[r];

        float angle = ai_lcg(&ai_arr[idx].rng) * 6.283185307f;
        float radius = ARENA_HALF * 0.88f;
        ai_arr[idx].roam_tx = cosf(angle) * radius;
        ai_arr[idx].roam_tz = sinf(angle) * radius;

        ai_arr[idx].roam_timer = 5.0f + ai_lcg(&ai_arr[idx].rng) * 7.0f;
    }
}

// ── Public API ────────────────────────────────────────────────────────────────

void ai_reset(AIBike *ai, int idx) {
    memset(&ai->bike, 0, sizeof(Bike));
    ai->bike.x           = AI_START[idx][0];
    ai->bike.z           = AI_START[idx][1];
    ai->bike.dir         = (Direction)(int)AI_START[idx][2];
    ai->bike.alive       = 1;
    ai->bike.wp_x[0]    = ai->bike.x;
    ai->bike.wp_z[0]    = ai->bike.z;
    ai->bike.wp_dist[0] = 0.0f;
    ai->bike.wp_count    = 1;
    ai->bike.total_dist  = 0.0f;

    ai->pending_turn      = 0;
    ai->pending_emergency = 0;
    ai->react_timer       = 0.0f;
    ai->cooldown          = 0.3f + (float)idx * 0.08f;
    ai->hunt_role         = HUNT_NONE;
    ai->hunt_timer        = 0.0f;

    ai->roam_timer        = 0.0f;
    ai->roam_tx           = 0.0f;
    ai->roam_tz           = 0.0f;
}

void ai_init(AIBike *ai, int idx) {
    unsigned int seed = 0xC0DEBABE + (unsigned int)idx * 0x9E3779B9u;
    ai->rng              = seed;
    ai->react_delay      = 0.15f + ai_lcg(&ai->rng) * 0.07f;
    ai->base_aggression  = 0.22f + ai_lcg(&ai->rng) * 0.16f;
    ai_reset(ai, idx);
}

void ai_update(AIBike *ai, float dt,
               Bike **all, int n,
               Bike *player, int n_ai_alive) {
    if (!ai->bike.alive) return;

    if (ai->react_timer > 0.0f) ai->react_timer -= dt;
    if (ai->cooldown    > 0.0f) ai->cooldown    -= dt;
    if (ai->hunt_timer  > 0.0f) {
        ai->hunt_timer -= dt;
        if (ai->hunt_timer <= 0.0f) ai->hunt_role = HUNT_NONE;
    }

    if (ai->roam_timer > 0.0f) {
        ai->roam_timer -= dt;
        if (ai->roam_timer <= 0.0f) {
            ai->hunt_role  = HUNT_INTERCEPT;
            ai->hunt_timer = 4.0f + ai_lcg(&ai->rng) * 5.0f;
            ai->roam_timer = 0.0f;
        }
    }

    if (ai->pending_turn != 0 && ai->react_timer <= 0.0f) {
        if (ai->pending_turn == -1) bike_turn_left (&ai->bike);
        else                        bike_turn_right(&ai->bike);

        int was_other_emergency = ai->pending_emergency;
        ai->pending_turn      = 0;
        ai->pending_emergency = 0;
        ai->cooldown = was_other_emergency ? 0.1f : TURN_COOLDOWN;
    }

    bike_update(&ai->bike, dt, all, n);
    if (!ai->bike.alive) return;

    if (ai->cooldown <= 0.0f && ai->pending_turn == 0)
        ai_decide(ai, dt, all, n, player, n_ai_alive);
}