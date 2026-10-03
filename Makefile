CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra
LDLIBS = -lX11 -lm

midnight: midnight.c
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)

install: midnight
	./install.sh

clean:
	rm -f midnight

.PHONY: install clean
