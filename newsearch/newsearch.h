#include "../nick/nick.h"
#include "../lib/sstring.h"
#include "../parser/parser.h"
#include "../channel/channel.h"
#include "../lib/flags.h"
#include "../authext/authext.h"
#include "../patricia/patricia.h"
#include "../whowas/whowas.h"

#ifndef __NEWSEARCH_H
#define __NEWSEARCH_H

#define    NSMAX_KILL_LIMIT       500
#define    NSMAX_GLINE_LIMIT      500

/* gline duration, in seconds */
#define    NSGLINE_DURATION       3600

#define    NSMAX_REASON_LEN       120
#define    NSMAX_NOTICE_LEN       250
#define    NSMAX_COMMAND_LEN      20

#define    RETURNTYPE_BOOL        0x01
#define    RETURNTYPE_INT         0x02
#define    RETURNTYPE_STRING      0x03
#define    RETURNTYPE_TYPE        0xFF
#define    RETURNTYPE_CONST       0x100

#define    VARIABLE_LEN    10
#define    MAX_VARIABLES   10

struct searchNode;
struct searchCtx;
struct coercedata;

typedef struct searchNode *(*searchParseFunc)(struct searchCtx *ctx, char *input);
typedef void (*replyFunc)(nick *np, char *format, ...);
typedef void (*wallFunc)(int level, char *format, ...);

typedef struct searchNode *(*parseFunc)(struct searchCtx *, int, char **);
typedef void (*freeFunc)(struct searchCtx *, struct searchNode *);
typedef void *(*exeFunc)(struct searchCtx *, struct searchNode *, void *);
typedef void (*ChanDisplayFunc)(struct searchCtx *, nick *, chanindex *);
typedef void (*NickDisplayFunc)(struct searchCtx *, nick *, nick *);
typedef void (*UserDisplayFunc)(struct searchCtx *, nick *, authname *);
typedef void (*WhowasDisplayFunc)(struct searchCtx *, nick *, whowas *);
typedef void (*HeaderFunc)(void *sender, void *arg);

struct coercedata {
  struct searchNode *child;
  union {
    char *stringbuf;
    unsigned long val;
  } u;
};

typedef struct searchNode {
  int returntype;
  exeFunc  exe;
  freeFunc free;
  void *localdata;
} searchNode;

struct searchVariable {
  char name[VARIABLE_LEN];
  struct searchNode data;
  struct coercedata cdata;
};

typedef struct searchCmd {
  void *defaultdisplayfunc;  
  sstring *name;
  CommandHandler handler;
  struct CommandTree *outputtree;
  struct CommandTree *searchtree;
} searchCmd;

typedef struct searchList { 
  void *cmd;
  sstring *name;  
  char *help;
  struct searchList *next;
} searchList;

typedef struct searchCtx {
  searchParseFunc parser;
  replyFunc reply;
  wallFunc wall;
  void *arg;
  struct searchVariable vars[MAX_VARIABLES];
  int lastvar;
  struct searchCmd *searchcmd;
  nick *sender;
  int limit;
  void *target;
  void *displayfn;
} searchCtx;

/* Core functions */
/* Logical  (BOOL -> BOOL)*/
struct searchNode *and_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *not_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *or_parse(searchCtx *ctx,  int argc, char **argv);

/* Comparison (INT -> BOOL) */
struct searchNode *eq_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *lt_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *gt_parse(searchCtx *ctx, int argc, char **argv);

/* String match (STRING -> BOOL) */
struct searchNode *match_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *regex_parse(searchCtx *ctx, int argc, char **argv);

/* Length (STRING -> INT) */
struct searchNode *length_parse(searchCtx *ctx, int argc, char **argv);

/* kill/gline actions (BOOL) */
struct searchNode *kill_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *gline_parse(searchCtx *ctx, int argc, char **argv);

/* notice action (BOOL) */
struct searchNode *notice_parse(searchCtx *ctx, int argc, char **argv);

/* Nick/Channel functions (various types) */
struct searchNode *nick_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *modes_parse(searchCtx *ctx, int argc, char **argv);

/* Nick functions (various types) */
struct searchNode *hostmask_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *realname_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *away_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *authname_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *authts_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *ident_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *host_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *channel_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *timestamp_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *country_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *ip_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *channels_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *server_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *authid_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *cidr_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *ipv6_parse(searchCtx *ctx, int argc, char **argv);

/* Whowas functions (various types) */
struct searchNode *quit_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *killed_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *renamed_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *age_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *newnick_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *reason_parse(searchCtx *ctx, int argc, char **argv);

/* Channel functions (various types) */
struct searchNode *exists_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *services_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *size_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *name_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *topic_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *oppct_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *cumodecount_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *cumodepct_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *hostpct_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *authedpct_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *kick_parse(searchCtx *ctx, int argc, char **argv);

/* Iteration functions */
struct searchNode *any_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *all_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *var_parse(searchCtx *ctx, int argc, char **argv);

/* Iteraterable functions */
struct searchNode *channeliter_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *nickiter_parse(searchCtx *ctx, int argc, char **argv);

/* Functions that operate on strings */
struct searchNode *cumodes_parse(searchCtx *ctx, int argc, char **argv);

/* Force a node to return the thing you want */
struct searchNode *coerceNode(searchCtx *ctx, struct searchNode *thenode, int type);

/* Registration functions */
searchCmd *registersearchcommand(char *name, int level, CommandHandler cmd, void *defaultdisplayfunc);
void deregistersearchcommand(searchCmd *scmd);
void registersearchterm(searchCmd *cmd, char *term, parseFunc parsefunc, int level, char *help);
void deregistersearchterm(searchCmd *cmd, char *term, parseFunc parsefunc);

void registerglobalsearchterm(char *term, parseFunc parsefunc, char *help);
void deregisterglobalsearchterm(char *term, parseFunc parsefunc);

void regdisp( searchCmd *cmd, const char *name, void *handler, int level, char *help);
void unregdisp( searchCmd *cmd, const char *name, void *handler);

/* Special nick* printf */
void nssnprintf(char *, size_t, const char *, nick *);

void displaystrerror(replyFunc reply, nick *np, const char *input);

extern const char *parseError;
extern nick *senderNSExtern;

void printnick(searchCtx *, nick *, nick *);
void printuser(searchCtx *, nick *, authname *);
void printchannel(searchCtx *, nick *, chanindex *);
void printwhowas(searchCtx *, nick *, whowas *);

void nicksearch_exe(struct searchNode *search, searchCtx *sctx);
void chansearch_exe(struct searchNode *search, searchCtx *sctx);
void usersearch_exe(struct searchNode *search, searchCtx *ctx);
void whowassearch_exe(struct searchNode *search, searchCtx *ctx);

int do_nicksearch_real(replyFunc reply, wallFunc wall, void *source, int cargc, char **cargv);
int do_chansearch_real(replyFunc reply, wallFunc wall, void *source, int cargc, char **cargv);
int do_usersearch_real(replyFunc reply, wallFunc wall, void *source, int cargc, char **cargv);
int do_whowassearch_real(replyFunc reply, wallFunc wall, void *source, int cargc, char **cargv);

void *literal_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void literal_free(searchCtx *ctx, struct searchNode *thenode);

struct searchVariable *var_register(searchCtx *ctx, char *arg, int type);
searchNode *var_get(searchCtx *ctx, char *arg);
void var_setstr(struct searchVariable *v, char *data);

void newsearch_ctxinit(searchCtx *ctx, searchParseFunc searchfn, replyFunc replyfn, wallFunc wallfn, void *arg, searchCmd *cmd, nick *sender, void *displayfn, int limit, void *target);

/* AST functions */

struct searchASTNode;

#define AST_NODE_CHILD 1
#define AST_NODE_LITERAL 2

/* items to store in the ast lookup cache */
#define AST_RECENT 10

struct searchASTExpr;

typedef struct searchASTNode {
  parseFunc fn;
  int argc;
  struct searchASTExpr *argv;
} searchASTNode;

typedef struct searchASTExpr {
  int type;
  union {
    char *literal;
    /* this was originally a pointer, but having it as one means you can't memcpy directly */
    struct searchASTNode child;
  } u;
} searchASTExpr;

typedef struct searchASTCache {
  searchASTExpr *tree;
  searchASTExpr *cache[AST_RECENT];
  int nextpos;
} searchASTCache;

#define __NSASTSizeOfArray(x) (sizeof(x) / sizeof(x[0]))

#define __NSASTExpr(x, y, ...) (searchASTExpr){.type = x, .u.y = __VA_ARGS__}
#define __NSASTRawNode(x, c, y) (searchASTNode){.fn = x, .argc = c, .argv = y}

#define __NSASTSizeNode(x, y) __NSASTRawNode(x, __NSASTSizeOfArray(y), y)
#define __NSASTNode(x, ...) __NSASTSizeNode(x, ((searchASTExpr []){__VA_ARGS__}))

#define __NSASTChild(...) __NSASTExpr(AST_NODE_CHILD, child, __VA_ARGS__)

#define NSASTLiteral(value) __NSASTExpr(AST_NODE_LITERAL, literal, value)
#define NSASTNode(fn, ...) __NSASTChild(__NSASTNode(fn, __VA_ARGS__))
#define NSASTManualNode(fn, count, children) __NSASTChild(__NSASTRawNode(fn, count, children))

searchNode *search_astparse(searchCtx *, char *);

int ast_nicksearch(searchASTExpr *tree, replyFunc reply, void *sender, wallFunc wall, NickDisplayFunc display, HeaderFunc header, void *headerarg, int limit, nick *);
int ast_chansearch(searchASTExpr *tree, replyFunc reply, void *sender, wallFunc wall, ChanDisplayFunc display, HeaderFunc header, void *headerarg, int limit, chanindex *);
int ast_usersearch(searchASTExpr *tree, replyFunc reply, void *sender, wallFunc wall, UserDisplayFunc display, HeaderFunc header, void *headerarg, int limit, authname *);
int ast_whowassearch(searchASTExpr *tree, replyFunc reply, void *sender, wallFunc wall, WhowasDisplayFunc display, HeaderFunc header, void *headerarg, int limit, whowas *);

char *ast_printtree(char *buf, size_t bufsize, searchASTExpr *expr, searchCmd *cmd);

int parseopts(int cargc, char **cargv, int *arg, int *limit, void **subset, void *display, CommandTree *sl, replyFunc reply, void *sender);

typedef int (*ASTFunc)(searchASTExpr *, replyFunc, void *, wallFunc, void *, HeaderFunc, void *, int limit, void *target);

/* erk */
extern searchList *globalterms;

extern searchCmd *reg_nicksearch;
extern searchCmd *reg_chansearch;
extern searchCmd *reg_usersearch;
extern searchCmd *reg_whowassearch;

extern UserDisplayFunc defaultuserfn;
extern NickDisplayFunc defaultnickfn;
extern ChanDisplayFunc defaultchanfn;
extern WhowasDisplayFunc defaultwhowasfn;

struct searchNode *argtoconststr(char *command, searchCtx *ctx, char *arg, char **p);

#endif
