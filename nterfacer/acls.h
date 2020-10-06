#ifndef __nterface_acls_H
#define __nterface_acls_H

struct handler;
struct service_node;
struct acls;

struct acls *parseacls(const char *c);
void freeacls(struct acls *a);
int checkacls(struct acls *a, struct service_node *service, struct handler *hl, int argc, char **argv);
char *printacls(struct acls *a, char *buf, size_t len);

#endif
