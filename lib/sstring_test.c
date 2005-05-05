#include "sstring.h"

int main(int argc, char **argv) {
  int i;
  sstring *ptrs[10];
  char buf[20];
  
  for (i=0;i<10;i++) {
    sprintf(buf,"String %02d\n",i);
    ptrs[i]=getsstring(buf,20);
  }
  
  for (i=0;i<10;i++) {
    printf(ptrs[i]->content);
  }
  
  for (i=0;i<10;i++) {
    freesstring(ptrs[i]);
  }
}
