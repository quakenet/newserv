/*
  nterfacer newserv control module
  Copyright (C) 2004 Chris Porter.
*/

#ifndef __nterfacer_control_H
#define __nterfacer_control_H

#include "nterfacer.h"

#define ERR_TARGET_NOT_FOUND            0x01
#define ERR_CHANSTATS_STATS_NOT_FOUND   0x02
#define ERR_TOO_MANY_ARGS               0x03
#define ERR_UNKNOWN_ERROR               0x99

extern struct service_node *n_node;

#endif

