#include "../nick/nick.h"
#include "../core/error.h"
#include "../lib/irc_string.h"
#include "../core/schedule.h"
#include <stdlib.h>

#include "trusts.h"

int trustdb_loaded = 0;

int trusts_load_db(void) {
  if(!dbconnected()) {
    Error("trusts", ERR_STOP, "Could not connect to database.");
    return 0;
  }

  if(trustdb_loaded)
    trusts_cleanup_db();

  trustdb_loaded = 1;

  trusts_create_tables();

  dbasyncquery(trusts_loadtrustgroups, NULL,
    "SELECT trustid,maxusage,maxclones,maxperident,maxperip,enforceident,startdate,lastused,expires,owneruserid,type,created,modified FROM trusts.groups WHERE enddate = 0");
  dbasyncquery(trusts_loadtrusthosts, NULL,
    "SELECT * FROM trusts.hosts WHERE enddate = 0");
  dbasyncquery(trusts_loadtrustblocks, NULL,
    "SELECT * FROM trusts.blocks");

  return 1;
}

void trusts_create_tables(void) {
  dbattach("trusts");
  dbcreatequery(
    "CREATE TABLE trusts.groups ("
    "trustid      INT4 NOT NULL PRIMARY KEY,"
    "startdate    INT4 NOT NULL,"
    "enddate      INT4 NOT NULL,"
    "owneruserid  INT4,"
    "maxusage     INT4 NOT NULL,"
    "enforceident INT2 NOT NULL,"
    "type         INT2,"
    "maxclones    INT4 NOT NULL,"
    "maxperident  INT4,"
    "maxperip     INT4,"
    "expires      INT4,"
    "lastused     INT4,"
    "modified     INT4,"
    "created      INT4"
    ") WITHOUT OIDS;"
    );

  dbcreatequery(
    "CREATE TABLE trusts.hosts ("
    "hostid     INT4 NOT NULL PRIMARY KEY,"
    "trustid    INT4 NOT NULL,"
    "startdate  INT4 NOT NULL,"
    "enddate    INT4,"
    "host       INET NOT NULL,"
    "maxusage   INT4 NOT NULL,"
    "lastused   INT4,"
    "expires    INT4 NOT NULL,"
    "modified   INT4,"
    "created    INT4"
    ") WITHOUT OIDS;"
    );

  dbcreatequery(
    "CREATE TABLE trusts.blocks ("
    "blockid        INT4 NOT NULL PRIMARY KEY,"
    "block          INET NOT NULL,"
    "owner          INT4,"
    "expires        INT4,"
    "startdate      INT4,"
    "reason_private VARCHAR,"
    "reason_public  VARCHAR"
    ") WITHOUT OIDS;"
    );

  dbcreatequery(
    "CREATE TABLE trusts.log ("
    "logid          SERIAL NOT NULL PRIMARY KEY,"
    "trustid        INT4 NOT NULL,"
    "timestamp      INT4 NOT NULL,"
    "userid         INT4 NOT NULL,"
    "type           INT2,"
    "message        VARCHAR"
    ") WITHOUT OIDS;"
  );
}

void trusts_cleanup_db(void) {
  dbdetach("trusts");
}

void trusts_loadtrustgroups(DBConn *dbconn, void *arg) {
  DBResult *pgres = dbgetresult(dbconn);
  int rows=0;
  trustgroup_t *t;

  if(!dbquerysuccessful(pgres)) {
    Error("trusts", ERR_ERROR, "Error loading trustgroup list.");
    dbclear(pgres);
    return;
  }

  trusts_lasttrustgroupid = 1; 

  while(dbfetchrow(pgres)) {

    t = createtrustgroupfromdb( 
                     /*id*/          strtoul(dbgetvalue(pgres,0),NULL,10),
                     /*maxusage*/    strtoul(dbgetvalue(pgres,1),NULL,10),
                     /*maxclones*/   strtoul(dbgetvalue(pgres,2),NULL,10),
                     /*maxperident*/ strtoul(dbgetvalue(pgres,3),NULL,10),
                     /*maxperip*/    strtoul(dbgetvalue(pgres,4),NULL,10),
        /*TODOTYPE*/ /*enforceident*/strtoul(dbgetvalue(pgres,5),NULL,10),
                     /*startdate*/   strtoul(dbgetvalue(pgres,6),NULL,10),
                     /*lastused*/    strtoul(dbgetvalue(pgres,7),NULL,10),
                     /*expire*/      strtoul(dbgetvalue(pgres,8),NULL,10),
                     /*ownerid*/     strtoul(dbgetvalue(pgres,9),NULL,10),
                     /*type*/        strtoul(dbgetvalue(pgres,10),NULL,10),
                     /*created*/     strtoul(dbgetvalue(pgres,11),NULL,10),
                     /*modified*/    strtoul(dbgetvalue(pgres,12),NULL,10)
                     );
    if(t->id > trusts_lasttrustgroupid)
      trusts_lasttrustgroupid = t->id;

    rows++;
  }

  Error("trusts",ERR_INFO,"Loaded %d trusts (highest ID was %lu)",rows,trusts_lasttrustgroupid);

  dbclear(pgres);
}

void trusts_loadtrusthosts(DBConn *dbconn, void *arg) {
  DBResult *pgres = dbgetresult(dbconn);
  int rows=0;
  trusthost_t *t;
  trustgroup_t *tg;
  patricia_node_t *node;
  struct irc_in_addr sin;
  unsigned char bits;

  if(!dbquerysuccessful(pgres)) {
    Error("trusts", ERR_ERROR, "Error loading trusthost list.");
    dbclear(pgres);
    return;
  }

  trusts_lasttrusthostid = 1;

  while(dbfetchrow(pgres)) {
    /*node*/
    ipmask_parse(dbgetvalue(pgres,4), &sin, &bits);
    node  = refnode(iptree, &sin, bits);

    /*tg*/
    int tgid = strtoul(dbgetvalue(pgres,1),NULL,10);
    tg=findtrustgroupbyid(tgid);
    if (!tg) { 
      Error("trusts", ERR_ERROR, "Error loading trusthosts - invalid group: %d.", tgid);
      continue;
    }

    t = createtrusthostfromdb( 
                    /*id*/        strtoul(dbgetvalue(pgres,0),NULL,10),
                    /*node*/      node,
                    /*startdate*/ strtoul(dbgetvalue(pgres,2),NULL,10),
                    /*lastused*/  strtoul(dbgetvalue(pgres,6),NULL,10),
                    /*expire*/    strtoul(dbgetvalue(pgres,7),NULL,10),
                    /*maxusage*/  strtoul(dbgetvalue(pgres,5),NULL,10),
                    /*trustgroup*/ tg,
                    /*created*/   strtoul(dbgetvalue(pgres,9),NULL,10),
                    /*modified*/ strtoul(dbgetvalue(pgres,8),NULL,10)
                  );
    node = 0;
    if(t->id > trusts_lasttrusthostid)
      trusts_lasttrusthostid = t->id;

    trusthost_addcounters(t);
    rows++;
  }

  Error("trusts",ERR_INFO,"Loaded %d trusthosts (highest ID was %lu)",rows,trusts_lasttrusthostid);

  dbclear(pgres);
}

void trusts_loadtrustblocks(DBConn *dbconn, void *arg) {
  DBResult *pgres = dbgetresult(dbconn);
  int rows=0;
  trustblock_t *t;

  if(!dbquerysuccessful(pgres)) {
    Error("trusts", ERR_ERROR, "Error loading trustblock list.");
    dbclear(pgres);
    return;
  }

  trusts_lasttrustblockid = 1;

  while(dbfetchrow(pgres)) {
//    t = gettrustblock();
    if(!t)
      Error("trusts", ERR_ERROR, "Unable to load trust block");

    t->id = strtoul(dbgetvalue(pgres,0),NULL,10);
    /* host */
    t->ownerid = strtoul(dbgetvalue(pgres,3),NULL,10);
    t->expire = strtoul(dbgetvalue(pgres,11),NULL,10);
    t->startdate = strtoul(dbgetvalue(pgres,2),NULL,10);
    /* reason_private */
    /* reason_public */

    if(t->id > trusts_lasttrustblockid)
      trusts_lasttrustblockid = t->id;

    //trusts_addtrustgrouptohash(t);
    rows++;
  }

  Error("trusts",ERR_INFO,"Loaded %d trustblocks (highest ID was %lu)",rows,trusts_lasttrustblockid);

  dbclear(pgres);
}


/* trust group */
void trustsdb_addtrustgroup(trustgroup_t *t) {
  dbquery("INSERT INTO trusts.groups (trustid,startdate,enddate,owneruserid,maxusage,enforceident,type,maxclones,maxperident,maxperip,expires,lastused,modified,created ) VALUES (%lu,%lu,0,%lu,%lu,%d,%d,%lu,%lu,%d,%lu,%lu,%lu,%lu )", t->id,t->startdate,t->ownerid,t->maxusage,t->enforceident,t->type,t->maxclones,t->maxperident,t->maxperip, t->expire, t->lastused, t->modified, t->created ); 
}

void trustsdb_updatetrustgroup(trustgroup_t *t) {
  dbquery("UPDATE trusts.groups SET startdate=%lu,owneruserid=%lu,maxusage=%lu,enforceident=%d,type=%d,maxclones=%lu, maxperident=%lu,maxperip=%d,expires=%lu,lastused=%lu,modified=%lu,created=%lu WHERE trustid = %lu", t->startdate, t->ownerid,t->maxusage,t->enforceident,t->type,t->maxclones,t->maxperident,t->maxperip, t->expire, t->lastused, t->modified, t->created, t->id);
}

void trustsdb_deletetrustgroup(trustgroup_t *t) {
  dbquery("UPDATE trusts.groups SET enddate = %jd WHERE trustid = %lu", (intmax_t)getnettime(), t->id);
}

/* trust host */
void trustsdb_addtrusthost(trusthost_t *th) {
  dbquery("INSERT INTO trusts.hosts (hostid,trustid,startdate,enddate,host,maxusage,lastused,expires,modified,created) VALUES (%lu,%lu,%lu,0,'%s/%d',%lu,%lu,%lu,%lu,%lu)", th->id, th->trustgroup->id, th->startdate, IPtostr(th->node->prefix->sin),irc_bitlen(&(th->node->prefix->sin),th->node->prefix->bitlen), th->maxused, th->lastused, th->expire, th->modified, th->created);
}

void trustsdb_updatetrusthost(trusthost_t *th) {
  dbquery("UPDATE trusts.hosts SET hostid=%lu,trustid=%lu,startdate=%lu,host='%s/%d',maxusage=%lu,lastused=%lu,expires=%lu,modified=%lu,created=%lu", th->id, th->trustgroup->id, th->startdate, IPtostr(th->node->prefix->sin), irc_bitlen(&(th->node->prefix->sin),th->node->prefix->bitlen), th->maxused, th->lastused, th->expire, th->modified, th->created);
}

void trustsdb_deletetrusthost(trusthost_t *th) {
  dbquery("UPDATE trusts.hosts SET enddate = %jd WHERE hostid = %lu", (intmax_t)getnettime(), th->id);
}

/* trust block */
void trustsdb_addtrustblock(trustblock_t *tb) {
  dbquery("INSERT INTO trusts.blocks ( blockid,block,owner,expires,startdate,reason_private,reason_public) VALUES (%lu,'%s/%d',%lu,%lu,%lu,'%s','%s')",tb->id, IPtostr(tb->node->prefix->sin), irc_bitlen(&(tb->node->prefix->sin),tb->node->prefix->bitlen), tb->ownerid, tb->expire, tb->startdate, tb->reason_private->content, tb->reason_public->content);
}

void trustsdb_updatetrustblock(trustblock_t *tb) {
  dbquery("UPDATE trusts.blocks SET blockid=%lu,block='%s/%d',owner=%lu,expires=%lu,startdate=%lu,reason_private='%s',reason_public='%s'", tb->id, IPtostr(tb->node->prefix->sin), irc_bitlen(&(tb->node->prefix->sin),tb->node->prefix->bitlen), tb->ownerid, tb->expire, tb->startdate, tb->reason_private->content, tb->reason_public->content);
}

void trustsdb_deletetrustblock(trustblock_t *tb) {
  dbquery("DELETE from trusts.blocks WHERE blockid = %lu", tb->id);
}

/* trust log */
/* logid, trustid, timestamp, userid, type, message */
/* @@@ TODO */

void trustsdb_logmessage(trustgroup_t *tg, unsigned long userid, int type, char *message) {
  /* maximum length of a trustlog message is ircd max length */
  char escmessage[2*512+1];
  
  dbescapestring(escmessage,message, strlen(message)); 
  dbquery("INSERT INTO trusts.log (trustid, timestamp, userid, type, message) VALUES ( %lu, %lu, %lu, %d, '%s')", tg->id, getnettime(), userid, type, escmessage);
}
