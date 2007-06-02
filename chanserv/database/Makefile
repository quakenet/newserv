
.PHONY: all
all: chanservdb.so chanservdb_updates.so

chanservdb_updates.so: chanservdb_updates.o
	ld -shared -Bdynamic ${LIBPGSQL} -o $@ $^

chanservdb.so: chanservdb.o chanservdb_updates.o chanservdb_alloc.o chanservdb_hash.o chanservdb_messages.o
	ld -shared -Bdynamic ${LIBPGSQL} -o $@ $^
        
