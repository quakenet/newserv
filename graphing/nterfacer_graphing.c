#include "../nterfacer/library.h"
#include "../nterfacer/nterfacer.h"

#include "graphing.h"

#include <time.h>

#define ERR_SERVER_NOT_MONITORED        0x01
#define ERR_BAD_PARAMETERS              0x02
#define ERR_BAD_ENTRY                   0x03

int handle_servernames(struct rline *li, int argc, char **argv);
int handle_serverdata(struct rline *li, int argc, char **argv);

static struct service_node *g_node;

void _init(void) {
  g_node = register_service("graphing");
  if(!g_node)
    return;

  register_handler(g_node, "servernames", 0, handle_servernames);
  register_handler(g_node, "serverdata", 3, handle_serverdata);
}

void _fini(void) {
  if(g_node)
    deregister_service(g_node);
}

int handle_servernames(struct rline *li, int argc, char **argv) {
  int i;

  for(i=0;i<MAXSERVERS;i++) {
    if((serverlist[i].linkstate != LS_INVALID) && servermonitored(i)) {
      if(ri_append(li, "%d:%s", i, serverlist[i].name->content) == BF_OVER)
        return ri_error(li, BF_OVER, "Buffer overflow");
    }
  }

  return ri_final(li);
}

int handle_serverdata(struct rline *li, int argc, char **argv) {
  int servernum = atoi(argv[0]);
  unsigned int entry = atoi(argv[1]);
  fsample_t count = atoi(argv[2]);
  time_t t = time(NULL) / GRAPHING_RESOLUTION;
  int i, ret;
  fsample_m *m;

  if(!servermonitored(servernum))
    return ri_error(li, ERR_SERVER_NOT_MONITORED, "Server not monitored");

  m = servergraphs[servernum];

  if(entry > m->pos) /* not inclusive as 0 is used for for the base index */
    return ri_error(li, ERR_BAD_PARAMETERS, "Bad parameters");

  for(i=0;i<count;i++) {
    fsample_t v;
    if(fsget_m(m, entry, t - m->entry[entry].freq * i, &v)) {
      ret = ri_append(li, "%d", v);
    } else {
      ret = ri_append(li, "-1", v);
    }
    if(ret == BF_OVER)
      return ri_error(li, BF_OVER, "Buffer overflow");
  }

  return ri_final(li);
}

