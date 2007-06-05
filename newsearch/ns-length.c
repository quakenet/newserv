/*
 * length functionality - returns the length of a string
 */

#include "newsearch.h"

#include "../lib/irc_string.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void length_free(struct searchNode *thenode);
void *length_exe(struct searchNode *thenode, void *theinput);

struct searchNode *length_parse(int type, int argc, char **argv) {
  struct searchNode *thenode, *childnode;

  if (!(thenode = (struct searchNode *)malloc(sizeof(struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }
  
  thenode->localdata  = NULL;
  thenode->returntype = RETURNTYPE_INT;
  thenode->exe  = length_exe;
  thenode->free = length_free;
  
  if (argc != 1) {
    parseError = "length: usage: length <string>";
    free(thenode);
    return NULL;
  }

  childnode = search_parse(type, argv[0]);
  if (!(thenode->localdata = coerceNode(childnode, RETURNTYPE_STRING))) {
    length_free(thenode);
    return NULL;
  }

  return thenode;
}

void length_free(struct searchNode *thenode) {
  struct searchNode *anode;
  
  if ((anode=thenode->localdata))
    (anode->free)(anode);
  
  free(thenode);
}

void *length_exe(struct searchNode *thenode, void *theinput) {
  char *strval;
  struct searchNode *anode=thenode->localdata;  

  strval=(char *)(anode->exe)(anode, theinput);
  
  return (void *)strlen(strval);
}
