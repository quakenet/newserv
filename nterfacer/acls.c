#define _DEFAULT_SOURCE /* strtok_r */
#include <stdio.h>
#include <string.h>

#include "nterfacer.h"
#include "logging.h"
#include "../lib/strlfunc.h"

#define MAX_ACLS 100

struct acls {
  char *service[MAX_ACLS];
  char *command[MAX_ACLS];
  char *arg0[MAX_ACLS];
  int size;
  char buf[];
};

static int addacl(struct acls *a, char *service, char *command, char *arg0) {
  if(a->size == MAX_ACLS) {
    nterface_log(nrl, NL_ERROR, "Too many ACLs");
    return 0;
  }

  a->service[a->size] = service;
  a->command[a->size] = command;
  a->arg0[a->size] = arg0;
  a->size++;
  return 1;
}

struct acls *parseacls(const char *c) {
  size_t len = strlen(c);
  struct acls *a = ntmalloc(sizeof(struct acls) + len + 1);
  if (!a) {
    nterface_log(nrl, NL_ERROR, "Unable to allocate memory for ACL -- set to DENY ALL");
    return NULL;
  }
  memcpy(a->buf, c, len + 1);
  a->size = 0;

  char *sp1, *service = strtok_r(a->buf, " ", &sp1);
  while(service) {
    char orig[512]; /* display only */
    strlcpy(orig, service, sizeof(orig));

    char *command = strchr(service, ':');
    if (command == NULL) {
      nterface_log(nrl, NL_ERROR, "Invalid acl: %s -- set to DENY ALL", orig);
      goto error;
    }

    *command++ = '\0';
    char *arg0 = strchr(command, ','); /* yeah we don't support commands with ',' in them... */
    if(arg0) {
      *arg0++ = '\0';
      if(strchr(arg0, ',')) {
        nterface_log(nrl, NL_ERROR, "Invalid acl (no more than one argument supported): %s -- set to DENY ALL", orig);
        goto error;
      }
    }

    if(!addacl(a, service, command, arg0))
      goto error;

    service = strtok_r(NULL, " ", &sp1);
  }

  return a;

error:
  ntfree(a);
  return NULL;
}

void freeacls(struct acls *a) {
  if(a)
    ntfree(a);
}

int checkacls(struct acls *a, struct service_node *s, struct handler *hl, int argc, char **argv) {
  if(!a)
    return 0;

  char *service = s->name->content;
  char *command = hl->command->content;
  for(int i=0;i<a->size;i++) {
    if (!strcmp(a->service[i], service) && !strcmp(a->command[i], command)) {
      if (a->arg0[i] == NULL)
        return 1;
      if (argc >= 1 && !strcmp(a->arg0[i], argv[0]))
        return 1;
    }
  }

  return 0;
}

char *printacls(struct acls *a, char *buf, size_t len) {
  if(a == NULL)
    return "DENY ALL";

  buf[0] = '\0';

  for(int i=0;i<a->size;i++) {
    char local[512];
    snprintf(local, sizeof(local), "%s%s:%s%s%s", i > 0 ? " " : "", a->service[i], a->command[i], a->arg0[i] ? "," : "", a->arg0[i] ? a->arg0[i] : "");
    strlcat(buf, local, len);
  }

  return buf;
}
