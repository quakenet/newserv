#include "../nick/nick.h"

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
typedef void *(*exeFunc)(struct searchNode *, int, void *);

/* Core functions */
struct searchNode *and_parse(int type, int argc, char **argv);
struct searchNode *not_parse(int type, int argc, char **argv);
struct searchNode *or_parse(int type, int argc, char **argv);
struct searchNode *eq_parse(int type, int argc, char **argv);
struct searchNode *lt_parse(int type, int argc, char **argv);
struct searchNode *gt_parse(int type, int argc, char **argv);
struct searchNode *match_parse(int type, int argc, char **argv);
struct searchNode *regex_parse(int type, int argc, char **argv);
struct searchNode *hostmask_parse(int type, int argc, char **argv);
struct searchNode *modes_parse(int type, int argc, char **argv);
struct searchNode *realname_parse(int type, int argc, char **argv);
struct searchNode *nick_parse(int type, int argc, char **argv);
struct searchNode *authname_parse(int type, int argc, char **argv);
struct searchNode *ident_parse(int type, int argc, char **argv);
struct searchNode *host_parse(int type, int argc, char **argv);
struct searchNode *channel_parse(int type, int argc, char **argv);
struct searchNode *timestamp_parse(int type, int argc, char **argv);
struct searchNode *country_parse(int type, int argc, char **argv);
struct searchNode *ip_parse(int type, int argc, char **argv);
struct searchNode *kill_parse(int type, int argc, char **argv);
struct searchNode *gline_parse(int type, int argc, char **argv);


struct searchNode *search_parse(int type, char *input);

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
