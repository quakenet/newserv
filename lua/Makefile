
.PHONY: all
all: lua.so

lua.so: lua.o luacommands.o luacontrol.o luabot.o
	ld -shared -Bdynamic -o $@ $^ ${LIBLUA} ${ADDLIB}
