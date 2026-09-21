#include "ipc.h"
#include "process.h"
#include "sync.h"
#include "runtime_abi.h"

#define IPC_MAX_CHANNELS 16u

/* Fixed-size queues keep the first 0.7 IPC layer deterministic and avoid
 * introducing a heap dependency into the process communication path. */
typedef struct {
    uint16_t size;
    uint8_t data[NEXUS_CHANNEL_MESSAGE_MAX];
} ipc_message_t;

typedef struct {
    uint8_t active;
    uint8_t closed[2];
    uint16_t reserved;
    uint64_t pid[2];
    uint32_t queue_head[2];
    uint32_t queue_count[2];
    ipc_message_t queue[2][NEXUS_CHANNEL_QUEUE_DEPTH];
} ipc_channel_t;

static ipc_channel_t g_channels[IPC_MAX_CHANNELS];
static nexus_spinlock_t g_lock;
static int g_ready;

static int find_side(const ipc_channel_t *channel, uint64_t pid) {
    if (!channel || !channel->active) return -1;
    if (channel->pid[0] == pid) return 0;
    if (channel->pid[1] == pid) return 1;
    return -1;
}

static int find_channel_for_handle(uint64_t pid, uint32_t handle, ipc_channel_t **out) {
    nexus_process_t *p = process_get(pid);
    if (!p || handle == 0 || handle > NEXUS_PROCESS_CHANNELS) return 0;
    uint32_t channel_id = p->ipc_handles[handle - 1];
    if (channel_id == 0 || channel_id > IPC_MAX_CHANNELS) return 0;
    ipc_channel_t *channel = &g_channels[channel_id - 1];
    int side = find_side(channel, pid);
    if (side < 0 || channel->closed[side]) return 0;
    if (out) *out = channel;
    return 1;
}

static int find_free_handle(nexus_process_t *p) {
    if (!p) return -1;
    for (uint32_t i = 0; i < NEXUS_PROCESS_CHANNELS; ++i) {
        if (p->ipc_handles[i] == 0) return (int)i;
    }
    return -1;
}

static int find_free_channel(void) {
    for (uint32_t i = 0; i < IPC_MAX_CHANNELS; ++i)
        if (!g_channels[i].active) return (int)i;
    return -1;
}

int nexus_ipc_init(void) {
    for (uint32_t i = 0; i < IPC_MAX_CHANNELS; ++i) {
        g_channels[i].active = 0;
        g_channels[i].closed[0] = g_channels[i].closed[1] = 0;
    }
    spinlock_init(&g_lock);
    g_ready = 1;
    return 1;
}

int nexus_ipc_ready(void) { return g_ready; }

int nexus_ipc_create(uint64_t owner_pid, uint64_t peer_pid, uint32_t *out_handle) {
    nexus_process_t *owner;
    nexus_process_t *peer;
    int owner_slot;
    int peer_slot;
    int channel_index;
    uint64_t irq_flags;

    if (!g_ready || !out_handle || owner_pid == 0 || peer_pid == 0 || owner_pid == peer_pid)
        return 0;
    owner = process_get(owner_pid);
    peer = process_get(peer_pid);
    if (!owner || !peer || owner->state == PROCESS_ZOMBIE || peer->state == PROCESS_ZOMBIE)
        return 0;

    owner_slot = find_free_handle(owner);
    peer_slot = find_free_handle(peer);
    if (owner_slot < 0 || peer_slot < 0) return 0;

    irq_flags = spinlock_lock_irqsave(&g_lock);
    channel_index = find_free_channel();
    if (channel_index < 0) {
        spinlock_unlock_irqrestore(&g_lock, irq_flags);
        return 0;
    }

    ipc_channel_t *channel = &g_channels[channel_index];
    channel->active = 1;
    channel->closed[0] = channel->closed[1] = 0;
    channel->pid[0] = owner_pid;
    channel->pid[1] = peer_pid;
    channel->queue_head[0] = channel->queue_head[1] = 0;
    channel->queue_count[0] = channel->queue_count[1] = 0;
    owner->ipc_handles[owner_slot] = (uint32_t)channel_index + 1;
    peer->ipc_handles[peer_slot] = (uint32_t)channel_index + 1;
    *out_handle = (uint32_t)owner_slot + 1;

    spinlock_unlock_irqrestore(&g_lock, irq_flags);
    return 1;
}

int nexus_ipc_close(uint64_t pid, uint32_t handle) {
    nexus_process_t *p = process_get(pid);
    if (!p || handle == 0 || handle > NEXUS_PROCESS_CHANNELS) return 0;

    uint64_t irq_flags = spinlock_lock_irqsave(&g_lock);
    uint32_t channel_id = p->ipc_handles[handle - 1];
    if (channel_id == 0 || channel_id > IPC_MAX_CHANNELS) {
        spinlock_unlock_irqrestore(&g_lock, irq_flags);
        return 0;
    }
    ipc_channel_t *channel = &g_channels[channel_id - 1];
    int side = find_side(channel, pid);
    if (side < 0) {
        spinlock_unlock_irqrestore(&g_lock, irq_flags);
        return 0;
    }
    channel->closed[side] = 1;
    p->ipc_handles[handle - 1] = 0;
    if (channel->closed[0] && channel->closed[1]) channel->active = 0;
    spinlock_unlock_irqrestore(&g_lock, irq_flags);
    return 1;
}

int nexus_ipc_send(uint64_t pid, uint32_t handle, const void *data, uint32_t size) {
    if (!data || size == 0 || size > NEXUS_CHANNEL_MESSAGE_MAX) return 0;
    nexus_process_t *p = process_get(pid);
    if (!p || p->state == PROCESS_ZOMBIE) return 0;

    uint64_t irq_flags = spinlock_lock_irqsave(&g_lock);
    ipc_channel_t *channel = 0;
    if (!find_channel_for_handle(pid, handle, &channel)) {
        spinlock_unlock_irqrestore(&g_lock, irq_flags);
        return 0;
    }
    int side = find_side(channel, pid);
    int target = side ^ 1;
    if (channel->closed[target] || channel->queue_count[target] >= NEXUS_CHANNEL_QUEUE_DEPTH) {
        spinlock_unlock_irqrestore(&g_lock, irq_flags);
        return 0;
    }
    uint32_t head = (channel->queue_head[target] + channel->queue_count[target]) % NEXUS_CHANNEL_QUEUE_DEPTH;
    ipc_message_t *message = &channel->queue[target][head];
    message->size = (uint16_t)size;
    const uint8_t *src = (const uint8_t *)data;
    for (uint32_t i = 0; i < size; ++i) message->data[i] = src[i];
    channel->queue_count[target]++;
    spinlock_unlock_irqrestore(&g_lock, irq_flags);
    return (int)size;
}

int nexus_ipc_recv(uint64_t pid, uint32_t handle, void *data, uint32_t capacity, uint32_t *out_size) {
    if (!data || capacity == 0 || !out_size) return 0;
    nexus_process_t *p = process_get(pid);
    if (!p || p->state == PROCESS_ZOMBIE) return 0;

    uint64_t irq_flags = spinlock_lock_irqsave(&g_lock);
    ipc_channel_t *channel = 0;
    if (!find_channel_for_handle(pid, handle, &channel)) {
        spinlock_unlock_irqrestore(&g_lock, irq_flags);
        return 0;
    }
    int side = find_side(channel, pid);
    if (channel->queue_count[side] == 0) {
        spinlock_unlock_irqrestore(&g_lock, irq_flags);
        return 0;
    }
    ipc_message_t *message = &channel->queue[side][channel->queue_head[side]];
    uint32_t copy_size = message->size < capacity ? message->size : capacity;
    uint8_t *dst = (uint8_t *)data;
    for (uint32_t i = 0; i < copy_size; ++i) dst[i] = message->data[i];
    *out_size = copy_size;
    channel->queue_head[side] = (channel->queue_head[side] + 1) % NEXUS_CHANNEL_QUEUE_DEPTH;
    channel->queue_count[side]--;
    spinlock_unlock_irqrestore(&g_lock, irq_flags);
    return 1;
}

uint32_t nexus_ipc_poll(uint64_t pid, uint32_t handle) {
    uint64_t irq_flags = spinlock_lock_irqsave(&g_lock);
    ipc_channel_t *channel = 0;
    if (!find_channel_for_handle(pid, handle, &channel)) {
        spinlock_unlock_irqrestore(&g_lock, irq_flags);
        return 0;
    }
    int side = find_side(channel, pid);
    uint32_t count = channel->queue_count[side];
    spinlock_unlock_irqrestore(&g_lock, irq_flags);
    return count;
}

void nexus_ipc_process_terminate(uint64_t pid) {
    nexus_process_t *p = process_get(pid);
    if (!p) return;

    uint64_t irq_flags = spinlock_lock_irqsave(&g_lock);
    for (uint32_t h = 0; h < NEXUS_PROCESS_CHANNELS; ++h) {
        uint32_t channel_id = p->ipc_handles[h];
        if (channel_id == 0 || channel_id > IPC_MAX_CHANNELS) continue;
        ipc_channel_t *channel = &g_channels[channel_id - 1];
        int side = find_side(channel, pid);
        if (side < 0) continue;
        channel->closed[side] = 1;
        p->ipc_handles[h] = 0;
        if (channel->closed[0] && channel->closed[1]) {
            channel->active = 0;
            continue;
        }
        uint64_t peer_pid = channel->pid[side ^ 1];
        nexus_process_t *peer = process_get(peer_pid);
        if (peer) {
            for (uint32_t ph = 0; ph < NEXUS_PROCESS_CHANNELS; ++ph) {
                if (peer->ipc_handles[ph] == channel_id) {
                    peer->ipc_handles[ph] = 0;
                    break;
                }
            }
        }
    }
    spinlock_unlock_irqrestore(&g_lock, irq_flags);
}

uint32_t nexus_ipc_open_count(uint64_t pid) {
    nexus_process_t *p = process_get(pid);
    if (!p) return 0;
    uint32_t count = 0;
    for (uint32_t i = 0; i < NEXUS_PROCESS_CHANNELS; ++i)
        if (p->ipc_handles[i] != 0) ++count;
    return count;
}
