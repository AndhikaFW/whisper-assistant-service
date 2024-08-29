LIBSPATH = lib/
CC = g++
CFLAGS = -Wall

default: 
	cp src/main.cpp $(LIBSPATH)whisper.cpp/examples/command/command.cpp; \
	cd $(LIBSPATH)whisper.cpp; \
	make command; \
	cp command ../../main; \
	cp -r models ../../

example:
	if [ ! -d bin ]; then mkdir bin; fi; \
	$(CC) $(CFLAGS) -o bin/output examples/output.cpp; \
	printf '#!/usr/bin/bash\n\n%s' '../bin/output $$@' > scripts/output.sh
	chmod +x scripts/output.sh

clean: 
	rm -rvf main
	cd $(LIBSPATH)whisper.cpp; make clean \
