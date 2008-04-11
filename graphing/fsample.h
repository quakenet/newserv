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
inline void fsset(fsample *f, fsample_t pos, fsample_t value);
inline fsample_t fsget(fsample *f, fsample_t pos, fsample_t *t);
inline fsample_t fsget_r(fsample *f, fsample_t pos, fsample_t *t);

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
inline fsample_t fsget_m(fsample_m *f, int entry, fsample_t pos, fsample_t *t);
inline fsample_t fsget_mr(fsample_m *f, int entry, fsample_t pos, fsample_t *t);

/* aggregates */
fsample_t fsamean(fsample_m *f, int entry, fsample_t pos, void *tag);
fsample_t fsapmean(fsample_m *f, int entry, fsample_t pos, void *tag);

#endif
