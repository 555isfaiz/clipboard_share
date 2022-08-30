SOURCES = main.c  msg.c  udp_helper.c
HEADERS = msg.h  udp_helper.h

ver = debug
 
ifeq ($(ver), debug)
CFLAGS = -g3 -ggdb -Wall -Wextra -fsanitize=address \
		 -fno-omit-frame-pointer -pthread
else
CFLAGS = -Og -Wall -Wextra -fsanitize=address \
		 -fno-omit-frame-pointer -pthread
endif

LDFLAGS = -fsanitize=address -pthread -ldl

.PHONY: main clean

main: $(SOURCES:.c=.o)
	$(CC) -o $@ $^ $(LDFLAGS)

$(SOURCES:.c=.o): $(HEADERS)

clean:
	rm -f main *.o