/*
  nterfaced
  Copyright (C) 2003-2004 Chris Porter.

  v1.03
    - altnames actually made useful (now called realname)
    - modified for new logging system
  v1.02
    - logging
  v1.01
    - support for alternate names
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../core/schedule.h"
#include "../core/config.h"

#include "nterfaced.h"
#include "library.h"
#include "logging.h"

struct service *services;
int service_count = 0;
struct nterface_auto_log *ndl;

void _init(void) {
  int loaded;
  int debug_mode = getcopyconfigitemintpositive("nterfaced", "debug", 0);

  ndl = nterface_open_log("nterfaced", "logs/nterfaced.log", debug_mode);

  loaded = load_config_file();
  if(!loaded) {
    nterface_log(ndl, NL_ERROR, "ND: No config lines loaded successfully.");
    return;
  } else {
    nterface_log(ndl, NL_INFO, "ND: Loaded %d service%s successfully.", loaded, loaded==1?"":"s");
  }

}

void _fini(void) {
  if(service_count) {
    service_count = 0;
    if(services) {
      int i;
      for(i=0;i<service_count;i++) {
        freesstring(services[i].service);
        if(services[i].realname)
          freesstring(services[i].realname);
        freesstring(services[i].transport_name);
      }
      free(services);
      services = NULL;
      service_count = 0;
    }
  }

  ndl = nterface_close_log(ndl);
}

int load_config_file(void) {
  int lines, i, loaded_lines = 0;
  struct service *new_services, *resized, *item, *ci;
  char buf[50];

  lines = getcopyconfigitemintpositive("nterfaced", "services", 0);
  if(lines < 1) {
    nterface_log(ndl, NL_ERROR, "ND: No services found in config file.");
    return 0;
  }
  nterface_log(ndl, NL_INFO, "ND: Loading %d service line%s from config file", lines, lines==1?"":"s");

  new_services = calloc(lines, sizeof(struct service));
  item = new_services;

  for(i=1;i<=lines;i++) {
    snprintf(buf, sizeof(buf), "service%d", i);
    item->service = getcopyconfigitem("nterfaced", buf, "", 100);
    if(!item->service) {
      MemError();
      continue;
    }

    if(!item->service->content) {
      nterface_log(ndl, NL_WARNING, "ND: No data found for service at item %d.", i);
      freesstring(item->service);
    }

    if(strchr(item->service->content, ',')) {
      nterface_log(ndl, NL_WARNING, "ND: Service %s (pos %d) contains a comma.", item->service->content, i);
      freesstring(item->service);
      continue;
    }

    for(ci=new_services;ci<item;ci++) {
      if(!strcmp(ci->service->content, item->service->content)) {
        nterface_log(ndl, NL_WARNING, "ND: Duplicate service %s (%d:%d), ignoring item at position %d.", item->service->content, ci - new_services + 1, i, i);
        item->service->content[0] = '\0';
        break;
      }
    }

    if(!item->service->content[0]) {
      freesstring(item->service);
      continue;
    }

    snprintf(buf, sizeof(buf), "servicerealname%d", i);
    
    item->realname = getcopyconfigitem("nterfaced", buf, "", 100);
    if(item->realname) {
      if(!item->realname->content) {
        freesstring(item->realname);
        item->realname = NULL;
      } else {
        if(strchr(item->service->content, ',')) {
          nterface_log(ndl, NL_WARNING, "ND: Service realname %s (pos %d) contains a comma.", item->realname->content, i);
          freesstring(item->realname);
          item->realname = NULL;
        }
      }      
    }

    snprintf(buf, sizeof(buf), "tag%d", i);
    item->tag = getcopyconfigitemintpositive("nterfaced", buf, 0);
    if(item->tag < 0) {
      nterface_log(ndl, NL_WARNING, "ND: Unable to load tag for item %d (service %s).", i, item->service->content);
      freesstring(item->service);
      if(item->realname)
        freesstring(item->realname);
      continue;
    }

    snprintf(buf, sizeof(buf), "transport%d", i);
    item->transport_name = getcopyconfigitem("nterfaced", buf, "", 50);
    if(!item->transport_name) {
      MemError();
      freesstring(item->service);
      if(item->realname)
        freesstring(item->realname);
      continue;
    }

    nterface_log(ndl, NL_DEBUG, "ND: Loaded service: %s, transport: %s tag: %d.", item->service->content, item->transport_name->content, item->tag);

    item++;
    loaded_lines++;
  }

  if(!loaded_lines) {
    free(new_services);
    return 0;
  }

  resized = realloc(new_services, sizeof(service) * loaded_lines);
  if(!resized) {
    MemError();
    free(new_services);
  }
  services = resized;
  service_count = loaded_lines;

  return service_count;
}

