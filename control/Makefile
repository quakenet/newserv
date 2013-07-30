include ../build.mk

.PHONY: all
all: control.so control_commands.so control_management.so

control-lite.so: control-lite.o

control.so: control.o control_db.o control_policy.o

control_commands.so: control_commands.o

control_management.so: control_management.o
