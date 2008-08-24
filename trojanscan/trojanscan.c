/*
 * Trojanscan version 2
 *
 * Trojanscan  copyright (C) Chris Porter 2002-2007
 * Newserv bits copyright (C) David Mansell 2002-2003
 * 
 * TODO: CHECK::
 *   - Poke splidge about +r'ing bots, potential problems:
 *     - users might whine about T clone stealing account
 *       - would have to steal one already in use, so if trojans start using /msg q whois they'll see
 *       (though they have to be authed for this, they could use a clone of their own however)
 */

#include "trojanscan.h"
#include "../lib/strlfunc.h"
#include "../lib/version.h"
#include "../core/nsmalloc.h"

#define tmalloc(x)     nsmalloc(POOL_TROJANSCAN, x)
#define tfree(x)       nsfree(POOL_TROJANSCAN, x)

MODULE_VERSION(TROJANSCAN_VERSION);

void trojanscan_phrasematch(channel *chp, nick *sender, trojanscan_phrases *phrase, char messagetype, char *matchbuf);
char *trojanscan_sanitise(char *input);
void trojanscan_refresh_settings(void);
static void trojanscan_part_watch(int hook, void *arg);
static void trojanscan_connect_nick(void *);

#define TROJANSCAN_SETTING_SIZE 256
#define TROJANSCAN_MAX_SETTINGS 50

static struct {
  char setting[TROJANSCAN_SETTING_SIZE];
  char value[TROJANSCAN_SETTING_SIZE];
} trojanscan_settings[TROJANSCAN_MAX_SETTINGS];

static int settingcount = 0;
static char *versionreply;
static int hooksregistered = 0;
static void *trojanscan_connect_nick_schedule;

void _init() {
  trojanscan_cmds = newcommandtree();

  addcommandtotree(trojanscan_cmds, "showcommands", TROJANSCAN_ACL_UNAUTHED, 0, &trojanscan_showcommands);
  addcommandtotree(trojanscan_cmds, "help", TROJANSCAN_ACL_UNAUTHED, 1, &trojanscan_help);
  addcommandtotree(trojanscan_cmds, "hello", TROJANSCAN_ACL_UNAUTHED | TROJANSCAN_ACL_OPER, 1, &trojanscan_hello);

  addcommandtotree(trojanscan_cmds, "join", TROJANSCAN_ACL_STAFF, 1, &trojanscan_userjoin);
  addcommandtotree(trojanscan_cmds, "chanlist", TROJANSCAN_ACL_STAFF, 0, &trojanscan_chanlist);
  addcommandtotree(trojanscan_cmds, "whois", TROJANSCAN_ACL_STAFF, 1, &trojanscan_whois);

  addcommandtotree(trojanscan_cmds, "changelev", TROJANSCAN_ACL_STAFF | TROJANSCAN_ACL_OPER, 2, &trojanscan_changelev);
  addcommandtotree(trojanscan_cmds, "deluser", TROJANSCAN_ACL_TEAMLEADER | TROJANSCAN_ACL_OPER, 2, &trojanscan_deluser);
  addcommandtotree(trojanscan_cmds, "mew", TROJANSCAN_ACL_STAFF, 2, &trojanscan_mew);
  addcommandtotree(trojanscan_cmds, "status", TROJANSCAN_ACL_STAFF | TROJANSCAN_ACL_OPER, 0, &trojanscan_status);
  addcommandtotree(trojanscan_cmds, "listusers", TROJANSCAN_ACL_TEAMLEADER, 0, &trojanscan_listusers);

  addcommandtotree(trojanscan_cmds, "rehash", TROJANSCAN_ACL_WEBSITE, 0, &trojanscan_rehash);

  addcommandtotree(trojanscan_cmds, "cat", TROJANSCAN_ACL_OPER, 1, &trojanscan_cat);

  addcommandtotree(trojanscan_cmds, "reschedule", TROJANSCAN_ACL_DEVELOPER | TROJANSCAN_ACL_OPER, 0, &trojanscan_reschedule);
  
  srand((int)time(NULL));

  trojanscan_connect_schedule = scheduleoneshot(time(NULL) + 1, &trojanscan_connect, NULL);
}

void _fini(void) {
  int i;
  struct trojanscan_realchannels *rp = trojanscan_realchanlist, *oldrp;
  struct trojanscan_rejoinlist *rj = trojanscan_schedulerejoins, *oldrj;
  
  if (trojanscan_nick)
    deregisterlocaluser(trojanscan_nick, NULL);
  
  if (trojanscan_connect_schedule)
    deleteschedule(trojanscan_connect_schedule, &trojanscan_connect, NULL);
    
  if (trojanscan_connect_nick_schedule)
    deleteschedule(trojanscan_connect_nick_schedule, &trojanscan_connect_nick, NULL);
    
  if(trojanscan_schedule)
    deleteschedule(trojanscan_schedule, &trojanscan_dojoin, NULL);

  if(trojanscan_poolschedule)
    deleteschedule(trojanscan_poolschedule, &trojanscan_repool, NULL);

  if(trojanscan_cloneschedule)
    deleteschedule(trojanscan_poolschedule, &trojanscan_registerclones, NULL);
  
  if(hooksregistered)
    deregisterhook(HOOK_CHANNEL_PART, trojanscan_part_watch);

  while(rp) {
    deleteschedule(rp->schedule, &trojanscan_dopart, (void *)rp);
    oldrp = rp;
    rp = rp->next;
    tfree(oldrp);
  }
  
  while(rj) {
    deleteschedule(rj->schedule, &trojanscan_rejoin_channel, (void *)rj);
    freesstring(rj->channel);
    oldrj = rj;
    rj = rj->next;
    tfree(oldrj);
  }

  if(trojanscan_initialschedule)
    deleteschedule(trojanscan_initialschedule, &trojanscan_fill_channels, NULL);
  
  deleteschedule(trojanscan_rehashschedule, &trojanscan_rehash_schedule, NULL);    

  for (i=0;i<TROJANSCAN_CLONE_TOTAL;i++)
    if(trojanscan_swarm[i].clone) {
      deregisterlocaluser(trojanscan_swarm[i].clone, NULL);
      derefnode(iptree, trojanscan_swarm[i].fakeipnode);
      trojanscan_swarm[i].clone = NULL;
    }
  trojanscan_free_database();
  trojanscan_free_channels();

  for (i=0;i<trojanscan_hostpoolsize;i++)
    freesstring(trojanscan_hostpool[i]);

  for (i=0;i<trojanscan_tailpoolsize;i++)
    freesstring(trojanscan_tailpool[i]);
  trojanscan_database_close();

  deletecommandfromtree(trojanscan_cmds, "showcommands", &trojanscan_showcommands);
  deletecommandfromtree(trojanscan_cmds, "help", &trojanscan_help);
  deletecommandfromtree(trojanscan_cmds, "hello", &trojanscan_hello);
  deletecommandfromtree(trojanscan_cmds, "join", &trojanscan_userjoin);
  deletecommandfromtree(trojanscan_cmds, "chanlist", &trojanscan_chanlist);
  deletecommandfromtree(trojanscan_cmds, "whois", &trojanscan_whois);
  deletecommandfromtree(trojanscan_cmds, "changelev", &trojanscan_changelev);
  deletecommandfromtree(trojanscan_cmds, "deluser", &trojanscan_deluser);
  deletecommandfromtree(trojanscan_cmds, "mew", &trojanscan_mew);
  deletecommandfromtree(trojanscan_cmds, "status", &trojanscan_status);
  deletecommandfromtree(trojanscan_cmds, "listusers", &trojanscan_listusers);
  deletecommandfromtree(trojanscan_cmds, "rehash", &trojanscan_rehash);
  deletecommandfromtree(trojanscan_cmds, "cat", &trojanscan_cat);
  deletecommandfromtree(trojanscan_cmds, "reschedule", &trojanscan_reschedule);

  destroycommandtree(trojanscan_cmds);
  nscheckfreeall(POOL_TROJANSCAN);
}

static void trojanscan_connect_nick(void *arg) {
  sstring *mnick, *myident, *myhost, *myrealname, *myauthname;
  channel *cp;

  mnick = getcopyconfigitem("trojanscan", "nick", "T", NICKLEN);
  myident = getcopyconfigitem("trojanscan", "ident", "trojanscan", NICKLEN);
  myhost = getcopyconfigitem("trojanscan", "hostname", "trojanscan.quakenet.org", HOSTLEN);
  myrealname = getcopyconfigitem("trojanscan", "realname", "Trojanscan v" TROJANSCAN_VERSION, REALLEN);
  myauthname = getcopyconfigitem("trojanscan", "authname", "T", ACCOUNTLEN);

  trojanscan_nick = registerlocaluser(mnick->content, myident->content, myhost->content, myrealname->content, myauthname->content, UMODE_SERVICE | UMODE_DEAF |
                                                                                                                          UMODE_OPER | UMODE_INV |
                                                                                                                          UMODE_ACCOUNT,
                                                                                                                          &trojanscan_handlemessages);                                                                                                                            
  freesstring(mnick);
  freesstring(myident);
  freesstring(myhost);
  freesstring(myrealname);
  freesstring(myauthname);

  cp = findchannel(TROJANSCAN_OPERCHANNEL);
  if (!cp) {
    localcreatechannel(trojanscan_nick, TROJANSCAN_OPERCHANNEL);
  } else {
    if(!localjoinchannel(trojanscan_nick, cp))
      localgetops(trojanscan_nick, cp);
  }

  cp = findchannel(TROJANSCAN_CHANNEL);
  if (!cp) {
    localcreatechannel(trojanscan_nick, TROJANSCAN_CHANNEL);
  } else {
    if(!localjoinchannel(trojanscan_nick, cp))
      localgetops(trojanscan_nick, cp);
  }

#ifdef TROJANSCAN_PEONCHANNEL
  cp = findchannel(TROJANSCAN_PEONCHANNEL);
  if (!cp) {
    localcreatechannel(trojanscan_nick, TROJANSCAN_PEONCHANNEL);
  } else {
    if(!localjoinchannel(trojanscan_nick, cp))
      localgetops(trojanscan_nick, cp);
  }
#endif
}

void trojanscan_connect(void *arg) {
  sstring *dbhost, *dbuser, *dbpass, *db, *dbport, *temp;
  int length, i;
  char buf[10];
  
  trojanscan_connect_schedule = NULL;
  
  for(i=0;i<TROJANSCAN_CLONE_TOTAL;i++)
    trojanscan_swarm[i].index = i; /* sure this could be done with pointer arithmetic... */

  trojanscan_hostpoolsize = 0;
  trojanscan_tailpoolsize = 0;
  trojanscan_hostmode = 0;
  trojanscan_poolschedule = NULL;
  trojanscan_cloneschedule = NULL;
  trojanscan_realchanlist = NULL;
  trojanscan_database.glines = 0;
  trojanscan_database.detections = 0;
    
  dbhost = getcopyconfigitem("trojanscan", "dbhost", "localhost", HOSTLEN);
  dbuser = getcopyconfigitem("trojanscan", "dbuser", "", NICKLEN);
  dbpass = getcopyconfigitem("trojanscan", "dbpass", "", REALLEN);
  db = getcopyconfigitem("trojanscan", "db", "", NICKLEN);
  
  dbport = getcopyconfigitem("trojanscan", "dbport", "3306", ACCOUNTLEN);
  
  length = snprintf(buf, sizeof(buf) - 1, "%d", TROJANSCAN_DEFAULT_MAXCHANS);
  temp = getcopyconfigitem("trojanscan", "maxchans", buf, length);

  trojanscan_maxchans = atoi(temp->content);
  freesstring(temp);

  length = snprintf(buf, sizeof(buf) - 1, "%d", TROJANSCAN_DEFAULT_CYCLETIME);
  temp = getcopyconfigitem("trojanscan", "cycletime", buf, length);

  trojanscan_cycletime = atoi(temp->content);
  freesstring(temp);
  
  length = snprintf(buf, sizeof(buf) - 1, "%d", TROJANSCAN_DEFAULT_PARTTIME);
  temp = getcopyconfigitem("trojanscan", "parttime", buf, length);
  trojanscan_part_time = atoi(temp->content);
  freesstring(temp);

  length = snprintf(buf, sizeof(buf) - 1, "%d", TROJANSCAN_DEFAULT_MAXUSERS);
  temp = getcopyconfigitem("trojanscan", "maxusers", buf, length);
  trojanscan_maxusers = atoi(temp->content);
  freesstring(temp);
  
  length = snprintf(buf, sizeof(buf) - 1, "%d", TROJANSCAN_MINIMUM_HOSTS_BEFORE_POOL);
  temp = getcopyconfigitem("trojanscan", "minpoolhosts", buf, length);
  trojanscan_min_hosts = atoi(temp->content);
  freesstring(temp);

  if ((trojanscan_cycletime / trojanscan_maxchans) < 1) {
    Error("trojanscan", ERR_FATAL, "Cycletime / maxchans < 1, increase cycletime or decrease maxchans else cycling breaks.");
    return; /* PPA: module failed to load */
  }
  
  length = snprintf(buf, sizeof(buf) - 1, "%d", TROJANSCAN_DEFAULT_MINIMUM_CHANNEL_SIZE);
  temp = getcopyconfigitem("trojanscan", "minchansize", buf, length);
  trojanscan_minchansize = atoi(temp->content);
  freesstring(temp);

  trojanscan_connect_nick(NULL);

  if (trojanscan_database_connect(dbhost->content, dbuser->content, dbpass->content, db->content, atoi(dbport->content)) < 0) {
    Error("trojanscan", ERR_FATAL, "Cannot connect to database host!");
    return; /* PPA: module failed to load */
  }
  
  trojanscan_database_query("CREATE TABLE phrases (id INT(10) PRIMARY KEY AUTO_INCREMENT, wormid INT(10) NOT NULL, phrase TEXT NOT NULL, priority INT(10) DEFAULT 0 NOT NULL, dateadded int(10), disabled BOOL DEFAULT 0 NOT NULL)");
  trojanscan_database_query("CREATE TABLE worms (id INT(10) PRIMARY KEY AUTO_INCREMENT, wormname TEXT NOT NULL, glinetype INT DEFAULT 0, data text, hitmsgs BOOL DEFAULT 1, hitchans BOOL DEFAULT 0, epidemic BOOL DEFAULT 0, privinfo text)");
  trojanscan_database_query("CREATE TABLE logs (id INT(10) PRIMARY KEY AUTO_INCREMENT, userid INT(10) NOT NULL, act TEXT NOT NULL, description TEXT NOT NULL, ts TIMESTAMP)");
  trojanscan_database_query("CREATE TABLE channels (id INT(10) PRIMARY KEY AUTO_INCREMENT, channel VARCHAR(%d) NOT NULL, exempt BOOL DEFAULT 0)", CHANNELLEN);
  trojanscan_database_query("CREATE TABLE users (id INT(10) PRIMARY KEY AUTO_INCREMENT, authname VARCHAR(%d) NOT NULL, authlevel TINYINT(4) NOT NULL)", ACCOUNTLEN);
  trojanscan_database_query("CREATE TABLE hits (id INT(10) PRIMARY KEY AUTO_INCREMENT, nickname VARCHAR(%d) NOT NULL, ident VARCHAR(%d) NOT NULL, host VARCHAR(%d) NOT NULL, phrase INT(10) NOT NULL, ts TIMESTAMP, messagetype VARCHAR(1) NOT NULL DEFAULT 'm', glined BOOL DEFAULT 1)", NICKLEN, USERLEN, HOSTLEN);
  trojanscan_database_query("CREATE TABLE settings (id INT(10) PRIMARY KEY AUTO_INCREMENT, setting VARCHAR(255) NOT NULL UNIQUE, value VARCHAR(255) NOT NULL)");
  trojanscan_database_query("CREATE TABLE wwwlogs (id INT(10) PRIMARY KEY AUTO_INCREMENT, authid INT(10) NOT NULL, ip VARCHAR(15), action TEXT, ts TIMESTAMP)");
  trojanscan_database_query("CREATE TABLE unknownlog (id INT(10) PRIMARY KEY AUTO_INCREMENT, data TEXT, user VARCHAR(%d) NOT NULL, ts TIMESTAMP)", NICKLEN+USERLEN+HOSTLEN+3);
  
  trojanscan_database_query("DELETE FROM settings WHERE setting = 'rehash' OR setting = 'changed'");
  trojanscan_database_query("INSERT INTO settings (setting, value) VALUES ('rehash','0')");
  trojanscan_database_query("INSERT INTO settings (setting, value) VALUES ('changed','0')");

  /* assumption: constants aren't supplied by someone evil */
  trojanscan_database_query("INSERT INTO settings (setting, value) VALUES ('versionreply','" TROJANSCAN_DEFAULT_VERSION_REPLY "')");
  
  trojanscan_refresh_settings();
  trojanscan_read_database(1);
 
  freesstring(dbhost);
  freesstring(dbuser);
  freesstring(dbpass);
  freesstring(db);
  freesstring(dbport);
  trojanscan_registerclones(NULL);
  
  trojanscan_rehashschedule = scheduleoneshot(time(NULL) + 60, &trojanscan_rehash_schedule, NULL);

  registerhook(HOOK_CHANNEL_PART, trojanscan_part_watch);
  hooksregistered = 1;
}

char *trojanscan_get_setting(char *setting) {
  int i;

  for(i=0;i<settingcount;i++)
    if(!strcmp(trojanscan_settings[i].setting, setting))
      return trojanscan_settings[i].value;

  return NULL;
}

void trojanscan_refresh_settings(void) {
  trojanscan_database_res *res;
  trojanscan_database_row sqlrow;
  int i = 0;

  if(trojanscan_database_query("SELECT setting, value FROM settings"))
    return;

  if(!(res = trojanscan_database_store_result(&trojanscan_sql)))
    return;

  if (trojanscan_database_num_rows(res) <= 0)
    return;

  while((sqlrow = trojanscan_database_fetch_row(res))) {
    strlcpy(trojanscan_settings[i].setting, sqlrow[0], TROJANSCAN_SETTING_SIZE);
    strlcpy(trojanscan_settings[i].value, sqlrow[1], TROJANSCAN_SETTING_SIZE);

    trojanscan_sanitise(trojanscan_settings[i].value);

    if(++i == TROJANSCAN_MAX_SETTINGS)
      break;
  }

  settingcount = i;

  trojanscan_database_free_result(res);

  /* optimisation hack */
  versionreply = trojanscan_get_setting("versionreply");
}

void trojanscan_rehash_schedule(void *arg) {
  char *v;
  trojanscan_rehashschedule = scheduleoneshot(time(NULL) + 60, &trojanscan_rehash_schedule, NULL);

  trojanscan_refresh_settings();

  v = trojanscan_get_setting("rehash");
  if(v && v[0] == '1') {
    trojanscan_mainchanmsg("n: rehash initiated by website. . .");
    trojanscan_read_database(0);
  }
}

void trojanscan_free_database(void) {
  int i;
  for(i=0;i<trojanscan_database.total_channels;i++)
    freesstring(trojanscan_database.channels[i].name);
  tfree(trojanscan_database.channels);
  for(i=0;i<trojanscan_database.total_phrases;i++) {
    if (trojanscan_database.phrases[i].phrase)
      pcre_free(trojanscan_database.phrases[i].phrase);
    if (trojanscan_database.phrases[i].hint)
      pcre_free(trojanscan_database.phrases[i].hint);
  }
  tfree(trojanscan_database.phrases);
  for(i=0;i<trojanscan_database.total_worms;i++)
    freesstring(trojanscan_database.worms[i].name);
  tfree(trojanscan_database.worms);
  trojanscan_database.total_channels = 0;
  trojanscan_database.total_phrases = 0;
  trojanscan_database.total_worms = 0;
  trojanscan_database.channels = NULL;
  trojanscan_database.phrases = NULL;
  trojanscan_database.worms = NULL;  
}

char *trojanscan_sanitise(char *input) {
  char *p;

  for(p=input;*p;p++)
    if(*p == '\r' || *p == '\n')
      *p = '!';

  return input;
}

sstring *trojanscan_getsstring(char *string, int length) {
  int i;
  
  for(i=0;i<length;i++) {
    if ((string[i] == '\r') || (string[i] == '\n')) {
      Error("trojanscan", ERR_WARNING, "Error reading %s at position %d, set to ERROR!", string, i+1);
      return getsstring("ERROR", sizeof("ERROR"));
    }
  }
  
  return getsstring(string, length);
}

int trojanscan_strip_codes(char *buf, size_t max, char *original) {
  int i, j, length = TROJANSCAN_MMIN(strlen(original), max-1);
  char *p2 = original, *p3, flag = 0;
  p3 = buf;
  for(i=0;i<length+1;i++) {
    switch (*p2) {
      case '\002':
      case '\017':
      case '\026':
      case '\037':
      break;
      case '\003':
        for(j=0;j<6;j++) {
          if ((i + 1) > length)
            break;
          if ((j == 4) && flag)
            break;
          p2++;
          i++;
          if ((j == 0) && (!((*p2 >= '0') && (*p2 <= '9'))))
            break;
          if (j == 1) {
            
            if (*p2 == ',') {
                if ((i + 1) > length)
                  break;
                if (!((*(p2 + 1) >= '0') && (*(p2 + 1) <= '9')))
                  break;
              flag = 1;
            } else if ((*p2 >= '0') && (*p2 <= '9')) {
              flag = 0;
            } else {
              break;
            }
          }
          if (j == 2) {
            if (flag) {
              if (!((*p2 >= '0') && (*p2 <= '9')))
                break;
            } else {
              if (*p2 != ',') {
                break;
              } else {
                if ((i + 1) > length)
                  break;
                if (!((*(p2 + 1) >= '0') && (*(p2 + 1) <= '9')))
                  break;
              }
            }
          }
          if ((j == 3) && (!((*p2 >= '0') && (*p2 <= '9'))))
            break;       
          if ((j == 4) && (!((*p2 >= '0') && (*p2 <= '9'))))
            break;       
        }
        p2--;
        i--;
        break;
      
      default:
        *p3 = *p2;
        p3++;
        break;
    }
    p2++;
  }
  return p3 - buf;
}

struct trojanscan_worms *trojanscan_find_worm_by_id(int id) {
  int i;
  for(i=0;i<trojanscan_database.total_worms;i++)
    if ((trojanscan_database.worms[i].id == id))
      return &trojanscan_database.worms[i];
  return NULL;
}

void trojanscan_read_database(int first_time) {
  const char *error;
  int erroroffset, i, tempresult;

  trojanscan_database_res *res;
  trojanscan_database_row sqlrow;

  if (!first_time) {
    trojanscan_free_database();
  } else {
    trojanscan_database.total_channels = 0;
    trojanscan_database.total_phrases = 0;
    trojanscan_database.total_worms = 0;
  }
  
  if (!(trojanscan_database_query("SELECT channel, exempt FROM channels"))) {
    if ((res = trojanscan_database_store_result(&trojanscan_sql))) {
      trojanscan_database.total_channels = trojanscan_database_num_rows(res);
      if (trojanscan_database.total_channels > 0) {
        if ((trojanscan_database.channels = (trojanscan_channels *)tmalloc(sizeof(trojanscan_channels) * trojanscan_database.total_channels))) {
          if ((trojanscan_database.total_channels>0) && trojanscan_database.channels) {
            i = 0;
            while((sqlrow = trojanscan_database_fetch_row(res))) {
              trojanscan_database.channels[i].name = trojanscan_getsstring(trojanscan_sanitise(sqlrow[0]), strlen(sqlrow[0]));
              trojanscan_database.channels[i].exempt = (sqlrow[1][0] == '1');
              i++;
            }
          }
        }
      }
      trojanscan_database_free_result(res);
    }
  } 
 
  if (!(trojanscan_database_query("SELECT id, wormname, glinetype, length(data), hitmsgs, hitchans, epidemic FROM worms"))) {
    if ((res = trojanscan_database_store_result(&trojanscan_sql))) {
      trojanscan_database.total_worms = trojanscan_database_num_rows(res);
      if (trojanscan_database.total_worms > 0) {
        if ((trojanscan_database.worms = (trojanscan_worms *)tmalloc(sizeof(trojanscan_worms) * trojanscan_database.total_worms))) {
          i = 0;
          while((sqlrow = trojanscan_database_fetch_row(res))) {
            trojanscan_database.worms[i].id = atoi(sqlrow[0]);
            trojanscan_database.worms[i].name = trojanscan_getsstring(trojanscan_sanitise(sqlrow[1]), strlen(sqlrow[1]));
            tempresult = atoi(sqlrow[2]);
            trojanscan_database.worms[i].glineuser = (tempresult == 0);
            trojanscan_database.worms[i].glinehost = (tempresult == 1);
            trojanscan_database.worms[i].monitor = (tempresult == 2);
            if(sqlrow[3]) {
              trojanscan_database.worms[i].datalen = ((atoi(sqlrow[3]) == 0) ? 0 : 1);
            } else {
              trojanscan_database.worms[i].datalen = 0;
            }
            
            trojanscan_database.worms[i].hitpriv = (atoi(sqlrow[4]) == 1);
            trojanscan_database.worms[i].hitchans = (atoi(sqlrow[5]) == 1);
            trojanscan_database.worms[i].epidemic = (atoi(sqlrow[6]) == 1);
            
            i++;
          }
        }
      }
      trojanscan_database_free_result(res);
    }
  } 
  
  if (!(trojanscan_database_query("SELECT id, phrase, wormid FROM phrases WHERE disabled = 0 ORDER BY priority DESC"))) {
    if ((res = trojanscan_database_store_result(&trojanscan_sql))) {
      trojanscan_database.total_phrases = trojanscan_database_num_rows(res);
      if (trojanscan_database.total_phrases > 0) {
        if ((trojanscan_database.phrases = (trojanscan_phrases *)tmalloc(sizeof(trojanscan_phrases) * trojanscan_database.total_phrases))) {
          i = 0;
          while((sqlrow = trojanscan_database_fetch_row(res))) {
            trojanscan_database.phrases[i].id = atoi(sqlrow[0]);
            trojanscan_database.phrases[i].worm = trojanscan_find_worm_by_id(atoi(sqlrow[2]));
            if (!(trojanscan_database.phrases[i].phrase = pcre_compile(sqlrow[1], PCRE_CASELESS, &error, &erroroffset, NULL))) {
              Error("trojanscan", ERR_WARNING, "Error compiling expression %s at offset %d: %s", sqlrow[1], erroroffset, error);
            } else {
              trojanscan_database.phrases[i].hint = pcre_study(trojanscan_database.phrases[i].phrase, 0, &error);
              if (error) {
                Error("trojanscan", ERR_WARNING, "Error studying expression %s: %s", sqlrow[1], error);
                pcre_free(trojanscan_database.phrases[i].phrase);
                trojanscan_database.phrases[i].phrase = NULL;
              }
            }
            i++;
          }
        }
      }
      trojanscan_database_free_result(res);
    }
  }

  trojanscan_database_query("UPDATE settings SET value = '0' where setting = 'rehash'");
}

void trojanscan_log(nick *np, char *event, char *details, ...) {
  int nickid = 0;
  char eevent[TROJANSCAN_QUERY_TEMP_BUF_SIZE], edetails[TROJANSCAN_QUERY_TEMP_BUF_SIZE], buf[513];
  va_list va;
    
  va_start(va, details);
  vsnprintf(buf, sizeof(buf) - 1, details, va);
  va_end(va);
  
  if (np)
    if (IsAccount(np))
      nickid = trojanscan_user_id_by_authname(np->authname);
  
  trojanscan_database_escape_string(eevent, event, strlen(event));
  trojanscan_database_escape_string(edetails, buf, strlen(buf));
  trojanscan_database_query("INSERT INTO logs (userid, act, description) VALUES ('%d', '%s', '%s')", nickid, eevent, edetails);
}

void trojanscan_generateclone(void *arg) {
  int i, loops = 0, modes = UMODE_XOPER | UMODE_INV;
  char c_nick[NICKLEN+1], c_ident[USERLEN+1], c_host[HOSTLEN+1], c_real[REALLEN+1];
  patricia_node_t *fakeip;

  i = (int)((long)arg);

  /* PPA: unlikely to be infinite */
  do {
    c_nick[0] = '\0';
    if (!loops && trojanscan_hostmode) /* only have one go at this */
      trojanscan_generatenick(c_nick, NICKLEN);
    if(!c_nick[0])
      trojanscan_gennick(c_nick, trojanscan_minmaxrand(7, TROJANSCAN_MMIN(13, NICKLEN)));
    loops++;
  } while ((getnickbynick(c_nick) != NULL));

  trojanscan_generateident(c_ident, USERLEN);
  if(!c_ident[0])
    trojanscan_genident(c_ident, trojanscan_minmaxrand(4, TROJANSCAN_MMIN(8, USERLEN)));
  
  if(trojanscan_hostmode) {
    trojanscan_generatehost(c_host, HOSTLEN, &fakeip);
    if(!c_host[0])
      trojanscan_genhost(c_host, HOSTLEN, &fakeip);
  } else {
    trojanscan_genhost(c_host, HOSTLEN, &fakeip);
  }
  
  trojanscan_generaterealname(c_real, REALLEN);
  if(!c_real[0])
    trojanscan_genreal(c_real, trojanscan_minmaxrand(15, TROJANSCAN_MMIN(50, REALLEN)));

  trojanscan_swarm[i].clone = registerlocaluser(c_nick, c_ident, c_host, c_real, NULL, modes, &trojanscan_clonehandlemessages);
  trojanscan_swarm[i].fakeipnode = fakeip;

  if(trojanscan_swarm[i].clone && !trojanscan_swarm_created) {
    nick *np = trojanscan_selectuser();
    if(np) /* select a 'random' sign on time for whois generation */
      trojanscan_swarm[i].clone->timestamp = np->timestamp;
  }
  trojanscan_swarm[i].remaining = trojanscan_minmaxrand(5, 100);

  trojanscan_swarm[i].sitting = 0;
  
}

void trojanscan_free_channels(void) {
  int i;
  if(trojanscan_chans) {
    for(i=0;i<trojanscan_activechans;i++) 
      freesstring(trojanscan_chans[i].channel);
    tfree(trojanscan_chans);
    trojanscan_chans = NULL;
    trojanscan_activechans = 0;
  }
}

void trojanscan_repool(void *arg) {
  if (trojanscan_generatepool() < TROJANSCAN_MINPOOLSIZE) {
    trojanscan_hostmode = 0;
    return;
  } else {
    trojanscan_hostmode = 1;
    trojanscan_poolschedule = scheduleoneshot(time(NULL) + TROJANSCAN_POOL_REGENERATION, &trojanscan_repool, NULL);
  }
}

void trojanscan_registerclones(void *arg) {
  unsigned int i;

  if (trojanscan_generatepool() < TROJANSCAN_MINPOOLSIZE) {
    trojanscan_hostmode = 0;
    trojanscan_cloneschedule = scheduleoneshot(time(NULL) + 10, &trojanscan_registerclones, NULL);
    return;
  } else {
    trojanscan_hostmode = 1;
    trojanscan_poolschedule = scheduleoneshot(time(NULL) + TROJANSCAN_POOL_REGENERATION, &trojanscan_repool, NULL);
    trojanscan_cloneschedule = NULL;
  }
  
  for (i=0;i<TROJANSCAN_CLONE_TOTAL;i++)
    trojanscan_generateclone((void *)((long)i));
  trojanscan_mainchanmsg("n: swarm (%d clones) created.", TROJANSCAN_CLONE_TOTAL);
  trojanscan_swarm_created = 1;

  trojanscan_initialschedule = scheduleoneshot(time(NULL) + 60, &trojanscan_fill_channels, NULL);
}

int trojanscan_status(void *sender, int cargc, char **cargv) {
  nick *np = (nick *)sender;
  trojanscan_log(np, "status", "");
  trojanscan_reply(np, "Channels in schedule: %d", trojanscan_activechans);
  trojanscan_reply(np, "Channels in database: %d", trojanscan_database.total_channels);  
  trojanscan_reply(np, "Phrases: %d", trojanscan_database.total_phrases);
  trojanscan_reply(np, "Worms: %d", trojanscan_database.total_worms);
  trojanscan_reply(np, "Detections: %d", trojanscan_database.detections);
  trojanscan_reply(np, "Glines: %d", trojanscan_database.glines);
  trojanscan_reply(np, "Host/tail pool size: %d", TROJANSCAN_POOLSIZE);
  trojanscan_reply(np, "Cycletime: %d", trojanscan_cycletime);
  trojanscan_reply(np, "Clones: %d", TROJANSCAN_CLONE_TOTAL);
  return CMD_OK;
}

int trojanscan_chanlist(void *sender, int cargc, char **cargv) {
  int i;
  nick *np = (nick *)sender;
  char buf[CHANNELLEN * 2 + 20];
  trojanscan_reply(np, "Channel list (%d total):", trojanscan_activechans);
  buf[0] = '\0';

  for(i=0;i<trojanscan_activechans;i++) {
    if(trojanscan_chans[i].channel->length + 3 > sizeof(buf) - strlen(buf)) {
      trojanscan_reply(np, "%s", buf);
      buf[0] = '\0';
    }

    /* if splidge sees this I'm going to die */
    strlcat(buf, trojanscan_chans[i].channel->content, sizeof(buf));
    strlcat(buf, " ", sizeof(buf));
  }
  if(buf[0])
    trojanscan_reply(np, "%s", buf);

  trojanscan_reply(np, "Done.");
  return CMD_OK;
}

int trojanscan_whois(void *sender, int cargc, char **cargv) {
  char *tochange;
  nick *np = (nick *)sender, *np2;
  int templevel;
  
  if (cargc < 1) {
    trojanscan_reply(np, "Not enough parameters.");
    return CMD_ERROR;
  }

  if (cargv[0][0] == '#') {
    tochange = cargv[0] + 1;
  } else {
    int i;
    np2 = getnickbynick(cargv[0]);
    if (!np2) {
      trojanscan_reply(np, "That nickname is not on the network.");
      return CMD_ERROR;
    }
    for(i=0;i<TROJANSCAN_CLONE_TOTAL;i++) {
      if(trojanscan_swarm[i].clone->nick && !ircd_strcmp(trojanscan_swarm[i].clone->nick, np2->nick)) {
        trojanscan_reply(np, "Nickname   : %s", np2->nick);
        trojanscan_reply(np, "Swarm      : yes");
        return CMD_OK;
      }
    }
    if (!IsAccount(np2)) {
      trojanscan_reply(np, "User is not authed.");
      return CMD_OK;
    }
    tochange = np2->authname;
  }

  templevel = trojanscan_user_level_by_authname(tochange);
  if (templevel == -1) {
    trojanscan_reply(np, "User does not exist.");
  } else {
    union trojanscan_userlevel flags;
    flags.number = templevel;
    trojanscan_reply(np, "Authname   : %s", tochange);
    trojanscan_reply(np, "Flags      : +" TROJANSCAN_FLAG_MASK, TrojanscanFlagsInfo(flags));
  }

  return CMD_OK;
}

void trojanscan_privmsg_chan_or_nick(channel *cp, nick *np, char *message, ...) {
  char buf[513];
  va_list va;
    
  if (!trojanscan_nick)
    return;
  
  va_start(va, message);
  vsnprintf(buf, sizeof(buf) - 1, message, va);
  va_end(va);
  
  if (cp) {
    sendmessagetochannel(trojanscan_nick, cp, buf);
  } else {
    sendmessagetouser(trojanscan_nick, np, buf);
  }

}

int trojanscan_mew(void *sender, int cargc, char **cargv) {
  nick *np = (nick *)sender, *np2 = NULL;
  channel *cp = NULL;

  if (cargc < 2) {
    trojanscan_reply(np, "Not enough paramaters.");
    return CMD_ERROR;
  }
  
  if(cargv[0][0] == '#') {
    if (!(cp = findchannel(cargv[0]))) {
      trojanscan_reply(np, "Channel not found.");
      return CMD_ERROR;
    }
    trojanscan_log(np, "mew", "%s %s", cp->index->name->content, cargv[1]);
  } else {
    if (!(np2 = getnickbynick(cargv[0]))) {
      trojanscan_reply(np, "Nickname is not present on the network.");
      return CMD_ERROR;
    }
    trojanscan_log(np, "mew", "%s %s", np2->nick, cargv[1]);
  }

  trojanscan_privmsg_chan_or_nick(cp, np2, "\001ACTION mews hopefully at %s\001", cargv[1]);

  if (cp) {
    trojanscan_reply(np, "Mewed at %s in %s.", cargv[1], cp->index->name->content);
  } else {
    trojanscan_reply(np, "Mewed at %s at %s.", cargv[1], np2->nick);
  }

  if(!IsOper(np))
    trojanscan_mainchanmsg("n: mew: %s %s (%s/%s)", cargv[1], cp?cp->index->name->content:np2->nick, np->nick, np->authname);

  return CMD_OK;
}

int trojanscan_cat(void *sender, int cargc, char **cargv) {
  nick *np = (nick *)sender, *np2 = NULL;
  channel *cp = NULL;
  FILE *cat;
  char buf[513], *p;
  
  if (cargc < 1) {
    trojanscan_reply(np, "Not enough paramaters.");
    return CMD_ERROR;
  }
  
  if(cargv[0][0] == '#') {
    if (!(cp = findchannel(cargv[0]))) {
      trojanscan_reply(np, "Channel not found.");
      return CMD_ERROR;
    }
    trojanscan_log(np, "cat", cp->index->name->content);
  } else {
    if (!(np2 = getnickbynick(cargv[0]))) {
      trojanscan_reply(np, "Nickname is not present on the network.");
      return CMD_ERROR;
    }
    trojanscan_log(np, "cat", np2->nick);
  }
  
  if ((!(cat = fopen(TROJANSCAN_CAT, "r")))) {
    trojanscan_reply(np, "Unable to open cat!");
    return CMD_ERROR;
  }
  
  while (fgets(buf, sizeof(buf) - 1, cat)) {
    if ((p = strchr(buf, '\n'))) {
      *p = '\0';
      trojanscan_privmsg_chan_or_nick(cp, np2, "%s", buf);
    } else if (feof(cat)) {
      trojanscan_privmsg_chan_or_nick(cp, np2, "%s", buf);
    }
  }
  
  fclose(cat);

  if (cp) {
    trojanscan_reply(np, "Spammed cat in %s.", cp->index->name->content);
  } else {
    trojanscan_reply(np, "Spammed cat at %s.", np2->nick);
  }
  
  return CMD_OK;
}

int trojanscan_reschedule(void *sender, int cargc, char **cargv) {
  nick *np = (nick *)sender;
  trojanscan_log(np, "reschedule", "");
  trojanscan_fill_channels(NULL);
  
  trojanscan_reply(np, "Rescheduled.");
  return CMD_OK;
}

int trojanscan_listusers(void *sender, int cargc, char **cargv) {
  nick *np = (nick *)sender;

  trojanscan_log(np, "listusers", "");

  trojanscan_reply(np, "User list:");
  
  if (!(trojanscan_database_query("SELECT authname, authlevel FROM users ORDER BY authlevel DESC, authname"))) {
    trojanscan_database_res *res;
    if ((res = trojanscan_database_store_result(&trojanscan_sql))) {
      if (trojanscan_database_num_rows(res)) {
        trojanscan_database_row sqlrow;
        union trojanscan_userlevel flags;
        while((sqlrow = trojanscan_database_fetch_row(res))) {
          flags.number = atoi(sqlrow[1]);
          trojanscan_reply(np, "%s +" TROJANSCAN_FLAG_MASK, sqlrow[0], TrojanscanIsDeveloper(flags) ? "d" : "", TrojanscanIsTeamLeader(flags) ? "t" : "", TrojanscanIsStaff(flags) ? "s" : "", TrojanscanIsWebsite(flags) ? "w" : "", TrojanscanIsCat(flags) ? "c" : "");
        }
      }
      trojanscan_database_free_result(res);
    }
  }

  trojanscan_reply(np, "Done.");
  return CMD_OK;
}

int trojanscan_help(void *sender, int cargc, char **cargv) {
  nick *np = (nick *)sender;

  if (cargc == 0) {
    trojanscan_reply(np, "Not enough parameters.");
    return CMD_ERROR;
  }
  
  if (!strcasecmp("help", cargv[0])) {
    trojanscan_reply(np, "Syntax: help <command name>");
    trojanscan_reply(np, "Gives help on commands.");
  } else if (!strcasecmp("status", cargv[0])) {
    trojanscan_reply(np, "Syntax: status");
    trojanscan_reply(np, "Gives statistical information about the bot.");
  } else if (!strcasecmp("join", cargv[0])) {
    trojanscan_reply(np, "Syntax: join <#channel>");
    trojanscan_reply(np, "Orders a clone to join supplied channel.");
  } else if (!strcasecmp("showcommands", cargv[0])) {
    trojanscan_reply(np, "Syntax: showcommands");
    trojanscan_reply(np, "Pretty obvious.");
  } else if (!strcasecmp("hello", cargv[0])) {
    trojanscan_reply(np, "Syntax: hello ?nickname?");
    trojanscan_reply(np, "Creates a new user.");
  } else if (!strcasecmp("rehash", cargv[0])) {
    trojanscan_reply(np, "Syntax: rehash");
    trojanscan_reply(np, "Reloads bot database.");
  } else if (!strcasecmp("changelev", cargv[0])) {
    trojanscan_reply(np, "Syntax: changelev <nickname or #authname> <flags>");
    trojanscan_reply(np, "Changes access flags of selected user to supplied input.");
    trojanscan_reply(np, "+d -> developer");
    trojanscan_reply(np, "+t -> team leader");
    trojanscan_reply(np, "+s -> staff");
    trojanscan_reply(np, "+w -> web management");
    trojanscan_reply(np, "+c -> cat access");
  } else if (!strcasecmp("deluser", cargv[0])) {
    trojanscan_reply(np, "Syntax: deluser <nickname or #authname>");
    trojanscan_reply(np, "Deletes selected user from my database.");
  } else if (!strcasecmp("mew", cargv[0])) {
    trojanscan_reply(np, "Syntax: mew <#channel or nickname> <nickname>");
    trojanscan_reply(np, "Gracefully mews at target in selected channel or query.");
  } else if (!strcasecmp("cat", cargv[0])) {
    trojanscan_reply(np, "Syntax: cat <#channel or nickname>");
    trojanscan_reply(np, "Shows the almightly cat.");
  } else if (!strcasecmp("reschedule", cargv[0])) {
    trojanscan_reply(np, "Syntax: reschedule");
    trojanscan_reply(np, "Recalculates bots schedule.");
  } else if (!strcasecmp("chanlist", cargv[0])) {
    trojanscan_reply(np, "Syntax: chanlist");
    trojanscan_reply(np, "Displays bots current channel list.");
  } else if (!strcasecmp("whois", cargv[0])) {
    trojanscan_reply(np, "Syntax: whois <nickname or #authname>");
    trojanscan_reply(np, "Displays information on given user.");
  } else if (!strcasecmp("whois", cargv[0])) {
    trojanscan_reply(np, "Syntax: listusers <flags>");
    trojanscan_reply(np, "Displays users with listusersing flags.");
  } else {  
    trojanscan_reply(np, "Command not found.");
    return CMD_ERROR;
  }
  
  return CMD_OK;
}

int trojanscan_hello(void *sender, int cargc, char **cargv) {
  nick *np = (nick *)sender, *toadd;
  char eaccount[TROJANSCAN_QUERY_TEMP_BUF_SIZE];
  int level = 0;
  
  if (cargc > 0) {
    toadd = getnickbynick(cargv[0]);
    if (!toadd) {
      trojanscan_reply(np, "That nickname is not on the network.");
      return CMD_ERROR;
    }
    if (!IsAccount(toadd)) {
      trojanscan_reply(np, "That user is not authed with the network.");
      return CMD_ERROR;
    }
  } else {
    if (!IsAccount(np)) {
      trojanscan_reply(np, "You are not authed with the network, auth before creating your user.");
      return CMD_ERROR;
    }
    toadd = np;
  }
  
  if (trojanscan_user_level_by_authname(toadd->authname)!=-1) {
    trojanscan_reply(np, "Authname (%s) is already on file.", toadd->authname);
    return CMD_ERROR;
  }
  
  trojanscan_log(np, "hello", toadd->authname);

  if (!(trojanscan_database_query("SELECT id FROM users LIMIT 1"))) {
    trojanscan_database_res *res;
    if ((res = trojanscan_database_store_result(&trojanscan_sql))) {
      if (trojanscan_database_num_rows(res) == 0)
        level = TROJANSCAN_ACL_DEVELOPER | TROJANSCAN_ACL_STAFF | TROJANSCAN_ACL_WEBSITE | TROJANSCAN_ACL_CAT;
      trojanscan_database_free_result(res);
    }
  }

  trojanscan_database_escape_string(eaccount, toadd->authname, strlen(toadd->authname));
  trojanscan_database_query("INSERT INTO users (authname, authlevel) VALUES ('%s', %d)", eaccount, level);
  trojanscan_reply(np, "Account added to database, account %s%s.", toadd->authname, level>0?" (first user so developer access)":"");
  
  return CMD_OK;
}

int trojanscan_user_level_by_authname(char *authname) {
  int result = -1, sl = strlen(authname);
  char eaccount[TROJANSCAN_QUERY_TEMP_BUF_SIZE];
  
  trojanscan_database_escape_string(eaccount, authname, sl);
  if (!(trojanscan_database_query("SELECT authlevel, authname FROM users WHERE authname = '%s'", eaccount))) {
    trojanscan_database_res *res;
    if ((res = trojanscan_database_store_result(&trojanscan_sql))) {
      if (trojanscan_database_num_rows(res) > 0) {
        trojanscan_database_row sqlrow = trojanscan_database_fetch_row(res);
        result = atoi(sqlrow[0]);
        strlcpy(authname, sqlrow[1], sl + 1);
      }
      trojanscan_database_free_result(res);
    }
  }
  return result;
}

int trojanscan_user_id_by_authname(char *authname) {
  int result = 0;
  char eaccount[TROJANSCAN_QUERY_TEMP_BUF_SIZE];

  trojanscan_database_escape_string(eaccount, authname, strlen(authname));
  if (!(trojanscan_database_query("SELECT id FROM users WHERE authname = '%s'", eaccount))) {
    trojanscan_database_res *res;
    if ((res = trojanscan_database_store_result(&trojanscan_sql))) {
      if (trojanscan_database_num_rows(res) > 0) {
        trojanscan_database_row sqlrow = trojanscan_database_fetch_row(res);
        result = atoi(sqlrow[0]);
      }
      trojanscan_database_free_result(res);
    }
  }
  return result;
}

struct trojanscan_clones *trojanscan_selectclone(char type) {
  struct trojanscan_clones *rc;
  int randomclone, hits = 0, minlimit, maxlimit;
  
  if(type == TROJANSCAN_WATCH_CLONES) {
    minlimit = TROJANSCAN_CLONE_MAX;
    maxlimit = minlimit + TROJANSCAN_WATCHCLONE_MAX - 1;
  } else {
    minlimit = 0;
    maxlimit = TROJANSCAN_CLONE_MAX - 1;
  }
  
  do {
    randomclone = trojanscan_minmaxrand(minlimit, maxlimit);
    if (hits++ > 200)
      return NULL;
    rc = &trojanscan_swarm[randomclone];
    if ((type == TROJANSCAN_NORMAL_CLONES) && (rc->sitting == 0) && (rc->remaining == 0))
      break;

  } while (rc->remaining == 0);
  
  if(type == TROJANSCAN_NORMAL_CLONES) {
    if ((rc->sitting == 0) && (rc->remaining == 0)) {
      if ((!rc->remaining) && (!rc->sitting)) {
        if (rc->clone) {
          deregisterlocaluser(rc->clone, NULL);
          derefnode(iptree, rc->fakeipnode);
          rc->clone = NULL;
        }
        trojanscan_generateclone((void *)((long)rc->index));
      }
    }
  }
  
  return rc;

}

/* hack hack hack */
int trojanscan_nickbanned(trojanscan_clones *np, channel *cp) {
  int ret;
  patricia_node_t *realipnode = np->clone->ipnode;

  np->clone->ipnode = np->fakeipnode;

  ret = nickbanned(np->clone, cp);

  np->clone->ipnode = realipnode;

  return ret;
}

struct trojanscan_realchannels *trojanscan_allocaterc(char *chan) {
  struct trojanscan_realchannels *rc;
  struct trojanscan_clones *clonep;
  channel *cp;
  int attempts_left = 10;
  
  if (!chan) {
    trojanscan_errorcode = 1; /* sorry splidge ;( */
    return NULL;
  }

  if(chan[0] != '#') {
    trojanscan_errorcode = 2;
    return NULL;
  }
  
  if (strlen(chan) > 1) {
    if(strrchr(chan, ',')) {
      trojanscan_errorcode = 3;
      return NULL;
    }

    if(strrchr(chan, ' ')) {
      trojanscan_errorcode = 4;
      return NULL;
    }
  }

  cp = findchannel(chan);
  if (!cp) {
    trojanscan_errorcode = 5;
    return NULL;
  }

  do {
    clonep = trojanscan_selectclone(TROJANSCAN_NORMAL_CLONES);
    if (!clonep) {
      trojanscan_errorcode = 6;
      return NULL;
    }
    if(!trojanscan_nickbanned(clonep, cp))
      break;
  } while (--attempts_left > 0);

  if (!attempts_left) {
    trojanscan_errorcode = 7;
    return NULL;
  }

  rc = (struct trojanscan_realchannels *)tmalloc(sizeof(struct trojanscan_realchannels));

  rc->next = NULL;
  rc->clone = clonep;
  rc->chan = cp;
  rc->donotpart = 0;
  rc->kickedout = 0;
  return rc;
}

void trojanscan_join(struct trojanscan_realchannels *rc) {
  struct trojanscan_realchannels *rp = trojanscan_realchanlist;
  
  if (rc->clone && rc->clone->clone) {
    if (!localjoinchannel(rc->clone->clone, rc->chan)) {
      rc->clone->remaining--;
      rc->clone->sitting++;  
      if (trojanscan_minmaxrand(1, TROJANSCAN_NICKCHANGE_ODDS)%TROJANSCAN_NICKCHANGE_ODDS == 0)
        trojanscan_donickchange((void *)rc->clone);

      rc->schedule = scheduleoneshot(time(NULL)+trojanscan_part_time, &trojanscan_dopart, (void *)rc);
  
      if (rp) {
        for(;rp->next;rp=rp->next);  
        rp->next = rc;
      } else {
        trojanscan_realchanlist = rc;
      }
    }
  }
    
}

int trojanscan_userjoin(void *sender, int cargc, char **cargv) {
  nick *np = (nick *)sender;
  struct trojanscan_realchannels *rc;
  
  if (cargc < 1) {
    trojanscan_reply(np, "Not enough paramaters");
    return CMD_ERROR;
  }

  if (!trojanscan_swarm_created) {
    trojanscan_reply(np, "My swarm is currently empty.");
    return CMD_OK;
  }
  
  if((rc = trojanscan_allocaterc(cargv[0]))) {
    trojanscan_log(np, "join", cargv[0]);
    trojanscan_join(rc);
    trojanscan_reply(np, "Clone has joined channel.");
    if(!IsOper(np))
      trojanscan_mainchanmsg("n: join: %s (%s/%s)", cargv[0], np->nick, np->authname);
  } else {
    if (trojanscan_errorcode == 5) {
      trojanscan_reply(np, "Not joining empty channel, check you entered the correct channel name.");
    } else {
      trojanscan_reply(np, "Clone could not join channel (error code %d)!", trojanscan_errorcode);
    }
  }
  return CMD_OK;
}

int trojanscan_rehash(void *sender, int cargc, char **cargv) {
  nick *np = (void *)sender;
  trojanscan_refresh_settings();
  trojanscan_read_database(0);
  trojanscan_log(np, "rehash", "");
  trojanscan_reply(np, "Done.");
  return CMD_OK;
}

int trojanscan_changelev(void *sender, int cargc, char **cargv) {
  nick *np = (nick *)sender, *np2;
  int templevel;
  char eaccount[TROJANSCAN_QUERY_TEMP_BUF_SIZE], *tochange, *p, mode = 1, error = 0, clast = 0, specialcase;
  union trojanscan_userlevel flags1, flags2;
  
  if (cargc < 2) {
    trojanscan_reply(np, "Not enough parameters.");
    return CMD_ERROR;
  }

  templevel = trojanscan_user_level_by_authname(np->authname);

  if (templevel == -1) {
    trojanscan_reply(np, "You do not have an account.");
    return CMD_ERROR;
  }
  
  flags1.number = templevel;
  
  if (cargv[0][0] == '#') {
    tochange = cargv[0] + 1;
  } else {
    np2 = getnickbynick(cargv[0]);
    if (!np2) {
      trojanscan_reply(np, "That nickname is not on the network.");
      return CMD_ERROR;
    }
    if (!IsAccount(np2)) {
      trojanscan_reply(np, "That user is not authed with the network.");
      return CMD_ERROR;
    }
    tochange = np2->authname;
  }
  
  templevel = trojanscan_user_level_by_authname(tochange);
  
  if (templevel == -1) {
    trojanscan_reply(np, "User does not exist.");
    return CMD_ERROR;
  }
  
  flags2.number = templevel;
  
  if (!ircd_strcmp(np->authname, tochange)) {
    specialcase = 1;
  } else {
    specialcase = 0;
  }
  
  for (p=cargv[1];*p;p++) {
    switch (*p) {
      case '+':
      case '-':
        mode = (*p == '+');
        break;
      case 'd':
        if (!TrojanscanIsDeveloper(flags1))
          clast = 1;
        flags2.values.developer = mode;
        break;
      case 't':
        if (!TrojanscanIsDeveloper(flags1))
          clast = 1;
        flags2.values.teamleader = mode;
        break;
      case 's':
        if (!TrojanscanIsLeastTeamLeader(flags1))
          clast = 1;
        flags2.values.staff = mode;
        break;
      case 'w':
        if (!TrojanscanIsDeveloper(flags1))
          clast = 1;
        flags2.values.website = mode;
        break;
      case 'c':
        if (!TrojanscanIsDeveloper(flags1))
          clast = 1;
        flags2.values.cat = mode;
        break;
      default:
        error = 1;
        goto last;
        break;  
    }
    if (clast == 1) {
      if (specialcase && !mode) { /* allow user to remove their own flags */
        clast = 0;
      } else {
        goto last;
      }
    }
  }  

  last:
  if (*p) {
    if (error) {
      trojanscan_reply(np, "Unknown mode: %c%c.", mode?'+':'-', *p);
    } else {
      trojanscan_reply(np, "You have insufficient privilidges to add/remove one or more flags specified.");
    }
    return CMD_ERROR;
  }
  
  trojanscan_log(np, "changelev", "%s %s", tochange, cargv[1]);
  trojanscan_database_escape_string(eaccount, tochange, strlen(tochange));
  trojanscan_database_query("UPDATE users SET authlevel = %d WHERE authname = '%s'", flags2.number, eaccount);
  
  trojanscan_reply(np, "Flags changed.");
  
  return CMD_OK;
}

int trojanscan_deluser(void *sender, int cargc, char **cargv) {
  nick *np = (nick *)sender, *to;
  int templevel;
  char eaccount[TROJANSCAN_QUERY_TEMP_BUF_SIZE], *account;
  union trojanscan_userlevel flags1, flags2;
  
  if (cargc < 1) {
    trojanscan_reply(np, "Not enough parameters.");
    return CMD_ERROR;
  }
  
  if (cargv[0][0] == '#') {
    account = cargv[0] + 1;
  } else {
    to = getnickbynick(cargv[0]);
    if (!to) {
      trojanscan_reply(np, "That nickname is not on the network.");
      return CMD_ERROR;
    }
    if (!IsAccount(to)) {
      trojanscan_reply(np, "That user is not authed with the network.");
      return CMD_ERROR;
    }
    account = to->authname;
  }
  
  flags1.number = trojanscan_user_level_by_authname(np->authname);
  templevel = trojanscan_user_level_by_authname(account);
  
  if (templevel == -1) {
    trojanscan_reply(np, "Auth %s does not exist.", account);
    return CMD_ERROR;
  }
  
  flags2.number = templevel;
  
  if (!TrojanscanIsDeveloper(flags1) && TrojanscanIsLeastTeamLeader(flags2)) {
      trojanscan_reply(np, "Your cannot delete %s as his/her flags equal or surpass your own.", account);
      return CMD_ERROR;
  }
  
  trojanscan_log(np, "deluser", account);
  trojanscan_database_escape_string(eaccount, account, strlen(account));
  trojanscan_database_query("DELETE FROM users WHERE authname = '%s'", eaccount);
  trojanscan_reply(np, "User deleted.");
  
  return CMD_OK;
}

int trojanscan_add_ll(struct trojanscan_prechannels **head, struct trojanscan_prechannels *newitem) {
  struct trojanscan_prechannels *position, *lastitem = NULL, *location = NULL;
  if (!*head) {
    *head = newitem;
    newitem->next = NULL;
    if (newitem->exempt) {
      return 0;
    } else {
      return 1;
    }
  }
  /* if its exempt, we don't give a monkeys where it is... */
  if (newitem->exempt) {
    newitem->next = *head;
    *head = newitem;
    return 0;
  }
  
  for(position=*head;position;lastitem=position,position=position->next) {
    if (!ircd_strcmp(position->name->content, newitem->name->content)) {
      tfree(newitem);
      return 0;
    }
    if (!location && (position->size < newitem->size)) {
      if (!lastitem) {
        location = *head;
      } else {
        location = lastitem;
      }
    }
  }
  if (!location) {
    newitem->next = NULL;
    lastitem->next = newitem;
  } else {
    newitem->next = location->next;
    location->next = newitem;
  }
  if(newitem->exempt) {
    return 0;
  } else {
    return 1;
  }
}

void trojanscan_watch_clone_update(struct trojanscan_prechannels *hp, int count) {
  int i, j, marked;
  struct trojanscan_prechannels *lp;
  struct trojanscan_templist *markedlist = NULL;

  if(count > 0) {
    markedlist = (struct trojanscan_templist *)tmalloc(count * sizeof(struct trojanscan_templist));
    if (!markedlist)
      return;
    memset(markedlist, 0, sizeof(struct trojanscan_templist) * count);
  }
  
  for(i=0;i<trojanscan_activechans;i++) {
    marked = 0;    
    if(markedlist) {
      for(lp=hp,j=0;j<count&&lp;j++,lp=lp->next) {
        if(!markedlist[j].active && !lp->exempt && !ircd_strcmp(lp->name->content, trojanscan_chans[i].channel->content)) { /* we're already on the channel */
          if(trojanscan_chans[i].watch_clone) {
            markedlist[j].active = 1;
            markedlist[j].watch_clone = trojanscan_chans[i].watch_clone;
            lp->watch_clone = trojanscan_chans[i].watch_clone;
          }
          marked = 1;
          break;
        }
      }
    }
    if(!marked && trojanscan_chans[i].watch_clone) {
      channel *cp = findchannel(trojanscan_chans[i].channel->content);
      if(cp)
        localpartchannel(trojanscan_chans[i].watch_clone->clone, cp, NULL);
    }
  }
  
  if(!markedlist)
    return;

  for(j=0,lp=hp;j<count&&lp;j++,lp=lp->next) {
    if((!markedlist[j].active || !markedlist[j].watch_clone) && !lp->exempt) {
      channel *cp = findchannel(lp->name->content);
      if(cp) {
        int attempts = 10;
        do {
          lp->watch_clone = trojanscan_selectclone(TROJANSCAN_WATCH_CLONES);      
          if(!lp->watch_clone)
            break;
          if(!trojanscan_nickbanned(lp->watch_clone, cp)) {
            if(localjoinchannel(lp->watch_clone->clone, cp))
              lp->watch_clone = NULL;
            break;
          }
        } while(--attempts > 0);
        if(!attempts)
          lp->watch_clone = NULL;

      }
    }
  }
  
  tfree(markedlist);
}

void trojanscan_fill_channels(void *arg) {
  struct trojanscan_prechannels *head = NULL, *lp, *last = NULL;
  int i, count, tempctime = 0;
  
  chanindex *chn;
  
  for (count=i=0;i<trojanscan_database.total_channels;i++) {
    lp = (trojanscan_prechannels *)tmalloc(sizeof(trojanscan_prechannels));
    lp->name = trojanscan_database.channels[i].name;
    lp->size = 65535;
    lp->exempt = trojanscan_database.channels[i].exempt;
    lp->watch_clone = NULL;
    if (trojanscan_add_ll(&head, lp))
      count++;
  }
  
  for (i=0;i<CHANNELHASHSIZE;i++) {
    for(chn=chantable[i];chn;chn=chn->next) {
      if (chn->channel && !IsKey(chn->channel) && !IsInviteOnly(chn->channel) && !IsRegOnly(chn->channel) && (chn->channel->users->totalusers >= trojanscan_minchansize)) {
        lp = (trojanscan_prechannels *)tmalloc(sizeof(trojanscan_prechannels));
        lp->name = chn->name;
        lp->size = chn->channel->users->totalusers;
        lp->exempt = 0;
        lp->watch_clone = NULL;
        if (trojanscan_add_ll(&head, lp))
          count++;
      }
    }
  }
  
  count = TROJANSCAN_MMIN(count, trojanscan_maxchans);
  
  trojanscan_watch_clone_update(head, count);
  
  trojanscan_free_channels();
  trojanscan_chans = (struct trojanscan_inchannel *)tmalloc(count * sizeof(struct trojanscan_inchannel));
  memset(trojanscan_chans, 0, count * sizeof(struct trojanscan_inchannel));
  trojanscan_activechans = count;
  i = 0;
    
  for(lp=head;lp;last=lp,lp=lp->next) {
    if (!(lp->exempt) && (i < count)) {
      trojanscan_chans[i].channel = getsstring(lp->name->content, lp->name->length);
      trojanscan_chans[i++].watch_clone = lp->watch_clone;
    }
    if (last)
      tfree(last);
  }

  if (last)
    tfree(last);

  if (trojanscan_activechans > 0) {
    tempctime = trojanscan_cycletime / trojanscan_activechans;
  } else {
    tempctime = 60;
    trojanscan_mainchanmsg("d: just escaped a divide by zero error (no activechans!), rescheduling in 60 seconds");
  }
  
  if(trojanscan_schedule)
    deleteschedule(trojanscan_schedule, &trojanscan_dojoin, NULL);
    
  trojanscan_channumber = 0;
    
  trojanscan_schedule = schedulerecurring(time(NULL) + tempctime, trojanscan_activechans + 1, tempctime, trojanscan_dojoin, NULL);
  
}

void trojanscan_dojoin(void *arg) {
  struct trojanscan_realchannels *rc;
  if (trojanscan_channumber >= trojanscan_activechans) {
    trojanscan_schedule = NULL;
    trojanscan_fill_channels(NULL);
  } else {
    if ((rc = trojanscan_allocaterc(trojanscan_chans[trojanscan_channumber++].channel->content)))
      trojanscan_join(rc);
  }
}


void trojanscan_dopart(void *arg) {
  struct trojanscan_realchannels *rc = (struct trojanscan_realchannels *)arg, *rp, *past = NULL;

  if (rc->kickedout) { /* there's a join scheduled, wait for it (reschedule) */
    rc->schedule = scheduleoneshot(time(NULL)+5, &trojanscan_dopart, (void *)rc);
    return;
  }
  
  if (rc->clone->clone && (!(rc->donotpart)))
    localpartchannel(rc->clone->clone, rc->chan, NULL);

  rc->clone->sitting--;

  for(rp=trojanscan_realchanlist;rp;rp=rp->next) {
    if (rp == rc) {
      if (!past) {
        trojanscan_realchanlist = rp->next;
      } else {
        past->next = rp->next;
      }
      tfree(rp);
      break;
    }
    past = rp;
  }

}

void trojanscan_donickchange(void *arg) { /* just incase I choose to make this schedule at some point */
  struct trojanscan_clones *clone = (trojanscan_clones *)arg;
  if (clone && clone->clone) {
    char c_nick[NICKLEN+1];
    int loops = 0;
    /* PPA: unlikely to be infinite */
    do {
      if ((loops++ < 10) && trojanscan_hostmode) {
        trojanscan_generatenick(c_nick, NICKLEN);
      } else {
        trojanscan_gennick(c_nick, trojanscan_minmaxrand(7, TROJANSCAN_MMIN(13, NICKLEN)));
      }
    } while (c_nick[0] && (getnickbynick(c_nick) != NULL));

    renamelocaluser(clone->clone, c_nick);
  }
  
}

int trojanscan_keysort(const void *v1, const void *v2) {
  return ((*(trojanscan_prechannels **)v2)->size - (*(trojanscan_prechannels **)v1)->size);
}

int trojanscan_showcommands(void *sender, int cargc, char **cargv) {
  nick *np = (nick *)sender;
  Command *cmdlist[100];
  int i, n;
  char level = 0;
   
  n = getcommandlist(trojanscan_cmds, cmdlist, 100);
  
  trojanscan_reply(np, "The following commands are registered at present:");
  
  for(i=0;i<n;i++) {
    if (cmdlist[i]->level & TROJANSCAN_ACL_STAFF) {
      level = 's';
    } else if (cmdlist[i]->level & TROJANSCAN_ACL_DEVELOPER) {
      level = 'd';
    } else if (cmdlist[i]->level & TROJANSCAN_ACL_TEAMLEADER) {
      level = 't';
    } else if (cmdlist[i]->level & TROJANSCAN_ACL_CAT) {
      level = 'c';
    } else if (cmdlist[i]->level & TROJANSCAN_ACL_WEBSITE) {
      level = 'w';
    } else if (cmdlist[i]->level & TROJANSCAN_ACL_UNAUTHED) {
      level = 0;
    }
    if (level) {
      trojanscan_reply(np, "%s (+%c)", cmdlist[i]->command->content, level);
    } else {
      trojanscan_reply(np, "%s", cmdlist[i]->command->content);
    }
  }
  trojanscan_reply(np, "End of list.");
  return CMD_OK;
}
  
void trojanscan_handlemessages(nick *target, int messagetype, void **args) {
  Command *cmd;
  char *cargv[50];
  int cargc, templevel;
  nick *sender;
  union trojanscan_userlevel level;
  
  switch(messagetype) {
    case LU_PRIVMSG:
    case LU_SECUREMSG:
      /* If it's a message, first arg is nick and second is message */
      sender = (nick *)args[0];

      if(strncmp(TROJANSCAN_VERSION_DETECT, args[1], sizeof(TROJANSCAN_VERSION_DETECT)-1)==0) {
        char p = ((char *)args[1])[sizeof(TROJANSCAN_VERSION_DETECT)-1];
        if((p == ' ') || (p == '\0') || (p == 1)) {
          trojanscan_reply(sender, "\001VERSION Trojanscan (or Tigger) Newserv module version %s by Chris Porter (slug), Newserv by David Mansell (splidge). Compiled " __DATE__ " " __TIME__ ".\001", TROJANSCAN_VERSION);
          return;
        }
      }

      /* Split the line into params */
      cargc = splitline((char *)args[1], cargv, 50, 0);
      if(cargc == 0 || !cargv[0])
        return;

      cmd=findcommandintree(trojanscan_cmds, cargv[0], 1);
      if (!cmd) {
        trojanscan_reply(sender, "Unknown command.");
        return;
      }
      
      if ((cmd->level & TROJANSCAN_ACL_OPER) && !IsOper(sender)) {
        trojanscan_reply(sender, "You need to be opered to use this command.");
        return;
      }

      /* bit grim code... */
      
      if (!(cmd->level & TROJANSCAN_ACL_UNAUTHED)) {
        if (!IsAccount(sender)) {
          trojanscan_reply(sender, "You must be authed with the network to access this command!");
          return;
        }
        templevel = trojanscan_user_level_by_authname(sender->authname);
        
        if (templevel == -1) {
          trojanscan_reply(sender, "You do not have an account.");
          return;
        }
        
        level.number = templevel;

        if ((cmd->level & TROJANSCAN_ACL_DEVELOPER) && !TrojanscanIsDeveloper(level)) {
          trojanscan_reply(sender, "Access denied.");
          return;
        }
        if ((cmd->level & TROJANSCAN_ACL_TEAMLEADER) && !TrojanscanIsLeastTeamLeader(level)) {
          trojanscan_reply(sender, "Access denied.");
          return;
        }
        if ((cmd->level & TROJANSCAN_ACL_STAFF) && !TrojanscanIsLeastStaff(level)) {
          trojanscan_reply(sender, "Access denied.");
          return;
        }
        if ((cmd->level & TROJANSCAN_ACL_CAT) && !TrojanscanIsCat(level)) {
          trojanscan_reply(sender, "Access denied.");
          return;
        }
        if ((cmd->level & TROJANSCAN_ACL_WEBSITE) && !TrojanscanIsLeastWebsite(level)) {
          trojanscan_reply(sender, "Access denied.");
          return;
        }
      }
      
      /* Check the maxargs */
      if (cmd->maxparams<(cargc-1)) {
        /* We need to do some rejoining */
        rejoinline(cargv[cmd->maxparams], cargc-(cmd->maxparams));
        cargc = (cmd->maxparams) + 1;
      }
      
      (cmd->handler)((void *)sender, cargc - 1, &(cargv[1]));
      break;
      
    case LU_KILLED:
      /* someone killed me?  Bastards */
      trojanscan_connect_nick_schedule = scheduleoneshot(time(NULL) + 1, &trojanscan_connect_nick, NULL);
      trojanscan_nick = NULL;
      break;
      
    default:
      break;
  }
}

static char trojanscan_getmtfromhooktype(int input) {
  switch(input) {
    case HOOK_CHANNEL_PART: return 'P';
    default:                return '?';
  }
}

char trojanscan_getmtfrommessagetype(int input) {
  switch(input) {
    case LU_PRIVMSG:    return 'm';
    case LU_PRIVNOTICE: return 'n';
    case LU_SECUREMSG:  return 's';
    case LU_CHANMSG:    return 'M';
    case LU_CHANNOTICE: return 'N';
    default:            return '?';
  }
}

static void trojanscan_process(nick *sender, channel *cp, char mt, char *pretext) {
  char text[513];
  unsigned int len;
  unsigned int i;
  struct trojanscan_worms *worm;
  int vector[30], detected = 0;

  trojanscan_strip_codes(text, sizeof(text) - 1, pretext);
      
  len = strlen(text);
      
  for(i=0;i<trojanscan_database.total_phrases;i++) {
    if (
         (
           (worm = trojanscan_database.phrases[i].worm)
         ) &&
         (
           (
             (
               (mt == 'm') || (mt == 's') || (mt == 'n')
             ) &&
             (
               (trojanscan_database.phrases[i].worm->hitpriv)
             )
           ) ||
           (
             (
               (mt == 'M') || (mt == 'N') || (mt == 'P')
             ) &&
             (
               (trojanscan_database.phrases[i].worm->hitchans)
             )
           )
         ) &&
         (trojanscan_database.phrases[i].phrase)
       ) {
      int pre = pcre_exec(trojanscan_database.phrases[i].phrase, trojanscan_database.phrases[i].hint, text, len, 0, 0, vector, 30);
      if(pre >= 0) {
        char matchbuf[513];
        matchbuf[0] = 0;
        matchbuf[512] = 0; /* hmm */
   
        if(pre > 1)
          if(pcre_copy_substring(text, vector, pre, 1, matchbuf, sizeof(matchbuf) - 1) <= 0)
            matchbuf[0] = 0;
           
        trojanscan_phrasematch(cp, sender, &trojanscan_database.phrases[i], mt, matchbuf);

        detected = 1;
        break;
      }
    }
  }
  if (!detected && (mt != 'N') && (mt != 'M')) {
    char etext[TROJANSCAN_QUERY_TEMP_BUF_SIZE], enick[TROJANSCAN_QUERY_TEMP_BUF_SIZE], eident[TROJANSCAN_QUERY_TEMP_BUF_SIZE], ehost[TROJANSCAN_QUERY_TEMP_BUF_SIZE];
    trojanscan_database_escape_string(etext, text, len);
    trojanscan_database_escape_string(enick, sender->nick, strlen(sender->nick));
    trojanscan_database_escape_string(eident, sender->ident, strlen(sender->ident));
    trojanscan_database_escape_string(ehost, sender->host->name->content, sender->host->name->length);
    trojanscan_database_query("INSERT INTO unknownlog (data, user) VALUES ('%s','%s!%s@%s')", etext, enick, eident, ehost);
  }
}

void trojanscan_clonehandlemessages(nick *target, int messagetype, void **args) {
  char *pretext = NULL;
  nick *sender;
  struct trojanscan_realchannels *rp;
  struct trojanscan_rejoinlist *rj;
  char mt = trojanscan_getmtfrommessagetype(messagetype);
  char *channel_name;
  channel *cp = NULL;
  int i;

  switch(messagetype) {
    case LU_PRIVMSG:
    case LU_SECUREMSG:
    case LU_PRIVNOTICE:
    
    pretext = (char *)args[1];    
    
    case LU_CHANMSG:
    case LU_CHANNOTICE:
      sender = (nick *)args[0];

      if (strlen(sender->nick) < 2)
        break;
      
      if (!pretext) {
        pretext = (char *)args[2];  
        cp = args[1];
      }

      if(strncmp(TROJANSCAN_VERSION_DETECT, pretext, sizeof(TROJANSCAN_VERSION_DETECT)-1)==0) {
        char p = pretext[sizeof(TROJANSCAN_VERSION_DETECT)-1];
        if((p == ' ') || (p == '\0') || (p == 1)) {
          int staff = 0;
          if (IsOper(sender)) {
            staff = 1;
          } else {
            if (IsAccount(sender)) {
              int templevel = trojanscan_user_level_by_authname(sender->authname);
              if (templevel != -1) {
                union trojanscan_userlevel level;
                level.number = templevel;
                if (TrojanscanIsLeastStaff(level))
                  staff = 1;
              }
            }
          }
          if (staff) {
            if(trojanscan_nick) {
              sendnoticetouser(target, sender, "\001VERSION T clone, check T for confirmation.\001");
              sendnoticetouser(trojanscan_nick, sender, "\001VERSION %s is part of my swarm.\001", target->nick);
            } else {
              sendnoticetouser(target, sender, "\001VERSION T clone, though since T is currently gone you'll have to version me again in a minute for confirmation.\001");
            }
          } else {
            sendnoticetouser(target, sender,  "\001VERSION %s\001", versionreply);
          }
        
          return;
        }
      }
      
      trojanscan_process(sender, cp, mt, pretext);
      break;         
    case LU_KILLED:
      /* someone killed me?  Bastards */

      /* PPA: we do NOT rejoin channels at this moment in time, it is possible to do this though */
      for (i=0;i<TROJANSCAN_CLONE_TOTAL;i++) {
        if (trojanscan_swarm[i].clone == target) {
          
          scheduleoneshot(time(NULL)+1, &trojanscan_generateclone, (void *)((long)i));
          if(i >= TROJANSCAN_CLONE_MAX) {
            int j;
            for(j=0;j<trojanscan_activechans;j++)
              if(trojanscan_chans[j].watch_clone == &trojanscan_swarm[i])
                trojanscan_chans[j].watch_clone = NULL;
          } else {
            for(rp=trojanscan_realchanlist;rp;rp=rp->next)
              if ((rp->clone == &(trojanscan_swarm[i])))
                rp->donotpart = 1;
          }
          derefnode(iptree, trojanscan_swarm[i].fakeipnode);
          trojanscan_swarm[i].clone = NULL;
          trojanscan_swarm[i].remaining = 0; /* bah */
          break;
        }
      }
      break;

    case LU_KICKED:
      channel_name = ((channel *)args[1])->index->name->content;
      for (i=0;i<trojanscan_activechans;i++) {
        if (!trojanscan_chans[i].watch_clone)
          continue;
        if ((trojanscan_chans[i].watch_clone->clone == target) && (!strcmp(trojanscan_chans[i].channel->content, channel_name)))
          break;
      }
      if(i != trojanscan_activechans) {
        int j;
        for(j=0;j<TROJANSCAN_CLONE_TOTAL;j++) {
          if(&trojanscan_swarm[j] == trojanscan_chans[i].watch_clone) {
            trojanscan_chans[i].watch_clone = NULL;
            break;
          }
        }
      } else {
      /*
        trojanscan_mainchanmsg("k: %s on %s by %s", target->nick, ((channel *)args[1])->index->name->content, (((nick *)args[0])->nick)?(((nick *)args[0])->nick):"(server)");
      */
        rj = (struct trojanscan_rejoinlist *)tmalloc(sizeof(struct trojanscan_rejoinlist));
        if (rj) {
          rj->rp = NULL;
          for(rp=trojanscan_realchanlist;rp;rp=rp->next)
               if ((rp->clone->clone == target) && (rp->chan == args[1])) {
                rp->kickedout++;
                rj->rp = rp;
                break;
              }
            if(!rj->rp) {
              tfree(rj);
              return;
            }

            rj->channel = getsstring(((channel *)args[1])->index->name->content, ((channel *)args[1])->index->name->length);
            if(!rj->channel) {
              trojanscan_mainchanmsg("d: unable to allocate memory for channel: %s upon rejoin", ((channel *)args[1])->index->name->content);
              tfree(rj);
              return;
            }

            rj->clone = rp->clone;
            rj->next = trojanscan_schedulerejoins;
            trojanscan_schedulerejoins = rj;

            rj->schedule = scheduleoneshot(time(NULL)+1, &trojanscan_rejoin_channel, (void *)rj);
          }
        }
        break;
    default:
      break;
  }
}

static void trojanscan_part_watch(int hook, void *arg) {
  void **arglist = (void **)arg;
  channel *cp = (channel *)arglist[0];
  nick *np = arglist[1];
  char *reason = arglist[2];

  if(!cp || !np || !reason || (*reason == '\0'))
    return;

  trojanscan_process(np, cp, trojanscan_getmtfromhooktype(hook), reason);
}

static int trojanscan_hostcount(nick *sender, int hostmode, char *mask, int masklen) {
  int usercount = 0, j;
  nick *np = NULL; /* sigh at warnings */

  if(hostmode)
    for (j=0;j<NICKHASHSIZE;j++)
      for (np=nicktable[j];np;np=np->next)
        if (np->ipnode==sender->ipnode)
          usercount++;

  if(usercount > TROJANSCAN_MAX_HOST_GLINE) {
    hostmode = 0;
    usercount = 0;
  }

  if(!hostmode)
    for (j=0;j<NICKHASHSIZE;j++)
      for (np=nicktable[j];np;np=np->next)
        if (np->ipnode==sender->ipnode && !ircd_strcmp(np->ident, sender->ident))
          usercount++;

  if(mask)
    snprintf(mask, masklen, "%s@%s", hostmode?"*":sender->ident, IPtostr(sender->p_ipaddr));

  return usercount;
}

void trojanscan_phrasematch(channel *chp, nick *sender, trojanscan_phrases *phrase, char messagetype, char *matchbuf) {
  char glinemask[HOSTLEN + USERLEN + NICKLEN + 4], enick[TROJANSCAN_QUERY_TEMP_BUF_SIZE], eident[TROJANSCAN_QUERY_TEMP_BUF_SIZE], ehost[TROJANSCAN_QUERY_TEMP_BUF_SIZE];
  unsigned int frequency;
  int glining = 0, usercount;
  struct trojanscan_worms *worm = phrase->worm;

  trojanscan_database.detections++;
  
  usercount = 0;
  if (worm->monitor) {
    usercount = -1;
  } else if(worm->glinehost || worm->glineuser) {
    glining = 1;

    usercount = trojanscan_hostcount(sender, worm->glinehost, glinemask, sizeof(glinemask));
  }
  
  if (!usercount) {
    trojanscan_mainchanmsg("w: user %s!%s@%s triggered infection monitor, yet no hosts found at stage 2 -- worm: %s", sender->nick, sender->ident, sender->host->name->content, worm->name->content);
    return;
  }
   
  if (glining && (usercount > trojanscan_maxusers)) {
    trojanscan_mainchanmsg("w: not glining %s!%s@%s due to too many users (%d) with mask: *!%s -- worm: %s)", sender->nick, sender->ident, sender->host->name->content, usercount, glinemask, worm->name->content);
    return;
  }

  if (glining && !worm->datalen) {
    trojanscan_mainchanmsg("w: not glining %s!%s@%s due to too lack of removal data with mask: *!%s (%d users) -- worm: %s)", sender->nick, sender->ident, sender->host->name->content, glinemask, usercount, worm->name->content);
    return;
  }
    
  trojanscan_database_escape_string(enick, sender->nick, strlen(sender->nick));
  trojanscan_database_escape_string(eident, sender->ident, strlen(sender->ident));
  trojanscan_database_escape_string(ehost, sender->host->name->content, sender->host->name->length);
  
  frequency = 1;
  
  if (!(trojanscan_database_query("SELECT COUNT(*) FROM hits WHERE glined = %d AND host = '%s'", glining, ehost))) {
    trojanscan_database_res *res;
    if ((res = trojanscan_database_store_result(&trojanscan_sql))) {
      trojanscan_database_row sqlrow;
      if ((trojanscan_database_num_rows(res) > 0) && (sqlrow = trojanscan_database_fetch_row(res)))
        frequency = atoi(sqlrow[0]) + 1;
      trojanscan_database_free_result(res);
    }
  } 

  if (!glining) {
    trojanscan_mainchanmsg("m: t: %c u: %s!%s@%s%s%s w: %s p: %d %s%s", messagetype, sender->nick, sender->ident, sender->host->name->content, messagetype=='N'||messagetype=='M'||messagetype=='P'?" #: ":"", messagetype=='N'||messagetype=='M'||messagetype=='P'?chp->index->name->content:"", worm->name->content, phrase->id, matchbuf[0]?" --: ":"", matchbuf[0]?matchbuf:"");
#ifdef TROJANSCAN_PEONCHANNEL
    trojanscan_peonchanmsg("m: t: %c u: %s!%s@%s%s%s%s w: %s %s%s", messagetype, sender->nick, sender->ident, (IsHideHost(sender)&&IsAccount(sender))?sender->authname:sender->host->name->content, (IsHideHost(sender)&&IsAccount(sender))?"."HIS_HIDDENHOST:"", messagetype=='N'||messagetype=='M'||messagetype=='P'?" #: ":"", messagetype=='N'||messagetype=='M'||messagetype=='P'?chp->index->name->content:"", worm->name->content, matchbuf[0]?" --: ":"", matchbuf[0]?matchbuf:"");
#endif
  } else {
    int glinetime = TROJANSCAN_FIRST_OFFENSE * frequency * (worm->epidemic?TROJANSCAN_EPIDEMIC_MULTIPLIER:1);
    if(glinetime > 7 * 24)
      glinetime = 7 * 24; /* can't set glines over 7 days with normal non U:lined glines */

    trojanscan_database_query("INSERT INTO hits (nickname, ident, host, phrase, messagetype, glined) VALUES ('%s', '%s', '%s', %d, '%c', %d)", enick, eident, ehost, phrase->id, messagetype, glining);
    trojanscan_database.glines++;
    
    irc_send("%s GL * +%s %d %zu :You (%s!%s@%s) are infected with a trojan (%s/%d), see %s%d for details - banned for %d hours\r\n", mynumeric->content, glinemask, glinetime * 3600, time(NULL), sender->nick, sender->ident, sender->host->name->content, worm->name->content, phrase->id, TROJANSCAN_URL_PREFIX, worm->id, glinetime);

    trojanscan_mainchanmsg("g: *!%s t: %c u: %s!%s@%s%s%s c: %d w: %s%s p: %d f: %d%s%s", glinemask, messagetype, sender->nick, sender->ident, sender->host->name->content, messagetype=='N'||messagetype=='M'||messagetype=='P'?" #: ":"", messagetype=='N'||messagetype=='M'||messagetype=='P'?chp->index->name->content:"", usercount, worm->name->content, worm->epidemic?"(E)":"", phrase->id, frequency, matchbuf[0]?" --: ":"", matchbuf[0]?matchbuf:"");
  }
}
            
void trojanscan_rejoin_channel(void *arg) {
  struct trojanscan_rejoinlist *rj2, *lrj, *rj = (struct trojanscan_rejoinlist *)arg;
  
  channel *cp = findchannel(rj->channel->content);
  freesstring(rj->channel);

  if (rj->rp) {
    rj->rp->kickedout--;
    if (!cp) {
      rj->rp->donotpart = 1; /* we were the last user on the channel, so we need to be VERY careful freeing it */
    } else {
      if(!rj->rp->donotpart && !rj->rp->kickedout) { /* check we're allowed to join channels (not killed), and we're the last one to join */
        if (trojanscan_nickbanned(rj->clone, cp)) {
          rj->rp->donotpart = 1;
        } else {
          localjoinchannel(rj->clone->clone, cp);
        }
      }
    }
  }
    
  rj2 = trojanscan_schedulerejoins;
  lrj = NULL;

  if (rj2 == rj) {
    trojanscan_schedulerejoins = rj->next;
    tfree(rj);
  } else {
    for(rj2=trojanscan_schedulerejoins;rj2;lrj=rj2,rj2=rj2->next) {
      if (rj2 == rj) {
        lrj->next = rj2->next;
        tfree(rj);
        break;
      }
    }
  }
 
}

void trojanscan_reply(nick *target, char *message, ... ) {
  char buf[513];
  va_list va;
    
  if (!trojanscan_nick)
    return;
  
  va_start(va, message);
  vsnprintf(buf, sizeof(buf) - 1, message, va);
  va_end(va);
  
  sendnoticetouser(trojanscan_nick, target, "%s", buf);
}


void trojanscan_mainchanmsg(char *message, ...) {
  char buf[513];
  va_list va;
  channel *cp;
  
  if (!trojanscan_nick)
    return;
  if (!(cp = findchannel(TROJANSCAN_CHANNEL)))
    return;
    
  va_start(va, message);
  vsnprintf(buf, sizeof(buf) - 1, message, va);
  va_end(va);
  
  sendmessagetochannel(trojanscan_nick, cp, "%s", buf);
}

#ifdef TROJANSCAN_PEONCHANNEL
void trojanscan_peonchanmsg(char *message, ...) {
  char buf[513];
  va_list va;
  channel *cp;
  
  if (!trojanscan_nick)
    return;
  if (!(cp = findchannel(TROJANSCAN_PEONCHANNEL)))
    return;
    
  va_start(va, message);
  vsnprintf(buf, sizeof(buf) - 1, message, va);
  va_end(va);
  
  sendmessagetochannel(trojanscan_nick, cp, "%s", buf);
}
#endif

int trojanscan_minmaxrand(float min, float max) {
  return (int)((max-min+1)*rand()/(RAND_MAX+min))+min;
}

char *trojanscan_iptostr(char *buf, int buflen, unsigned int ip) {
  snprintf(buf, buflen, "%d.%d.%d.%d", ip >> 24, (ip >> 16) & 255, (ip >> 8) & 255, ip & 255);
  return buf;
}

char trojanscan_genchar(int ty) {
  /* hostname and realname characters*/
  if (!ty) {
    if (!(trojanscan_minmaxrand(0, 40) % 10)) {
      return trojanscan_minmaxrand(48, 57);
    } else {
      return trojanscan_minmaxrand(97, 122);
    }
  /* ident characters - without numbers*/
  } else if (ty == 1) {
    return trojanscan_minmaxrand(97, 122);
  /* ident characters - with numbers*/
  } else if (ty == 2) {
    ty = trojanscan_minmaxrand(97, 125);
    if (ty > 122) return trojanscan_minmaxrand(48, 57);
    return ty;
  /* nick characters - with and without numbers*/
  } else if (ty == 3 || ty == 4) {
    if (!(trojanscan_minmaxrand(0, 59) % 16)) {
      char weirdos[6] = { '\\', '|', '[', '{', ']', '}' };
      return weirdos[trojanscan_minmaxrand(0, 5)];
    }
    if (ty == 4) {
      ty = trojanscan_minmaxrand(65, 93);
      if (ty > 90) return trojanscan_minmaxrand(48, 57);
    } else {
      ty = trojanscan_minmaxrand(65, 90);
    }
    if (!(trojanscan_minmaxrand(0, 40) % 8)) return ty;
    return ty + 32;
  /* moron check */
  } else {
    return ' ';
  }
}

void trojanscan_gennick(char *ptc, char size) {
  int i;
  for (i=0;i<size;i++) {
    if (i == 0) {
      ptc[i] = trojanscan_genchar(3);
    } else {
      ptc[i] = trojanscan_genchar(4);
    }
  }
  ptc[i] = '\0';
}

void trojanscan_genident(char *ptc, char size) {
  int i;
  for (i=0;i<size;i++) {
    if (i == 0) {
      ptc[i] = trojanscan_genchar(1);
    } else {
      ptc[i] = trojanscan_genchar(2);
    }
  }
  ptc[i] = '\0';
}

void trojanscan_genhost(char *ptc, char size, patricia_node_t **fakeipnode) {
  int dots = trojanscan_minmaxrand(2, 5), i, dotexist = 0, cur;
  struct irc_in_addr ipaddress;

  while (!dotexist) {
    for (i=0;i<size;i++) {
      ptc[i] = trojanscan_genchar(0);
      if ((i > 5) && (i < (size-4))) {
        if ((ptc[i-1] != '.') && (ptc[i-1] != '-')) {
          cur = trojanscan_minmaxrand(1,size / dots);
          if (cur < 3) {
            if (cur == 1) {
              ptc[i] = '.';
              dotexist = 1;
            } else {
              ptc[i] = '-';
            }
          }
        }
      }
    }
  }
  ptc[i] = '\0';

  memset(&ipaddress, 0, sizeof(ipaddress));
  ((unsigned short *)(ipaddress.in6_16))[5] = 65535;
  ((unsigned short *)(ipaddress.in6_16))[6] = trojanscan_minmaxrand(0, 65535);
  ((unsigned short *)(ipaddress.in6_16))[7] = trojanscan_minmaxrand(0, 65535);

  *fakeipnode = refnode(iptree, &ipaddress, PATRICIA_MAXBITS);
}

void trojanscan_genreal(char *ptc, char size) {
  int spaces = trojanscan_minmaxrand(2, 4), i;
  for (i=0;i<size;i++) {
    ptc[i] = trojanscan_genchar(0);
    if ((i > 5) && (i < (size-4))) {
      if (ptc[i-1] != ' ') {
        if (trojanscan_minmaxrand(1,size / spaces) == 1) ptc[i] = ' ';
      }
    }
  }
  ptc[i] = '\0';
}

int trojanscan_is_not_octet(char *begin, int length) {
  int i;
  if(length > 3)
    return 0;
  for(i=0;i<length;i++) {
    if (!((*begin >= '0') && (*begin <= '9')))
      return 0;
    begin++;
  }
  return 1;
}

int trojanscan_generatepool(void) {
  int i, k = 0, j = 0, loops = 0;
  char *p, *pp;
  nick *np;
  
  for (i=0;i<NICKHASHSIZE;i++)
    for (np=nicktable[i];np;np=np->next)
      j++;
  
  if(j < trojanscan_min_hosts)
    return 0;
  
  if(TROJANSCAN_HOST_MODE == TROJANSCAN_STEAL_HOST)
    return TROJANSCAN_MINPOOLSIZE;
    
  i = 0;
  do {
    for (j=trojanscan_minmaxrand(0, NICKHASHSIZE-1);j<NICKHASHSIZE;j++) {
      if (nicktable[j]) {
        for(p=nicktable[j]->host->name->content, pp=p;*p;) {
          if (*++p == '.') {
            if (!trojanscan_is_not_octet(pp, p-pp)) {
              if (i < TROJANSCAN_POOLSIZE) {
                if (i < trojanscan_hostpoolsize)
                  freesstring(trojanscan_hostpool[i]);
                trojanscan_hostpool[i] = getsstring(pp, p-pp);
                i++;
              } else {
                if (k >= TROJANSCAN_POOLSIZE)
                 break;
              }
            }
            pp=++p;
          }
        }
        if (!trojanscan_is_not_octet(pp, p-pp)) {
          if (k < TROJANSCAN_POOLSIZE) {
            if (k < trojanscan_tailpoolsize)
              freesstring(trojanscan_tailpool[k]);
            trojanscan_tailpool[k] = getsstring(pp, p-pp);
            k++;
          } else {
            if (i >= TROJANSCAN_POOLSIZE)
              break;
          }
        }
      }
    }
    loops++;
  } while ((loops < 5) && ((i < TROJANSCAN_POOLSIZE) || (k < TROJANSCAN_POOLSIZE)));

  trojanscan_hostpoolsize = i;
  trojanscan_tailpoolsize = k;
  return i;
}

nick *trojanscan_selectuser(void) {
  int target = trojanscan_minmaxrand(0, 500), loops = 150, j;
  nick *np;
  do {
    for (j=trojanscan_minmaxrand(0, NICKHASHSIZE-1);j<NICKHASHSIZE;j++)
      for(np=nicktable[j];np;np=np->next)
        if (!--target)
          return np;
  } while(--loops > 0);
  return NULL;
}

host *trojanscan_selecthost(void) {
  int target = trojanscan_minmaxrand(0, 500), loops = 150, j;
  host *hp;
  do {
    for (j=trojanscan_minmaxrand(0, HOSTHASHSIZE-1);j<HOSTHASHSIZE;j++)
      for(hp=hosttable[j];hp;hp=hp->next)
        if (!--target)
          return hp;
  } while(--loops > 0);

  return NULL;
}

void trojanscan_generatehost(char *buf, int maxsize, patricia_node_t **fakeip) {
  struct irc_in_addr ipaddress;

  if(TROJANSCAN_HOST_MODE == TROJANSCAN_STEAL_HOST) {
    host *hp;
    int loops = 20;

    buf[0] = '\0';

    do {
      hp = trojanscan_selecthost();
      if(hp && (hp->clonecount <= TROJANSCAN_MAX_CLONE_COUNT) && !trojanscan_isip(hp->name->content)) {
        strlcpy(buf, hp->name->content, maxsize + 1);
        if(hp->nicks) {
          *fakeip = hp->nicks->ipnode;
	  patricia_ref_prefix(hp->nicks->ipnode->prefix);
        } else {
          memset(&ipaddress, 0, sizeof(ipaddress));
	  ((unsigned short *)(ipaddress.in6_16))[5] = 65535; 
          ((unsigned short *)(ipaddress.in6_16))[6] = trojanscan_minmaxrand(0, 65535);
          ((unsigned short *)(ipaddress.in6_16))[7] = trojanscan_minmaxrand(0, 65535);

          *fakeip = refnode(iptree, &ipaddress, PATRICIA_MAXBITS);
        }
        break;
      }
    } while(--loops > 0);
  } else {
    char *cpos;
    int pieces = trojanscan_minmaxrand(2, 4), totallen = 0, a = 0, i;
    int *choices = tmalloc(sizeof(int) * (pieces + 1));
    int *lengths = tmalloc(sizeof(int) * (pieces + 1));
  
    choices[pieces] = trojanscan_minmaxrand(0, trojanscan_tailpoolsize-1);
    lengths[pieces] = strlen(trojanscan_tailpool[choices[pieces]]->content) + 1;
    totallen += lengths[pieces];

    for (i=0;i<pieces;i++) {
      choices[i] = trojanscan_minmaxrand(0, trojanscan_hostpoolsize-1);
      lengths[i] = strlen(trojanscan_hostpool[choices[i]]->content) + 1;
      if (totallen+lengths[i] > maxsize) {
        choices[i] = choices[pieces];
        lengths[i] = lengths[pieces];
        pieces-=(pieces-i);
        break;
      }
      totallen += lengths[i];
    }

    for (i=0;i<pieces;i++) {
      for (cpos=trojanscan_hostpool[choices[i]]->content; *cpos;)
        buf[a++] = *cpos++;
      buf[a++] = '.';
    }

    for (cpos=trojanscan_tailpool[choices[i]]->content; *cpos;) {
      buf[a++] = *cpos++;
    }

    buf[a] = '\0';
    tfree(choices);
    tfree(lengths);

    memset(&ipaddress, 0, sizeof(ipaddress));
    ((unsigned short *)(ipaddress.in6_16))[5] = 65535;
    ((unsigned short *)(ipaddress.in6_16))[6] = trojanscan_minmaxrand(0, 65535);
    ((unsigned short *)(ipaddress.in6_16))[7] = trojanscan_minmaxrand(0, 65535);

    *fakeip = refnode(iptree, &ipaddress, PATRICIA_MAXBITS);
  }
}

void trojanscan_generatenick(char *buf, int maxsize) {
  int bits = trojanscan_minmaxrand(2, 3), loops = 0, wanttocopy, len = 0, i, d = 0, newmaxsize = maxsize - trojanscan_minmaxrand(0, 7);
  nick *np;

  if(newmaxsize > 2)
    maxsize = newmaxsize;

  do {
    np = trojanscan_selectuser();
    if(np) {
      wanttocopy = trojanscan_minmaxrand(1, (strlen(np->nick) / 2) + 3);
      for(i=0;((i<wanttocopy) && (len<maxsize));i++)
        buf[len++] = np->nick[i];
      if(++d > bits) {
        buf[len] = '\0';
        return;
      }
    }
  } while (++loops < 10);
  buf[0] = '\0';
}

void trojanscan_generateident(char *buf, int maxsize) {
  nick *np = trojanscan_selectuser();
  buf[0] = '\0';
  if(np)
    strlcpy(buf, np->ident, maxsize + 1);
}

void trojanscan_generaterealname(char *buf, int maxsize) {
  nick *np = trojanscan_selectuser();
  buf[0] = '\0';
  if(np)
    strlcpy(buf, np->realname->name->content, maxsize + 1);
}

void trojanscan_database_close(void) {
  mysql_close(&trojanscan_sql);
}

int trojanscan_database_connect(char *dbhost, char *dbuser, char *dbpass, char *db, unsigned int port) {
  mysql_init(&trojanscan_sql);
  if (!mysql_real_connect(&trojanscan_sql, dbhost, dbuser, dbpass, db, port, NULL, 0))
    return -1;
  return 0;
}

void trojanscan_database_escape_string(char *dest, char *source, size_t length) {
  mysql_escape_string(dest, source, length);
}

int trojanscan_database_query(char *format, ...) {
  char trojanscan_sqlquery[TROJANSCAN_QUERY_BUF_SIZE];
  va_list va;
  
  va_start(va, format);
  vsnprintf(trojanscan_sqlquery, sizeof(trojanscan_sqlquery) - 1, format, va);
  va_end(va);
  return mysql_query(&trojanscan_sql, trojanscan_sqlquery);
}

int trojanscan_database_num_rows(trojanscan_database_res *res) {
  return mysql_num_rows(res);
}

trojanscan_database_res *trojanscan_database_store_result() {
  return mysql_store_result(&trojanscan_sql);
}

trojanscan_database_row trojanscan_database_fetch_row(trojanscan_database_res *res) {
  return mysql_fetch_row(res);
}

void trojanscan_database_free_result(trojanscan_database_res *res) {
  mysql_free_result(res);
}

int trojanscan_isip(char *host) {
  char *p = host, components = 0, length = 0;

  for(;*p;p++) {
    if(*p == '.') {
      if(((!length) || (length = 0)) || (++components > 3))
        return 0;
    } else {
      if ((++length > 3) || !isdigit(*p))
        return 0;
    }
  }
  return components == 3;
}

