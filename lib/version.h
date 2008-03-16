#ifndef __VERSION_H

#define __VERSION_H

#ifndef BUILDID
#define _BUILDID "unknown"
#else
#define _BUILDID "BUILDID"
#endif

#define MODULE_VERSION(id) const char *_version(void) { return (id[0]=='\0')?_BUILDID:(id "/" _BUILDID); };

#endif
