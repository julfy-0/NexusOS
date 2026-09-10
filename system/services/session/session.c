#include "session.h"
static const char *g_user="nexus";
int nexus_session_init(void){return 1;}
const char *nexus_session_user(void){return g_user;}
