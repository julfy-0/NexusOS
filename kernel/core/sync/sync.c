#include "sync.h"
#include <stddef.h>

static uint64_t irq_save_local(void) {
    uint64_t flags;
    __asm__ volatile ("pushfq; pop %0" : "=r"(flags));
    __asm__ volatile ("cli" ::: "memory");
    return flags;
}

static void irq_restore_local(uint64_t flags) {
    if (flags & (1ULL << 9)) __asm__ volatile ("sti" ::: "memory");
}

void spinlock_init(nexus_spinlock_t *lock) {
    if (lock) lock->value = 0;
}

void spinlock_lock(nexus_spinlock_t *lock) {
    if (!lock) return;
    for (;;) {
        uint32_t expected = 0;
        if (__atomic_compare_exchange_n(&lock->value, &expected, 1,
                                        0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) return;
        __asm__ volatile ("pause" ::: "memory");
    }
}

int spinlock_try_lock(nexus_spinlock_t *lock) {
    if (!lock) return 0;
    uint32_t expected = 0;
    return __atomic_compare_exchange_n(&lock->value, &expected, 1,
                                       0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED) ? 1 : 0;
}

void spinlock_unlock(nexus_spinlock_t *lock) {
    if (!lock) return;
    __atomic_store_n(&lock->value, 0, __ATOMIC_RELEASE);
}

uint64_t spinlock_lock_irqsave(nexus_spinlock_t *lock) {
    uint64_t flags = irq_save_local();
    spinlock_lock(lock);
    return flags;
}

void spinlock_unlock_irqrestore(nexus_spinlock_t *lock, uint64_t flags) {
    spinlock_unlock(lock);
    irq_restore_local(flags);
}

void mutex_init(nexus_mutex_t *mutex) {
    if (!mutex) return;
    mutex->owner = 0;
    mutex->locked = 0;
    scheduler_wait_queue_init(&mutex->waiters);
}

int mutex_try_lock(nexus_mutex_t *mutex) {
    if (!mutex || !scheduler_is_ready()) return 0;
    uint64_t flags = irq_save_local();
    uint64_t tid = scheduler_current_thread_id();
    int acquired = 0;
    if (!mutex->locked) {
        mutex->locked = 1;
        mutex->owner = tid;
        acquired = 1;
    }
    irq_restore_local(flags);
    return acquired;
}

void mutex_lock(nexus_mutex_t *mutex) {
    if (!mutex || !scheduler_is_ready()) return;

    for (;;) {
        if (mutex_try_lock(mutex)) return;

        uint64_t tid = scheduler_current_thread_id();
        if (tid == 0) return; /* Bootstrap context must never block forever. */

        uint64_t flags = irq_save_local();
        if (!mutex->locked) {
            mutex->locked = 1;
            mutex->owner = tid;
            irq_restore_local(flags);
            return;
        }

        if (!scheduler_wait_queue_contains(&mutex->waiters, tid))
            scheduler_wait_queue_push_locked(&mutex->waiters, tid);
        irq_restore_local(flags);

        thread_block_on_wait_queue(&mutex->waiters);

        /* mutex_unlock() transfers ownership before waking us. */
        if (mutex->locked && mutex->owner == tid) return;
    }
}

void mutex_unlock(nexus_mutex_t *mutex) {
    if (!mutex || !scheduler_is_ready()) return;

    uint64_t flags = irq_save_local();
    uint64_t tid = scheduler_current_thread_id();
    if (!mutex->locked || mutex->owner != tid) {
        irq_restore_local(flags);
        return;
    }

    uint64_t next = scheduler_wait_queue_pop_locked(&mutex->waiters);
    if (next != 0) {
        /* Transfer ownership directly to the waiter. This avoids a window
         * where another thread could steal the mutex before the waiter runs. */
        mutex->owner = next;
        mutex->locked = 1;
        scheduler_wake_thread_locked(next);
    } else {
        mutex->owner = 0;
        mutex->locked = 0;
    }
    irq_restore_local(flags);
}

int mutex_is_locked(const nexus_mutex_t *mutex) {
    return mutex && mutex->locked ? 1 : 0;
}

uint64_t mutex_owner(const nexus_mutex_t *mutex) {
    return mutex ? mutex->owner : 0;
}
