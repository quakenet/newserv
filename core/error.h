/* error.h:
 *
 * Error flagging routines 
 */


#define ERR_DEBUG    0
#define ERR_INFO     1
#define ERR_WARNING  2
#define ERR_ERROR    3
#define ERR_FATAL    4

void Error(char *source, int severity, char *reason, ... );

