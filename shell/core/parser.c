#include "parser.h"

static int space(char c) { return c==' ' || c=='\t'; }

int shell_parse_line(char *line, shell_parsed_command_t *out, int max) {
    int n=0; char *p=line;
    while (*p) {
        while (space(*p) || *p==';') p++;
        if (!*p) break;
        if (n>=max) return -1;
        shell_parsed_command_t *c=&out[n];
        c->argc=0; c->next=SHELL_OP_END; c->redirect_in=0; c->redirect_out=0; c->append=0;
        while (*p) {
            while (space(*p)) p++;
            if (!*p) break;
            if (*p==';') { p++; c->next=SHELL_OP_SEQ; break; }
            if (*p=='&' && p[1]=='&') { p+=2; c->next=SHELL_OP_AND; break; }
            if (*p=='|') { p++; c->next=SHELL_OP_PIPE; break; }
            int redir=0, append=0;
            if (*p=='<' || *p=='>') { redir=*p; p++; if (redir=='>' && *p=='>') { append=1; p++; } while(space(*p))p++; }
            char *start=p, *w=p; char quote=0;
            while (*p) {
                if (quote) { if (*p==quote) { quote=0; p++; continue; } if (*p=='\\' && p[1]) { p++; *w++=*p++; continue; } *w++=*p++; continue; }
                if (*p=='\'' || *p=='\"') { quote=*p++; continue; }
                if (*p=='\\' && p[1]) { p++; *w++=*p++; continue; }
                if (space(*p)||*p==';'||*p=='|'||(*p=='&'&&p[1]=='&')) break;
                *w++=*p++;
            }
            if (quote) return -2;
            *w='\0';
            if (redir) { if (!*start) return -3; if (redir=='<') c->redirect_in=start; else { c->redirect_out=start; c->append=append; } }
            else { if (c->argc>=SHELL_MAX_TOKENS-1) return -1; c->argv[c->argc++]=start; }
        }
        c->argv[c->argc]=0;
        if (c->argc==0 && !c->redirect_in && !c->redirect_out) return -3;
        n++;
    }
    return n;
}
