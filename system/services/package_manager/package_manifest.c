#include "package_manifest.h"

static void copy_value(char *dst, int max, const char *v) { int i=0; while(v && v[i] && v[i]!='\r' && v[i]!='\n' && i<max-1) { dst[i]=v[i]; ++i; } dst[i]=0; }
static int line(const char *text, const char *key, char *out, int max) {
    int k=0; while(key[k]) ++k;
    const char *p=text;
    while(p && *p) {
        int i=0; while(p[i] && p[i]!='\n') ++i;
        int match=1; for(int j=0;j<k;j++) if(p[j]!=key[j]) {match=0;break;}
        if(match && p[k]==':') { int off=k+1; while(off<i && (p[off]==' '||p[off]=='\t')) ++off; copy_value(out,max,p+off); return 1; }
        p += i; if(*p=='\n') ++p;
    }
    return 0;
}
int nexus_package_manifest_parse(const char *text, nexus_package_manifest_t *out) {
    if(!text||!out) return 0;
    out->name[0]=out->id[0]=out->version[0]=out->author[0]=out->type[0]=out->entry[0]=out->icon[0]=0;
    line(text,"Name",out->name,sizeof(out->name)); line(text,"ID",out->id,sizeof(out->id));
    line(text,"Version",out->version,sizeof(out->version)); line(text,"Author",out->author,sizeof(out->author));
    line(text,"Type",out->type,sizeof(out->type)); line(text,"Entry",out->entry,sizeof(out->entry));
    line(text,"Icon",out->icon,sizeof(out->icon));
    return out->name[0] && out->id[0];
}
