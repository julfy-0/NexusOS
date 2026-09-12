#ifndef NEXUSOS_SYNC_H
#define NEXUSOS_SYNC_H

#include <stdint.h>
#include "scheduler.h"

/* Kernel synchronization primitives introduced in NexusOS 0.5.3.5. */

typedef struct {
    volatile uint32_t value;
} nexus_spinlock_t;

typedef struct {
    volatile uint64_t owner;
    uint32_t locked;
    nexus_wait_queue_t waiters;
} nexus_mutex_t;

void spinlock_init(nexus_spinlock_t *lock);
void spinlock_lock(nexus_spinlock_t *lock);
int spinlock_try_lock(nexus_spinlock_t *lock);
void spinlock_unlock(nexus_spinlock_t *lock);
uint64_t spinlock_lock_irqsave(nexus_spinlock_t *lock);
void spinlock_unlock_irqrestore(nexus_spinlock_t *lock, uint64_t flags);

void mutex_init(nexus_mutex_t *mutex);
void mutex_lock(nexus_mutex_t *mutex);
int mutex_try_lock(nexus_mutex_t *mutex);
void mutex_unlock(nexus_mutex_t *mutex);
int mutex_is_locked(const nexus_mutex_t *mutex);
uint64_t mutex_owner(const nexus_mutex_t *mutex);

#endif
