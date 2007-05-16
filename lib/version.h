#ifndef __VERSION_H

#define __VERSION_H

#ifndef BUILDID
#define BUILDID "unknown"
#endif

#define MODULE_VERSION(id) const char *_version(void) { return (id[0]=='\0')?BUILDID:(id "/" BUILDID); };

#endif
