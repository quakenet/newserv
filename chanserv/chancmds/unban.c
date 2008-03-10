/*
 * CMDNAME: unban
 * CMDLEVEL: QCMD_AUTHED | QCMD_ALIAS
 * CMDARGS: 2
 * CMDDESC: Removes a single ban from a channel.
 * CMDFUNC: csc_dobandel
 * CMDPROTO: int csc_dobandel(void *source, int cargc, char **cargv);
 * CMDHELP: Usage: UNBAN <channel> <ban>
 * CMDHELP: Removes the specified persistent or channel ban, where:
 * CMDHELP: channel - the channel to use
 * CMDHELP: ban     - either a ban mask (nick!user@host), or #number (see BANLIST)
 * CMDHELP: Removing channel bans requires operator (+o) access on the named channel.
 * CMDHELP: Removing persistent bans requires master (+m) access on the named channel.
 * CMDHELP: UNBAN is an alias for BANDEL.
 */

/* This is an alias for BANDEL */

