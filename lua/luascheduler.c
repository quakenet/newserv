#include "../core/schedule.h"
#include "../core/error.h"
#include "lua.h"

#define SCHEDULER_METATABLE "newserv.scheduler"
#define SCHEDULER_ENTRY_METATABLE "newserv.schedulerentry"

static int lua_scheduler_new(lua_State *ps) {
  lua_scheduler *result;
  lua_list *ll = lua_listfromstate(ps);
  if(!ll)
    return 0;

  result = (lua_scheduler *)lua_newuserdata(ps, sizeof(lua_scheduler));
  if(!result)
    return 0;

  luaL_getmetatable(ps, SCHEDULER_METATABLE);
  lua_setmetatable(ps, -2);

  result->count = 0;
  result->entries = NULL;
  result->ps = ps;
  result->next = ll->schedulers;
  ll->schedulers = result;

  return 1;
}

static void remove_entry(lua_scheduler_entry *entry) {
  if(entry->prev) {
    entry->prev->next = entry->next;
  } else {
    entry->sched->entries = entry->next;
  }

  if(entry->next) {
    entry->next->prev = entry->prev;
  } else {
    /* no tail pointer! */
  }

  entry->next = NULL;
  entry->prev = NULL;
  entry->psch = NULL;
}

static void free_entry(lua_scheduler_entry *entry) {
  luaL_unref(entry->sched->ps, LUA_REGISTRYINDEX, entry->ref);
  free(entry);
}

static int valid_entry(lua_scheduler_entry *entry) {
  if(!entry || !entry->psch) {
    Error("lua", ERR_WARNING, "Invalid scheduler entry: %p", entry);
    return 0;
  }

  return 1;    
}

static void scheduler_exec(void *arg) {
  lua_scheduler_entry *entry = (lua_scheduler_entry *)arg;

  if(!valid_entry(entry))
    return;

  remove_entry(entry);

  _lua_vpcall(entry->sched->ps, (void *)(long)entry->ref, LUA_POINTERMODE, "");

  free_entry(entry);
}

static lua_scheduler *lua_toscheduler(lua_State *ps, int arg) {
  void *ud = luaL_checkudata(ps, 1, SCHEDULER_METATABLE);
  if(!ud)
    return NULL;

  return (lua_scheduler *)ud;
}

static int lua_scheduler_add(lua_State *ps) {
  lua_scheduler_entry *entry;
  lua_scheduler *sched;

  if(!lua_isuserdata(ps, 1) || !lua_isint(ps, 2) || !lua_isfunction(ps, 3))
    return 0;

  sched = (lua_scheduler *)lua_toscheduler(ps, 1);
  if(!sched)
    return 0;

  entry = (lua_scheduler_entry *)malloc(sizeof(lua_scheduler_entry));
  if(!entry)
    return 0;

  entry->ref = luaL_ref(ps, LUA_REGISTRYINDEX);
  entry->sched = sched;
  entry->psch = scheduleoneshot(lua_toint(ps, 2), scheduler_exec, entry);
  if(!entry->psch) {
    luaL_unref(ps, LUA_REGISTRYINDEX, entry->ref);
    free(entry);
    return 0;
  }

  entry->prev = NULL;
  entry->next = sched->entries;

  /* note: we have no tail pointer */

  if(sched->entries)
    sched->entries->prev = entry;

  sched->entries = entry;
  sched->count++;

  lua_pushlightuserdata(ps, entry);
  return 1;
}

static void delete_entry(lua_scheduler_entry *entry) {
  deleteschedule(entry->psch, scheduler_exec, entry);
  remove_entry(entry);
  free_entry(entry);
}

static int lua_scheduler_remove(lua_State *ps) {
  lua_scheduler_entry *entry;

  if(!lua_islightuserdata(ps, 1))
    LUA_RETURN(ps, LUA_FAIL);

  entry = (lua_scheduler_entry *)lua_topointer(ps, 1);

  if(!valid_entry(entry))
    LUA_RETURN(ps, LUA_FAIL);

  delete_entry(entry);

  LUA_RETURN(ps, LUA_OK);
}

static int lua_scheduler_count(lua_State *ps) {
  lua_scheduler *sched;

  if(!lua_islightuserdata(ps, 1))
    return 0;

  sched = (lua_scheduler *)lua_topointer(ps, 1);
  lua_pushint(ps, sched->count);

  return 1;
}

static void scheduler_free(lua_list *ll, lua_scheduler *sched) {
  lua_scheduler *prev, *l;

  if(!sched->ps)
    return;

  while(sched->entries)
    delete_entry(sched->entries);

  for(prev=NULL,l=ll->schedulers;l;prev=l,l=l->next) {
    if(l != sched)
      continue;

    if(prev) {
      prev->next = sched->next;
    } else {
      ll->schedulers = sched->next;
    }

    break;
  }

  sched->ps = NULL;
}

void lua_scheduler_freeall(lua_list *ll) {
  while(ll->schedulers)
    scheduler_free(ll, ll->schedulers);
}

/* called at GC time only */
static int lua_scheduler_free(lua_State *ps) {
  lua_list *ll = lua_listfromstate(ps);
  lua_scheduler *sched;

  if(!ll || !lua_isuserdata(ps, 1))
    return 0;

  sched = lua_toscheduler(ps, 1);
  if(!sched)
    return 0;

  scheduler_free(ll, sched);

  return 0;
}

void lua_registerschedulercommands(lua_State *ps) {
  lua_register(ps, "raw_scheduler_new", lua_scheduler_new);
  lua_register(ps, "raw_scheduler_count", lua_scheduler_count);
  lua_register(ps, "raw_scheduler_add", lua_scheduler_add);
  lua_register(ps, "raw_scheduler_remove", lua_scheduler_remove);

  luaL_newmetatable(ps, SCHEDULER_METATABLE);
  lua_pushcfunction(ps, lua_scheduler_free);
  lua_setfield(ps, -2, "__gc");  
}
