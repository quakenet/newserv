#include "graphing.h"
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

int dump(fsample_m *f, fsample_t start, fsample_t stop) {
  int i, j;

  for(i=start;i<stop;i++) {
    printf("%2d ", i);
    for(j=0;j<f->pos;j++) {
      fsample_t t;
      int moo = fsget_m(f, j, i, &t);

      if(!moo) {
        int moo2 = fsget_mr(f, j, i, &t);
        printf("X%2d(%2d) ", t, moo2);
      } else {
        printf(" %2d(%2d) ", t, moo);
      }
    }
    printf("\n");
  }

  return 0;
}


int main(int cargc, char **cargv) {
  fsample_m *f;
  int stop = time(NULL) / GRAPHING_RESOLUTION;
  struct stat sb;

  if(cargc < 2) {
    puts("insufficient args");
    return 1;
  }

  if(stat(cargv[1], &sb) == -1) {
    puts("file doesn't exist");
    return 2;
  }

  f = fsopen_m(0, cargv[1], SAMPLES, NULL, NULL);
  if(!f)
    return 3;

  dump(f, stop - 100, stop);

  fsclose_m(f);

  return 0;
}

