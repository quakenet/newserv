#define PATROL_POOLSIZE 1000 /* 1000 */
#define PATROL_MINPOOLSIZE 500 /* 500 */
#define PATROL_MINIMUM_HOSTS_BEFORE_POOL 5000 /* 5000 */

#define PATROL_POOL_REGENERATION 3600

#define PATROL_HOST_POOL      0x00
#define PATROL_STEAL_HOST     0x01

#define PATROL_HOST_MODE      PATROL_STEAL_HOST

#define PATROL_MAX_CLONE_COUNT 3

#define PATROL_MMIN(a, b) a > b ? b : a

int patrol_repool(void);
nick *patrol_generateclone(int extraumodes, UserMessageHandler handler);
void patrol_nickchange(nick *np);
