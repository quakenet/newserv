/*
 * modules.c:
 *
 * Provides functions for dealing with dynamic modules.
 */
 
#include <stdlib.h>
#include <dlfcn.h>
#include "modules.h"
#include "../lib/array.h" 
#include "../lib/sstring.h"
#include "../lib/irc_string.h"
#include "../lib/splitline.h"
#include "../lib/strlfunc.h"
#include "config.h"
#include "error.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "schedule.h"

#define DEPFILE	"modules.dep"

#define MAXMODULES 100

/* Module dependency stuff.
 *
 * "parents" are modules we rely on in order to work.
 * Whenever we load a module we must ensure all parents are present first.
 *
 * "children" are modules which rely on us.
 * Whenever we remove a module, we must ensure all children are removed.
 * If it's a reload, we will reload them afterwards.
 *
 * Now, depmod.pl cunningly arranges the file so that each modules' parents are
 * always defined earlier in the file.  So we can allocate and fill in the parents 
 * array as we read the file.  Note that we can also arrange for numchildren to be
 * correct on each node when we've finished.
 *
 * We then do a second pass.  At each node, we allocate the space for the
 * children array and reset numchildren to 0.  We can also then populate our
 * parents' children array with ourselves, using "numchildren" to get the
 * list in order.
 * 
 * At the end of the second pass we should have a correctly filled in array!
 */

sstring *safereload_str;
schedule *safereload_sched;

struct module_dep {
  sstring *name;
  unsigned int numparents;
  unsigned int numchildren;
  struct module_dep **parents;
  struct module_dep **children;
  unsigned int reloading;
};

array modules;

struct module_dep moduledeps[MAXMODULES];
unsigned int knownmodules=0;

sstring *moddir;
sstring *modsuffix;

/* Clear out and free the dependencies. */
void clearmoduledeps() {
  unsigned int i;
  
  for (i=0;i<knownmodules;i++) {
    freesstring(moduledeps[i].name);
    if (moduledeps[i].parents) {
      free(moduledeps[i].parents);
      moduledeps[i].parents=NULL;
    }
    if (moduledeps[i].children) {
      free(moduledeps[i].children);
      moduledeps[i].children=NULL;
    }
  }
  
  knownmodules=0;
}

/* Get a pointer to the given module's dependency record. */
struct module_dep *getmoduledep(const char *name) {
  unsigned int i;
  
  for (i=0;i<knownmodules;i++) {
    if (!strcmp(name,moduledeps[i].name->content))
      return &(moduledeps[i]);
  }
  
  return NULL;
}

/* Populate the dependency array.  Read the monster description above for details.
 * This relies on moddir being set, so call this after setting it up. */
void initmoduledeps() {
  FILE *fp;
  char buf[1024];
  char *largv[100];
  char largc;
  char *ch;
  struct module_dep *mdp, *tmdp;
  unsigned int i,j;
  
  sprintf(buf,"%s/%s",moddir->content,DEPFILE);
  
  if (!(fp=fopen(buf,"r"))) {
    Error("core",ERR_WARNING,"Unable to open module dependency file: %s",buf);
  } else {
    /* First pass */
    while (!feof(fp)) {
      fgets(buf, sizeof(buf), fp);
      if (feof(fp))
        break;
      
      /* Chomp off that ruddy newline. */
      for (ch=buf;*ch;ch++) {
        if (*ch=='\n' || *ch=='\r') {
          *ch='\0';
          break;
        }
      }
        
      /* We have a space-delimited list of things.  Whatever will we do with that? :) */
      largc=splitline(buf,largv,100,0);
      
      if (largc<1)
        continue;
      
      /* Add us to the array */
      i=knownmodules++;
      if (i>=MAXMODULES) {
        Error("core",ERR_ERROR,
               "Too many modules in dependency file; rebuild with higher MAXMODULES.  Module dependencies disabled.\n");
        clearmoduledeps();
        return;
      }
      
      mdp=&(moduledeps[i]);
      
      mdp->name=getsstring(largv[0],100);
      mdp->numparents=largc-1;
      mdp->numchildren=0; /* peace, for now */
      mdp->parents=malloc(mdp->numparents * sizeof(struct module_dep *));
      
      /* Fill in the parents array */
      for (i=0;i<(largc-1);i++) {
        if (!(mdp->parents[i]=getmoduledep(largv[i+1]))) {
          Error("core",ERR_WARNING,"Couldn't find parent module %s of %s.  Module dependencies disabled.",
                  largv[i+1],largv[0]);
          clearmoduledeps();
          return;
        }
        mdp->parents[i]->numchildren++; /* break the bad news */
      }
    }
    
    /* Second pass */
    for (i=0;i<knownmodules;i++) {
      mdp=&(moduledeps[i]);
      
      /* Allocate child array */
      if (mdp->numchildren) {
        mdp->children=malloc(mdp->numchildren * sizeof(struct module_dep *));
        mdp->numchildren=0; /* if only real life were this simple */
      }
      
      /* Now fill in the children arrays of our parents (bear with me on this) */
      for (j=0;j<mdp->numparents;j++) {
        tmdp=mdp->parents[j]; /* This is just... */
        
        tmdp->children[tmdp->numchildren++]=mdp; /* ... to give this line a chance at making sense */
      }
    }
  }
}

void modulerehash() {
  int i;
  sstring **mods;
  array *autoloads;
  
  if (moddir!=NULL)
    freesstring(moddir);
  
  if (modsuffix!=NULL)
    freesstring(modsuffix);

  clearmoduledeps();
  
  moddir=getcopyconfigitem("core","moduledir",".",100);
  modsuffix=getcopyconfigitem("core","modulesuffix",".so",5);  

  initmoduledeps();

  /* Check for auto-load modules */
  autoloads=getconfigitems("core","loadmodule");
  if (autoloads!=NULL) {
    mods=(sstring **)(autoloads->content);
    for (i=0;i<autoloads->cursi;i++) {
      insmod(mods[i]->content);
    }
  }
}

void initmodules() {
  array_init(&modules,sizeof(module));
  array_setlim1(&modules,5);
  array_setlim2(&modules,10);
  
  moddir=NULL;
  modsuffix=NULL;
  modulerehash();  
}

int insmod(char *modulename) {
  int i;
  module *mods;
  char buf[1024];
  const char *(*verinfo)(void);
  struct module_dep *mdp;

  delchars(modulename,"./\\;");
  
  if (isloaded(modulename)) {
    Error("core",ERR_DEBUG,"Tried to load already loaded module: %s",modulename);
    return 1;
  }
  
  if (strlen(modulename)>100) {
    Error("core",ERR_WARNING,"Module name too long: %s",modulename);  
    return 1;
  }

  if ((mdp=getmoduledep(modulename))) {
    for (i=0;i<mdp->numparents;i++) {
      if (!isloaded(mdp->parents[i]->name->content)) {
        if (insmod(mdp->parents[i]->name->content)) {
          Error("core",ERR_WARNING,"Error loading dependant module %s (needed by %s)",
                   mdp->parents[i]->name->content,modulename);
          return 1;
        }
      }
    }
  } else {
    Error("core",ERR_WARNING,"Loading module %s without dependency information.",modulename);
  }

  i=array_getfreeslot(&modules);
  mods=(module *)(modules.content);

  sprintf(buf,"%s/%s%s",moddir->content,modulename,modsuffix->content);
  
  mods[i].handle=dlopen(buf,RTLD_NOW|RTLD_GLOBAL);
  
  if(mods[i].handle==NULL) {
    Error("core",ERR_ERROR,"Loading module %s failed: %s",modulename,dlerror());
    array_delslot(&modules,i);
    return -1;
  }

  mods[i].name=getsstring(modulename,MODULENAMELEN);

  verinfo=dlsym(mods[i].handle,"_version");
  if(verinfo) {
    mods[i].version=verinfo();
  }  else {
    mods[i].version=NULL;
  }

  Error("core",ERR_INFO,"Loaded module %s OK.",modulename);
  
  return 0;
}

int getindex(char *modulename) {
  int i;
  module *mods;
  
  mods=(module *)(modules.content);
  for(i=0;i<modules.cursi;i++)
    if (!strcmp(mods[i].name->content,modulename))
      return i;
      
  return -1;
}

char *lsmod(int index) {
  module *mods;

  if (index < 0 || index >= modules.cursi)
    return NULL;

  mods=(module *)(modules.content);
  return mods[index].name->content;
}

const char *lsmodver(int index) {
  module *mods;

  if (index < 0 || index >= modules.cursi)
    return NULL;

  mods=(module *)(modules.content);
  return mods[index].version;
}

int isloaded(char *modulename) {
  if (getindex(modulename)==-1)
    return 0;
  else
    return 1;
}


int rmmod(char *modulename) {
  int i,j;
  module *mods;
  struct module_dep *mdp;
  
  delchars(modulename,"./\\;");
  
  i=getindex(modulename);
  if (i<0)
    return 1;

  if ((mdp=getmoduledep(modulename))) {
    for (j=0;j<mdp->numchildren;j++) {
      if (isloaded(mdp->children[j]->name->content)) {
        if (rmmod(mdp->children[j]->name->content)) {
          Error("core",ERR_WARNING,"Unable to remove child module %s (depends on %s)",
                 mdp->children[j]->name->content, modulename);
          return 1;
        }
      }
    }

    /* We may have removed other modules - reaquire the index number in case it has changed. */
    i=getindex(modulename);
    if (i<0)
      return 1;
  } else {
    Error("core",ERR_WARNING,"Removing module %s without dependency information",modulename);
  }
  
  mods=(module *)(modules.content);
    
#ifdef BROKEN_DLCLOSE
  {
    void (*fini)();
    fini = dlsym(mods[i].handle, "__fini");
    if(!dlerror())
      fini();
  }
#endif

  dlclose(mods[i].handle);
  freesstring(mods[i].name);
  array_delslot(&modules,i);

  Error("core",ERR_INFO,"Removed module %s.",modulename);
  
  return 0;
}    

/* Set the reload mark on the indicated module, if loaded, and all its
 * children */
void setreloadmark(struct module_dep *mdp) {
  unsigned int i;
  
  if (!isloaded(mdp->name->content))
    return;
  
  for (i=0;i<mdp->numchildren;i++) 
    setreloadmark(mdp->children[i]);
  
  mdp->reloading=1;
}

/* preparereload: this function marks in the dependency tree all the 
 * modules which will be unloaded if this one is removed. */
void preparereload(char *modulename) {
  unsigned int i;
  struct module_dep *mdp;
  
  delchars(modulename,"./\\;");
  
  /* First, clear the mark off all dependant modules */
  for (i=0;i<knownmodules;i++)
    moduledeps[i].reloading=0;

  /* Do nothing if this module is not loaded */
  i=getindex(modulename);
  if (i<0)
    return;
    
  if ((mdp=getmoduledep(modulename))) {
    setreloadmark(mdp);
  }
}

/* reloadmarked: this function loads all the modules marked for reloading. 
 * The way the array is ordered means we will do this in the "right" order. 
 * This means we won't do an isloaded() check for each one - we shouldn't
 * get any "already loaded" warnings so let them come out if they happen.
 */
void reloadmarked(void) {
  unsigned int i;
  
  for (i=0;i<knownmodules;i++) {
    if (moduledeps[i].reloading) {
      insmod(moduledeps[i].name->content);
    }
  }
}

void safereloadcallback(void *arg) {
  safereload_sched=NULL;
  
  if (!safereload_str)
    return;
  
  preparereload(safereload_str->content);
  rmmod(safereload_str->content);
  insmod(safereload_str->content);
  reloadmarked();

  freesstring(safereload_str);
  safereload_str=NULL;
}

void safereload(char *themodule) {
  if (safereload_str)
    freesstring(safereload_str);
  
  safereload_str=getsstring(themodule, 100);
  
  if (safereload_sched)
    deleteschedule(safereload_sched, safereloadcallback, NULL);
  
  scheduleoneshot(1, safereloadcallback, NULL);
}

void newserv_shutdown() {
  module *mods;
  char buf[1024];

  while (modules.cursi) {
    mods=(module *)(modules.content);

    strlcpy(buf, mods[0].name->content, sizeof(buf));
    rmmod(buf);
  }
  
  clearmoduledeps();

  if (moddir!=NULL)
    freesstring(moddir);

  if (modsuffix!=NULL)
    freesstring(modsuffix);

  Error("core",ERR_INFO,"All modules removed.  Exiting.");
}

/* very slow, make sure you cache the pointer! */
void *ndlsym(char *modulename, char *fn) {
  module *mods=(module *)(modules.content);
  int i=getindex(modulename);

  if (i<0)
    return NULL;

  return dlsym(mods[i].handle, fn);
}
