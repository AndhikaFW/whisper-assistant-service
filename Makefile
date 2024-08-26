LIBSPATH = lib/
CC = g++
CFLAGS = -Wall

default: 
	cp main.cpp $(LIBSPATH)whisper.cpp/examples/command/command.cpp; \
	cd $(LIBSPATH)whisper.cpp; \
	make command; \
	cp command ../../main; \
	cp -r models ../../

example:
	mkdir bin; \
	$(CC) $(CFLAGS) -o bin/output examples/output.cpp

clean: 
	rm -rvf main
	cd $(LIBSPATH)whisper.cpp; make clean \
