/* proxyscanalloc.c */

#include "proxyscan.h"
#include "../core/nsmalloc.h"

scan *getscan() {
  return nsmalloc(POOL_PROXYSCAN, sizeof(scan));
}

void freescan(scan *sp) {
  nsfree(POOL_PROXYSCAN, sp);
}

cachehost *getcachehost() {
  return nsmalloc(POOL_PROXYSCAN, sizeof(cachehost));
}

void freecachehost(cachehost *chp) {
  nsfree(POOL_PROXYSCAN, chp);
} 

pendingscan *getpendingscan() {
  return nsmalloc(POOL_PROXYSCAN, sizeof(pendingscan));
}

void freependingscan(pendingscan *psp) {
  nsfree(POOL_PROXYSCAN, psp);
}

foundproxy *getfoundproxy() {
  return nsmalloc(POOL_PROXYSCAN, sizeof(foundproxy));
}

void freefoundproxy(foundproxy *fpp) {
  nsfree(POOL_PROXYSCAN, fpp);
}

extrascan *getextrascan() {
  return nsmalloc(POOL_PROXYSCAN, sizeof(extrascan));
}

void freeextrascan(extrascan *esp) {
  nsfree(POOL_PROXYSCAN, esp);
}

