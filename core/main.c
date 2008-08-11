#include "../lib/sstring.h"
#include "events.h"
#include "schedule.h"
#include "hooks.h"
#include "modules.h"
#include "config.h"
#include "error.h"
#include "nsmalloc.h"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>

int newserv_shutdown_pending;
int newserv_sigusr1_pending;

void initseed();
void init_logfile();
void siginthandler(int sig);
void sigusr1handler(int sig);
void sigsegvhandler(int sig);
void handlecore(void);

static void (*oldsegv)(int);

int main(int argc, char **argv) {
  initseed();
  inithooks();
  inithandlers();
  initschedule();

  init_logfile();
  
  initsstring();
  
  if (argc>1) {
    initconfig(argv[1]);
  } else {  
    initconfig("newserv.conf");
  }

  /* Loading the modules will bring in the bulk of the code */
  initmodules();
  signal(SIGINT, siginthandler);
  signal(SIGUSR1, sigusr1handler);
  oldsegv = signal(SIGSEGV, sigsegvhandler);

  /* Main loop */
  for(;;) {
    handleevents(10);  
    doscheduledevents(time(NULL));
    if (newserv_shutdown_pending) {
      newserv_shutdown();
      break;
    }

    if (newserv_sigusr1_pending) {
      signal(SIGUSR1, sigusr1handler);
      triggerhook(HOOK_CORE_SIGUSR1, NULL);
      newserv_sigusr1_pending=0;
    }
  }  

  freeconfig();
  finisstring();  

  nsexit();
}

/*
 * seed the pseudo-random number generator, rand()
 */
void initseed() {
  struct timeval t;

  gettimeofday(&t, NULL);
  srand(t.tv_usec);
}

void siginthandler(int sig) {
  newserv_shutdown_pending = 1;
}

void sigusr1handler(int sig) {
  newserv_sigusr1_pending = 1;
}

void sigsegvhandler(int sig) {
  handlecore();

  oldsegv(sig);
}

