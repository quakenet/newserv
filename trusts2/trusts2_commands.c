#include "../core/schedule.h"
#include "../lib/irc_string.h"
#include "../localuser/localuserchannel.h"
#include "../control/control.h"
#include "trusts.h"
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "../lib/version.h"

MODULE_VERSION("");

static int commandsregistered;

void _init(void) {
  registerhook(HOOK_TRUSTS_DBLOADED, trusts_cmdinit);

  /* Now that the database is in a separate module it might be loaded already. */
  if (trusts_loaded)
    trusts_cmdinit(HOOK_TRUSTS_DBLOADED, NULL);

}

void _fini(void) {
  deregisterhook(HOOK_TRUSTS_DBLOADED, trusts_cmdinit);
  trusts_cmdfini(0, NULL);
}

void trusts_cmdinit(int hooknum, void *arg) {
  if(commandsregistered)
    return;
  registercontrolcmd("trustgroupadd",10,7,trust_groupadd);
  registercontrolcmd("trustgroupmodify",10,4,trust_groupmodify);
  registercontrolcmd("trustgroupdel",10,2,trust_groupdel);

  registercontrolcmd("trustcomment",10,2,trust_comment);

  registercontrolcmd("trustadd",10,3,trust_add);
  registercontrolcmd("trustdel",10,2,trust_del);


  registercontrolcmd("truststats",10,2,trust_stats);
  registercontrolcmd("trustdump",10,2,trust_dump);

  registercontrolcmd("trustlog", 10,2, trust_dotrustlog);

  commandsregistered = 1;
  removeusers = 0;
}

void trusts_cmdfini() {
  if(!commandsregistered)
    return;

  deregistercontrolcmd("trustgroupadd",trust_groupadd);
  deregistercontrolcmd("trustgroupmodify",trust_groupmodify);
  deregistercontrolcmd("trustgroupdel",trust_groupdel);

  deregistercontrolcmd("trustcomment",trust_comment);

  deregistercontrolcmd("trustadd",trust_add);
  deregistercontrolcmd("trustdel",trust_del);


  deregistercontrolcmd("truststats",trust_stats);
  deregistercontrolcmd("trustdump",trust_dump);

  deregistercontrolcmd("trustlog", trust_dotrustlog);

  commandsregistered = 0;
  removeusers = 0;
}

/*TODO*/
/* tgh - should this have a 'maxclones limit'? */

int trust_groupadd(void *source, int cargc, char **cargv) {
  nick *sender=(nick *)source;
  int expiry;
  unsigned long maxclones;
  unsigned short maxperip;
  unsigned long  maxperident;
  int enforceident;
  unsigned long ownerid;
  trustgroup_t *t;

  if (cargc < 6) {
    controlreply(sender,"Usage: trustgroupadd howmany howlong maxperident maxperip enforceident ownerid");
    return CMD_ERROR;
  }

  maxclones = strtoul(cargv[0],NULL,10);
  if ( maxclones > 10000 ) { 
    /* we allow 0 for unlimited trusts, and only warn on this */
    controlreply(sender, "WARNING: large maximum number of clients - %lu", maxclones);
  }
  expiry = durationtolong(cargv[1]);
  if (expiry > (365 * 86400) ) {
    controlreply(sender,"ERROR: Invalid duration given - temporary trusts must be less than 1 year");
    return CMD_ERROR;
  }
  ownerid  = strtoul(cargv[5],NULL,10);
  maxperip = strtoul(cargv[3],NULL,10);
  if (maxperip > 500) {
    controlreply(sender, "ERROR: MaxPerIP value should be less then 500 (if set)");
    return CMD_ERROR;
  }
  maxperident = strtoul(cargv[2],NULL,10);
  if (maxperident > 50) {
    controlreply(sender, "ERROR: MaxPerIdent value should be less then 50 (if set)");
    return CMD_ERROR;
  }
  if (((cargv[4][0]!='0') && (cargv[4][0]!='1')) || (cargv[4][1]!='\0')) {
    controlreply(sender,"ERROR: enforceident is a boolean setting, that means it can only be 0 or 1");
    return CMD_ERROR;
  }
  enforceident = cargv[4][0] == '1';

  if ( findtrustgroupbyownerid(ownerid) ) {
    controlreply(sender, "ERROR: Q User ID %d already has a trustgroup", ownerid);
    return CMD_ERROR;
  }
  if (ownerid > 2147483646 ) {
    controlreply(sender, "ERROR: Invalid Q User ID: %d", ownerid);
    return CMD_ERROR;
  }

  t = createtrustgroup( ++trusts_lasttrustgroupid, maxclones, maxperident, maxperip,  enforceident, ownerid);

  if(!t) {
    controlreply(sender,"ERROR: An error occured adding trustgroup");
    return CMD_ERROR;
  }

  trustsdb_addtrustgroup(t);

  controlreply(sender,"Adding trustgroup with ID %lu", t->id);
  controlreply(sender,"Connections: %d, Enforceident %d, Per ident: %d, Per IP %d",maxclones,enforceident,maxperident,maxperip);
  controlreply(sender,"Expires: %d, User ID: %d", expiry, ownerid);
  controlwall(NO_OPER, NL_TRUSTS, "NewTrust: ID: %lu, Connections: %d, Enforceident %d, Per ident: %d, Per IP %d, Owner %d", t->id,maxclones,enforceident,maxperident,maxperip, ownerid);
  return CMD_OK;
}

int trust_del(void *source, int cargc, char **cargv) {
  nick *sender=(nick *)source;
  struct irc_in_addr sin;
  unsigned char bits;
  patricia_node_t *node;
  trustgroup_t *tg;

  if (cargc<1) {
    controlreply(sender,"Syntax: trustdel IP[/mask]");
    return CMD_OK;
  }

  if (ipmask_parse(cargv[0], &sin, &bits) == 0) {
    controlreply(sender, "ERROR: Invalid mask.");
    return CMD_ERROR;
  }

  if (!is_normalized_ipmask(&sin,bits)) {
    controlreply(sender, "ERROR: non-normalized mask.");
    return CMD_ERROR;
  }

  node = refnode(iptree, &sin, bits);
  if(!node->exts[tgh_ext]) {
    controlreply(sender,"ERROR: That CIDR was not trusted.");
    return CMD_ERROR;
  } else {
    /*TODO: only allow a host to be removed if <X users? subnets? bah */
    tg = ((trusthost_t *)node->exts[tgh_ext])->trustgroup;
    controlreply(sender,"%s removed from trustgroup #%lu",cargv[0],tg->id);
    controlwall(NO_OPER, NL_TRUSTS, "%s removed from trustgroup #%lu",cargv[0],tg->id);
    trustsdb_deletetrusthost(node->exts[tgh_ext]);
    trusthost_free(node->exts[tgh_ext]);
    node->exts[tgh_ext] = NULL;

    trusthost_t* thptr;
    int hash = trusts_gettrusthostgroupidhash(tg->id);
    for (thptr = trusthostgroupidtable[hash]; thptr; thptr = thptr->nextbygroupid ) {
      if ( thptr->trustgroup->id == tg->id ) {
        return CMD_OK;
      }
    }
    controlreply(sender,"Empty trustgroup #%lu Removed",tg->id);
    controlwall(NO_OPER, NL_TRUSTS, "Empty trustgroup #%lu removed",tg->id);
    trustgroup_free( tg );

  }
  return CMD_OK;
}

int trust_add(void *source, int cargc, char **cargv) {
  nick *sender=(nick *)source;
  trustgroup_t *tg;
  struct irc_in_addr sin;
  unsigned char bits;
  patricia_node_t *node, *inode, *parent;
  int expiry = 0;
  trusthost_t *th;

  if (cargc<2) {
    controlreply(sender,"Syntax: trustadd <#groupid> IP[/mask] <duration>");
    return CMD_OK;
  }

  if(cargv[0][0]== '#'){
    /* find group by id */
    tg=findtrustgroupbyid(strtol(&cargv[0][1],NULL,10));
  } else {
    /* find group by id */
    tg=findtrustgroupbyid(strtol(cargv[0],NULL,10));
  }

  if(tg == NULL) {
    controlreply(sender,"ERROR: A trustgroup with that ID does not exist.");
    return CMD_ERROR;
  }

  if (tg->id==0) {
    controlreply(sender,"INTERNAL ERROR: Trustgroup has ID 0");
    return CMD_ERROR;
  }

  if (ipmask_parse(cargv[1], &sin, &bits) == 0) {
    controlreply(sender, "ERROR: Invalid mask.");
    return CMD_ERROR;
  }

  if (!is_normalized_ipmask(&sin,bits)) {
    controlreply(sender, "ERROR: non-normalized mask.");
    return CMD_ERROR;
  }

  if ( irc_in_addr_is_ipv4(&sin) ) {
    if (bits>128 || bits<112) {
      controlreply(sender,"ERROR: Not a valid netmask (needs to be between 16 and 32)");
      return CMD_ERROR;
    }
  } else {
    if ( bits>64) {
      controlreply(sender,"ERROR: Not a valid ipv6 netmask ");
      return CMD_ERROR;
    }
  }

  if (cargc == 3) {
    expiry = getnettime() + durationtolong(cargv[2]);
    if (expiry<1) {
      controlreply(sender,"ERROR: Invalid duration given");
      return CMD_ERROR;
    }
  }

  node  = refnode(iptree, &sin, bits);
  if(node->exts[tgh_ext]) {
    /* this mask is already trusted */
    controlreply(sender,"ERROR: This mask is already trusted by trustgroup %lu.", ((trusthost_t *)node->exts[tgh_ext])->trustgroup->id);
    return CMD_ERROR;
  }

  /* check child status */
  PATRICIA_WALK(node, inode)
  {
    th = inode->exts[tgh_ext];
    if (th) {
      /* we have a child trustgroup */
      /* Criteria 1: we can't add two hosts into the same group */
      if (th->trustgroup == tg) {
        controlreply(sender,"ERROR: A child subnet is already in this trustgroup, remove that subnet first (%s/%d)", IPtostr(inode->prefix->sin),irc_bitlen(&(inode->prefix->sin),inode->prefix->bitlen));
        return CMD_ERROR;
      } 
    }
  }
  PATRICIA_WALK_END;

  /* check parents too */
  parent = node->parent;
  while (parent) {
    if( parent->exts[tgh_ext]) {
      th = parent->exts[tgh_ext];
      /* we have a parent trustgroup */
      /* Criteria 1: we can't add two hosts into the same group */
      if (th->trustgroup == tg) {
        controlreply(sender,"ERROR: A parent subnet is already in this trustgroup (%s/%d)", IPtostr(parent->prefix->sin),irc_bitlen(&(parent->prefix->sin),parent->prefix->bitlen));
        return CMD_ERROR;
      }
      /* even if we find 1 parent, we continue to the top */
    }
    parent = parent->parent;
  }

  th = trusthostadd(node, tg, expiry );
  if ( !th ) {
    controlreply(sender,"ERROR: Unable to add trusted host");
    return CMD_ERROR;
  }
 
  trustsdb_addtrusthost(th);
  controlreply(sender,"Added %s to trustgroup #%lu",cargv[1],tg->id);
  controlwall(NO_OPER, NL_TRUSTS, "Added %s to trustgroup #%lu",cargv[1],tg->id);
  return CMD_OK;
}

int trust_dump(void *source, int cargc, char **cargv) {
  nick *sender=(nick *)source;

  char tmps3[512];
  trustgroup_t *g;
  unsigned long startid=0;
  long num=0, count=0, lines=0;

  if (cargc<2) {
    controlreply(sender, "Syntax: trustdump <start #id> <number>");
    controlreply(sender, "Dumps <number> trustgroups starting from <start #id>.");
    controlreply(sender, "This allows to dump very large numbers of groups,");
    controlreply(sender, "so use with care.");
    return CMD_ERROR;
  }
  strncpy(tmps3,cargv[0],20);
  tmps3[20]='\0';
  num = atoi(cargv[1]);

  if (tmps3[0] != '#') {
    controlreply(sender, "First parameter has to be a trust ID (prefixed with #).");
    return CMD_ERROR;
  }

  startid=strtoul(&tmps3[1], NULL, 10);
  if (num < 1) {
    controlreply(sender, "Cannot return fewer than 1 group.");
    return CMD_ERROR;
  }
  if (num >= 500) {
    controlreply(sender, "Will not list more than 500 groups in one go.");
    return CMD_ERROR;
  }

  if (startid > trusts_lasttrustgroupid) {
    controlreply(sender, "Start ID cannot exceed maximum group ID (#%ld).", trusts_lasttrustgroupid);
    return CMD_ERROR;
  }

  do {
    g=findtrustgroupbyid(startid);
    startid++;
  } while ((g == NULL) && (startid <= (trusts_lasttrustgroupid+1)));
  if (g == NULL) {
    controlreply(sender, "Failed to find nearest start group.");
    return CMD_ERROR;
  }

  while (startid <= (trusts_lasttrustgroupid+1)) {
    if (g == NULL) {
      g=findtrustgroupbyid(startid);
      startid++;
      continue;
    }
    controlreply(sender, "G,#%lu,%lu,%lu,%d,%lu,%lu,%lu",
      g->id, g->currenton, g->maxclones, g->enforceident, g->maxperident,
      g->maxusage, g->lastused);
    lines++;

    trusthost_t* thptr;

    int hash = trusts_gettrusthostgroupidhash(g->id);
    for (thptr = trusthostgroupidtable[hash]; thptr; thptr = thptr->nextbygroupid ) {
      if ( thptr->trustgroup->id == g->id ) {
        /* TODO: expire here - trusthost_free(thptr);*/
        controlreply(sender, "H,#%lu,%s/%d,%lu,%lu,%lu", g->id,
                              IPtostr(((patricia_node_t *)thptr->node)->prefix->sin),
                              irc_bitlen(&(((patricia_node_t *)thptr->node)->prefix->sin),((patricia_node_t *)thptr->node)->prefix->bitlen),
                              0 /*a->currentlyon*/,
                              0 /*a->maxusage*/,
                              0 /* a->lastused*/);
        lines++;
      }
    }

    count++;
    if (count >= num) {
      break;
    }
    g=findtrustgroupbyid(startid);
    startid++;
  }
  controlreply(sender, "End of list, %ld groups and %ld lines returned.", count, lines);
  return CMD_OK;
}

int trust_groupmodify(void *source, int cargc, char **cargv) {
  nick *sender=(nick *)source;
  unsigned long oldvalue, newvalue;
  char *mod;
  trustgroup_t *tg;

  if (cargc<3 || cargc==4) {
    controlreply(sender,"Syntax: trustgroupmodify <#groupid> <what> [+|-|=]number");
    controlreply(sender,"        +20 means add 20, =20 replaces current value, -20 means subtract");
    controlreply(sender,"        what: maxclones, maxperident, maxperip, enforceident, ownerid");
    return CMD_OK;
  }

  if(cargv[0][0]== '#'){
    /* find group by id */
    tg=findtrustgroupbyid(strtol(&cargv[0][1],NULL,10));
  } else {
    /* find group by id */
    tg=findtrustgroupbyid(strtol(cargv[0],NULL,10));
  }

  if(tg == NULL) {
    controlreply(sender,"ERROR: A trustgroup with that ID does not exist.");
    return CMD_ERROR;
  }

  if (tg->id==0) {
    controlreply(sender,"INTERNAL ERROR: Trustgroup has ID 0");
    return CMD_ERROR;
  }

  switch ( cargv[2][0] ) {
    case '+':
    case '-':
    case '=':
      mod = cargv[2];
      break;
    default:
      controlreply(sender,"ERROR: invalid modifier specified (values values are +,-,=)");
      return CMD_ERROR;
  }
  newvalue = strtoul(&cargv[2][1],NULL,10);
  
  if (ircd_strcmp(cargv[1], "maxclones")==0) {
    oldvalue = tg->maxclones;
    switch (*mod) {
      case '+':
        newvalue = oldvalue + newvalue; 
        break;
      case '-':
        if (newvalue > oldvalue) {
          controlreply(sender, "ERROR: maxclones cannot be less than 0");
          return CMD_ERROR; 
        }
        newvalue = oldvalue - newvalue;
        if (newvalue == 0) {
          controlreply(sender, "ERROR: maxclones limit would be 0 - unlimited maxclones can only be set with '='");
          return CMD_ERROR;
        }
        break;
    }

    if (newvalue > 1000000) {
      controlreply(sender, "ERROR: large maximum number of clients - %lu", newvalue);
      return CMD_ERROR;
    }
    if (newvalue > 10000) {
      controlreply(sender, "WARNING: large maximum number of clients - %lu", newvalue);
    }

    tg->maxclones = newvalue;
  } else if (ircd_strcmp(cargv[1], "maxperident")==0) {
    oldvalue = tg->maxperident;
    switch (*mod) {
      case '+':
        newvalue = oldvalue + newvalue;
        break;
      case '-':
        if (newvalue > oldvalue) {
          controlreply(sender, "ERROR: maxperident cannot be less than 0");
          return CMD_ERROR;
        }
        newvalue = oldvalue - newvalue;
        if (newvalue == 0) {
          controlreply(sender, "ERROR: maxperident limit would be 0 - unlimited maxclones can only be set with '='");
          return CMD_ERROR;
        }
        break;
    }

    if (newvalue > 50) {
      controlreply(sender, "ERROR: MaxPerIdent value should be less then 50 (if set)");
      return CMD_ERROR;
    }
    tg->maxperident=newvalue;  
  } else if (ircd_strcmp(cargv[1], "maxperip")==0) {
    oldvalue = tg->maxperip;
    switch (*mod) {
      case '+':
        newvalue = oldvalue + newvalue;
        break;
      case '-':
        if (newvalue > oldvalue) {
          controlreply(sender, "ERROR: maxperip cannot be less than 0");
          return CMD_ERROR;
        }
        newvalue = oldvalue - newvalue;
        if (newvalue == 0) {
          controlreply(sender, "ERROR: maxperip limit would be 0 - unlimited maxclones can only be set with '='");
          return CMD_ERROR;
        }
        break;
    }

    if (newvalue > 500) {
      controlreply(sender, "ERROR: MaxPerIP value should be less then 500 (if set)");
      return CMD_ERROR;
    }
    tg->maxperip = newvalue;
  } else if (ircd_strcmp(cargv[1], "enforceident")==0) {
    oldvalue = tg->enforceident;
    if ( (newvalue != 0 && newvalue != 1) || *mod != '=' ) {
      controlreply(sender,"ERROR: enforceident is a boolean setting, that means it can only be 0 or 1, and can only be set by '='");
      return CMD_ERROR; 
    }
    tg->enforceident = newvalue;
  } else if (ircd_strcmp(cargv[1], "ownerid")==0) {
    oldvalue = tg->ownerid; 
    if ( *mod != '=' ) {
      controlreply(sender,"ERROR: Q user ID can only be set by '='");
      return CMD_ERROR;
    }
    if ( findtrustgroupbyownerid(newvalue) ) {
      controlreply(sender, "ERROR: Q User ID %d already has a trustgroup", newvalue);
      return CMD_ERROR;
    }

    if (newvalue > 2147483646 ) {
      controlreply(sender, "ERROR: Invalid Q User ID: %d", newvalue);
      return CMD_ERROR;
    }

    tg->ownerid = newvalue;
  }
  controlreply(sender, "Modification: %s changed to %lu from %lu for trustgroup %lu", cargv[1], newvalue, oldvalue, tg->id);
  controlwall(NO_OPER, NL_TRUSTS, "Modification: %s changed to %lu from %lu for trustgroup %lu", cargv[1], newvalue, oldvalue, tg->id);

  trustsdb_updatetrustgroup(tg);
  return CMD_OK;
}

int trust_groupdel(void *source, int cargc, char **cargv) {
  nick *sender=(nick *)source;
  trusthost_t *thptr, *nthptr;
  trustgroup_t *tg;
  patricia_node_t *node;

  if (cargc<1) {
    controlreply(sender,"Syntax: trustgroupdel <#id|id>");
    return CMD_OK;
  }

  if(cargv[0][0]== '#'){
    /* find group by id */
    tg=findtrustgroupbyid(strtol(&cargv[0][1],NULL,10));
  } else {
    /* find group by id */
    tg=findtrustgroupbyid(strtol(cargv[0],NULL,10));
  }

  if(tg == NULL) {
    controlreply(sender,"ERROR: A trustgroup with that ID does not exist.");
    return CMD_ERROR;
  }

  if (tg->id==0) {
    controlreply(sender,"INTERNAL ERROR: Trustgroup has ID 0");
    return CMD_ERROR;
  }

  /* we have a trustgroup to remove */
  int hash = trusts_gettrusthostgroupidhash(tg->id);
  for (thptr = trusthostgroupidtable[hash]; thptr; thptr = nthptr ) {
    nthptr = thptr->nextbygroupid;  
    if(thptr->trustgroup == tg) {
      node = thptr->node;
      controlwall(NO_OPER, NL_TRUSTS, "%s/%d removed from trustgroup #%lu",IPtostr(thptr->node->prefix->sin),irc_bitlen(&(thptr->node->prefix->sin),thptr->node->prefix->bitlen),tg->id);
      controlreply(sender,"%s/%d removed from trustgroup #%lu",IPtostr(thptr->node->prefix->sin),irc_bitlen(&(thptr->node->prefix->sin),thptr->node->prefix->bitlen),tg->id);
      trustsdb_deletetrusthost(thptr);
      trusthost_free(thptr);
      node->exts[tgh_ext] = NULL;
    }
  }
  controlwall(NO_OPER, NL_TRUSTS, "removed trustgroup #%lu",tg->id);
  controlreply(sender,"removed trustgroup #%lu",tg->id);
  trustsdb_deletetrustgroup(tg);
  trustgroup_free(tg);
  return CMD_OK;

}

int trust_stats(void *source, int cargc, char **cargv) {
  nick *sender=(nick *)source;
  trustgroup_t *tg; trusthost_t* thptr; int i;
  unsigned long thcount=0, ucount=0, mcount=0, tgcount=0;
  unsigned long hentries=0;
  unsigned long netcount4[33];
  unsigned long netucount4[33];
  unsigned long netmcount4[33];
  unsigned long netcount6[129];
  unsigned long netucount6[129];
  unsigned long netmcount6[129];

  int maxthmask4 = 32;
  int maxthmask6 = 128;

  for (i=0; i<33; i++) {
    netcount4[i]=0;
    netucount4[i]=0;
    netmcount4[i]=0;
  }

  for (i=0; i<129; i++) {
    netcount6[i]=0;
    netucount6[i]=0;
    netmcount6[i]=0;
  }

  for ( i = 0; i < TRUSTS_HASH_GROUPSIZE ; i++ ) {
    for ( tg = trustgroupidtable[i]; tg; tg = tg -> nextbyid ) {
      /*check active*/
      tgcount++;
    }
  }

  for ( i = 0; i < TRUSTS_HASH_HOSTSIZE ; i++ ) {
    for ( thptr = trusthostidtable[i]; thptr; thptr = thptr-> nextbyid ) {
      /*check active*/
      hentries++;
      thcount++;
      ucount+=thptr->node->usercount;
      mcount+=thptr->maxusage;
      if(irc_in_addr_is_ipv4(&((patricia_node_t *)thptr->node)->prefix->sin)) {
        netcount4[((patricia_node_t *)thptr->node)->prefix->bitlen-96]++;
        netucount4[((patricia_node_t *)thptr->node)->prefix->bitlen-96]+=thptr->node->usercount;
        netmcount4[((patricia_node_t *)thptr->node)->prefix->bitlen-96]+=thptr->maxusage;
        if( (((patricia_node_t *)thptr->node)->prefix->bitlen-96) < maxthmask4 ) {
          maxthmask4 = (((patricia_node_t *)thptr->node)->prefix->bitlen-96);
        }
      } else {
        controlreply(sender, "%s", IPtostr(((patricia_node_t *)thptr->node)->prefix->sin)); 
        netcount6[((patricia_node_t *)thptr->node)->prefix->bitlen]++;
        netucount6[((patricia_node_t *)thptr->node)->prefix->bitlen]+=thptr->node->usercount;
        netmcount6[((patricia_node_t *)thptr->node)->prefix->bitlen]+=thptr->maxusage;
        if( ((patricia_node_t *)thptr->node)->prefix->bitlen < maxthmask6 ) {
          maxthmask6 = ((patricia_node_t *)thptr->node)->prefix->bitlen;
        }
      }
    }
  }
  controlreply(sender, "Online trust users:   %lu", ucount);
  controlreply(sender, "Maximum online users: %lu", mcount);
  controlreply(sender, "Trust groups:         %lu", tgcount);
  controlreply(sender, "Maximum group ID:     #%lu", trusts_lasttrustgroupid);
  controlreply(sender, "Trusted hosts/nets:   %lu", thcount);
  controlreply(sender, "Largest subnet (v4):  /%d", maxthmask4);
  controlreply(sender, "Largest subnet (v6):  /%d", maxthmask6);
  controlreply(sender, "IPv4 Subnets:");
  for (i=0; i<32; i++) {
    if (netcount4[i]==0) continue;
    controlreply(sender, "|-*/%d (Netcount: %lu Cur: %lu Max: %lu)", i, netcount4[i], netucount4[i], netmcount4[i]);
  }
  controlreply(sender, "`-*/32 (Netcount: %lu Cur: %lu Max: %lu)", netcount4[32], netucount4[32], netmcount4[32]);
  controlreply(sender, "IPv6 Subnets:");
  for (i=0; i<128; i++) {
    if (netcount6[i]==0) continue;
    controlreply(sender, "|-*/%d (Netcount: %lu Cur: %lu Max: %lu)", i, netcount6[i], netucount6[i], netmcount6[i]);
  }
  controlreply(sender, "`-*/128 (Netcount: %lu Cur: %lu Max: %lu)", netcount6[128], netucount6[128], netmcount6[128]);

  return CMD_OK;
}


int trust_comment(void *source, int cargc, char **cargv) {
  nick *sender=(nick *)source;
  trustgroup_t *tg;

  if (cargc<2) {
    controlreply(sender,"Syntax: trustcomment <#groupid> <comment>");
    return CMD_OK;
  }

  if(cargv[0][0]== '#'){
    /* find group by id */
    tg=findtrustgroupbyid(strtol(&cargv[0][1],NULL,10));
  } else {
    /* find group by id */
    tg=findtrustgroupbyid(strtol(cargv[0],NULL,10));
  }

  if(tg == NULL) {
    controlreply(sender,"A trustgroup with that ID does not exist.");
    return CMD_ERROR;
  }

  if (tg->id==0) {
    controlreply(sender,"Internal error: Trustgroup has ID 0");
    return CMD_ERROR;
  }

  trustsdb_logmessage(tg, 0, 1, cargv[1]);

  controlreply(sender, "Comment: %s for trustgroup %lu", cargv[1], tg->id);
  controlwall(NO_OPER, NL_TRUSTS, "Comment: %s for trustgroup %lu", cargv[1], tg->id);

  return CMD_OK;
}

int trust_dotrustlog(void *source, int cargc, char **cargv) {
  nick *np=source;
  unsigned long interval;
  int trustid; 

  if (cargc < 1) {
    controlreply(np,"Syntax: trustlog <#groupid> [duration]");
    return CMD_ERROR;
  }

  if(cargv[0][0]== '#'){
    trustid = strtol(&cargv[0][1],NULL,10);
  } else {
    trustid = strtol(cargv[0],NULL,10);
  }

  if (cargc > 1)
    interval=getnettime() - durationtolong(cargv[1]);
  else
    interval=0;

  trustsdb_retrievetrustlog(np, trustid, interval);
  return CMD_OK;
}
