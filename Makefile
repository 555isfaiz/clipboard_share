PLATFORM_SOURCES =  
PLATFORM_HEADERS = 

EXTRA_FLAGS = 

UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
        PLATFORM_SOURCES += linux.c
		PLATFORM_HEADERS += linux.h
		EXTRA_FLAGS += -lX11 -lXfixes
    endif
    ifeq ($(UNAME_S),Darwin)
        PLATFORM_SOURCES += mac.c
		PLATFORM_HEADERS += mac.h
		EXTRA_FLAGS += -framework CoreGraphics -framework CoreFoundation -framework ApplicationServices
    endif

SOURCES = main.c  msg.c  udp_helper.c  $(PLATFORM_SOURCES)
HEADERS = msg.h  udp_helper.h  $(PLATFORM_HEADERS)

ver = debug
 
ifeq ($(ver), debug)
CFLAGS = -g3 -ggdb -Wall -fsanitize=address \
		 -fno-omit-frame-pointer -pthread
else
CFLAGS = -Og -Wall -fsanitize=address \
		 -fno-omit-frame-pointer -pthread
endif

LDFLAGS = -fsanitize=address -pthread -ldl $(EXTRA_FLAGS)

.PHONY: main clean

clipboardshare: $(SOURCES:.c=.o)
	$(CC) -o $@ $^ $(LDFLAGS)

$(SOURCES:.c=.o): $(HEADERS)

clean:
	rm -f main *.o

install:
	sudo cp clipboardshare /usr/bin/clipboardshare