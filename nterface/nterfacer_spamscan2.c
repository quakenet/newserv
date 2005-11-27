/*
  nterfacer spamscan2 module
  Copyright (C) 2005 Chris Porter.

*/

#include "../localuser/localuserchannel.h"
#include "../channel/channel.h"

#include "../spamscan2/spamscan2.h"

#include "library.h"
#include "nterfacer.h"

#define ERR_TARGET_NOT_FOUND            0x01
#define ERR_CHANNEL_NOT_REGISTERED      0x02
#define ERR_PROFILE_DOES_NOT_EXIST      0x03
#define ERR_BAD_FLAGS                   0x04

int handle_getprofile(struct rline *li, int argc, char **argv);
int handle_setprofile(struct rline *li, int argc, char **argv);

struct service_node *s_node;

void _init(void) {
  s_node = register_service("S");
  if(!s_node)
    return;

  register_handler(s_node, "getprofile", 1, handle_getprofile);
  register_handler(s_node, "setprofile", 3, handle_setprofile);
}

void _fini(void) {
  if(s_node)
    deregister_service(s_node);
}

int handle_getprofile(struct rline *li, int argc, char **argv) {
  spamscan_channelsettings *cs;
  
  if(argv[0][0] != '#')
    return ri_error(li, ERR_TARGET_NOT_FOUND, "Channel not found");

  cs = spamscan_getchannelsettings(argv[0], 0);
  if(!cs)
    return ri_error(li, ERR_CHANNEL_NOT_REGISTERED, "Channel not registered");

  ri_append(li, "%s,%s", cs->cp?cs->cp->profilename:"unknown", printflags(cs->flags, s_cfflags));
  return ri_final(li);
}

int handle_setprofile(struct rline *li, int argc, char **argv) {
  spamscan_channelsettings *cs;
  spamscan_channelprofile *cp;
  
  if(argv[0][0] != '#')
    return ri_error(li, ERR_TARGET_NOT_FOUND, "Channel not found");

  cs = spamscan_getchannelsettings(argv[0], 0);
  if(!cs)
    return ri_error(li, ERR_CHANNEL_NOT_REGISTERED, "Channel not registered");

  cp = spamscan_getchannelprofile(argv[1], 0);
  if(!cp)
    return ri_error(li, ERR_PROFILE_DOES_NOT_EXIST, "Profile does not exist");

  if(setflags(&cs->flags, SPAMSCAN_CF_ALL, argv[2], s_cfflags, REJECT_UNKNOWN) != REJECT_NONE)
    return ri_error(li, ERR_BAD_FLAGS, "Bad flags");

  /* TODO: beat Cruicky till he refactors this functionality */
  spamscan_checkchannelpresence(findchannel(cs->channelname)); /* what if findchannel returns NULL? */
  cs->cp = cp;
  cs->modified = time(NULL);
  cs->modifiedby = spamscan_getaccountsettings("nterfacer", 1);
  spamscan_updatechanneldb(cs);

  ri_append(li, "Done.");
  return ri_final(li);
}

