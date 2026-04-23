#include "ai.h"
#include <math.h>
#include <string.h>

/* ── Tuning ──────────────────────────────────────────────────────────────────*/
#define LOOKAHEAD_STEP      1.0f
#define LOOKAHEAD_N         220      /* increased for better foresight              */
#define DANGER_STEPS        14       /* base; scaled by speed_mult in ai_decide     */
#define TURN_COOLDOWN       0.22f
#define OTHER_TRAIL_DANGER  8        /* base; scaled by speed_mult                  */
#define TEAM_AVOID_PENALTY  35.0f
#define LOOKAHEAD_R         (COLLISION_R + 0.5f)

#define ALIGN_SCALE         48.0f
#define HUNT_DURATION_MIN   12.0f
#define HUNT_DURATION_VAR   6.0f

/* Minimum clear-ahead distance before AI will even consider dashing */
#define AI_DASH_MIN_CLEAR   30

/* Dash tuning */
#define AI_DASH_HUNT_RANGE   45.0f  /* distance within which hunters dash    */
#define AI_DASH_HUNT_RATE    0.7f   /* events/sec at hunt_range/2 or closer  */
#define AI_DASH_RANDOM_RATE  0.04f  /* events/sec background random dash     */

static float aggression_scale(int n_alive) {
    if (n_alive <= 1) return 22.0f;
    if (n_alive == 2) return 18.0f;
    if (n_alive == 3) return 14.0f;
    if (n_alive == 4) return 11.0f;
    if (n_alive == 5) return 10.0f;
    return 9.5f;
}

static const float AI_START[MAX_AI][3] = {
    {  10.0f,  -64.0f, (float)DIR_SOUTH },
    {  30.0f,  -64.0f, (float)DIR_SOUTH },
    {  50.0f,  -64.0f, (float)DIR_SOUTH },
    { -10.0f,  -64.0f, (float)DIR_SOUTH },
    { -30.0f,  -64.0f, (float)DIR_SOUTH },
    { -50.0f,  -64.0f, (float)DIR_SOUTH },
};

/* ── LCG ─────────────────────────────────────────────────────────────────────*/
static float ai_lcg(unsigned int *s) {
    *s = *s * 1664525u + 1013904223u;
    return (float)(*s & 0x7FFFFFFFu) / 2147483647.0f;
}

/* ── Lookahead ───────────────────────────────────────────────────────────────*/
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

static int teammate_clear_steps(float px, float pz, Direction dir,
                                Bike *self, Bike **all, int n) {
    for (int i = 1; i <= LOOKAHEAD_N; i++) {
        float nx = px + DDX[dir] * LOOKAHEAD_STEP * i;
        float nz = pz + DDZ[dir] * LOOKAHEAD_STEP * i;
        if (fabsf(nx) > ARENA_HALF - 2.0f || fabsf(nz) > ARENA_HALF - 2.0f)
            return i;
        for (int b = 0; b < n; b++) {
            if (all[b] == self) continue;
            if (bike_trail_point_hits(all[b], nx, nz, LOOKAHEAD_R, 0))
                return i;
        }
    }
    return LOOKAHEAD_N;
}

static int score_after_turn(AIBike *ai, int turn_dir, Bike **all, int n) {
    Direction new_dir = (Direction)(((int)ai->bike.dir + turn_dir + 4) % 4);
    float nx = ai->bike.x + DDX[new_dir] * LOOKAHEAD_STEP;
    float nz = ai->bike.z + DDZ[new_dir] * LOOKAHEAD_STEP;
    return score_dir(nx, nz, new_dir, &ai->bike, all, n);
}

/* ── Decision ────────────────────────────────────────────────────────────────*/
static void ai_decide(AIBike *ai, float dt,
                      Bike **all, int n,
                      Bike *player, int n_ai_alive) {
    (void)dt;
    Bike      *b   = &ai->bike;
    Direction  cur = b->dir;
    Direction  left  = (Direction)((cur + 3) % 4);
    Direction  right = (Direction)((cur + 1) % 4);

    /* ── Speed-aware danger thresholds ──────────────────────────────────────
       At higher speed the bike covers more ground per frame, so the AI needs
       a proportionally larger clear-space buffer to have time to react.       */
    int eff_danger = (int)(DANGER_STEPS * b->speed_mult + 1.5f);
    int eff_other  = (int)(OTHER_TRAIL_DANGER * b->speed_mult + 1.5f);
    /* Minimum viable space after a turn — must be comfortable, not barely ok */
    int post_min   = (int)(eff_danger * 1.8f);

    int sa = score_dir(b->x, b->z, cur,   b, all, n);
    int sl = score_dir(b->x, b->z, left,  b, all, n);
    int sr = score_dir(b->x, b->z, right, b, all, n);

    int tl = teammate_clear_steps(b->x, b->z, left,  b, all, n);
    int ts = teammate_clear_steps(b->x, b->z, cur,   b, all, n);
    int tr = teammate_clear_steps(b->x, b->z, right, b, all, n);

    int emergency = (sa < eff_danger);

    int other_steps = 999;
    for (int i = 1; i <= eff_other + 6; ++i) {
        float nx = b->x + DDX[cur] * LOOKAHEAD_STEP * i;
        float nz = b->z + DDZ[cur] * LOOKAHEAD_STEP * i;
        int hit_other = 0;
        for (int bb = 0; bb < n; ++bb) {
            if (all[bb] == b) continue;
            if (bike_trail_point_hits(all[bb], nx, nz, LOOKAHEAD_R, 0)) {
                hit_other = 1; break;
            }
        }
        if (hit_other) { other_steps = i; break; }
    }
    int other_emergency = (other_steps <= eff_other);
    if (other_emergency) emergency = 1;

    /* When dashing: reduce straight bias so the AI turns sooner rather than
       barrelling into tight corners at 1.5× speed.                            */
    float straight_bias = emergency ? 0.0f
                        : (b->speed_mult > 1.0f ? 13.0f : 22.0f);
    float scores[3] = { (float)sl, (float)sa, (float)sr };
    scores[1] += straight_bias;

    float tx = 0.0f, tz = 0.0f;
    int has_target = 0;

    if (ai->roam_timer > 0.0f) {
        tx = ai->roam_tx; tz = ai->roam_tz; has_target = 1;
    } else if (player && player->alive) {
        has_target = 1;
        tx = player->x; tz = player->z;

        float pdx = DDX[player->dir];
        float pdz = DDZ[player->dir];
        float ax  = b->x - player->x;
        float az  = b->z - player->z;
        float proj    = ax * pdx + az * pdz;
        float dir_dot = DDX[b->dir] * pdx + DDZ[b->dir] * pdz;

        float lead = 28.0f;
        if (proj > 20.0f && dir_dot > 0.58f)      lead = 55.0f;
        else if (ai->hunt_role == HUNT_INTERCEPT)  lead = 42.0f;
        else if (proj < 0.0f)                      lead = 20.0f;

        if (proj > 12.0f && dir_dot > 0.5f) {
            tx = player->x + pdx * lead;
            tz = player->z + pdz * lead;
        } else if (ai->hunt_role == HUNT_INTERCEPT) {
            tx = player->x + pdx * lead;
            tz = player->z + pdz * lead;
        }
    }

    /* Only apply target-seeking when there's enough room to manoeuvre.
       When space is tight, pure survival scoring takes over.                  */
    int min_score = (sl < sa) ? sl : sa;
    min_score = (sr < min_score) ? sr : min_score;
    int has_room = (min_score > eff_danger * 2);

    if (!emergency && has_target && has_room) {
        float eff_agg = ai->base_aggression * aggression_scale(n_ai_alive);
        if (n_ai_alive <= 2)       eff_agg = 1.9f;
        else if (n_ai_alive <= 3)  eff_agg = fmaxf(eff_agg, 1.4f);
        else                       eff_agg = fminf(eff_agg, 1.25f);

        for (int i = 0; i < 3; ++i) {
            Direction d = (i == 0 ? left : (i == 1 ? cur : right));
            float dx = tx - b->x, dz = tz - b->z;
            float dist = hypotf(dx, dz);
            if (dist > 1.0f) {
                dx /= dist; dz /= dist;
                float align = dx * DDX[d] + dz * DDZ[d];
                /* Space factor: weight aggression by relative space available */
                float sf = fminf((float)scores[i] / (float)(eff_danger * 6.0f), 1.0f);
                scores[i] += align * 34.0f * eff_agg * (sf * sf);
            }
        }
    }

    /* Teammate trail avoidance */
    float t_scores[3] = { (float)tl, (float)ts, (float)tr };
    float team_thresh = 12.0f * b->speed_mult;
    for (int i = 0; i < 3; ++i) {
        if (t_scores[i] < team_thresh) {
            float penalty = TEAM_AVOID_PENALTY * (1.0f - t_scores[i] / team_thresh);
            scores[i] -= penalty;
        }
    }

    int best = 1;
    if (scores[0] > scores[best]) best = 0;
    if (scores[2] > scores[best]) best = 2;
    int turn = (best == 0 ? -1 : (best == 2 ? 1 : 0));

    /* Occasionally keep going straight to avoid jittery turning, but only
       when there's genuinely plenty of room and not already dashing fast.    */
    if (!emergency && turn != 0
        && b->speed_mult <= 1.0f
        && ai_lcg(&ai->rng) < 0.05f) {
        turn = 0;
    }

    if (turn != 0) {
        int post = score_after_turn(ai, (turn == -1) ? 3 : 1, all, n);

        /* Stricter post-turn check: ensure enough space remains to react after
           the turn, especially important at dash speed.                        */
        if (post < post_min && sa >= eff_danger) {
            /* Try the other lateral direction before giving up */
            int alt_turn = -turn;
            int alt_post = score_after_turn(ai,
                           (alt_turn == -1) ? 3 : 1, all, n);
            if (alt_post >= post_min) {
                turn = alt_turn;
                post = alt_post;
            } else {
                /* Both turns are cramped — stay straight and re-evaluate soon */
                ai->cooldown = 0.04f;
                return;
            }
        }

        ai->pending_turn      = turn;
        ai->pending_emergency = other_emergency;

        /* Faster reaction when dashing (the bike is moving faster, so waiting
           the full react_delay at normal speed would overshoot the turn point) */
        float base_delay = ai->react_delay / b->speed_mult;
        float delay = emergency ? (other_emergency ? 0.006f : 0.032f)
                                : base_delay + (ai_lcg(&ai->rng) - 0.5f) * 0.04f;
        ai->react_timer = fmaxf(delay, 0.006f);
    } else {
        ai->cooldown = 0.06f;
    }
}

/* ── Pack coordinator ────────────────────────────────────────────────────────*/
static float pack_cooldown = 0.0f;

void ai_pack_coordinator(AIBike *ai_arr, int n_ai, int n_alive, Bike *player, float dt) {
    if (n_alive < 1 || !player || !player->alive) return;

    float min_dist = 1e9f;
    int token_idx = -1;
    for (int i = 0; i < n_ai; i++) {
        AIBike *ai = &ai_arr[i];
        if (!ai->bike.alive) continue;
        if (ai->hunt_timer > 0.0f || ai->roam_timer > 0.0f) continue;
        float dist = hypotf(ai->bike.x - player->x, ai->bike.z - player->z);
        if (dist < min_dist) { min_dist = dist; token_idx = i; }
    }
    if (token_idx != -1) {
        ai_arr[token_idx].hunt_role  = HUNT_INTERCEPT;
        ai_arr[token_idx].hunt_timer = 1.0f;
    }

    pack_cooldown -= dt;
    if (pack_cooldown > 0.0f) return;
    pack_cooldown = 2.8f + (float)n_alive * 0.35f;

    int alive[MAX_AI], na = 0;
    for (int i = 0; i < n_ai; i++)
        if (ai_arr[i].bike.alive) alive[na++] = i;
    if (na == 0) return;

    unsigned int *rng = &ai_arr[alive[0]].rng;

    if (na >= 1) {
        int pa = (int)(ai_lcg(rng) * na) % na;
        float dur = HUNT_DURATION_MIN + HUNT_DURATION_VAR * ai_lcg(rng);
        ai_arr[alive[pa]].hunt_role  = HUNT_DIRECT;
        ai_arr[alive[pa]].hunt_timer = dur;

        int pb = -1;
        if (na >= 2) {
            do { pb = (int)(ai_lcg(rng) * na) % na; } while (pb == pa);
            ai_arr[alive[pb]].hunt_role  = HUNT_DIRECT;
            ai_arr[alive[pb]].hunt_timer = dur * (0.75f + 0.5f * ai_lcg(rng));
        }

        if (na >= 3 && ai_lcg(rng) < 0.65f) {
            int pc;
            do { pc = (int)(ai_lcg(rng) * na) % na; } while (pc == pa || pc == pb);
            ai_arr[alive[pc]].hunt_role  = HUNT_DIRECT;
            ai_arr[alive[pc]].hunt_timer = dur * 0.8f;
        }
    }

    float roam_chance = 0.06f + (float)n_alive * 0.008f;
    if (ai_lcg(rng) < roam_chance) {
        int r   = (int)(ai_lcg(rng) * na) % na;
        int idx = alive[r];
        float angle  = ai_lcg(&ai_arr[idx].rng) * 6.283185307f;
        float radius = ARENA_HALF * 0.88f;
        ai_arr[idx].roam_tx    = cosf(angle) * radius;
        ai_arr[idx].roam_tz    = sinf(angle) * radius;
        ai_arr[idx].roam_timer = 4.0f + ai_lcg(&ai_arr[idx].rng) * 5.0f;
    }
}

/* ── Public API ──────────────────────────────────────────────────────────────*/
void ai_reset(AIBike *ai, int idx) {
    memset(&ai->bike, 0, sizeof(Bike));
    ai->bike.x            = AI_START[idx][0];
    ai->bike.z            = AI_START[idx][1];
    ai->bike.y            = ARENA_TOP;
    ai->bike.vy           = 0.0f;
    ai->bike.falling      = 0;
    ai->bike.dir          = (Direction)(int)AI_START[idx][2];
    ai->bike.alive        = 1;
    ai->bike.wp_x[0]     = ai->bike.x;
    ai->bike.wp_z[0]     = ai->bike.z;
    ai->bike.wp_dist[0]  = 0.0f;
    ai->bike.wp_count     = 1;
    ai->bike.total_dist   = 0.0f;
    ai->bike.trail_end_dist = 1e30f;

    ai->bike.speed_mult    = 1.0f;
    ai->bike.dash_timer    = 0.0f;
    ai->bike.dash_cooldown = 0.0f;

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
    ai->rng             = seed;
    ai->react_delay     = 0.15f + ai_lcg(&ai->rng) * 0.07f;
    ai->base_aggression = 0.22f + ai_lcg(&ai->rng) * 0.16f;
    ai_reset(ai, idx);
}

void ai_update(AIBike *ai, float dt,
               Bike **all, int n,
               Bike *player, int n_ai_alive) {
    Bike *b = &ai->bike;
    if (!b->alive) {
        bike_update(b, dt, all, n);
        return;
    }

    if (ai->react_timer > 0.0f) ai->react_timer -= dt;
    if (ai->cooldown    > 0.0f) ai->cooldown    -= dt;
    if (ai->hunt_timer  > 0.0f) {
        ai->hunt_timer -= dt;
        if (ai->hunt_timer <= 0.0f) ai->hunt_role = HUNT_NONE;
    }
    if (ai->roam_timer  > 0.0f) {
        ai->roam_timer -= dt;
        if (ai->roam_timer <= 0.0f) {
            ai->hunt_role  = HUNT_INTERCEPT;
            ai->hunt_timer = 6.0f + ai_lcg(&ai->rng) * 7.0f;
            ai->roam_timer = 0.0f;
        }
    }

    if (ai->pending_turn != 0 && ai->react_timer <= 0.0f) {
        if (ai->pending_turn == -1) bike_turn_left (&ai->bike);
        else                        bike_turn_right(&ai->bike);
        int was_other    = ai->pending_emergency;
        ai->pending_turn = ai->pending_emergency = 0;
        ai->cooldown     = was_other ? 0.1f : TURN_COOLDOWN;
    }

    bike_update(&ai->bike, dt, all, n);
    if (!ai->bike.alive) return;

    if (ai->cooldown <= 0.0f && ai->pending_turn == 0)
        ai_decide(ai, dt, all, n, player, n_ai_alive);

    /* ── AI Dash decision ────────────────────────────────────────────────── */
    if (!b->falling && b->dash_cooldown <= 0.0f && b->dash_timer <= 0.0f) {
        /* Hard gate: never dash when the road ahead is too short — at 1.5×
           speed the bike needs more metres to manoeuvre, so require at least
           AI_DASH_MIN_CLEAR clear units before even rolling the dice.         */
        int safe_ahead = score_dir(b->x, b->z, b->dir, b, all, n);
        if (safe_ahead < AI_DASH_MIN_CLEAR) goto skip_dash;

        /* Also check both lateral directions: if both are cramped we're in a
           corridor and dashing would make it impossible to escape.            */
        {
            Direction left_d  = (Direction)((b->dir + 3) % 4);
            Direction right_d = (Direction)((b->dir + 1) % 4);
            int sl_d = score_dir(b->x, b->z, left_d,  b, all, n);
            int sr_d = score_dir(b->x, b->z, right_d, b, all, n);
            int best_lateral = (sl_d > sr_d) ? sl_d : sr_d;
            if (best_lateral < DANGER_STEPS * 2) goto skip_dash;
        }

        {
            float dash_prob = 0.0f;

            /* Hunting: scale probability by closeness to player */
            if (ai->hunt_role != HUNT_NONE && player && player->alive) {
                float dist = hypotf(b->x - player->x, b->z - player->z);
                if (dist < AI_DASH_HUNT_RANGE) {
                    float ratio = 1.0f - (dist / AI_DASH_HUNT_RANGE);
                    dash_prob  += AI_DASH_HUNT_RATE * ratio * dt;
                }
            }

            /* Background random dash */
            dash_prob += AI_DASH_RANDOM_RATE * dt;

            if (ai_lcg(&ai->rng) < dash_prob)
                bike_activate_dash(b);
        }
        skip_dash:;
    }
}
