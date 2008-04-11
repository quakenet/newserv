#include "fsample.h"

#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdlib.h>

struct fsample_p {
  fsample_t v;
  unsigned char iteration;
};

struct fsample_header {
  unsigned char iteration;
  fsample_t lastpos;
};

struct fsample {
  struct fsample_p *m;
  int fd;
  size_t samples, len;
  struct fsample_header *header;
};

fsample *fsopen(char *filename, size_t samples) {
  fsample *f = (fsample *)malloc(sizeof(fsample));
  if(!f)
    return NULL;

  f->len = samples * sizeof(struct fsample_p) + sizeof(struct fsample_header);
  f->fd = open(filename, O_RDWR|O_CREAT, 0600);

  if((f->fd == -1) || (lseek(f->fd, f->len - 1, SEEK_SET) == -1)) {
    free(f);
    return NULL;
  }
  write(f->fd, "", 1);

  f->header = mmap(NULL, f->len, PROT_READ|PROT_WRITE, MAP_NOCORE|MAP_SHARED, f->fd, 0);
  f->m = (struct fsample_p *)(f->header + 1);

  if(f->m == MAP_FAILED) {
    close(f->fd);
    free(f);
    return NULL;
  }

  f->samples = samples;

  if(f->header->iteration == 0)
    f->header->iteration = 1;

  return f;
}

void fsclose(fsample *f) {
  munmap(f->m, f->len);
  close(f->fd);
  free(f);
}

static inline unsigned char previteration(fsample *f) {
  unsigned char p = f->header->iteration - 1;
  if(p == 0)
    p = 255;
  return p;
}

 
/* doesn't support writing to negative numbers... */
inline void fsset(fsample *f, fsample_t pos, fsample_t value) {
  fsample_t actualpos = pos % f->samples;
  struct fsample_p *p = &f->m[actualpos];

  if(f->header->lastpos > actualpos) {
    f->header->iteration++;
    if(f->header->iteration == 0)
      f->header->iteration = 1;
  }

  f->header->lastpos = actualpos;

  p->iteration = f->header->iteration;
  p->v = value;
}

static inline fsample_t mmod(int x, int y) {
#if -5 % 3 == -2
  int v = x % y;
  if(v < 0)
    v = v + y;
  return v;
#else
#error Unknown modulo operator function.
#endif
}

/* API functions only have access to positive indicies */
inline fsample_t __fsget(fsample *f, int pos, fsample_t *t) {
  struct fsample_p *p = &f->m[mmod(pos, f->samples)];
  
  if(p->iteration != f->header->iteration) {
    unsigned char prev = previteration(f);

    if(prev != p->iteration || (pos <= f->header->lastpos)) {
      /*printf("bad: prev: %d p->iteration: %d, pos: %d lastpos: %d\n", prev, p->iteration, pos, f->header->lastpos);*/
      return 0;
    }
  }

  *t = p->v;
  return p->iteration;
}

inline fsample_t fsget_r(fsample *f, fsample_t pos, fsample_t *t) {
  struct fsample_p *p = &f->m[mmod(pos, f->samples)];

  *t = p->v;
  return p->iteration;
}

inline fsample_t fsget(fsample *f, fsample_t pos, fsample_t *t) {
  return __fsget(f, pos, t);
}

int fsadd_m(fsample_m *f, char *filename, size_t freq, DeriveValueFn derive, void *tag) {
  f->entry[f->pos].freq = freq;

  f->entry[f->pos].f = fsopen(filename, f->samples / freq);
  if(!f->entry[f->pos].f)
    return 0;

  f->entry[f->pos].derive = derive;
  f->entry[f->pos].tag = tag;
  f->pos++;
  return 1;
}

fsample_m *fsopen_m(size_t count, char *filename, size_t samples) {
  fsample_m *n = (fsample_m *)malloc(sizeof(fsample_m) + (count + 1) * sizeof(fsample_m_entry));
  if(!n)
    return NULL;

  n->samples = samples;
  n->pos = 0;

  if(!fsadd_m(n, filename, 1, NULL, 0)) {
    free(n);
    return NULL;
  }

  return n;
}

void fsset_m(fsample_m *f, fsample_t pos, fsample_t value) {
  int i;

  for(i=0;i<f->pos;i++) {
    fsample_t v;

    if((pos + 1) % f->entry[i].freq != 0)
      continue;

    if(f->entry[i].derive) {
      v = (f->entry[i].derive)(f, i, pos, f->entry[i].tag);
    } else {
      v = value;
    }

    fsset(f->entry[i].f, pos / f->entry[i].freq, v);
  }
}

void fsclose_m(fsample_m *f) {
  int i;

  for(i=0;i<f->pos;i++)
    fsclose(f->entry[i].f);
  free(f);
}

inline fsample_t __fsget_m(fsample_m *f, int entry, int pos, fsample_t *t) {
  return __fsget(f->entry[entry].f, pos / f->entry[entry].freq, t);
}

inline fsample_t fsget_m(fsample_m *f, int entry, fsample_t pos, fsample_t *t) {
  return fsget(f->entry[entry].f, pos / f->entry[entry].freq, t);
}

inline fsample_t fsget_mr(fsample_m *f, int entry, fsample_t pos, fsample_t *t) {
  return fsget_r(f->entry[entry].f, pos / f->entry[entry].freq, t);
}

fsample_t fsamean(fsample_m *f, int entry, fsample_t pos, void *tag) {
  fsample_t c = 0;
  long samples = (long)tag;
  int count = 0;
  int rpos = pos / f->entry[0].freq;
  int i;
  fsample_t t;

  for(i=0;i<samples;i++) {
    if(__fsget_m(f, 0, rpos - i, &t)) {
      c+=t;
      count++;
    } else {
/*      printf("bad :(\n");*/
    }
  }

  if(count == 0)
    return 0;

  return c / count;
}

fsample_t fsapmean(fsample_m *f, int entry, fsample_t pos, void *tag) {
  fsample_t c = 0;
  long samples = (long)tag;
  int count = 0;
  int rpos = pos / f->entry[entry - 1].freq;
  int i;
  fsample_t t;

  for(i=0;i<=samples;i++) {
    if(__fsget(f->entry[entry - 1].f, rpos - i, &t)) {
      c+=t;
      count++;
    } else {
      /*printf("bad :(\n");*/
    }
  }

  if(count == 0)
    return 0;

  return c / count;
}
