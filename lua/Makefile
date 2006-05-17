
.PHONY: all
all: lua.so

lua.so: lua.o luacommands.o luacontrol.o luabot.o lualocal.o
	ld -shared -Bdynamic -o $@ $^ ${LIBLUA} ${ADDLIB}
