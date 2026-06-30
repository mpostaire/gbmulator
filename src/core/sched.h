#pragma once

#include <stdint.h>

#define SCHED_MAX_EVENTS 64

typedef void (*sched_cb_t)(void *user_data);

typedef struct {
    uint64_t cycle;     // cycle to run this event
    uint64_t period;    // repeat period for this event
    void    *user_data; // user data to pass to event callback
} sched_event_t;

typedef struct {
    uint64_t cycle; // current scheduler cycle

    uint64_t active_events; // bitfield of scheduled events (bit n set --> event ID n is scheduled)

    uint8_t sorted_event_ids[SCHED_MAX_EVENTS];    // sorted event IDs (event with lowest run cycle first)
    uint8_t event_id_sorted_pos[SCHED_MAX_EVENTS]; // event IDs --> position in sorted_event_ids[]

    uint8_t head;        // index of first event ID of sorted_event_ids to run
    uint8_t event_count; // number of scheduled events

    sched_event_t     events[SCHED_MAX_EVENTS]; // events data
    const sched_cb_t *callbacks;                // events callbacks
} sched_t;

void sched_init(sched_t *sched, const sched_cb_t *callbacks);

void sched_add(sched_t *sched, uint8_t id, void *user_data, uint64_t delay, uint64_t period);

static inline void sched_once(sched_t *sched, uint8_t id, void *user_data, uint64_t delay) {
    sched_add(sched, id, user_data, delay, 0);
}

static inline void sched_repeat(sched_t *sched, uint8_t id, void *user_data, uint64_t delay) {
    sched_add(sched, id, user_data, delay, delay);
}

void sched_cancel(sched_t *sched, uint8_t id);

void sched_run(sched_t *sched, uint64_t cycles);
