

.PHONY: all build clean

all: build

build: cdeps

cdeps: cdeps.o

install: build
	cp cdeps ~/bin/

clean:
	rm *.o
	rm cdeps
