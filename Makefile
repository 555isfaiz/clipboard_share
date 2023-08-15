PLATFORM_SOURCES =  

EXTRA_FLAGS = 

UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
        PLATFORM_SOURCES += linux.c
		EXTRA_FLAGS += -lX11 -lXfixes
    endif
    ifeq ($(UNAME_S),Darwin)
        PLATFORM_SOURCES += mac.c
		EXTRA_FLAGS += -framework CoreGraphics -framework CoreFoundation -framework ApplicationServices
    endif

SOURCES = main.c  msg.c  connection.c  $(PLATFORM_SOURCES)
HEADERS = utils.h  msg.h  connection.h  clipboard.h

ifdef DEBUG
CFLAGS = -g3 -ggdb3 -Wall -fsanitize=address -fstack-protector-all \
		-fno-omit-frame-pointer -pthread
else
CFLAGS = -O3 -Wall -fsanitize=address -fstack-protector-all \
		-fno-omit-frame-pointer -pthread
endif

LDFLAGS = -fsanitize=address -pthread -ldl $(EXTRA_FLAGS)

.PHONY: clipboardshare clean

clipboardshare: $(SOURCES:.c=.o)
	$(CC) -o $@ $^ $(LDFLAGS)

$(SOURCES:.c=.o): $(HEADERS)

clean:
	rm -f clipboardshare *.o

install:
	sudo cp clipboardshare /usr/bin/clipboardshare
