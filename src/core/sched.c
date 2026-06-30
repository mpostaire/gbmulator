#include <assert.h>

#include "sched.h"
#include "log.h"

static inline bool is_scheduled(const sched_t *sched, uint8_t id) {
    return (sched->active_events & (UINT64_C(1) << id)) != 0;
}

static uint32_t find_insert_pos(const sched_t *sched, uint64_t cycle) {
    // TODO replace with branchless binary sort IF event array is mostly filled all the time? or always?
    uint32_t lo = sched->head;
    uint32_t hi = sched->head + sched->event_count;

    while (lo < hi) {
        uint32_t mid = (lo + hi) >> 1;
        uint8_t  id  = sched->sorted_event_ids[mid];

        if (sched->events[id].cycle <= cycle)
            lo = mid + 1;
        else
            hi = mid;
    }

    return lo;
}

static void trim_inactive(sched_t *sched) {
    // LOG_WARN("trim_inactive");

    if (sched->head == 0)
        return;

    for (uint32_t i = 0; i < sched->event_count; i++) {
        uint8_t id = sched->sorted_event_ids[sched->head + i];

        sched->sorted_event_ids[i]     = id;
        sched->event_id_sorted_pos[id] = i;
    }

    sched->head = 0;
}

static void insert_event_id(sched_t *sched, uint8_t id) {
    if (sched->head + sched->event_count >= SCHED_MAX_EVENTS)
        trim_inactive(sched);

    assert(sched->event_count < SCHED_MAX_EVENTS);

    uint32_t pos = find_insert_pos(sched, sched->events[id].cycle);
    uint32_t end = sched->head + sched->event_count;

    // move right everything after inserted event
    for (uint32_t i = end; i > pos; i--) {
        uint8_t moved = sched->sorted_event_ids[i - 1];

        sched->sorted_event_ids[i]        = moved;
        sched->event_id_sorted_pos[moved] = i;
    }

    // insert event
    sched->sorted_event_ids[pos]   = id;
    sched->event_id_sorted_pos[id] = pos;

    sched->event_count++;
}

// this is only called with a rescheduling or when a non repeating event ends
static void remove_event_id(sched_t *sched, uint8_t id) {
    uint32_t to_remove_pos = sched->event_id_sorted_pos[id];
    uint32_t tail          = sched->head + sched->event_count - 1;

    // move left everything after removed event
    for (uint32_t i = to_remove_pos; i < tail; i++) {
        uint8_t moved = sched->sorted_event_ids[i + 1];

        sched->sorted_event_ids[i]        = moved;
        sched->event_id_sorted_pos[moved] = i;
    }

    sched->event_count--;
}

void sched_add(sched_t *sched, uint8_t id, void *user_data, uint64_t delay, uint64_t period) {
    if (is_scheduled(sched, id))
        remove_event_id(sched, id);
    else
        sched->active_events |= (UINT64_C(1) << id);

    sched->events[id].cycle     = sched->cycle + delay;
    sched->events[id].period    = period;
    sched->events[id].user_data = user_data;

    insert_event_id(sched, id);
}

void sched_init(sched_t *sched, const sched_cb_t *callbacks) {
    *sched = (sched_t){ .callbacks = callbacks };
}

void sched_cancel(sched_t *sched, uint8_t id) {
    if (!is_scheduled(sched, id))
        return;

    remove_event_id(sched, id);

    sched->active_events &= ~(UINT64_C(1) << id);
}

void sched_run(sched_t *sched, uint64_t cycles) {
    uint64_t target = sched->cycle + cycles;

    while (sched->event_count) {
        uint8_t       id    = sched->sorted_event_ids[sched->head];
        sched_event_t event = sched->events[id];

        if (event.cycle > target)
            break;

        sched->cycle = event.cycle;

        sched->head++;
        sched->event_count--;
        sched->active_events &= ~(UINT64_C(1) << id);

        LOG_DEBUG("[SCHED] (0x%" PRIX64 ") event %" PRIu8, sched->cycle, id);
        sched->callbacks[id](event.user_data);

        if (event.period)
            sched_add(sched, id, event.user_data, event.period, event.period);
    }

    sched->cycle = target;
}
