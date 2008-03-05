# Automatically generated Makefile, do not edit.

.PHONY: all Makefile
all: Makefile chanserv_authcmds.so

Makefile:
	../mkcommandlist.pl chanserv_authcmds.so

chanserv_authcmds.so: auth.o authhistory.o challenge.o challengeauth.o email.o hello.o login.o newpass.o requestmasterpassword.o requestpassword.o setemail.o setmasterpassword.o setpassword.o commandlist.o 
	 ld -shared -Bdynamic -o $@ $^ 
