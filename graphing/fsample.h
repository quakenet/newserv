#ifndef __FSAMPLE_H
#define __FSAMPLE_H

#include <sys/types.h>

typedef u_int32_t fsample_t;

typedef struct fsample fsample;

/* nice loss of type safety here... */
typedef void *(*CoreHandlerAddFn)(void *handler, void *arg);
typedef void (*CoreHandlerDelFn)(void *);

/* single sample functions */
fsample *fsopen(char *filename, size_t samples, CoreHandlerAddFn chafn, CoreHandlerDelFn chdfn);
void fsclose(fsample *f);

struct fsample_m;
typedef fsample_t (*DeriveValueFn)(struct fsample_m *v, int entry, fsample_t pos, void *tag);

typedef struct fsample_m_entry {
  size_t freq;
  fsample *f;
  void *tag;
  DeriveValueFn derive;
} fsample_m_entry;

typedef struct fsample_m {
  size_t pos, samples;
  CoreHandlerAddFn chafn;
  CoreHandlerDelFn chdfn;
  struct fsample_m_entry entry[];
} fsample_m;

/* multiple sample functions */
fsample_m *fsopen_m(size_t count, char *filename, size_t samples, CoreHandlerAddFn chafn, CoreHandlerDelFn chdfn);
void fsclose_m(fsample_m *f);
int fsadd_m(fsample_m *f, char *filename, size_t freq, DeriveValueFn derive, void *tag);
void fsset_m(fsample_m *f, fsample_t pos, fsample_t value);

/* aggregates */
fsample_t fsamean(fsample_m *f, int entry, fsample_t pos, void *tag);
fsample_t fsapmean(fsample_m *f, int entry, fsample_t pos, void *tag);

struct fsample_p {
  fsample_t v;
  unsigned char iteration;
};

struct fsample_header {
  char magic[4];
  fsample_t version;
  unsigned char iteration;
  fsample_t lastpos;
};

struct fsample {
  struct fsample_p *m;
  int fd;
  size_t samples, len;
  struct fsample_header *header;
  void *corehandler;
  CoreHandlerDelFn corehandlerdel;
};

static inline unsigned char previteration(fsample *f) {
  unsigned char p = f->header->iteration - 1;
  if(p == 0)
    p = 255;
  return p;
}

/* doesn't support writing to negative numbers... */
static inline void fsset(fsample *f, fsample_t pos, fsample_t value) {
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
static inline fsample_t __fsget(fsample *f, int pos, fsample_t *t) {
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

static inline fsample_t fsget_r(fsample *f, fsample_t pos, fsample_t *t) {
  struct fsample_p *p = &f->m[mmod(pos, f->samples)];

  *t = p->v;
  return p->iteration;
}

static inline fsample_t fsget(fsample *f, fsample_t pos, fsample_t *t) {
  return __fsget(f, pos, t);
}

static inline fsample_t __fsget_m(fsample_m *f, int entry, int pos, fsample_t *t) {
  return __fsget(f->entry[entry].f, pos / f->entry[entry].freq, t);
}

static inline fsample_t fsget_m(fsample_m *f, int entry, fsample_t pos, fsample_t *t) {
  return fsget(f->entry[entry].f, pos / f->entry[entry].freq, t);
}

static inline fsample_t fsget_mr(fsample_m *f, int entry, fsample_t pos, fsample_t *t) {
  return fsget_r(f->entry[entry].f, pos / f->entry[entry].freq, t);
}

#endif
