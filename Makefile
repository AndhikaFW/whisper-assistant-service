LIBSPATH = lib/

default: 
	cp main.cpp $(LIBSPATH)whisper.cpp/examples/command/command.cpp; \
	cd $(LIBSPATH)whisper.cpp; \
	make command; \
	cp command ../../main; \
	cp -r models ../../

clean: 
	rm -rvf main
	cd $(LIBSPATH)whisper.cpp; make clean \
