#include <stdio.h>
#include <string.h>
#include "../irc/irc.h"
#include "../lib/irc_string.h"
#include "request_block.h"

/* array of blocks */
array rqblocks;

/* are we currently loading blocks? */
int rq_loading;

int rq_initblocks(void) {
  array_init(&rqblocks, sizeof(rq_block));
  array_setlim1(&rqblocks, 5);
  array_setlim2(&rqblocks, 20);

  rq_loading = 0;

  rq_loadblocks();

  rq_addblock("#qnet*", "Reserved for QuakeNet use only.", "request", 0, 0);
  rq_addblock("#help*", "Reserved for QuakeNet use only.", "request", 0, 0);

  return 1;
}

void rq_finiblocks(void) {
  int i;
  rq_block block;

  for (i = 0; i < rqblocks.cursi; i++) {
    block = ((rq_block*)rqblocks.content)[i];

    freesstring(block.pattern);
    freesstring(block.reason);
    freesstring(block.creator);
  }

  array_free(&rqblocks);
}

rq_block *rq_findblock(const char *pattern) {
  int i;
  rq_block block;

  for (i = rqblocks.cursi - 1; i >= 0; i--) {
    block = ((rq_block*)rqblocks.content)[i];

    if (match2strings(block.pattern->content, pattern)) {
      if (block.expires != 0 && block.expires < getnettime())
        rq_removeblock(block.pattern->content);
      else
        return &(((rq_block*)rqblocks.content)[i]);
    }
  }

  return NULL;
}

void rq_addblock(const char *pattern, const char *reason, const char *creator, time_t created, time_t expires) {
  int slot;
  rq_block *block;

  if (rq_findblock(pattern) != NULL)
    return;

  slot = array_getfreeslot(&rqblocks);

  block = &(((rq_block*)rqblocks.content)[slot]);

  block->pattern = getsstring(pattern, CHANNELLEN);
  block->reason = getsstring(reason, RQ_BLOCKLEN);
  block->creator = getsstring(creator, ACCOUNTLEN);
  block->created = created == 0 ? getnettime() : created;
  block->expires = expires;

  rq_saveblocks();
}

int rq_removeblock(const char *pattern) {
  int i;
  rq_block block;

  for (i = 0; i < rqblocks.cursi; i++) {
    block = ((rq_block*)rqblocks.content)[i];

    if (ircd_strcmp(block.pattern->content, pattern) == 0) {
      freesstring(block.pattern);
      freesstring(block.reason);
      freesstring(block.creator);

      array_delslot(&rqblocks, i);

      rq_saveblocks();

      return 1;
    }
  }

  return 0;
}

/* pattern reason creator created expires */
int rq_parseline(char *line) {
  char pattern[CHANNELLEN+1];
  char reason[RQ_BLOCKLEN+1];
  char creator[ACCOUNTLEN+1];
  time_t created, expires;

  if (sscanf(line, "%s %s %lu %lu %[^\n]", pattern, creator, &created, &expires, reason) < 2) /* \n won't be there anyway, but %s won't return the whole string */
    return 0; /* invalid block */

  /* tell rq_addblock that it should not save the blocks to disk this time */
  rq_loading = 1;
  rq_addblock(pattern, reason, creator, created, expires);
  rq_loading = 0;
  
  return 1;
}

int rq_loadblocks(void) {
  char line[4096];
  FILE *rqdata;
  int count;

  rqdata = fopen(RQ_BLOCKFILE, "r");

  if (rqdata == NULL)
    return 0;

  count = 0;

  while (!feof(rqdata)) {
    if (fgets(line, sizeof(line), rqdata) == NULL)
      break;

    if (line[strlen(line) - 1] == '\n')
      line[strlen(line) - 1] = '\0';

    if (line[strlen(line) - 1] == '\r')
      line[strlen(line) - 1] = '\0';

    if (line[0] != '\0') {
      if (rq_parseline(line))
        count++;
    }
  }

  fclose(rqdata);

  return count;
}

int rq_saveblocks(void) {
  FILE *rqdata;
  int i, count = 0;

  /* don't save the blocks if we're currently loading them from the disk */
  if (rq_loading)
    return 0;

  rqdata = fopen(RQ_BLOCKFILE, "w");

  if (rqdata == NULL)
    return 0;

  rq_block block;

  for (i = 0; i < rqblocks.cursi; i++) {
    block = ((rq_block*)rqblocks.content)[i];

    fprintf(rqdata, "%s %s %lu %lu %s\n", block.pattern->content, block.creator->content, block.created, block.expires, block.reason->content);
  }

  fclose(rqdata);

  return count;
}
