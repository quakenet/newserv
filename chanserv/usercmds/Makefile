# Automatically generated Makefile, do not edit.

.PHONY: all Makefile
all: Makefile chanserv_usercmds.so

Makefile:
	../mkcommandlist.pl chanserv_usercmds.so

chanserv_usercmds.so: accounthistory.o cleanupdb.o deluser.o domainmode.o info.o language.o listflags.o maillock.o rollbackaccount.o spewdb.o spewdomain.o spewemail.o spewpass.o suspenduser.o suspenduserlist.o unsuspenduser.o usercomment.o userflags.o whoami.o whois.o commandlist.o init.o 
	 ld -shared -Bdynamic -o $@ $^ 
