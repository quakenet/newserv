/* fsck.c - check some internal structures */

#include "../control/control.h"
#include "../nick/nick.h"
#include "../channel/channel.h"
#include "../server/server.h"
#include "../lib/version.h"

MODULE_VERSION("")

int dofsck(void *source, int cargc, char **cargv);

void _init() {
  registercontrolhelpcmd("fsck",NO_DEVELOPER,0,dofsck, "Usage: fsck\nRuns internal structure check.");
}

void _fini() {
  deregistercontrolcmd("fsck",dofsck);
}

int dofsck(void *source, int cargc, char **cargv) {
  int i,j;
  nick *sender=source, *np, *np2;
  host *hp;
  realname *rnp;
  unsigned int nickmarker;
  int errors=0;
  unsigned int nummask,mnum;

  /* Start off with strict nick consistency checks.. */

  controlreply(sender,"- Performing nickname checks"); 

  nickmarker=nextnickmarker();

  for(i=0;i<HOSTHASHSIZE;i++)
    for(hp=hosttable[i];hp;hp=hp->next)
      hp->marker=0;
  
  for(i=0;i<REALNAMEHASHSIZE;i++)
    for(rnp=realnametable[i];rnp;rnp=rnp->next)
      rnp->marker=0;

  controlreply(sender," - Scanning nick hash table");
  
  for (i=0;i<NICKHASHSIZE;i++) {
    for(np=nicktable[i];np;np=np->next) {
      if (np->marker==nickmarker) {
        controlreply(sender, "ERROR: bumped into the same nick %s/%s twice in hash table.",longtonumeric(np->numeric,5),np->nick);
        errors++;
      }
      
      /* Mark this nick so we can check we found them all */
      np->marker=nickmarker;

      /* Check that we can find this nick with the lookup functions */
      if (getnickbynick(np->nick) != np) {
	controlreply(sender, "ERROR: can't find %s/%s using getnickbynick().",
		     longtonumeric(np->numeric,5),np->nick);
	errors++;
      }

      if (getnickbynumeric(np->numeric) != np) {
	controlreply(sender, "ERROR: can't find %s/%s using getnickbynumeric().",
		     longtonumeric(np->numeric,5),np->nick);
	errors++;
      }

      /* Check that the nick appears in the list of nicks using its host */
      if (findhost(np->host->name->content) != np->host) {
	controlreply(sender, "ERROR: can't find %s/%s's host (%s) using findhost().",
		     longtonumeric(np->numeric,5),np->nick,np->host->name->content);
	errors++;
      }

      for(np2=np->host->nicks;np2;np2=np2->nextbyhost)
	if (np2==np)
	  break;

      if (!np2) {
	controlreply(sender, "ERROR: can't find %s/%s (host=%s) on the host users list.",
		     longtonumeric(np->numeric,5),np->nick,np->host->name->content);
	errors++;
      }

      np->host->marker++;

      /* Same for realnames */
      if (findrealname(np->realname->name->content) != np->realname) {
	controlreply(sender, "ERROR: can't find %s/%s's realname (%s) using findrealname().",
		     longtonumeric(np->numeric,5),np->nick,np->realname->name->content);
	errors++;
      }

      for(np2=np->realname->nicks;np2;np2=np2->nextbyrealname)
	if (np2==np)
	  break;

      if (!np2) {
	controlreply(sender, 
		     "ERROR: can't find %s/%s (realname=%s) on the realname users list.",
		     longtonumeric(np->numeric,5),np->nick,np->realname->name->content);
	errors++;
      }

      np->realname->marker++;

      if (IsAccount(np) && !np->authname[0]) {
	controlreply(sender, "ERROR: nick %s/%s is +r but no authname stored.",
		     longtonumeric(np->numeric,5),np->nick);
	errors++;
      }

/*
      if (!IsAccount(np) && np->authname[0]) {
	controlreply(sender, "ERROR: nick %s/%s is -r but carries authname '%s'.",
		     longtonumeric(np->numeric,5),np->nick,np->authname);
	errors++;
      }
*/

      if (IsSetHost(np) && (!np->sethost || !np->shident)) {
	controlreply(sender, "ERROR: nick %s/%s is +h but nick or hostname not set.",
		     longtonumeric(np->numeric,5),np->nick);
	errors++;
      }

      if (!IsSetHost(np) && (np->sethost || np->shident)) {
	controlreply(sender, "ERROR: nick %s/%s is -h but has nick or hostname set.",
		     longtonumeric(np->numeric,5),np->nick);
	errors++;
      }

    }
  }

  controlreply(sender," - Scanning server user tables");
      
  for(i=0;i<MAXSERVERS;i++) {
    if (serverlist[i].linkstate != LS_INVALID) {
      nummask=((MAXSERVERS-1)<<18) | serverlist[i].maxusernum;
      for (j=0;j<=serverlist[i].maxusernum;j++) {
	if ((np=servernicks[i][j])) {
	  mnum=(i<<18) | j;
	  if ((np->numeric & nummask) != mnum) {
	    controlreply(sender, "ERROR: nick %s/%s has wrong masked numeric.",longtonumeric(np->numeric,5),np->nick);
	    errors++;
          }
	  if (np->marker != nickmarker) {
	    controlreply(sender, "ERROR: nick %s/%s in server user table but not hash!",
			 longtonumeric(np->numeric,5),np->nick);
	    errors++;
	  }
	  np->marker=0;
	}
      }
    }
  }

  controlreply(sender," - Scanning host and realname tables");
  
  for (i=0;i<HOSTHASHSIZE;i++) {
    for (hp=hosttable[i];hp;hp=hp->next) {

      /* Check that the user counts match up */
      if (hp->clonecount != hp->marker) {
	controlreply(sender,
		     "ERROR: host %s has inconsistent clone count (stored=%d, measured=%u)",
		     hp->name->content,hp->clonecount,hp->marker);
	errors++;
      }
      
      for (np=hp->nicks;np;np=np->nextbyhost) {
	if (np->host != hp) {
	  controlreply(sender, 
		       "ERROR: nick %s/%s is in list for wrong host "
		       "(in list for %s, actual host %s).",
		       longtonumeric(np->numeric,5),np->nick,hp->name->content,
		       np->host->name->content);
	  errors++;
	}
      }
      
      hp->marker=0;
    }
  }

  for (i=0;i<REALNAMEHASHSIZE;i++) {
    for (rnp=realnametable[i];rnp;rnp=rnp->next) {
      if (rnp->usercount != rnp->marker) {
	controlreply(sender,
		     "ERROR: realname '%s' has inconsistent clone count "
		     "(stored=%d, measured=%d).",
		     rnp->name->content,rnp->usercount,rnp->marker);
	errors++;
      }

      for (np=rnp->nicks;np;np=np->nextbyrealname) {
	if (np->realname != rnp) {
	  controlreply(sender, 
		       "ERROR: nick %s/%s is in list for wrong realname "
		       "(in list for '%s', actual realname '%s').",
		       longtonumeric(np->numeric,5),np->nick,
		       rnp->name->content, np->realname->name->content);
	  errors++;
	}
      }
      
      rnp->marker=0;
    }
  }

  if (errors) 
    controlreply(sender,"All checks complete. %d errors found.",errors);
  else 
    controlreply(sender,"All checks complete. No errors found.");

  return CMD_OK;
}

