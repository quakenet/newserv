#ifndef __VERSION_H

#define __VERSION_H

#ifndef BUILDID
#define _BUILDID "unknown"
#else
#define _BUILDID BUILDID
#endif

#define MODULE_VERSION(id) const char *_version(const char **version) { *version=(id[0]=='\0')?NULL:id; return _BUILDID; }

#endif
