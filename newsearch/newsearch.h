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

#define    NSMAX_REASON_LEN       120
#define    NSMAX_NOTICE_LEN       250


#define    RETURNTYPE_BOOL        0x01
#define    RETURNTYPE_INT         0x02
#define    RETURNTYPE_STRING      0x03
#define    RETURNTYPE_TYPE        0xFF
#define    RETURNTYPE_CONST       0x100

struct searchNode;
struct searchCtx;

typedef struct searchNode *(*searchParseFunc)(struct searchCtx *ctx, int type, char *input);
typedef void (*replyFunc)(nick *np, char *format, ...);

typedef struct searchCtx {
  searchParseFunc parser;
  replyFunc reply;
} searchCtx;

typedef struct searchNode *(*parseFunc)(searchCtx *, int, int, char **);
typedef void (*freeFunc)(searchCtx *, struct searchNode *);
typedef void *(*exeFunc)(searchCtx *, struct searchNode *, void *);
typedef void (*ChanDisplayFunc)(searchCtx *, nick *, chanindex *);
typedef void (*NickDisplayFunc)(searchCtx *, nick *, nick *);

/* Core functions */
/* Logical  (BOOL -> BOOL)*/
struct searchNode *and_parse(searchCtx *ctx, int type, int argc, char **argv);
struct searchNode *not_parse(searchCtx *ctx, int type, int argc, char **argv);
struct searchNode *or_parse(searchCtx *ctx, int type, int argc, char **argv);

/* Comparison (INT -> BOOL) */
struct searchNode *eq_parse(searchCtx *ctx, int type, int argc, char **argv);
struct searchNode *lt_parse(searchCtx *ctx, int type, int argc, char **argv);
struct searchNode *gt_parse(searchCtx *ctx, int type, int argc, char **argv);

/* String match (STRING -> BOOL) */
struct searchNode *match_parse(searchCtx *ctx, int type, int argc, char **argv);
struct searchNode *regex_parse(searchCtx *ctx, int type, int argc, char **argv);

/* Length (STRING -> INT) */
struct searchNode *length_parse(searchCtx *ctx, int type, int argc, char **argv);

/* kill/gline actions (BOOL) */
struct searchNode *kill_parse(searchCtx *ctx, int type, int argc, char **argv);
struct searchNode *gline_parse(searchCtx *ctx, int type, int argc, char **argv);

/* notice action (BOOL) */
struct searchNode *notice_parse(searchCtx *ctx, int type, int argc, char **argv);

/* Nick/Channel functions (various types) */
struct searchNode *nick_parse(searchCtx *ctx, int type, int argc, char **argv);
struct searchNode *modes_parse(searchCtx *ctx, int type, int argc, char **argv);

/* Nick functions (various types) */
struct searchNode *hostmask_parse(searchCtx *ctx, int type, int argc, char **argv);
struct searchNode *realname_parse(searchCtx *ctx, int type, int argc, char **argv);
struct searchNode *authname_parse(searchCtx *ctx, int type, int argc, char **argv);
struct searchNode *authts_parse(searchCtx *ctx, int type, int argc, char **argv);
struct searchNode *ident_parse(searchCtx *ctx, int type, int argc, char **argv);
struct searchNode *host_parse(searchCtx *ctx, int type, int argc, char **argv);
struct searchNode *channel_parse(searchCtx *ctx, int type, int argc, char **argv);
struct searchNode *timestamp_parse(searchCtx *ctx, int type, int argc, char **argv);
struct searchNode *country_parse(searchCtx *ctx, int type, int argc, char **argv);
struct searchNode *ip_parse(searchCtx *ctx, int type, int argc, char **argv);
struct searchNode *channels_parse(searchCtx *ctx, int type, int argc, char **argv);
struct searchNode *server_parse(searchCtx *ctx, int type, int argc, char **argv);
struct searchNode *authid_parse(searchCtx *ctx, int type, int argc, char **argv);

/* Channel functions (various types) */
struct searchNode *exists_parse(searchCtx *ctx, int type, int argc, char **argv);
struct searchNode *services_parse(searchCtx *ctx, int type, int argc, char **argv);
struct searchNode *size_parse(searchCtx *ctx, int type, int argc, char **argv);
struct searchNode *name_parse(searchCtx *ctx, int type, int argc, char **argv);
struct searchNode *topic_parse(searchCtx *ctx, int type, int argc, char **argv);
struct searchNode *oppct_parse(searchCtx *ctx, int type, int argc, char **argv);
struct searchNode *hostpct_parse(searchCtx *ctx, int type, int argc, char **argv);
struct searchNode *authedpct_parse(searchCtx *ctx, int type, int argc, char **argv);
struct searchNode *kick_parse(searchCtx *ctx, int type, int argc, char **argv);

/* Interpret a string to give a node */
struct searchNode *search_parse(searchCtx *ctx, int type, char *input);

/* Force a node to return the thing you want */
struct searchNode *coerceNode(searchCtx *ctx, struct searchNode *thenode, int type);

/* Registration functions */
void registersearchterm(char *term, parseFunc parsefunc);
void deregistersearchterm(char *term, parseFunc parsefunc);
void regchandisp(const char *name, ChanDisplayFunc handler);
void unregchandisp(const char *name, ChanDisplayFunc handler);
void regnickdisp(const char *name, NickDisplayFunc handler);
void unregnickdisp(const char *name, NickDisplayFunc handler);

/* Special nick* printf */
void nssnprintf(char *, size_t, const char *, nick *);

typedef struct searchNode {
  int returntype;
  exeFunc  exe;
  freeFunc free;
  void *localdata;
} searchNode;

extern const char *parseError;

void printnick(searchCtx *, nick *, nick *);

void nicksearch_exe(struct searchNode *search, searchCtx *sctx, nick *sender, NickDisplayFunc display, int limit);
void chansearch_exe(struct searchNode *search, searchCtx *sctx, nick *sender, ChanDisplayFunc display, int limit);
