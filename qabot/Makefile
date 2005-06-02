
.PHONY: all
all: qabot.so

qabot.so: qabot.o qabot_bot.o qabot_chancommands.o qabot_commands.o qabot_dbase.o qabot_help.o
	ld -shared -Bdynamic -o $@ $^
