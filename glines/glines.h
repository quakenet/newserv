#ifndef __GLINES_H
#define __GLINES_H

void glinesetbynick(nick *, int, char *, char *, int);
void glinesetbynode( patricia_node_t *, int, char *, char *);
void glinesetbyhost(char *, char *, int, char *, char *);

void unglinebyhost(char *, char *, int, char *);

#define GLINE_FORCE_IDENT 1

#endif
