#include <stdio.h>

#include "../irc/irc_config.h"
#include "../lib/irc_string.h"
#include "../nick/nick.h"
#include "request.h"
#include "user.h"

r_user_t *r_userlist = NULL;
int ru_loading = 0;

r_user_t *ru_find(char *name);

int ru_create(char *name, unsigned int level) {
	r_user_t *user, *trav;

	if (ru_setlevel(name, level) != 0)
		return 1;

	user = (r_user_t*)malloc(sizeof(r_user_t));

	if (user == NULL)
		return 0;

	user->next = NULL;
	strlcpy(user->name, name, sizeof(user->name));
	user->level = level;

	if (r_userlist == NULL)
		r_userlist = user;
	else {
		trav = r_userlist;

		while (trav->next)
			trav = trav->next;

		trav->next = user;
	}

	ru_persist();
	
	return 1;
}

void ru_destroy(char *name) {
	r_user_t *user = r_userlist;

	if (user && ircd_strcmp(user->name, name) == 0) {
		free(r_userlist);

		r_userlist = NULL;

		ru_persist();
		
		return;
	}

	if (user == NULL)
		return;
	
	while (user->next) {
		if (ircd_strcmp(user->next->name, name) == 0) {
			user->next = user->next->next;

			free(user->next);
		}

		user = user->next;
	}

	ru_persist();
}

int ru_parseline(char *line) {
  char name[ACCOUNTLEN];
  unsigned int level;
  int result;

  if (sscanf(line, "%s %lu", name, &level) < 2)
    return 0;

  ru_loading = 1;
  result = ru_create(name, level);
  ru_loading = 0;

  return result;
}

int ru_load(void) {
  char line[4096];
  FILE *rudata;
  int count;

  rudata = fopen(RQ_USERFILE, "r");

  if (rudata == NULL)
    return 0;

  count = 0;

  while (!feof(rudata)) {
    if (fgets(line, sizeof(line), rudata) == NULL)
      break;

    if (line[strlen(line) - 1] == '\n')
      line[strlen(line) - 1] = '\0';

    if (line[strlen(line) - 1] == '\r')
      line[strlen(line) - 1] = '\0';

    if (line[0] != '\0') {
      if (ru_parseline(line))
        count++;
    }
  }

  fclose(rudata);

  return count;
}

int ru_persist(void) {
  FILE *rudata;
  int i, count = 0;
  r_user_t *user = r_userlist;

  if (ru_loading)
    return;

  rudata = fopen(RQ_USERFILE, "w");

  if (rudata == NULL)
    return 0;

  while (user) {
    fprintf(rudata, "%s %lu\n", user->name, user->level);

    user = user->next;
  }

  fclose(rudata);

  return count;
}

r_user_t *ru_find(char *name) {
	r_user_t *user = r_userlist;

	while (user) {
		if (ircd_strcmp(user->name, name) == 0)
			return user;

		user = user->next;
	}

	return NULL;
}

unsigned int ru_getlevel(nick *np) {
	if (IsOper(np))
		return 999;
	else if (!IsAccount(np))
		return 0;

	return ru_getlevel_str(np->authname);
}

unsigned int ru_getlevel_str(char *name) {
	r_user_t *user = ru_find(name);

	if (user)
		return user->level;
	else
		return 0;
}

int ru_setlevel(char *name, unsigned int level) {
	r_user_t *user = ru_find(name);

	if (user) {
		user->level = level;

		ru_persist();

		return 1;
	} else
		return 0;
}
