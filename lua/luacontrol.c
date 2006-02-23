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

void lua_startcontrol(void) {
  registercontrolhelpcmd("inslua", NO_DEVELOPER, 1, &lua_inslua, "Usage: inslua <script>\nLoads the supplied Lua script..");
  registercontrolhelpcmd("rmlua", NO_DEVELOPER, 1, &lua_rmlua, "Usage: rmlua <script>\nUnloads the supplied Lua script.");
  registercontrolhelpcmd("reloadlua", NO_DEVELOPER, 1, &lua_reloadlua, "Usage: reloadlua <script>\nReloads the supplied Lua script.");
  registercontrolhelpcmd("lslua", NO_DEVELOPER, 0, &lua_lslua, "Usage: lslua\nLists all currently loaded Lua scripts.");
}

void lua_destroycontrol(void) {
  deregistercontrolcmd("inslua", &lua_inslua);
  deregistercontrolcmd("rmlua", &lua_rmlua);
  deregistercontrolcmd("reloadlua", &lua_reloadlua);
  deregistercontrolcmd("lslua", &lua_lslua);
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
  lua_list *l = lua_head;

  controlreply(np, "Loaded scripts:");

  for(l=lua_head;l;l=l->next)
    controlreply(np, "%s", l->name->content);

  controlreply(np, "Done.");

  return CMD_OK;
}

