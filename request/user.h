/* R user system */

typedef struct r_user_s {
	struct r_user_s *next;
	char name[ACCOUNTLEN];
	unsigned int level;
} r_user_t;

extern r_user_t *r_userlist;

int ru_load(void);
int ru_persist(void);

int ru_create(char *name, unsigned int level);
void ru_destroy(char *name);
unsigned int ru_getlevel(nick *np);
unsigned int ru_getlevel_str(char *name);
int ru_setlevel(char *name, unsigned int level);
