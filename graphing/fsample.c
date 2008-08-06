#include "fsample.h"

#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

static fsample_t version = 1;

static void fscorefree(void *arg) {
  fsample *f = (fsample *)arg;
  munmap(f->header, f->len);
}

fsample *fsopen(char *filename, size_t samples, CoreHandlerAddFn chafn, CoreHandlerDelFn chdfn) {
  int flags = 0;
  int new = 0;
  fsample *f = (fsample *)malloc(sizeof(fsample));
  if(!f)
    return NULL;

  f->len = samples * sizeof(struct fsample_p) + sizeof(struct fsample_header);
  f->fd = open(filename, O_RDWR|O_CREAT, 0600);

  if(!lseek(f->fd, 0, SEEK_END))
    new = 1;

  if((f->fd == -1) || (lseek(f->fd, f->len - 1, SEEK_SET) == -1)) {
    free(f);
    return NULL;
  }
  write(f->fd, "", 1);

  flags = MAP_SHARED;
#ifdef MAP_NOCORE
  flags|=MAP_NOCORE;
#endif

  f->header = mmap(NULL, f->len, PROT_READ|PROT_WRITE, flags, f->fd, 0);
  if(f->header == MAP_FAILED) {
    close(f->fd);
    free(f);
    return NULL;
  }

  if(!new && (memcmp(f->header->magic, "FSAMP", sizeof(f->header->magic)) || (version != f->header->version))) {
    munmap(f->header, f->len);
    close(f->fd);
    free(f);
    return NULL;
  }

  if(chafn && chdfn) {
    f->corehandler = chafn(fscorefree, f);
    f->corehandlerdel = chdfn;
  } else {
    f->corehandler = NULL;
    f->corehandlerdel = NULL;
  }

  memcpy(f->header->magic, "FSAMP", sizeof(f->header->magic));
  f->header->version = version;

  f->m = (struct fsample_p *)(f->header + 1);
  f->samples = samples;

  if(f->header->iteration == 0)
    f->header->iteration = 1;

  return f;
}

void fsclose(fsample *f) {
  munmap(f->header, f->len);
  close(f->fd);

  if(f->corehandlerdel && f->corehandler)
    (f->corehandlerdel)(f->corehandler);

  free(f);
}

int fsadd_m(fsample_m *f, char *filename, size_t freq, DeriveValueFn derive, void *tag) {
  fsample_m_entry *p = &f->entry[f->pos];

  p->f = fsopen(filename, f->samples / freq, f->chafn, f->chdfn);
  if(!p->f)
    return 0;

  p->freq = freq;
  p->derive = derive;
  p->tag = tag;
  f->pos++;
  return 1;
}

fsample_m *fsopen_m(size_t count, char *filename, size_t samples, CoreHandlerAddFn chafn, CoreHandlerDelFn chdfn) {
  fsample_m *n = (fsample_m *)malloc(sizeof(fsample_m) + (count + 1) * sizeof(fsample_m_entry));
  if(!n)
    return NULL;

  n->samples = samples;
  n->pos = 0;
  n->chafn = chafn;
  n->chdfn = chdfn;

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
