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
  int size;
  char buf[];
};

static int addacl(struct acls *a, char *service, char *command) {
  if(a->size == MAX_ACLS) {
    nterface_log(nrl, NL_ERROR, "Too many ACLs");
    return 0;
  }

  a->service[a->size] = service;
  a->command[a->size] = command;
  a->size++;
  return 1;
}

struct acls *parseacls(const char *c) {
  size_t len = strlen(c);
  struct acls *a = ntmalloc(sizeof(struct acls) + len + 1);
  if (!a) {
    nterface_log(nrl, NL_ERROR, "Unable to allocate memory for ACLs");
    return NULL;
  }
  memcpy(a->buf, c, len + 1);
  a->size = 0;

  char *sp1, *service = strtok_r(a->buf, " ", &sp1);
  while(service) {
    char *token2 = strchr(service, ':');
    if (token2 == NULL) {
      nterface_log(nrl, NL_ERROR, "Invalid acl: %s -- IGNORED", service);
    } else {
      *token2 = '\0';
      char *sp2, *command = strtok_r(token2 + 1, ",", &sp2);
      while(command) {
        addacl(a, service, command);
        command = strtok_r(NULL, ",", &sp2);
      }
    }
    service = strtok_r(NULL, " ", &sp1);
  }

  return a;
}

void freeacls(struct acls *a) {
  if(a)
    ntfree(a);
}

int checkacls(struct acls *a, struct service_node *s, struct handler *hl) {
  if(!a)
    return 0;

  char *service = s->name->content;
  char *command = hl->command->content;
  for(int i=0;i<a->size;i++)
    if (!strcmp(a->service[i], service) && !strcmp(a->command[i], command))
      return 1;

  return 0;
}

char *printacls(struct acls *a, char *buf, size_t len) {
  if(a == NULL)
    return NULL;

  buf[0] = '\0';

  for(int i=0;i<a->size;i++) {
    char local[512];
    snprintf(local, sizeof(local), "%s:%s ", a->service[i], a->command[i]);
    strlcat(buf, local, len);
  }
  size_t l = strlen(buf);
  if (l > 0)
    buf[l - 1] = '\0';

  return buf;
}
