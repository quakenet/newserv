/* hooks to the N system, most are O(n) */
#ifndef HHOOKS_H
#define HHOOKS_H

void helpmod_hook_quit(int, void*);
void helpmod_hook_part(int, void*);
void helpmod_hook_join(int, void*);
void helpmod_hook_channel_newnick(int, void*);
void helpmod_hook_channel_lostnick(int, void*);
void helpmod_hook_nick_lostnick(int, void*);
void helpmod_hook_nick_account(int, void*);

void helpmod_hook_channel_opped(int, void*);
void helpmod_hook_channel_deopped(int, void*);
void helpmod_hook_channel_voiced(int, void*);
void helpmod_hook_channel_devoiced(int, void*);

void helpmod_hook_channel_topic(int, void*);

void helpmod_registerhooks(void);
void helpmod_deregisterhooks(void);

#endif
