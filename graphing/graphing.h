#ifndef __GRAPHING_H
#define __GRAPHING_H

#include "../server/server.h"
#include "../graphing/fsample.h"

extern fsample_m *servergraphs[MAXSERVERS];
int servermonitored(int servernum);

#define GRAPHING_DATASETS 5
#define GRAPHING_RESOLUTION 5

#define PERMINUTE (60 / GRAPHING_RESOLUTION)
#define SAMPLES PERMINUTE * 60 * 24 * 7 * 4 * 12

#endif
