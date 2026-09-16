#include "vfs.h"
#include "console.h"
#include "mount.h"
#include "fat32.h"

extern int strcmp(const char *a, const char *b);

typedef enum {
    VFS_NODE_FREE = 0,
    VFS_NODE_FILE,
    VFS_NODE_DIR
} vfs_node_type_t;

typedef struct {
    vfs_node_type_t type;
    char name[VFS_NAME_LEN];
    int parent;                    /* индекс родителя, -1 только у корня */
    char content[VFS_CONTENT_LEN]; /* используется только для файлов */
    int content_len;
} vfs_node_t;

static vfs_node_t g_nodes[VFS_MAX_NODES];
static int g_cwd; /* индекс текущей директории */
static char g_cwd_path[256];

static int path_len(const char *s) {
    int n = 0; while (s[n] != '\0') n++; return n;
}

static void path_copy(char *dst, const char *src) {
    int i = 0;
    while (src[i] != '\0' && i < 255) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

static int path_is_mount(const char *path, char *mountpoint) {
    if (vfs_mount_count() <= 0) return 0;
    return vfs_mount_resolve(path, mountpoint, 256);
}

static int path_join(char *out, const char *base, const char *name) {
    int n = 0, i = 0;
    if (name[0] == '/') { path_copy(out, name); return 0; }
    while (base[i] && n < 255) out[n++] = base[i++];
    if (n == 0) out[n++] = '/';
    if (n > 1 && out[n-1] != '/') out[n++] = '/';
    i = 0;
    while (name[i] && n < 255) out[n++] = name[i++];
    out[n] = '\0';
    return 0;
}

static int fat_relative(const char *full, const char *mountpoint, char *out) {
    int ml = path_len(mountpoint);
    if (strcmp(full, mountpoint) == 0) { path_copy(out, "/"); return 1; }
    if (ml > 1 && full[ml] == '/' && strcmp(full, mountpoint) != 0) {
        path_copy(out, full + ml); return 1;
    }
    return 0;
}

static int local_strlen(const char *s) {
    int n = 0;
    while (s[n] != '\0') n++;
    return n;
}

static void copy_truncate(char *dst, const char *src, int max_len) {
    int i = 0;
    while (src[i] != '\0' && i < max_len - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static int find_child(int parent, const char *name) {
    for (int i = 0; i < VFS_MAX_NODES; i++) {
        if (g_nodes[i].type != VFS_NODE_FREE &&
            g_nodes[i].parent == parent &&
            strcmp(g_nodes[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int find_free_node(void) {
    /* Индекс 0 зарезервирован под корень, начинаем с 1. */
    for (int i = 1; i < VFS_MAX_NODES; i++) {
        if (g_nodes[i].type == VFS_NODE_FREE) {
            return i;
        }
    }
    return -1;
}

static int has_children(int parent) {
    for (int i = 0; i < VFS_MAX_NODES; i++) {
        if (g_nodes[i].type != VFS_NODE_FREE && g_nodes[i].parent == parent) {
            return 1;
        }
    }
    return 0;
}

void vfs_init(void) {
    for (int i = 0; i < VFS_MAX_NODES; i++) {
        g_nodes[i].type = VFS_NODE_FREE;
    }

    g_nodes[0].type = VFS_NODE_DIR;
    g_nodes[0].parent = -1;
    copy_truncate(g_nodes[0].name, "/", VFS_NAME_LEN);

    g_cwd = 0;
    path_copy(g_cwd_path, "/");

    /* Mountpoint directories are namespace entries, not FAT data. */
    g_nodes[1].type = VFS_NODE_DIR; g_nodes[1].parent = 0; copy_truncate(g_nodes[1].name, "mnt", VFS_NAME_LEN);
    g_nodes[2].type = VFS_NODE_DIR; g_nodes[2].parent = 1; copy_truncate(g_nodes[2].name, "disk0", VFS_NAME_LEN);
}

int vfs_mkdir(const char *name) {
    char candidate[256], mountpoint[256], rel[256];
    path_join(candidate, g_cwd_path, name);
    int mi = path_is_mount(candidate, mountpoint);
    const vfs_mount_t *m = mi >= 0 ? vfs_mount_get(mi) : 0;
    if (m && strcmp(m->fstype, "fat32") == 0 && fat32_is_mounted() &&
        !(m->flags & VFS_MOUNT_RDONLY) && fat_relative(candidate, mountpoint, rel))
        return fat32_mkdir(rel) ? 0 : -1;

    if (g_cwd < 0) return -1;
    if (name[0] == '\0') return -1;
    if (find_child(g_cwd, name) != -1) return -1;

    int idx = find_free_node();
    if (idx < 0) return -1;

    g_nodes[idx].type = VFS_NODE_DIR;
    g_nodes[idx].parent = g_cwd;
    copy_truncate(g_nodes[idx].name, name, VFS_NAME_LEN);
    return 0;
}

int vfs_touch(const char *name) {
    char candidate[256], mountpoint[256], rel[256];
    path_join(candidate, g_cwd_path, name);
    int mi = path_is_mount(candidate, mountpoint);
    const vfs_mount_t *m = mi >= 0 ? vfs_mount_get(mi) : 0;
    if (m && strcmp(m->fstype, "fat32") == 0 && fat32_is_mounted() &&
        !(m->flags & VFS_MOUNT_RDONLY) && fat_relative(candidate, mountpoint, rel))
        return fat32_write_file(rel, 0, 0) ? 0 : -1;

    if (g_cwd < 0) return -1;
    if (name[0] == '\0') return -1;
    if (find_child(g_cwd, name) != -1) return -1;

    int idx = find_free_node();
    if (idx < 0) return -1;

    g_nodes[idx].type = VFS_NODE_FILE;
    g_nodes[idx].parent = g_cwd;
    copy_truncate(g_nodes[idx].name, name, VFS_NAME_LEN);
    g_nodes[idx].content[0] = '\0';
    g_nodes[idx].content_len = 0;
    return 0;
}

int vfs_rm(const char *name) {
    if (g_cwd < 0) return -1;
    if ((g_cwd == 0 && strcmp(name, "mnt") == 0) ||
        (g_cwd == 1 && strcmp(name, "disk0") == 0)) return -1;
    int idx = find_child(g_cwd, name);
    if (idx < 0) return -1;

    if (g_nodes[idx].type == VFS_NODE_DIR && has_children(idx)) {
        return -1; /* директория не пуста */
    }

    g_nodes[idx].type = VFS_NODE_FREE;
    return 0;
}

int vfs_cd(const char *name) {
    char candidate[256];
    char mountpoint[256];
    char rel[256];

    if (strcmp(name, ".") == 0) return 0;

    if (strcmp(name, "..") == 0) {
        if (g_cwd == 0) return 0;
        if (g_cwd_path[0] != '/') return -1;
        int n = path_len(g_cwd_path);
        while (n > 1 && g_cwd_path[n-1] != '/') n--;
        if (n > 1) n--;
        g_cwd_path[n] = '\0';
        if (g_cwd_path[0] == '\0') path_copy(g_cwd_path, "/");
        /* If we leave a mounted tree, map back to its namespace node. */
        int idx = 0;
        if (strcmp(g_cwd_path, "/") != 0) {
            char tmp[256]; path_copy(tmp, g_cwd_path);
            char *part = tmp + 1; int parent = 0;
            while (*part) {
                char *slash = part; while (*slash && *slash != '/') slash++;
                char saved = *slash; *slash = '\0';
                int next = find_child(parent, part);
                *slash = saved;
                if (next < 0) { idx = -1; break; }
                parent = next; idx = next;
                if (!saved) break;
                part = slash + 1;
            }
        }
        if (idx >= 0) g_cwd = idx;
        return 0;
    }

    path_join(candidate, g_cwd_path, name);
    if (path_is_mount(candidate, mountpoint) && fat32_is_mounted()) {
        if (!fat_relative(candidate, mountpoint, rel) || !fat32_is_directory(rel)) return -1;
        path_copy(g_cwd_path, candidate);
        g_cwd = -1; /* mounted VFS backend */
        return 0;
    }

    int idx = -1;
    if (name[0] == '/') {
        char tmp[256]; path_copy(tmp, candidate);
        char *part = tmp + 1; int parent = 0;
        while (*part) {
            char *slash = part; while (*slash && *slash != '/') slash++;
            char saved = *slash; *slash = '\0';
            int next = find_child(parent, part);
            *slash = saved;
            if (next < 0 || g_nodes[next].type != VFS_NODE_DIR) return -1;
            parent = next; idx = next;
            if (!saved) break;
            part = slash + 1;
        }
        if (idx < 0) idx = 0;
    } else {
        idx = find_child(g_cwd < 0 ? 0 : g_cwd, name);
        if (idx < 0 || g_nodes[idx].type != VFS_NODE_DIR) return -1;
    }
    g_cwd = idx;
    path_copy(g_cwd_path, candidate);
    return 0;
}

void vfs_ls(void) {
    char mountpoint[256], rel[256];
    if (g_cwd < 0 && path_is_mount(g_cwd_path, mountpoint) && fat32_is_mounted() && fat_relative(g_cwd_path, mountpoint, rel)) {
        fat32_list(rel);
        return;
    }
    int any = 0;
    for (int i = 0; i < VFS_MAX_NODES; i++) {
        if (g_nodes[i].type != VFS_NODE_FREE && g_nodes[i].parent == g_cwd) {
            console_print(g_nodes[i].name);
            if (g_nodes[i].type == VFS_NODE_DIR) console_print("/");
            console_print("\n"); any = 1;
        }
    }
    if (!any) console_print("(empty)\n");
}

static void print_path_of(int idx) {
    int stack[VFS_MAX_NODES];
    int depth = 0;
    int cur = idx;

    while (cur != 0 && depth < VFS_MAX_NODES) {
        stack[depth++] = cur;
        cur = g_nodes[cur].parent;
    }

    console_print("/");
    for (int i = depth - 1; i >= 0; i--) {
        console_print(g_nodes[stack[i]].name);
        if (i > 0) {
            console_print("/");
        }
    }
}

void vfs_pwd(void) {
    console_print(g_cwd_path);
    console_print("\n");
}

void vfs_cat(const char *name) {
    char candidate[256], mountpoint[256], rel[256];
    if (g_cwd < 0 || name[0] == '/') {
        path_join(candidate, g_cwd_path, name);
        if (path_is_mount(candidate, mountpoint) && fat32_is_mounted() && fat_relative(candidate, mountpoint, rel)) {
            static unsigned char buf[64 * 1024];
            unsigned int size = 0;
            if (fat32_read_file(rel, buf, sizeof(buf) - 1, &size)) {
                buf[size] = 0; console_print((const char *)buf); console_print("\n"); return;
            }
        }
    }
    int idx = find_child(g_cwd, name);
    if (idx < 0 || g_nodes[idx].type != VFS_NODE_FILE) {
        console_print("cat: no such file: "); console_print(name); console_print("\n"); return;
    }
    console_print(g_nodes[idx].content); console_print("\n");
}

int vfs_write(const char *name, const char *content) {
    char candidate[256], mountpoint[256], rel[256];
    path_join(candidate, g_cwd_path, name);
    int mi = path_is_mount(candidate, mountpoint);
    const vfs_mount_t *m = mi >= 0 ? vfs_mount_get(mi) : 0;
    if (m && strcmp(m->fstype, "fat32") == 0 && fat32_is_mounted() &&
        !(m->flags & VFS_MOUNT_RDONLY) && fat_relative(candidate, mountpoint, rel)) {
        if (!content) return -1;
        int len = local_strlen(content);
        return fat32_write_file(rel, content, (unsigned int)len) ? 0 : -1;
    }

    if (g_cwd < 0) return -1;
    int idx = find_child(g_cwd, name);

    if (idx < 0) {
        idx = find_free_node();
        if (idx < 0) return -1;
        g_nodes[idx].type = VFS_NODE_FILE;
        g_nodes[idx].parent = g_cwd;
        copy_truncate(g_nodes[idx].name, name, VFS_NAME_LEN);
    } else if (g_nodes[idx].type != VFS_NODE_FILE) {
        return -1; /* нельзя писать в директорию */
    }

    copy_truncate(g_nodes[idx].content, content, VFS_CONTENT_LEN);
    g_nodes[idx].content_len = local_strlen(g_nodes[idx].content);
    return 0;
}

int vfs_append(const char *name, const char *content) {
    if (g_cwd < 0) return -1;
    int idx = find_child(g_cwd, name);

    if (idx < 0) {
        idx = find_free_node();
        if (idx < 0) return -1;
        g_nodes[idx].type = VFS_NODE_FILE;
        g_nodes[idx].parent = g_cwd;
        copy_truncate(g_nodes[idx].name, name, VFS_NAME_LEN);
        g_nodes[idx].content[0] = '\0';
        g_nodes[idx].content_len = 0;
    } else if (g_nodes[idx].type != VFS_NODE_FILE) {
        return -1;
    }

    int cur_len = g_nodes[idx].content_len;
    int i = 0;
    while (content[i] != '\0' && cur_len + i < VFS_CONTENT_LEN - 1) {
        g_nodes[idx].content[cur_len + i] = content[i];
        i++;
    }
    g_nodes[idx].content[cur_len + i] = '\0';
    g_nodes[idx].content_len = cur_len + i;
    return 0;
}

const char *vfs_get_content(const char *name) {
    int idx = find_child(g_cwd, name);
    if (idx < 0 || g_nodes[idx].type != VFS_NODE_FILE) {
        return 0;
    }
    return g_nodes[idx].content;
}

int vfs_read(const char *name, char *buffer, int capacity) {
    if (!buffer || capacity <= 0) return -1;
    const char *content = vfs_get_content(name);
    if (!content) { buffer[0] = '\0'; return -1; }
    int i = 0;
    while (content[i] && i < capacity - 1) { buffer[i] = content[i]; i++; }
    buffer[i] = '\0';
    return i;
}

char *vfs_split_word(char *s) {
    char *rest = s;
    while (*rest != '\0' && *rest != ' ') {
        rest++;
    }
    if (*rest == ' ') {
        *rest = '\0';
        rest++;
        while (*rest == ' ') {
            rest++;
        }
    }
    return rest;
}

int vfs_rmdir(const char *name) {
    if (g_cwd < 0) return -1;
    if ((g_cwd == 0 && strcmp(name, "mnt") == 0) ||
        (g_cwd == 1 && strcmp(name, "disk0") == 0)) return -1;
    int idx = find_child(g_cwd, name);
    if (idx < 0 || g_nodes[idx].type != VFS_NODE_DIR) {
        return -1;
    }
    if (has_children(idx)) {
        return -1;
    }
    g_nodes[idx].type = VFS_NODE_FREE;
    return 0;
}

int vfs_cp(const char *src, const char *dst) {
    if (g_cwd < 0) return -1;
    int sidx = find_child(g_cwd, src);
    if (sidx < 0 || g_nodes[sidx].type != VFS_NODE_FILE) {
        return -1;
    }
    if (find_child(g_cwd, dst) != -1) {
        return -1;
    }

    int didx = find_free_node();
    if (didx < 0) {
        return -1;
    }

    g_nodes[didx].type = VFS_NODE_FILE;
    g_nodes[didx].parent = g_cwd;
    copy_truncate(g_nodes[didx].name, dst, VFS_NAME_LEN);
    copy_truncate(g_nodes[didx].content, g_nodes[sidx].content, VFS_CONTENT_LEN);
    g_nodes[didx].content_len = g_nodes[sidx].content_len;
    return 0;
}

int vfs_mv(const char *src, const char *dst) {
    if (g_cwd < 0) return -1;
    int idx = find_child(g_cwd, src);
    if (idx < 0) {
        return -1;
    }
    if (find_child(g_cwd, dst) != -1) {
        return -1;
    }

    copy_truncate(g_nodes[idx].name, dst, VFS_NAME_LEN);
    return 0;
}

void vfs_find(const char *name) {
    int found = 0;
    for (int i = 0; i < VFS_MAX_NODES; i++) {
        if (g_nodes[i].type != VFS_NODE_FREE && strcmp(g_nodes[i].name, name) == 0) {
            print_path_of(i);
            console_print("\n");
            found = 1;
        }
    }
    if (!found) {
        console_print("find: nothing found\n");
    }
}

static int is_under(int idx, int root) {
    int cur = g_nodes[idx].parent;
    while (cur != -1) {
        if (cur == root) {
            return 1;
        }
        cur = g_nodes[cur].parent;
    }
    return 0;
}

void vfs_du(unsigned int *out_bytes, unsigned int *out_files) {
    unsigned int bytes = 0;
    unsigned int files = 0;

    for (int i = 0; i < VFS_MAX_NODES; i++) {
        if (g_nodes[i].type == VFS_NODE_FILE && is_under(i, g_cwd)) {
            bytes += (unsigned int)g_nodes[i].content_len;
            files++;
        }
    }

    *out_bytes = bytes;
    *out_files = files;
}

void vfs_df(unsigned int *out_used, unsigned int *out_total) {
    unsigned int used = 0;

    for (int i = 0; i < VFS_MAX_NODES; i++) {
        if (g_nodes[i].type != VFS_NODE_FREE) {
            used++;
        }
    }

    *out_used = used;
    *out_total = VFS_MAX_NODES;
}


int vfs_gui_list(char names[][VFS_NAME_LEN], unsigned char is_dir[], int max_entries) {
    int count = 0;
    if (max_entries <= 0 || g_cwd < 0) return 0;
    for (int i = 0; i < VFS_MAX_NODES && count < max_entries; i++) {
        if (g_nodes[i].type != VFS_NODE_FREE && g_nodes[i].parent == g_cwd) {
            copy_truncate(names[count], g_nodes[i].name, VFS_NAME_LEN);
            is_dir[count] = (g_nodes[i].type == VFS_NODE_DIR) ? 1 : 0;
            count++;
        }
    }
    return count;
}

void vfs_gui_getcwd(char *out, int out_size) {
    if (!out || out_size <= 0) return;
    int i = 0;
    while (g_cwd_path[i] && i < out_size - 1) { out[i] = g_cwd_path[i]; i++; }
    out[i] = '\0';
}

/* 0.6.0 path-oriented VFS backend. The RAM tree remains the primary
 * namespace; mounted FAT32 paths are forwarded to the persistent backend. */
static int normalize_absolute(const char *path, char *out) {
    char combined[256];
    char components[32][VFS_NAME_LEN];
    int depth = 0;
    const char *p;

    if (!path || !out) return 0;
    if (path[0] == '/') {
        copy_truncate(combined, path, sizeof(combined));
    } else {
        path_join(combined, g_cwd_path, path);
    }

    p = combined;
    while (*p) {
        char comp[VFS_NAME_LEN];
        int n = 0;
        while (*p == '/') p++;
        while (*p && *p != '/') {
            if (n < VFS_NAME_LEN - 1) comp[n++] = *p;
            p++;
        }
        comp[n] = '\0';
        if (n == 0 || (n == 1 && comp[0] == '.')) continue;
        if (n == 2 && comp[0] == '.' && comp[1] == '.') {
            if (depth > 0) depth--;
            continue;
        }
        if (depth >= 32) return 0;
        copy_truncate(components[depth++], comp, VFS_NAME_LEN);
    }

    out[0] = '/';
    out[1] = '\0';
    int pos = 1;
    for (int i = 0; i < depth; ++i) {
        if (pos > 1 && pos < 255) out[pos++] = '/';
        for (int j = 0; components[i][j] && pos < 255; ++j) out[pos++] = components[i][j];
    }
    out[pos] = '\0';
    return 1;
}

static int resolve_ram_path(const char *abs, int *out_idx) {
    if (!abs || abs[0] != '/') return 0;
    if (strcmp(abs, "/") == 0) {
        if (out_idx) *out_idx = 0;
        return 1;
    }

    int parent = 0;
    const char *p = abs + 1;
    while (*p) {
        char comp[VFS_NAME_LEN];
        int n = 0;
        while (*p && *p != '/') {
            if (n < VFS_NAME_LEN - 1) comp[n++] = *p;
            p++;
        }
        comp[n] = '\0';
        while (*p == '/') p++;
        int idx = find_child(parent, comp);
        if (idx < 0) return 0;
        parent = idx;
    }
    if (out_idx) *out_idx = parent;
    return 1;
}

int vfs_read_path(const char *path, uint8_t *buffer, uint64_t capacity,
                  uint64_t offset, uint64_t *out_read) {
    char abs[256], mountpoint[256], rel[256];
    if (out_read) *out_read = 0;
    if (!buffer || capacity == 0 || !normalize_absolute(path, abs)) return -1;

    if (path_is_mount(abs, mountpoint) && fat32_is_mounted() && fat_relative(abs, mountpoint, rel)) {
        static uint8_t temp[64 * 1024];
        unsigned int size = 0;
        if (!fat32_read_file(rel, temp, sizeof(temp), &size)) return -1;
        if (offset >= size) return 0;
        uint64_t n = capacity;
        if (n > size - offset) n = size - offset;
        for (uint64_t i = 0; i < n; ++i) buffer[i] = temp[offset + i];
        if (out_read) *out_read = n;
        return 0;
    }

    int idx = -1;
    if (!resolve_ram_path(abs, &idx) || g_nodes[idx].type != VFS_NODE_FILE) return -1;
    uint64_t size = (uint64_t)g_nodes[idx].content_len;
    if (offset >= size) return 0;
    uint64_t n = capacity;
    if (n > size - offset) n = size - offset;
    for (uint64_t i = 0; i < n; ++i) buffer[i] = (uint8_t)g_nodes[idx].content[offset + i];
    if (out_read) *out_read = n;
    return 0;
}

int vfs_file_size_path(const char *path, uint64_t *out_size) {
    char abs[256], mountpoint[256], rel[256];
    if (out_size) *out_size = 0;
    if (!normalize_absolute(path, abs)) return -1;

    if (path_is_mount(abs, mountpoint) && fat32_is_mounted() && fat_relative(abs, mountpoint, rel)) {
        static uint8_t temp[64 * 1024];
        unsigned int size = 0;
        if (!fat32_read_file(rel, temp, sizeof(temp), &size)) return -1;
        if (out_size) *out_size = size;
        return 0;
    }

    int idx = -1;
    if (!resolve_ram_path(abs, &idx) || g_nodes[idx].type != VFS_NODE_FILE) return -1;
    if (out_size) *out_size = (uint64_t)g_nodes[idx].content_len;
    return 0;
}

int vfs_is_dir_path(const char *path) {
    char abs[256];
    if (!normalize_absolute(path, abs)) return 0;
    if (strcmp(abs, "/") == 0) return 1;
    char mountpoint[256], rel[256];
    if (path_is_mount(abs, mountpoint) && fat32_is_mounted() && fat_relative(abs, mountpoint, rel)) {
        return fat32_is_directory(rel);
    }
    int idx = -1;
    return resolve_ram_path(abs, &idx) && g_nodes[idx].type == VFS_NODE_DIR;
}

int vfs_mkdir_path(const char *path) {
    char abs[256];
    if (!normalize_absolute(path, abs) || strcmp(abs, "/") == 0) return -1;

    char parent[256];
    copy_truncate(parent, abs, sizeof(parent));
    char *slash = 0;
    for (char *p = parent + 1; *p; ++p) if (*p == '/') slash = p;
    if (!slash) {
        return vfs_mkdir(abs + 1);
    }
    *slash = '\0';
    const char *name = slash + 1;
    int parent_idx = -1;
    if (!resolve_ram_path(parent, &parent_idx) || g_nodes[parent_idx].type != VFS_NODE_DIR) return -1;
    if (find_child(parent_idx, name) >= 0) return -1;
    int idx = find_free_node();
    if (idx < 0) return -1;
    g_nodes[idx].type = VFS_NODE_DIR;
    g_nodes[idx].parent = parent_idx;
    copy_truncate(g_nodes[idx].name, name, VFS_NAME_LEN);
    return 0;
}

int vfs_unlink_path(const char *path) {
    char abs[256];
    if (!normalize_absolute(path, abs)) return -1;
    int idx = -1;
    if (!resolve_ram_path(abs, &idx) || idx <= 2 || g_nodes[idx].type == VFS_NODE_FREE) return -1;
    if (g_nodes[idx].type == VFS_NODE_DIR && has_children(idx)) return -1;
    g_nodes[idx].type = VFS_NODE_FREE;
    return 0;
}

int vfs_write_path(const char *path, const uint8_t *buffer, uint64_t size,
                   uint64_t offset, uint64_t *out_written) {
    char abs[256], mountpoint[256], rel[256];
    if (out_written) *out_written = 0;
    if (!buffer || !normalize_absolute(path, abs)) return -1;

    if (path_is_mount(abs, mountpoint) && fat32_is_mounted() && fat_relative(abs, mountpoint, rel)) {
        static uint8_t temp[64 * 1024];
        unsigned int old = 0;
        (void)fat32_read_file(rel, temp, sizeof(temp), &old);
        if (offset > sizeof(temp) || size > sizeof(temp) - offset) return -1;
        uint64_t end = offset + size;
        if (end > old) old = (unsigned int)end;
        for (uint64_t i = 0; i < size; ++i) temp[offset + i] = buffer[i];
        if (!fat32_write_file(rel, temp, old)) return -1;
        if (out_written) *out_written = size;
        return 0;
    }

    int idx = -1;
    if (!resolve_ram_path(abs, &idx)) {
        char parent[256];
        copy_truncate(parent, abs, sizeof(parent));
        char *slash = 0;
        for (char *p = parent + 1; *p; ++p) if (*p == '/') slash = p;
        if (!slash) return -1;
        *slash = '\0';
        const char *name = slash + 1;
        int parent_idx = -1;
        if (!resolve_ram_path(parent, &parent_idx) || g_nodes[parent_idx].type != VFS_NODE_DIR) return -1;
        idx = find_child(parent_idx, name);
        if (idx < 0) {
            idx = find_free_node();
            if (idx < 0) return -1;
            g_nodes[idx].type = VFS_NODE_FILE;
            g_nodes[idx].parent = parent_idx;
            copy_truncate(g_nodes[idx].name, name, VFS_NAME_LEN);
            g_nodes[idx].content_len = 0;
            g_nodes[idx].content[0] = '\0';
        }
    }

    if (g_nodes[idx].type != VFS_NODE_FILE || offset >= VFS_CONTENT_LEN) return -1;
    if (size > (uint64_t)VFS_CONTENT_LEN - offset) size = (uint64_t)VFS_CONTENT_LEN - offset;
    for (uint64_t i = 0; i < size; ++i) g_nodes[idx].content[offset + i] = (char)buffer[i];
    uint64_t end = offset + size;
    if (end > (uint64_t)g_nodes[idx].content_len) g_nodes[idx].content_len = (int)end;
    g_nodes[idx].content[g_nodes[idx].content_len] = '\0';
    if (out_written) *out_written = size;
    return 0;
}

int vfs_rename_path(const char *src, const char *dst) {
    char a[256], b[256];
    if (!normalize_absolute(src, a) || !normalize_absolute(dst, b)) return -1;
    int idx = -1;
    if (!resolve_ram_path(a, &idx) || idx <= 2) return -1;

    char parent[256];
    copy_truncate(parent, b, sizeof(parent));
    char *slash = 0;
    for (char *p = parent + 1; *p; ++p) if (*p == '/') slash = p;
    if (!slash) return -1;
    *slash = '\0';
    const char *name = slash + 1;
    int parent_idx = -1;
    if (!resolve_ram_path(parent, &parent_idx) || g_nodes[parent_idx].type != VFS_NODE_DIR) return -1;
    if (find_child(parent_idx, name) >= 0) return -1;
    g_nodes[idx].parent = parent_idx;
    copy_truncate(g_nodes[idx].name, name, VFS_NAME_LEN);
    return 0;
}
