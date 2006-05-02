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
#define ERR_MEMORY_ERROR                0x05
#define ERR_CHANNEL_ALREADY_REGISTERED  0x06

int handle_getprofile(struct rline *li, int argc, char **argv);
int handle_setprofile(struct rline *li, int argc, char **argv);
int handle_delchan(struct rline *li, int argc, char **argv);
int handle_addchan(struct rline *li, int argc, char **argv);

struct service_node *s_node;

void _init(void) {
  s_node = register_service("S2");
  if(!s_node)
    return;

  register_handler(s_node, "getprofile", 1, handle_getprofile);
  register_handler(s_node, "setprofile", 3, handle_setprofile);
  register_handler(s_node, "delchan", 1, handle_delchan);
  register_handler(s_node, "addchan", 2, handle_addchan);
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

  ri_append(li, "%s", cs->cp?cs->cp->profilename:"unknown");
  ri_append(li, "%s", printflags(cs->flags, s_cfflags));
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
  cs->modifiedby = spamscan_getaccountsettings("nterfacer", 0); /* ugly hack, should be 1... */
  spamscan_updatechanneldb(cs);

  ri_append(li, "Done.");
  return ri_final(li);
}

int handle_delchan(struct rline *li, int argc, char **argv) {
  if(argv[0][0] != '#')
    return ri_error(li, ERR_TARGET_NOT_FOUND, "Channel not found");

  if(spamscan_deletechannelsettings(argv[0]))
    return ri_error(li, ERR_CHANNEL_NOT_REGISTERED, "Channel not registered");

  ri_append(li, "Done");
  return ri_final(li);
}

int handle_addchan(struct rline *li, int argc, char **argv) {
  spamscan_channelsettings *cs;
  spamscan_channelprofile *cp;
  flag_t flags = 0;
  chanindex *sc_index;

  if(argv[0][0] != '#')
    return ri_error(li, ERR_TARGET_NOT_FOUND, "Channel not found");

  if(spamscan_getchannelsettings(argv[0], 0))
    return ri_error(li, ERR_CHANNEL_ALREADY_REGISTERED, "Channel already registered");

  if((argc > 2) && (setflags(&flags, SPAMSCAN_CF_ALL, argv[2], s_cfflags, REJECT_UNKNOWN) != REJECT_NONE))
    return ri_error(li, ERR_BAD_FLAGS, "Bad flags");

  cp = spamscan_getchannelprofile(argv[1], 0);
  if(!cp)
    return ri_error(li, ERR_PROFILE_DOES_NOT_EXIST, "Profile does not exist");

  cs = spamscan_getchannelsettings(argv[0], 1);
  if(!cs)
    return ri_error(li, ERR_MEMORY_ERROR, "Memory allocation error");

  cs->cp = cp;
  cs->flags = flags;
  cs->modified = cs->added = time(NULL);
  cs->modifiedby = cs->addedby = spamscan_getaccountsettings("nterfacer", 0); /* ugly hack, should be 1... */

  sc_index = findorcreatechanindex(argv[0]);
  if(sc_index) {
    spamscan_channelext *ce = spamscan_getchanext(sc_index, 1);
    ce->cs = cs;

    if (s_nickname && !CFIsSuspended(cs) && sc_index->channel) {
      ce->joinedchan = 1;
      localjoinchannel(s_nickname, sc_index->channel);
      localgetops(s_nickname, sc_index->channel);
    }    
  }

  spamscan_insertchanneldb(cs);
  spamscan_log("ADDCHAN: %s added %s with flags %s", "nterfacer", argv[0], printflags(cs->flags, s_cfflags));

  ri_append(li, "Done");
  return ri_final(li);
}


