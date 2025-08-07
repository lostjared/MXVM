PREFIX=/usr/local
SOURCE=tetris.mxvm
LDFLAGS= -lSDL2 -lSDL2_ttf

all:
	PATH=$(PREFIX)/bin:$$PATH LDFLAGS="$(LDFLAGS)" mxvmc --path $(PREFIX)/lib $(SOURCE) --action compile

clean:
	rm -f *.s
