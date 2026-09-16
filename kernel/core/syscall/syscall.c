#include "syscall.h"
#include "process.h"
#include "scheduler.h"
#include "vmm.h"
#include "console.h"
#include "vfs.h"
#include "elf_loader.h"
#include "usermode.h"
#include "pmm.h"
#include "nexus_version.h"
#include <stddef.h>
#include <stdint.h>

/* Saved GPR layout produced by syscall_entry.S. */
typedef struct {
    uint64_t r15,r14,r13,r12,r11,r10,r9,r8;
    uint64_t rbp,rdi,rsi,rdx,rcx,rbx,rax;
    uint64_t rip,cs,rflags,user_rsp,ss;
} __attribute__((packed)) syscall_interrupt_frame_t;

typedef struct {
    uint64_t pid;
    uint64_t ppid;
    uint32_t state;
    uint32_t priority;
    uint64_t cpu_ticks;
    uint64_t start_tick;
} nexus_process_info_t;

typedef struct { uint64_t size; uint64_t type; uint64_t flags; } nexus_stat_t;

static int g_ready;
#define SYSCALL_MAX_RW 4096ULL
#define SYSCALL_MAX_PATH 120ULL
#define FD_READABLE 1u
#define FD_WRITABLE 2u
#define O_CREAT 4u
#define O_TRUNC 8u
#define O_APPEND 16u

static int copy_user_string(uint64_t pid, uint64_t va, char *out, uint64_t cap) {
    uint64_t i;
    if (!out || cap < 2 || !process_user_range_valid(pid,va,cap,VMM_PAGE_WRITABLE)) return 0;
    if (!process_user_read(pid,va,out,cap-1)) return 0;
    out[cap-1]=0;
    for(i=0;i<cap-1;i++) if(out[i]==0) return 1;
    return 0;
}

void syscall_init(void) { g_ready = 1; }
int syscall_ready(void) { return g_ready; }

uint64_t syscall_dispatch(uint64_t number, uint64_t a0, uint64_t a1, uint64_t a2) {
    nexus_process_t *p = process_current();
    if (!g_ready || !p || p->state != PROCESS_RUNNING || p->scheduler_thread_id == 0 ||
        !(p->flags & PROCESS_FLAG_USER_CONTEXT)) return (uint64_t)-1;

    switch(number) {
        case NEXUS_SYS_NOP: return 0;
        case NEXUS_SYS_GETPID: return p->pid;
        case NEXUS_SYS_GETPPID: return p->parent_pid;
        case NEXUS_SYS_GETPRIORITY: return p->priority;
        case NEXUS_SYS_SETPRIORITY: return process_set_priority(p->pid,(uint32_t)a0)?0:(uint64_t)-1;
        case NEXUS_SYS_GETNAME: {
            if (!process_user_range_valid(p->pid,a0,a1,VMM_PAGE_WRITABLE) || a1==0 || a1>256) return (uint64_t)-1;
            char n[PROCESS_NAME_LEN]; if(!process_get_name(p->pid,n,sizeof(n))) return (uint64_t)-1;
            uint64_t len=0; while(n[len]&&len+1<a1)len++; if(!process_user_write(p->pid,a0,n,len+1))return (uint64_t)-1; return len;
        }
        case NEXUS_SYS_SETNAME: {
            char n[PROCESS_NAME_LEN]; if(a1==0||a1>=sizeof(n)||!process_user_range_valid(p->pid,a0,a1,VMM_PAGE_WRITABLE)||!process_user_read(p->pid,a0,n,a1))return(uint64_t)-1; n[a1-1]=0; return process_set_name(p->pid,n)?0:(uint64_t)-1;
        }
        case NEXUS_SYS_GETCWD: {
            if (a1 == 0 || a1 > PROCESS_CWD_LEN ||
                !process_user_range_valid(p->pid, a0, a1, VMM_PAGE_WRITABLE)) {
                return (uint64_t)-1;
            }
            char cwd[PROCESS_CWD_LEN];
            if (!process_get_cwd(p->pid, cwd, sizeof(cwd))) return (uint64_t)-1;
            uint64_t l = 0;
            while (cwd[l] && l + 1 < a1) ++l;
            if (!process_user_write(p->pid, a0, cwd, l + 1)) return (uint64_t)-1;
            return l;
        }
        case NEXUS_SYS_CHDIR: { char path[PROCESS_CWD_LEN]; if(!copy_user_string(p->pid,a0,path,sizeof(path))||!vfs_is_dir_path(path))return(uint64_t)-1; return process_set_cwd(p->pid,path)?0:(uint64_t)-1; }
        case NEXUS_SYS_WRITE: {
            if (a2==0||a2>SYSCALL_MAX_RW||!process_fd_is_valid(p->pid,a0)||!process_user_range_valid(p->pid,a1,a2,0)) return (uint64_t)-1;
            char b[SYSCALL_MAX_RW]; if(!process_user_read(p->pid,a1,b,a2))return(uint64_t)-1;
            if(process_fd_is_writable(p->pid,a0)) { for(uint64_t i=0;i<a2;i++) console_putchar(b[i]); return a2; }
            int n=process_fd_write(p->pid,a0,b,a2); return n<0?(uint64_t)-1:(uint64_t)n;
        }
        case NEXUS_SYS_READ: {
            if(a2==0||a2>SYSCALL_MAX_RW||!process_fd_is_valid(p->pid,a0)||!process_user_range_valid(p->pid,a1,a2,VMM_PAGE_WRITABLE))return(uint64_t)-1;
            char b[SYSCALL_MAX_RW]; int n=process_fd_read(p->pid,a0,b,a2); if(n<0)return(uint64_t)-1; if(!process_user_write(p->pid,a1,b,(uint64_t)n))return(uint64_t)-1; return(uint64_t)n;
        }
        case NEXUS_SYS_OPEN: {
            char path[PROCESS_CWD_LEN]; if(!copy_user_string(p->pid,a0,path,sizeof(path)))return(uint64_t)-1; int fd=process_fd_open(p->pid,path,(uint32_t)a1); return fd<0?(uint64_t)-1:(uint64_t)fd;
        }
        case NEXUS_SYS_CLOSE: return process_fd_close(p->pid,a0)?0:(uint64_t)-1;
        case NEXUS_SYS_SEEK: { uint64_t pos=0; if(!process_fd_seek(p->pid,a0,(int64_t)a1,(uint32_t)a2,&pos))return(uint64_t)-1; return pos; }
        case NEXUS_SYS_TELL: { uint64_t pos=0; if(!process_fd_tell(p->pid,a0,&pos))return(uint64_t)-1; return pos; }
        case NEXUS_SYS_STAT: { char path[PROCESS_CWD_LEN]; if(!copy_user_string(p->pid,a0,path,sizeof(path))||!process_user_range_valid(p->pid,a1,sizeof(nexus_stat_t),VMM_PAGE_WRITABLE))return(uint64_t)-1; uint64_t sz=0; if(vfs_file_size_path(path,&sz)==0){nexus_stat_t st={sz,1,0}; return process_user_write(p->pid,a1,&st,sizeof(st))?0:(uint64_t)-1;} if(vfs_is_dir_path(path)){nexus_stat_t st={0,2,0}; return process_user_write(p->pid,a1,&st,sizeof(st))?0:(uint64_t)-1;} return(uint64_t)-1; }
        case NEXUS_SYS_MKDIR: { char path[PROCESS_CWD_LEN]; if(!copy_user_string(p->pid,a0,path,sizeof(path)))return(uint64_t)-1; return vfs_mkdir_path(path)==0?0:(uint64_t)-1; }
        case NEXUS_SYS_RMDIR: { char path[PROCESS_CWD_LEN]; if(!copy_user_string(p->pid,a0,path,sizeof(path)))return(uint64_t)-1; return vfs_unlink_path(path)==0?0:(uint64_t)-1; }
        case NEXUS_SYS_UNLINK: { char path[PROCESS_CWD_LEN]; if(!copy_user_string(p->pid,a0,path,sizeof(path)))return(uint64_t)-1; return vfs_unlink_path(path)==0?0:(uint64_t)-1; }
        case NEXUS_SYS_RENAME: { char src[PROCESS_CWD_LEN],dst[PROCESS_CWD_LEN]; if(!copy_user_string(p->pid,a0,src,sizeof(src))||!copy_user_string(p->pid,a1,dst,sizeof(dst)))return(uint64_t)-1; return vfs_rename_path(src,dst)==0?0:(uint64_t)-1; }
        case NEXUS_SYS_PROCESS_ENUM: {
            if(a1==0||a2<sizeof(nexus_process_info_t)||!process_user_range_valid(p->pid,a0,a1,VMM_PAGE_WRITABLE))return(uint64_t)-1;
            uint64_t cap=a1/sizeof(nexus_process_info_t), n=0; for(uint64_t pid=1;pid<4096&&n<cap;pid++){ nexus_process_t*q=process_get(pid); if(!q)continue; nexus_process_info_t info={q->pid,q->parent_pid,(uint32_t)q->state,q->priority,q->cpu_ticks,q->start_tick}; if(!process_user_write(p->pid,a0+n*sizeof(info),&info,sizeof(info)))return(uint64_t)-1; n++; } return n;
        }
        case NEXUS_SYS_WAITPID: {
            uint64_t wanted=a0; for(uint64_t pid=1;pid<4096;pid++){ nexus_process_t*q=process_get(pid); if(!q||q->parent_pid!=p->pid||(wanted&&wanted!=pid)||q->state!=PROCESS_ZOMBIE)continue; if(a1 && process_user_range_valid(p->pid,a1,sizeof(uint64_t),VMM_PAGE_WRITABLE)){ if(!process_user_write(p->pid,a1,&q->exit_code,sizeof(uint64_t)))return(uint64_t)-1;} return pid; } return 0;
        }
        case NEXUS_SYS_EXEC: {
            char path[PROCESS_CWD_LEN]; if(!copy_user_string(p->pid,a0,path,sizeof(path)))return(uint64_t)-1; uint64_t child=process_create(p->pid); if(!child)return(uint64_t)-1; if(!nexus_elf_load_process(child,path)){process_exit(child);process_reap(child,0);return(uint64_t)-1;} uint64_t tid=thread_create_user_process(child); if(!tid){process_exit(child);process_reap(child,0);return(uint64_t)-1;} nexus_process_t*cp=process_get(child); if(!cp||!usermode_prepare(child,cp->user_entry,cp->user_stack_base,cp->kernel_stack)){process_exit(child);return(uint64_t)-1;} scheduler_request_reschedule(); return child;
        }
        case NEXUS_SYS_EXIT: return (uint64_t)-1;
        case NEXUS_SYS_MMAP: {
            uint64_t pages=(a1+4095ULL)/4096ULL; if(a1==0||pages==0)return(uint64_t)-1; uint64_t flags=VMM_PAGE_WRITABLE|VMM_PAGE_NX; return process_alloc_user_range(p->pid,pages,flags);
        }
        case NEXUS_SYS_MUNMAP: {
            return process_unmap_user_range(p->pid,a0,a1)==1?0:(uint64_t)-1;
        }
        case NEXUS_SYS_BRK: {
            uint64_t requested=a0; if(requested==0)return p->heap_end; if(requested<p->heap_base||requested>=PROCESS_USER_LIMIT)return(uint64_t)-1; uint64_t old=p->heap_end; if(requested>old){uint64_t pages=(requested-old+4095ULL)/4096ULL; uint64_t base=process_alloc_user_range(p->pid,pages,VMM_PAGE_WRITABLE|VMM_PAGE_NX); if(!base||base!=old)return(uint64_t)-1; p->heap_end=old+pages*4096ULL;} else if(requested<old){uint64_t cut=(old-requested+4095ULL)/4096ULL; uint64_t base=old-cut*4096ULL; if(!process_unmap_user_range(p->pid,base,cut*4096ULL))return(uint64_t)-1; p->heap_end=base;} return p->heap_end;
        }
        case NEXUS_SYS_GETCPUTICKS: return process_cpu_ticks(p->pid);
        case NEXUS_SYS_GETSTARTTICK: return process_start_tick(p->pid);
        case NEXUS_SYS_GETUPTIME_MS: return scheduler_elapsed_ms();
        case NEXUS_SYS_PROCESS_COUNT: return process_count();
        case NEXUS_SYS_ZOMBIE_COUNT: return process_zombie_count();
        case NEXUS_SYS_GETENV: {
            if (a1 == 0 || a1 > PROCESS_ENV_VALUE_LEN || !process_user_range_valid(p->pid, a0, PROCESS_ENV_KEY_LEN, VMM_PAGE_WRITABLE) || !process_user_range_valid(p->pid, a2, a1, VMM_PAGE_WRITABLE)) return (uint64_t)-1;
            char key[PROCESS_ENV_KEY_LEN], value[PROCESS_ENV_VALUE_LEN];
            if (!process_user_read(p->pid, a0, key, sizeof(key))) return (uint64_t)-1;
            key[sizeof(key)-1] = 0;
            if (!process_get_env(p->pid, key, value, sizeof(value))) return (uint64_t)-1;
            uint64_t len = 0; while (value[len] && len + 1 < a1) ++len;
            return process_user_write(p->pid, a2, value, len + 1) ? len : (uint64_t)-1;
        }
        case NEXUS_SYS_SETENV: {
            char key[PROCESS_ENV_KEY_LEN], value[PROCESS_ENV_VALUE_LEN];
            if (!copy_user_string(p->pid, a0, key, sizeof(key)) || !copy_user_string(p->pid, a1, value, sizeof(value))) return (uint64_t)-1;
            return process_set_env(p->pid, key, value) ? 0 : (uint64_t)-1;
        }
        case NEXUS_SYS_UNSETENV: {
            char key[PROCESS_ENV_KEY_LEN];
            if (!copy_user_string(p->pid, a0, key, sizeof(key))) return (uint64_t)-1;
            return process_unset_env(p->pid, key) ? 0 : (uint64_t)-1;
        }
        case NEXUS_SYS_FD_COUNT: {
            uint64_t n = 0;
            for (uint64_t i = 0; i < PROCESS_MAX_FDS; ++i) if (p->fds[i].type != PROCESS_FD_UNUSED) ++n;
            return n;
        }
        case NEXUS_SYS_MEMINFO: {
            if (!process_user_range_valid(p->pid, a0, 4ULL * sizeof(uint64_t), VMM_PAGE_WRITABLE)) return (uint64_t)-1;
            uint64_t info[4] = {
                pmm_total_pages() * 4096ULL,
                pmm_free_pages() * 4096ULL,
                pmm_used_pages() * 4096ULL,
                p->memory_limit_bytes
            };
            return process_user_write(p->pid, a0, info, sizeof(info)) ? 0 : (uint64_t)-1;
        }

        case NEXUS_SYS_GETTID:
            return scheduler_current_thread_id();
        case NEXUS_SYS_GETVERSION: {
            if (a1 == 0 || a1 > 64 || !process_user_range_valid(p->pid, a0, a1, VMM_PAGE_WRITABLE)) return (uint64_t)-1;
            const char *v = NEXUS_VERSION_STRING;
            uint64_t len = 0; while (v[len] && len + 1 < a1) ++len;
            return process_user_write(p->pid, a0, v, len + 1) ? len : (uint64_t)-1;
        }
        case NEXUS_SYS_GETARCH: {
            if (a1 == 0 || a1 > 32 || !process_user_range_valid(p->pid, a0, a1, VMM_PAGE_WRITABLE)) return (uint64_t)-1;
            const char *arch = "x86_64";
            uint64_t len = 0; while (arch[len] && len + 1 < a1) ++len;
            return process_user_write(p->pid, a0, arch, len + 1) ? len : (uint64_t)-1;
        }
        case NEXUS_SYS_GETPAGESIZE:
            return 4096ULL;
        case NEXUS_SYS_GETUSERBASE:
            return PROCESS_USER_BASE;
        case NEXUS_SYS_GETUSERLIMIT:
            return PROCESS_USER_LIMIT;
        case NEXUS_SYS_GETSTACKTOP:
            return PROCESS_USER_STACK_TOP;
        case NEXUS_SYS_GETHEAPBASE:
            return p->heap_base;
        case NEXUS_SYS_GETHEAPEND:
            return p->heap_end;
        case NEXUS_SYS_GETFDTYPE:
            if (a0 >= PROCESS_MAX_FDS || p->fds[a0].type == PROCESS_FD_UNUSED) return (uint64_t)-1;
            return (uint64_t)p->fds[a0].type;
        case NEXUS_SYS_GETFDOFFSET:
            if (a0 >= PROCESS_MAX_FDS || p->fds[a0].type == PROCESS_FD_UNUSED) return (uint64_t)-1;
            return p->fds[a0].offset;
        case NEXUS_SYS_GETFD_FLAGS:
            if (a0 >= PROCESS_MAX_FDS || p->fds[a0].type == PROCESS_FD_UNUSED) return (uint64_t)-1;
            return p->fds[a0].flags;
        case NEXUS_SYS_GETQUANTUM_EXPIRATIONS:
            return scheduler_quantum_expirations();
        case NEXUS_SYS_GETCONTEXT_SWITCHES:
            return scheduler_context_switches();
        case NEXUS_SYS_GETTIMER_HZ:
            return scheduler_timer_hz();
        case NEXUS_SYS_GETQUANTUM_TICKS:
            return scheduler_quantum_ticks();
        case NEXUS_SYS_GETTHREAD_COUNT:
            return scheduler_thread_count();
        case NEXUS_SYS_GETMAX_THREADS:
            return scheduler_max_threads();
        case NEXUS_SYS_GETREADY_COUNT:
            return scheduler_ready_count();
        case NEXUS_SYS_GETSLEEPING_COUNT:
            return scheduler_sleeping_count();
        case NEXUS_SYS_GETTHREAD_SWITCHES:
            return scheduler_thread_switches(a0);
        case NEXUS_SYS_GETTHREAD_RUNTIME:
            return scheduler_thread_runtime_ticks(a0);

        default: return (uint64_t)-1;
    }
}

static int syscall_frame_valid(const syscall_interrupt_frame_t *f) {
    if (!f) return 0;
    if ((f->cs&3U)!=3U||(f->ss&3U)!=3U) return 0;
    if ((f->cs&~7ULL)!=(0x23ULL&~7ULL)||(f->ss&~7ULL)!=(0x1BULL&~7ULL))return 0;
    if ((f->rflags&2ULL)==0||(f->rflags&(3ULL<<12))||(f->rflags&(1ULL<<17))||!(f->rflags&(1ULL<<9)))return 0;
    if (f->rip<PROCESS_USER_BASE||f->rip>=PROCESS_USER_LIMIT||f->user_rsp<PROCESS_USER_BASE||f->user_rsp>PROCESS_USER_STACK_TOP)return 0;
    return 1;
}

uint64_t syscall_interrupt_handler(uint64_t *frame) {
    syscall_interrupt_frame_t *f=(syscall_interrupt_frame_t*)frame;
    if(!g_ready||!syscall_frame_valid(f)){f->rax=(uint64_t)-1;return NEXUS_SYSCALL_ACTION_RETURN;}
    nexus_process_t*p=process_current(); if(!p||p->state!=PROCESS_RUNNING||!p->scheduler_thread_id||!(p->flags&PROCESS_FLAG_USER_CONTEXT)||!process_user_range_valid(p->pid,f->rip,1,0)||!process_user_range_valid(p->pid,f->user_rsp,1,VMM_PAGE_WRITABLE)){f->rax=(uint64_t)-1;return NEXUS_SYSCALL_ACTION_RETURN;}
    if(f->rax==NEXUS_SYS_EXIT){ if(!process_exit_with_code(p->pid,f->rdi,1)){f->rax=(uint64_t)-1;return NEXUS_SYSCALL_ACTION_RETURN;} thread_exit(); return NEXUS_SYSCALL_ACTION_EXIT; }
    f->rax=syscall_dispatch(f->rax,f->rdi,f->rsi,f->rdx); return NEXUS_SYSCALL_ACTION_RETURN;
}
