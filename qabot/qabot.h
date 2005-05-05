/* qabot.h */
#ifndef _QABOT_H
#define _QABOT_H

#define QABOT_NICK        "QABot"
#define QABOT_USER        "qabot"
#define QABOT_HOST        "quakenet.org"
#define QABOT_NAME        "Question & Answer Bot v1.0"
#define QABOT_ACCT        "QABot"
#define QABOT_UMDE        UMODE_SERVICE|UMODE_OPER|UMODE_INV|UMODE_ACCOUNT
#define QABOT_CHILD_UMODE UMODE_INV
#define TUTOR_NAME        "#tutorial bot"
#define TUTOR_ACCOUNT     "Tutor"

#define QABOT_HOMECHAN "#qnet.pr"

#define QAUFLAG_STAFF     0x01
#define QAUFLAG_ADMIN     0x02
#define QAUFLAG_DEVELOPER 0x04

#define QAIsStaff(x)     ((x)->flags & (QAUFLAG_STAFF|QAUFLAG_ADMIN|QAUFLAG_DEVELOPER))
#define QAIsAdmin(x)     ((x)->flags & (QAUFLAG_ADMIN|QAUFLAG_DEVELOPER))
#define QAIsDeveloper(x) ((x)->flags & QAUFLAG_DEVELOPER)

#define QUESTIONINTERVAL       30
#define SPAMINTERVAL           10
#define ASKWAIT                30
#define QUEUEDQUESTIONINTERVAL 2
#define QUESTIONHASHSIZE       1000

#define QABOT_SAVEWAIT 3600

#define QABOT_MICTIMEOUT  180

#define QAB_CONTROLCHAR 0x01
#define QAB_COLOUR      0x02
#define QAB_AUTHEDONLY  0x04
#define QAB_LINEBREAK   0x08
#define QAB_FLOODDETECT 0x10
#define QAB_FLOODSTOP   0x20
#define QAB_BLOCKMARK   0x40
/*#define QAB_
#define QAB_*/

#define QABBLOCK_ACCOUNT 0
#define QABBLOCK_HOST    1
#define QABBLOCK_TEXT    2

#define QAQ_NEW      0x00
#define QAQ_ANSWERED 0x01
#define QAQ_OFFTOPIC 0x02
#define QAQ_SPAM     0x03
#define QAQ_QSTATE   0x07

#define QAC_QUESTIONCHAN 0x01
#define QAC_STAFFCHAN    0x02

#define DEFAULTBOTFLAGS QAB_CONTROLCHAR|QAB_COLOUR|QAB_LINEBREAK|QAB_FLOODSTOP

typedef struct qab_user {
  char             authname[ACCOUNTLEN + 1];
  flag_t           flags;
  time_t           created;

  struct qab_user* next;
  struct qab_user* prev;
} qab_user;

typedef struct qab_spam {
  char*            message;
  
  struct qab_spam* next;
} qab_spam;

typedef struct qab_question {
  int                  id;
  char*                question;
  flag_t               flags;
  char                 nick[NICKLEN + 1];
  unsigned long        numeric;
  unsigned long        crc;
  char*                answer;
  
  struct qab_question* next;
} qab_question;

typedef struct qab_answer {
  qab_question*      question;
  char               nick[NICKLEN + 1];
  
  struct qab_answer* next;
} qab_answer;

typedef struct qab_block {
  char              type;
  char              creator[ACCOUNTLEN + 1];
  time_t            created;
  char*             blockstr;

  struct qab_block* next;
  struct qab_block* prev;
} qab_block;

typedef struct qab_textsection {
  qab_spam*               lines;
  qab_spam*               lines_tail;
  int                     line_count;
  
  struct qab_textsection* next;
  struct qab_textsection* prev;
} qab_textsection;

typedef struct qab_text {
  char             name[NICKLEN + 1];
  qab_textsection* sections;
  qab_textsection* sections_tail;
  int              section_count;
  
  struct qab_text* next;
  struct qab_text* prev;
} qab_text;

typedef struct qab_bot {
  char             nick[NICKLEN + 1];
  char             user[USERLEN + 1];
  char*            host;
  
  nick*            np;
  
  chanindex*       public_chan;
  chanindex*       question_chan;
  chanindex*       staff_chan;
  
  flag_t           flags;
  
  /*int              question_interval;*/
  int              spam_interval;
  int              ask_wait;
  int              queued_question_interval;
  
  time_t           lastmic;
  int              mic_timeout;
  
  qab_block*       blocks;
  int              block_count;
  
  qab_question*    questions[QUESTIONHASHSIZE];
  int              lastquestionID;
  int              answered;
  
  int              spammed;
  
  qab_spam*        nextspam;
  qab_spam*        lastspam;
  time_t           spamtime;
  
  qab_answer*      answers;
  
  unsigned long    micnumeric;
  
  qab_textsection* recording_section;
  qab_text*        texts;

  struct qab_bot*  next;
  struct qab_bot*  prev;
} qab_bot;

extern time_t qab_startime;
extern int qabot_nickext;
extern int qabot_spam_nickext;
extern int qabot_chanext;
extern nick* qabot_nick;
extern CommandTree* qabot_commands;
extern qab_user* qabot_users;
extern qab_bot* qab_bots;
extern unsigned long qab_lastq_crc;
extern int qab_lastq_count;

/* qabot.c */
void qabot_lostnick(int hooknum, void* arg);
void qabot_channel_part(int hooknum, void* arg);
void qabot_createfakeuser(void* arg);
void qabot_handler(nick* me, int type, void** args);
void qabot_child_handler(nick* me, int type, void** args);
char* qabot_getvictim(nick* np, char* target);
const char* qabot_uflagtostr(flag_t flags);
const char* qabot_formattime(time_t tme);

/* qabot_bot.c */
qab_bot* qabot_getbot();
int qabot_addbot(char* nickname, char* user, char* host, char* pub_chan, char* qu_chan, char* stff_chan, flag_t flags, int spam_interval, int ask_wait, int queued_question_interval, nick* sender);
void qabot_delbot(qab_bot* bot);
channel* qabot_getchannel(char* channel_name);
void qabot_spam(void* arg);
void qabot_spamstored(void* arg);
void qabot_createbotfakeuser(void* arg);
qab_bot* qabot_getcurrentbot();
channel* qabot_getcurrentchannel();
void qabot_timer(void* arg);

/* qabot_chancommands.c */
int qabot_dochananswer(void* np, int cargc, char** cargv);
int qabot_dochanblock(void* np, int cargc, char** cargv);
int qabot_dochanclear(void* np, int cargc, char** cargv);
int qabot_dochanclosechan(void* np, int cargc, char** cargv);
int qabot_dochanconfig(void* np, int cargc, char** cargv);
int qabot_dochanhelp(void* np, int cargc, char** cargv);
int qabot_dochanlistblocks(void* np, int cargc, char** cargv);
int qabot_dochanmic(void* np, int cargc, char** cargv);
int qabot_dochanmoo(void* np, int cargc, char** cargv);
int qabot_dochanofftopic(void* np, int cargc, char** cargv);
int qabot_dochanopenchan(void* np, int cargc, char** cargv);
int qabot_dochanping(void* np, int cargc, char** cargv);
int qabot_dochanreset(void* np, int cargc, char** cargv);
int qabot_dochanspam(void* np, int cargc, char** cargv);
int qabot_dochanstatus(void* np, int cargc, char** cargv);
int qabot_dochanunblock(void* np, int cargc, char** cargv);
int qabot_dochanlisttexts(void* np, int cargc, char** cargv);
int qabot_dochanshowsection(void* np, int cargc, char** cargv);
int qabot_dochanaddtext(void* np, int cargc, char** cargv);
int qabot_dochandeltext(void* np, int cargc, char** cargv);
int qabot_dochanaddsection(void* np, int cargc, char** cargv);
int qabot_dochandelsection(void* np, int cargc, char** cargv);
int qabot_dochanrecord(void* np, int cargc, char** cargv);

/* qabot_commands.c */
int qabot_doshowcommands(void* sender, int cargc, char** cargv);
int qabot_dohelp(void* sender, int cargc, char** cargv);
int qabot_dohello(void* sender, int cargc, char** cargv);
int qabot_dosave(void* sender, int cargc, char** cargv);
int qabot_dolistbots(void* sender, int cargc, char** cargv);
int qabot_dolistusers(void* sender, int cargc, char** cargv);
int qabot_doshowbot(void* sender, int cargc, char** cargv);
int qabot_doaddbot(void* sender, int cargc, char** cargv);
int qabot_dodelbot(void* sender, int cargc, char** cargv);
int qabot_doadduser(void* sender, int cargc, char** cargv);
int qabot_dodeluser(void* sender, int cargc, char** cargv);
int qabot_dochangelev(void* sender, int cargc, char** cargv);
int qabot_dowhois(void* sender, int cargc, char** cargv);
int qabot_dostatus(void* sender, int cargc, char** cargv);

/* qabot_dbase.c */
void qabot_loaddb();
void qabot_savedb();
void qabot_savetimer(void* arg);
void qabot_adduser(const char* authname, flag_t flags, time_t created);
void qabot_deluser(const char* authname);
void qabot_squelchuser(qab_user* user);
qab_user* qabot_getuser(const char* authname);
qab_bot* qabot_findbot(const char* nickname);

/* qabot_help.c */
int qabot_showhelp(nick* np, char* arg);

/* qabot_texts.c */
void qabot_freetext(qab_bot* bot, qab_text* text);
void qabot_freesection(qab_text* text, qab_textsection* section);

#endif
