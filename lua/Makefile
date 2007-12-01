
.PHONY: all
all: lua.so nterfacer_lua.so

lua.so: lua.o luacommands.o luacontrol.o luabot.o lualocal.o luadebug.o luadb.o
	ld -shared -Bdynamic -o $@ $^ ${LIBLUA} ${ADDLIB}

nterfacer_lua.so: nterfacer_lua.o
	ld -shared -Bdynamic -o $@ $^

