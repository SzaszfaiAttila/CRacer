#ifndef AI_H
#define AI_H

#include "bike.h"
#include "config.h"

// Hunt role — assigned by the pack coordinator
typedef enum {
    HUNT_NONE      = 0,  // normal survival + mild aggression
    HUNT_DIRECT    = 1,  // bias toward player's current position
    HUNT_INTERCEPT = 2,  // bias toward player's projected position (ahead in their dir)
} HuntRole;

typedef struct {
    Bike         bike;
    float        react_timer;    // countdown to executing pending_turn
    int          pending_turn;   // -1 left | 0 none | +1 right
    int          pending_emergency; // 1 = this pending turn uses fast 0.1f cooldown (other-trail avoid)
    float        cooldown;       // time before next decision is allowed
    float        react_delay;    // base human-like reaction time ~150–220ms
    float        base_aggression;// fixed personality trait (0..1)
    HuntRole     hunt_role;      // current coordination role
    float        hunt_timer;     // remaining time for this role
    float        roam_timer;     // >0 = currently roaming toward edge
    float        roam_tx;        // target position while roaming
    float        roam_tz;
    unsigned int rng;
} AIBike;

// idx = 0..MAX_AI-1, sets personality + starting position
void ai_init  (AIBike *ai, int idx);
void ai_reset (AIBike *ai, int idx);

// Call once per frame.
//   all[]      = every Bike in the game (player + AIs)
//   n          = total count of all[]
//   player     = player's Bike* (for targeting)
//   n_ai_alive = how many AI opponents are currently alive (drives aggression scaling)
void ai_update(AIBike *ai, float dt,
               Bike **all, int n,
               Bike *player, int n_ai_alive);

// Called once per frame from main to assign pack-hunt roles across all AIs.
// ai_arr = array of AIBike[n_ai], n_alive = alive count
void ai_pack_coordinator(AIBike *ai_arr, int n_ai, int n_alive, Bike *player, float dt);

#endif