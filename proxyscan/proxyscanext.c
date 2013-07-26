/*
 * proxyscanext.c:
 *  This file deals with extended ports to scan.
 */

#include "proxyscan.h"
#include <time.h>
#include <stdio.h>
#include "../core/error.h"
#include <string.h>

extrascan *addextrascan(unsigned short port, unsigned char type) {
  extrascan *esp;

  esp=getextrascan();
  esp->port=port;
  esp->type=type;
  
  return esp;
}

void delextrascan(extrascan *esp) {
  freeextrascan(esp);
}

/*
 * Returns a cachehost * for the named IP
 */

extrascan *findextrascan(patricia_node_t *node) {
  extrascan *esp;

  if( (extrascan *)node->exts[ps_extscan_ext] ) {
    esp = (extrascan *)node->exts[ps_extscan_ext];
    if (esp) {
      /* valid: return it */
      return esp;
    }
  }

  /* Not found: return NULL */
  return NULL;
}

void loadextrascans() {
  FILE *fp;
  unsigned short port;
  char buf[512];
  extrascan *esp=NULL;
  char ip[512];
  int res;
  struct irc_in_addr sin;
  unsigned char bits;
  patricia_node_t *node;

  if ((fp=fopen("data/ports.txt","r"))==NULL) {
    Error("proxyscan",ERR_ERROR,"Unable to open ports file for reading!");
    return;
  }


  while (!feof(fp)) {
    fgets(buf,512,fp);
    if (feof(fp)) {
      break;
    }

    res=sscanf(buf,"%s %hu",ip,&port);

    if (res<2)
      continue;

    if (0 == ipmask_parse(ip,&sin, &bits)) {
      /* invalid mask */
    } else {
      node = refnode(iptree, &sin, bits);
      if( node ) {
        esp=addextrascan(port, STYPE_SOCKS4);
	esp->nextbynode = (extrascan *)node->exts[ps_extscan_ext];
        node->exts[ps_extscan_ext] = esp;

        esp=addextrascan(port, STYPE_SOCKS5);
        esp->nextbynode = (extrascan *)node->exts[ps_extscan_ext];
        node->exts[ps_extscan_ext] = esp;

        esp=addextrascan(port, STYPE_HTTP);
        esp->nextbynode = (extrascan *)node->exts[ps_extscan_ext];
        node->exts[ps_extscan_ext] = esp;
      }
    }
  }

  fclose(fp);
}


unsigned int extrascancount() {
  unsigned int total=0;
  extrascan *esp;
  patricia_node_t *node;

  PATRICIA_WALK (iptree->head, node) {
    if ( node->exts[ps_extscan_ext] ) {
      esp = (extrascan *) node->exts[ps_extscan_ext];
      if (esp) 
        total++;
    }
  } PATRICIA_WALK_END;

  return total;
}

