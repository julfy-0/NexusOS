#include "app_manager.h"
#include "system.h"
static nexus_app_t g_apps[16]; static int g_count;
static int eq(const char*a,const char*b){int i=0;while(a&&b&&a[i]&&b[i]&&a[i]==b[i])++i;return a&&b&&a[i]==0&&b[i]==0;}
int nexus_app_manager_init(void){g_count=0;return 1;}
int nexus_app_register(const nexus_app_t *app){if(!app||!app->id||g_count>=16)return -1;g_apps[g_count++]=*app;return g_count-1;}
int nexus_app_count(void){return g_count;}
const nexus_app_t *nexus_app_at(int i){return i>=0&&i<g_count?&g_apps[i]:0;}
const nexus_app_t *nexus_app_find(const char *id){for(int i=0;i<g_count;i++)if(eq(g_apps[i].id,id))return &g_apps[i];return 0;}
int nexus_app_launch(const char *id){const nexus_app_t*a=nexus_app_find(id);if(!a)return 0;nexus_system_enter_application();return 1;}
