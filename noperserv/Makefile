
.PHONY: all
all: noperserv.so noperserv_commands.so

noperserv.so: noperserv.o noperserv_db.o noperserv_hooks.o noperserv_policy.o
	ld -shared -Bdynamic ${LIBPGSQL} -o $@ $^

noperserv_commands.so: noperserv_commands.o
	ld -shared -Bdynamic ${LIBPGSQL} -o $@ $^
