# Automatically generated Makefile, do not edit.

.PHONY: all Makefile
all: Makefile chanserv_usercmds.so

Makefile:
	../mkcommandlist.pl chanserv_usercmds.so

chanserv_usercmds.so: deluser.o info.o language.o listflags.o spewdb.o spewemail.o spewpass.o suspenduser.o suspenduserlist.o unsuspenduser.o usercomment.o userflags.o whois.o commandlist.o 
	 ld -shared -Bdynamic -o $@ $^ 
