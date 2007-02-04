/* authhash.h */

#ifndef __AUTHHASH_H
#define __AUTHHASH_H

nick *getnickbyauth(const char *auth);

extern int nextbyauthext;
#define nextbyauth(x) (nick *)(x->exts[nextbyauthext])
#define authhashloaded() (findnickext("authhash") != -1)

#endif
