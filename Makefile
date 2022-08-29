SOURCES = main.c  msg.c  udp_helper.c
HEADERS = msg.h  udp_helper.h

LDFLAGS = -fsanitize=address -pthread -ldl
CFLAGS = -Og -ggdb -Wall -Wextra -fsanitize=address \
		 -fno-omit-frame-pointer -pthread

.PHONY: main clean

main: $(SOURCES:.c=.o)
	$(CC) -o $@ $^ $(LDFLAGS)

$(SOURCES:.c=.o): $(HEADERS)

clean:
	rm -f main *.o