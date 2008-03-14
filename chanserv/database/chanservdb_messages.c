#define COMPILING_MESSAGES
#include "../chanserv_messages.h"

#include "../chanserv.h"

cslang *cslanguages[MAXLANG];
unsigned int cslangcount;

sstring *csmessages[MAXLANG][MAXMESSAGES];

void initmessages() {
  int i;
  int j;
  
  for (i=0;i<MAXLANG;i++) {
    cslanguages[i]=NULL;
    for (j=0;j<MAXMESSAGES;j++) {
      csmessages[i][j]=NULL;
    }
  }
}
