/* error.h:
 *
 * Error flagging routines 
 */

/* SEVERITY GUIDELINES:
 *
 * ERR_STOP:  Something so bad has happened that the whole service must
 *            stop.  Something like a malloc() error or similar magnitude
 *            error.
 *
 * ERR_FATAL: Something so bad has happened that the module cannot usefully
 *            function any more, but won't necessarily prevent other modules
 *            from working.
 *
 * ERR_ERROR: Something is wrong, perhaps a data structure is inconsistent
 *            or something similar has gone wrong internally.  This might
 *            indicate a larger problem but the module can continue working
 *            for now.
 *
 * ERR_WARNING: Something slightly out of the ordinary has happened (e.g. 
 *              the IRCD sent a bogus message).  The module can continue
 *              working though without serious harm.
 *
 * ERR_INFO: Not an error condition.  Something noteworthy has happened,
 *           like a major module has started up.  These will typically be
 *           seen all the time on most setups so try not to be too spammy.
 *
 * ERR_DEBUG: Not an error condition.  Something less significant has
 *            happened which might be of interest if someone is attempting
 *            to debug a problem.  Nevertheless try not to be too spammy.
 */

#define ERR_DEBUG    0
#define ERR_INFO     1
#define ERR_WARNING  2
#define ERR_ERROR    3
#define ERR_FATAL    4
#define ERR_STOP     5

void Error(char *source, int severity, char *reason, ... );
