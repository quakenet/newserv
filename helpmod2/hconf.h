/* H configuration (database) file */
#ifndef HCONF_H
#define HCONF_H

#include "huser.h"
#include "hchannel.h"
#include "haccount.h"
#include "hban.h"
#include "hlamer.h"
#include "hterm.h"
#include "hstat.h"
#include "hticket.h"

int helpmod_config_read(const char *);
int helpmod_config_write(const char *);

int helpmod_config_read_account(FILE *);
int helpmod_config_write_account(FILE *, haccount*);

int helpmod_config_read_channel(FILE *);
int helpmod_config_write_channel(FILE *, hchannel*);

int helpmod_config_read_ban(FILE *);
int helpmod_config_write_ban(FILE *, hban*);

int helpmod_config_read_hlc_profile(FILE *);
int helpmod_config_write_hlc_profile(FILE *, hlc_profile*);

int helpmod_config_read_term(FILE *, hterm**);
int helpmod_config_write_term(FILE *, hterm*);

int helpmod_config_read_chanstats(FILE *, hstat_channel*);
int helpmod_config_write_chanstats(FILE *, hstat_channel*);

int helpmod_config_read_stats(FILE *, hstat_account*);
int helpmod_config_write_stats(FILE *, hstat_account*);

int helpmod_config_read_globals(FILE *);
int helpmod_config_write_globals(FILE *);

int helpmod_config_read_report(FILE *);
int helpmod_config_write_report(FILE *, hchannel*);

int helpmod_config_read_ticket(FILE *);
int helpmod_config_write_ticket(FILE *, hticket*, hchannel*);

int helpmod_config_read_version(FILE *);
int helpmod_config_write_version(FILE *);

void helpmod_config_scheduled_events(void);

/* FILEFORMAT

Any line starting with '%' is considered a comment and ignored
Empty lines between sections are allowed
White space is allowed in the entries

*/

#endif
