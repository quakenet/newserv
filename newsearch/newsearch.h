#include "../nick/nick.h"
#include "../parser/parser.h"
#include "../channel/channel.h"
#include "../lib/flags.h"

#define    SEARCHTYPE_CHANNEL     1
#define    SEARCHTYPE_NICK        2


#define    NSMAX_KILL_LIMIT       500
#define    NSMAX_GLINE_LIMIT      500


#define    NSMAX_GLINE_CLONES     5


/* gline duration, in seconds */
#define    NSGLINE_DURATION       3600


#define    RETURNTYPE_BOOL        0x01
#define    RETURNTYPE_INT         0x02
#define    RETURNTYPE_STRING      0x03
#define    RETURNTYPE_TYPE        0xFF
#define    RETURNTYPE_CONST       0x100

struct searchNode;

typedef struct searchNode *(*parseFunc)(int, int, char **);
typedef void (*freeFunc)(struct searchNode *);
typedef void *(*exeFunc)(struct searchNode *, void *);

/* Core functions */
/* Logical  (BOOL -> BOOL)*/
struct searchNode *and_parse(int type, int argc, char **argv);
struct searchNode *not_parse(int type, int argc, char **argv);
struct searchNode *or_parse(int type, int argc, char **argv);

/* Comparison (INT -> BOOL) */
struct searchNode *eq_parse(int type, int argc, char **argv);
struct searchNode *lt_parse(int type, int argc, char **argv);
struct searchNode *gt_parse(int type, int argc, char **argv);

/* String match (STRING -> BOOL) */
struct searchNode *match_parse(int type, int argc, char **argv);
struct searchNode *regex_parse(int type, int argc, char **argv);

/* Length (STRING -> INT) */
struct searchNode *length_parse(int type, int argc, char **argv);

/* kill/gline actions (BOOL) */
struct searchNode *kill_parse(int type, int argc, char **argv);
struct searchNode *gline_parse(int type, int argc, char **argv);

/* Nick/Channel functions (various types) */
struct searchNode *nick_parse(int type, int argc, char **argv);
struct searchNode *modes_parse(int type, int argc, char **argv);

/* Nick functions (various types) */
struct searchNode *hostmask_parse(int type, int argc, char **argv);
struct searchNode *realname_parse(int type, int argc, char **argv);
struct searchNode *authname_parse(int type, int argc, char **argv);
struct searchNode *ident_parse(int type, int argc, char **argv);
struct searchNode *host_parse(int type, int argc, char **argv);
struct searchNode *channel_parse(int type, int argc, char **argv);
struct searchNode *timestamp_parse(int type, int argc, char **argv);
struct searchNode *country_parse(int type, int argc, char **argv);
struct searchNode *ip_parse(int type, int argc, char **argv);

/* Channel functions (various types) */
struct searchNode *exists_parse(int type, int argc, char **argv);
struct searchNode *services_parse(int type, int argc, char **argv);
struct searchNode *size_parse(int type, int argc, char **argv);
struct searchNode *name_parse(int type, int argc, char **argv);
struct searchNode *topic_parse(int type, int argc, char **argv);
struct searchNode *oppct_parse(int type, int argc, char **argv);
struct searchNode *hostpct_parse(int type, int argc, char **argv);
struct searchNode *authedpct_parse(int type, int argc, char **argv);

/* Interpret a string to give a node */
struct searchNode *search_parse(int type, char *input);

/* Force a node to return the thing you want */
struct searchNode *coerceNode(struct searchNode *thenode, int type);

/* Registration functions */
void registersearchterm(char *term, parseFunc parsefunc);
void deregistersearchterm(char *term, parseFunc parsefunc);

void *trueval(int type);
void *falseval(int type);

typedef struct searchNode {
  int returntype;
  exeFunc  exe;
  freeFunc free;
  void *localdata;
} searchNode;

extern const char *parseError;
