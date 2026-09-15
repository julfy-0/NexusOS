#include "package_manager.h"
#include "package_manifest.h"
#include "package_loader.h"
#include "app_manager.h"
#include "fat32.h"

#define NEXUS_DISCOVERY_MAX 16
#define NEXUS_INSTALL_BUFFER 65536u
#define NEXUS_INSTALL_MAX_FILES 8

static int g_initialized;
static int g_count;
static nexus_package_manifest_t g_manifests[NEXUS_DISCOVERY_MAX];

static int text_eq(const char *a, const char *b) {
    int i = 0;
    if (!a || !b) return 0;
    while (a[i] && b[i] && a[i] == b[i]) ++i;
    return a[i] == 0 && b[i] == 0;
}

static void path_join(char *out, int cap, const char *root, const char *name, const char *leaf) {
    int n = 0;
    if (root) while (root[n] && n < cap - 1) { out[n] = root[n]; ++n; }
    if (n && out[n - 1] != '/' && n < cap - 1) out[n++] = '/';
    if (name) while (*name && n < cap - 1) out[n++] = *name++;
    if (leaf && n < cap - 1) {
        if (n && out[n - 1] != '/') out[n++] = '/';
        while (*leaf && n < cap - 1) out[n++] = *leaf++;
    }
    out[n] = 0;
}

static int discover_root(const char *root, nexus_app_source_t source) {
    char names[NEXUS_DISCOVERY_MAX][13];
    unsigned char dirs[NEXUS_DISCOVERY_MAX];
    int n = fat32_list_entries(root, names, dirs, NEXUS_DISCOVERY_MAX);
    if (n < 0) return 0;

    int added = 0;
    for (int i = 0; i < n && g_count < NEXUS_DISCOVERY_MAX; ++i) {
        if (!dirs[i]) continue;
        if (text_eq(names[i], ".") || text_eq(names[i], "..")) continue;

        char manifest_path[192];
        char text[512];
        unsigned int size = 0;
        path_join(manifest_path, sizeof(manifest_path), root, names[i], "manifest.nxm");
        if (!nexus_package_load_manifest(manifest_path, text, sizeof(text) - 1, &size)) continue;
        text[size < sizeof(text) ? size : sizeof(text) - 1] = 0;

        nexus_package_manifest_t *m = &g_manifests[g_count];
        if (!nexus_package_manifest_parse(text, m)) continue;
        if (nexus_app_find(m->id)) continue;

        nexus_app_t app;
        app.id = m->id;
        app.name = m->name;
        app.entry = m->entry[0] ? m->entry : manifest_path;
        app.builtin = 0;
        app.source = source;
        if (nexus_app_register(&app) >= 0) {
            ++g_count;
            ++added;
        }
    }
    return added;
}

int nexus_package_manager_init(void) {
    g_initialized = 1;
    g_count = 0;
    return 1;
}

int nexus_package_is_package_path(const char *path) {
    int n=0; while(path && path[n]) ++n;
    return n >= 3 && path[n-3]=='.' && path[n-2]=='n' && path[n-1]=='x';
}

int nexus_package_discover(const char *root) {
    if (!g_initialized) nexus_package_manager_init();
    if (!root || !fat32_is_mounted()) return 0;
    if (text_eq(root, "/system/apps")) return discover_root(root, NEXUS_APP_SOURCE_SYSTEM);
    if (text_eq(root, "/userdata/apps")) return discover_root(root, NEXUS_APP_SOURCE_USERDATA);
    return 0;
}

static void package_path_join(char *out, unsigned int cap, const char *root, const char *leaf) {
    unsigned int n = 0;
    if (!out || cap == 0) return;
    while (root && root[n] && n + 1 < cap) { out[n] = root[n]; ++n; }
    if (n && out[n - 1] != '/' && n + 1 < cap) out[n++] = '/';
    while (leaf && *leaf && n + 1 < cap) out[n++] = *leaf++;
    out[n] = 0;
}

static int is_space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

static int valid_short_name(const char *s) {
    int len = 0, dot = -1, base = 0, ext = 0;
    if (!s || !s[0]) return 0;
    while (s[len]) {
        char c = s[len];
        if (c == '/') return 0;
        if (c == '.') {
            if (dot >= 0) return 0;
            dot = len;
        }
        ++len;
        if (len > 12) return 0;
    }
    if (dot < 0) base = len;
    else { base = dot; ext = len - dot - 1; }
    if (base < 1 || base > 8 || ext < 0 || ext > 3) return 0;
    if (s[0] == '.') return 0;
    for (int i = 0; i < len; ++i) {
        if (i == dot) continue;
        char c = s[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '$' ||
            c == '~' || c == '!' || c == '#' || c == '%' || c == '&' ||
            c == '\'' || c == '(' || c == ')' || c == '@' || c == '^' || c == '`') continue;
        return 0;
    }
    return 1;
}

static int parse_file_list(const char *text, char out[][13], int max_files) {
    int count = 0, i = 0;
    if (!text || !text[0]) return 0;
    while (text[i]) {
        while (text[i] && (is_space(text[i]) || text[i] == ',' || text[i] == ';')) ++i;
        if (!text[i]) break;
        int n = 0;
        while (text[i] && text[i] != ',' && text[i] != ';' && !is_space(text[i])) {
            if (n < 12) out[count][n++] = text[i];
            ++i;
        }
        if (n == 0 || n > 12 || count >= max_files) return -1;
        out[count][n] = 0;
        if (!valid_short_name(out[count])) return -1;
        ++count;
    }
    return count;
}

static int has_name(char names[][13], int count, const char *name) {
    for (int i = 0; i < count; ++i) if (text_eq(names[i], name)) return 1;
    return 0;
}

static int copy_small_file(const char *source, const char *destination) {
    static unsigned char buffer[NEXUS_INSTALL_BUFFER];
    unsigned int size = 0;
    if (!fat32_read_file(source, buffer, sizeof(buffer), &size)) return 0;
    if (size == sizeof(buffer)) return 0;
    return fat32_write_file(destination, buffer, size);
}

static int validate_entry_name(const char *entry) {
    return valid_short_name(entry);
}

int nexus_package_install(const char *source_dir, const char *destination_dir) {
    static char manifest_text[4096];
    nexus_package_manifest_t manifest;
    char files[NEXUS_INSTALL_MAX_FILES][13];
    char written[NEXUS_INSTALL_MAX_FILES][13];
    unsigned int manifest_size = 0;
    char source_manifest[256];
    char destination_manifest[256];
    char source_path[256];
    char destination_path[256];
    int file_count = 0, written_count = 0, created_dir = 0;

    if (!g_initialized) nexus_package_manager_init();
    if (!fat32_is_mounted() || !source_dir || !destination_dir) return 0;
    if (source_dir[0] != '/' || destination_dir[0] != '/') return 0;
    if (fat32_is_directory(destination_dir)) return 0;

    package_path_join(source_manifest, sizeof(source_manifest), source_dir, "manifest.nxm");
    package_path_join(destination_manifest, sizeof(destination_manifest), destination_dir, "manifest.nxm");
    if (!nexus_package_load_manifest(source_manifest, manifest_text, sizeof(manifest_text) - 1, &manifest_size)) return 0;
    if (manifest_size >= sizeof(manifest_text)) return 0;
    manifest_text[manifest_size] = 0;
    if (!nexus_package_manifest_parse(manifest_text, &manifest)) return 0;
    if (nexus_app_find(manifest.id)) return 0;

    if (manifest.files[0]) {
        file_count = parse_file_list(manifest.files, files, NEXUS_INSTALL_MAX_FILES);
        if (file_count <= 0) return 0;
    } else if (manifest.entry[0]) {
        if (!validate_entry_name(manifest.entry)) return 0;
        files[0][0] = 0;
        for (int i = 0; manifest.entry[i] && i < 12; ++i) files[0][i] = manifest.entry[i];
        files[0][12] = 0;
        file_count = 1;
    }

    if (manifest.entry[0] && !validate_entry_name(manifest.entry)) return 0;
    if (manifest.entry[0] && !has_name(files, file_count, manifest.entry)) return 0;
    if (has_name(files, file_count, "manifest.nxm")) return 0;

    /* Validate every source payload before changing the destination. */
    for (int i = 0; i < file_count; ++i) {
        package_path_join(source_path, sizeof(source_path), source_dir, files[i]);
        if (fat32_is_directory(source_path)) return 0;
        static unsigned char validation_buffer[NEXUS_INSTALL_BUFFER];
        unsigned int payload_size = 0;
        if (!fat32_read_file(source_path, validation_buffer, sizeof(validation_buffer), &payload_size)) return 0;
        if (payload_size == sizeof(validation_buffer)) return 0;
    }

    if (!fat32_mkdir(destination_dir)) return 0;
    created_dir = 1;

    for (int i = 0; i < file_count; ++i) {
        package_path_join(source_path, sizeof(source_path), source_dir, files[i]);
        package_path_join(destination_path, sizeof(destination_path), destination_dir, files[i]);
        if (!copy_small_file(source_path, destination_path)) goto rollback;
        for (int j = 0; j < 13; ++j) written[written_count][j] = files[i][j];
        ++written_count;
    }

    /* Publication/commit point: discovery cannot see the app until the manifest exists. */
    if (!fat32_write_file(destination_manifest, manifest_text, manifest_size)) goto rollback;
    (void)nexus_package_discover("/userdata/apps");
    return 1;

rollback:
    for (int i = written_count - 1; i >= 0; --i) {
        package_path_join(destination_path, sizeof(destination_path), destination_dir, written[i]);
        (void)fat32_remove_file(destination_path);
    }
    if (created_dir) (void)fat32_remove_empty_dir(destination_dir);
    return 0;
}

int nexus_package_count(void) { return g_count; }
