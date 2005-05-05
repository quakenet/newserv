/* Chanserv related plugins for chansearch.. */

#include "chanserv.h"
#include "../chansearch/chansearch.h"

int cs_cschanserv(void *source, int cargc, char **cargv);
int cs_cschanlevsize(void *source, int cargc, char **cargv);

void _init() {
  regchansearchfunc("hasq", 0, cs_cschanserv);
  regchansearchfunc("chanlev", 1, cs_cschanlevsize);
}

void _fini() {
  unregchansearchfunc("hasq", cs_cschanserv);
  unregchansearchfunc("chanlev", cs_cschanlevsize);
}

int cs_cschanservexe(chanindex *cip, void *arg) {
  return (cip->exts[chanservext]==NULL);
}

int cs_cschanserv(void *source, int cargc, char **cargv) {
  filter *thefilter=(filter *)source;
  
  thefilter->sf=cs_cschanservexe;
  thefilter->arg=NULL;

  return 0;
}

int cs_cschanlevsizeexe(chanindex *cip, void *arg) {
  int minlength=(int)arg;
  regchan *rcp;
  regchanuser *rcup;
  int i,j;
  
  if (!(rcp=cip->exts[chanservext]))
    return 1;
  
  j=0;
  for (i=0;i<REGCHANUSERHASHSIZE;i++) {
    for (rcup=rcp->regusers[i];rcup;rcup=rcup->nextbychan) {
      j++;
      if (j>=minlength)
	return 0;
    }
  }

  return 1;
}

int cs_cschanlevsize(void *source, int cargc, char **cargv) {
  filter *thefilter=(filter *)source;
  
  if (cargc<1)
    return 1;

  thefilter->sf=cs_cschanlevsizeexe;
  thefilter->arg=(void *)strtoul(cargv[0],NULL,10);

  return 0;
}
