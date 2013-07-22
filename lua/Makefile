include ../build.mk

CFLAGS+=$(INCLUA) $(INCDBAPI)
LDFLAGS+=$(LIBLUA) $(LIBDBAPI)

.PHONY: all
all: lua.so nterfacer_lua.so

lua.so: lua.o luacommands.o luacontrol.o luabot.o lualocal.o luadebug.o luadb.o luasocket.o luacrypto.o luascheduler.o
	@-ldline-@ $(LDFLAGS)

nterfacer_lua.so: nterfacer_lua.o
	@-ldline-@
