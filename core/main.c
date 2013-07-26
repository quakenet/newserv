#include "../lib/sstring.h"
#include "../lib/valgrind.h"
#include "events.h"
#include "schedule.h"
#include "hooks.h"
#include "modules.h"
#include "config.h"
#include "error.h"
#include "nsmalloc.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>

void initseed();
void init_logfile();
void fini_logfile();
void siginthandler(int sig);
void sigusr1handler(int sig);
void sigsegvhandler(int sig);
void sighuphandler(int sig);
void handlecore(void);
void handlesignals(void);

int newserv_shutdown_pending;
static int newserv_sigint_pending, newserv_sigusr1_pending, newserv_sighup_pending;
static void (*oldsegv)(int);

int main(int argc, char **argv) {
  char *config = "newserv.conf";

  nsinit();

  initseed();
  inithooks();
  inithandlers();
  initschedule();

  init_logfile();
  
  initsstring();
  
  if (argc>1) {
    if (strcmp(argv[1], "--help")==0) {
      printf("Syntax: %s [config]\n", argv[0]);
      puts("");
      printf("Default configuration file unless specified: %s\n", config);

      return 0;
    }

    config = argv[1];
  }

  initconfig(config);

  /* Loading the modules will bring in the bulk of the code */
  initmodules();
  signal(SIGINT, siginthandler);
  signal(SIGUSR1, sigusr1handler);
  signal(SIGHUP, sighuphandler);
  oldsegv = signal(SIGSEGV, sigsegvhandler);

  /* Main loop */
  for(;;) {
    handleevents(10);  
    doscheduledevents(time(NULL));

    if (newserv_shutdown_pending) {
      newserv_shutdown();
      break;
    }

    handlesignals();
  }  

  freeconfig();
  finisstring();  

  fini_logfile();
  finischedule();
  finihandlers();

  nsexit();

  if (RUNNING_ON_VALGRIND) {
    /* We've already manually called _fini for each of the modules. Make sure
     * it's not getting called again when the libraries are unloaded. */
    _exit(0);
  }

  return 0;
}

void handlesignals(void) {
  if (newserv_sigusr1_pending) {
    signal(SIGUSR1, sigusr1handler);
    Error("core", ERR_INFO, "SIGUSR1 received.");
    triggerhook(HOOK_CORE_SIGUSR1, NULL);
    newserv_sigusr1_pending=0;
   }

  if (newserv_sighup_pending) {
    signal(SIGHUP, sighuphandler);
    Error("core", ERR_INFO, "SIGHUP received, rehashing...");
    triggerhook(HOOK_CORE_REHASH, (void *)1);
    newserv_sighup_pending=0;
  }

  if (newserv_sigint_pending) {
    Error("core", ERR_INFO, "SIGINT received, terminating.");
    triggerhook(HOOK_CORE_SIGINT, NULL);
    newserv_sigint_pending=0;
    newserv_shutdown_pending=1;
  }
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
  newserv_sigint_pending = 1;
}

void sigusr1handler(int sig) {
  newserv_sigusr1_pending = 1;
}

void sighuphandler(int sig) {
  newserv_sighup_pending = 1;
}

void sigsegvhandler(int sig) {
  handlecore();

  oldsegv(sig);
}
