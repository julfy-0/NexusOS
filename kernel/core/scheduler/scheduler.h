#ifndef NEXUSOS_SCHEDULER_H
#define NEXUSOS_SCHEDULER_H

#include <stdint.h>

/* NexusOS 0.5.3.6 — Process-safe Event Integration. */
#define NEXUS_SCHEDULER_DEFAULT_QUANTUM_MS 100u
#define NEXUS_SCHEDULER_MAX_THREADS 8u

typedef struct {
    uint64_t ids[NEXUS_SCHEDULER_MAX_THREADS];
    uint32_t head;
    uint32_t count;
} nexus_wait_queue_t;

void scheduler_init(uint32_t timer_hz);
void scheduler_tick_irq(void);
void scheduler_process(void);
void scheduler_request_reschedule(void);

uint64_t thread_create(void (*entry)(void *), void *arg);
uint64_t thread_create_user_process(uint64_t pid);
void thread_yield(void);
void thread_sleep_ms(uint64_t milliseconds);
int thread_wakeup(uint64_t id);
void thread_exit(void);

void scheduler_wait_queue_init(nexus_wait_queue_t *queue);
int scheduler_wait_queue_contains(const nexus_wait_queue_t *queue, uint64_t id);
void scheduler_wait_queue_push_locked(nexus_wait_queue_t *queue, uint64_t id);
uint64_t scheduler_wait_queue_pop_locked(nexus_wait_queue_t *queue);
void scheduler_wake_thread_locked(uint64_t id);
int thread_block_on_wait_queue(nexus_wait_queue_t *queue);
int scheduler_wake_thread(uint64_t id);

int scheduler_is_ready(void);
int scheduler_reschedule_pending(void);
uint64_t scheduler_ticks(void);
uint64_t scheduler_elapsed_ms(void);
uint64_t scheduler_quantum_expirations(void);
uint64_t scheduler_context_switches(void);
uint32_t scheduler_timer_hz(void);
uint32_t scheduler_quantum_ticks(void);
uint32_t scheduler_thread_count(void);
uint32_t scheduler_max_threads(void);
uint32_t scheduler_ready_count(void);
uint32_t scheduler_sleeping_count(void);
uint64_t scheduler_current_thread_id(void);
uint64_t scheduler_thread_switches(uint64_t id);
uint64_t scheduler_thread_runtime_ticks(uint64_t id);

#endif
