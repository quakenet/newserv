/* Copyright (C) Chris Porter 2005 */
/* ALL RIGHTS RESERVED. */
/* Don't put this into the SVN repo. */
/* Copyright (C) Chris Porter 2005 */

#include "../control/control.h"
#include "../nick/nick.h"

#include "lua.h"

int lua_inslua(void *sender, int cargc, char **cargv);
int lua_rmlua(void *sender, int cargc, char **cargv);
int lua_reloadlua(void *sender, int cargc, char **cargv);
int lua_lslua(void *sender, int cargc, char **cargv);
int lua_forcegc(void *sender, int cargc, char **cargv);
void lua_controlstatus(int hooknum, void *arg);

void lua_startcontrol(void) {
  registercontrolhelpcmd("inslua", NO_DEVELOPER, 1, &lua_inslua, "Usage: inslua <script>\nLoads the supplied Lua script..");
  registercontrolhelpcmd("rmlua", NO_DEVELOPER, 1, &lua_rmlua, "Usage: rmlua <script>\nUnloads the supplied Lua script.");
  registercontrolhelpcmd("reloadlua", NO_DEVELOPER, 1, &lua_reloadlua, "Usage: reloadlua <script>\nReloads the supplied Lua script.");
  registercontrolhelpcmd("lslua", NO_DEVELOPER, 0, &lua_lslua, "Usage: lslua\nLists all currently loaded Lua scripts and shows their memory usage.");
  registercontrolhelpcmd("forcegc", NO_DEVELOPER, 1, &lua_forcegc, "Usage: forcegc ?script?\nForces a full garbage collection for a specific script (if supplied), all scripts otherwise.");
  registerhook(HOOK_CORE_STATSREQUEST, lua_controlstatus);
}

void lua_destroycontrol(void) {
  deregistercontrolcmd("inslua", &lua_inslua);
  deregistercontrolcmd("rmlua", &lua_rmlua);
  deregistercontrolcmd("reloadlua", &lua_reloadlua);
  deregistercontrolcmd("lslua", &lua_lslua);
  deregistercontrolcmd("forcegc", &lua_forcegc);
  deregisterhook(HOOK_CORE_STATSREQUEST, lua_controlstatus);
}

int lua_inslua(void *sender, int cargc, char **cargv) {
  nick *np = (nick *)sender;
  char *script = cargv[0];

  if(cargc < 1) {
    controlreply(np, "Usage: inslua <script>");
    return CMD_ERROR;
  }

  if(lua_scriptloaded(script)) {
    controlreply(np, "Script %s already loaded, or name not valid.", script);
    return CMD_ERROR;
  }

  if(lua_loadscript(script)) {
    controlreply(np, "Script %s loaded.", script);
    return CMD_OK;
  } else {
    controlreply(np, "Unable to load script %s.", script);
    return CMD_ERROR;
  }
}

int lua_rmlua(void *sender, int cargc, char **cargv) {
  nick *np = (nick *)sender;
  char *script = cargv[0];
  lua_list *l;

  if(cargc < 1) {
    controlreply(np, "Usage: rmmod <script>");
    return CMD_ERROR;
  }

  l = lua_scriptloaded(script);
  if(!l) {
    controlreply(np, "Script %s is not loaded.", script);
    return CMD_ERROR;
  }

  lua_unloadscript(l);
  controlreply(np, "Script %s unloaded.", script);

  return CMD_OK;
}

int lua_reloadlua(void *sender, int cargc, char **cargv) {
  if(cargc < 1) {
    controlreply((nick *)sender, "Usage: reloadlua <script>");
    return CMD_ERROR;
  }

  lua_rmlua(sender, cargc, cargv);

  return lua_inslua(sender, cargc, cargv);
}

int lua_lslua(void *sender, int cargc, char **cargv) {
  nick *np = (nick *)sender;
  lua_list *l;

  controlreply(np, "Loaded scripts:");

  for(l=lua_head;l;l=l->next)
    controlreply(np, "%s (mem: %dKb calls: %lu user: %0.2fs sys: %0.2fs)", l->name->content, lua_gc(l->l, LUA_GCCOUNT, 0), l->calls, (double)((double)l->ru_utime.tv_sec + (double)l->ru_utime.tv_usec / USEC_DIFFERENTIAL), (double)((double)l->ru_stime.tv_sec + (double)l->ru_stime.tv_usec / USEC_DIFFERENTIAL));

  controlreply(np, "Done.");

  return CMD_OK;
}

void lua_controlstatus(int hooknum, void *arg) {
  char buf[1024];
  int memusage = 0;
  lua_list *l;

  if ((long)arg <= 10)
    return;

  for(l=lua_head;l;l=l->next)
    memusage+=lua_gc(l->l, LUA_GCCOUNT, 0);

  snprintf(buf, sizeof(buf), "Lua     : %dKb in use in total by scripts.", memusage);
  triggerhook(HOOK_CORE_STATSREPLY, buf);
}

int lua_forcegc(void *sender, int cargc, char **cargv) {
  nick *np = (nick *)sender;
  lua_list *l;
  int membefore = 0, memafter = 0;
  if(cargc == 0) {
    for(l=lua_head;l;l=l->next) {
      membefore+=lua_gc(l->l, LUA_GCCOUNT, 0);
      controlreply(np, "GC'ing %s. . .", l->name->content);
      lua_gc(l->l, LUA_GCCOLLECT, 0);
      memafter+=lua_gc(l->l, LUA_GCCOUNT, 0);
    }
  } else {
    l = lua_scriptloaded(cargv[0]);
    if(!l) {
      controlreply(np, "Script %s is not loaded.", cargv[0]);
      return CMD_ERROR;
    }

    membefore+=lua_gc(l->l, LUA_GCCOUNT, 0);
    lua_gc(l->l, LUA_GCCOLLECT, 0);
    memafter+=lua_gc(l->l, LUA_GCCOUNT, 0);
  }
  controlreply(np, "Done.");
  controlreply(np, "Freed: %dKb (%0.2f%% of %dKb) -- %dKb now in use.", membefore - memafter, ((double)membefore - (double)memafter) / (double)membefore * 100, membefore, memafter);
  return CMD_OK;
}
