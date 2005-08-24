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
#include "config.h"
#include "error.h"
#include <stdio.h>
#include <string.h>

array modules;

sstring *moddir;
sstring *modsuffix;

void modulerehash() {
  int i;
  sstring **mods;
  array *autoloads;
  
  if (moddir!=NULL)
    freesstring(moddir);
  
  if (modsuffix!=NULL)
    freesstring(modsuffix);
  
  moddir=getcopyconfigitem("core","moduledir",".",100);
  modsuffix=getcopyconfigitem("core","modulesuffix",".so",5);  

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
  char buf[512];

  delchars(modulename,"./\\;");
  
  if (isloaded(modulename)) {
    Error("core",ERR_DEBUG,"Tried to load already loaded module: %s",modulename);
    return 1;
  }
  
  if (strlen(modulename)>100) {
    Error("core",ERR_WARNING,"Module name too long: %s",modulename);  
    return 1;
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

int isloaded(char *modulename) {
  if (getindex(modulename)==-1)
    return 0;
  else
    return 1;
}

int rmmod(char *modulename) {
  int i;
  module *mods;
  
  delchars(modulename,"./\\;");
  
  i=getindex(modulename);
  if (i<0)
    return 1;
  
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
