CPPFLAGS = -DNDEBUG
CFLAGS = -std=c99 -Wall -Wextra -pedantic -O3 -MMD -MP
#LDFLAGS = -O3

objects := ttyasc.o

PREFIX ?= /usr/local
bindir = /bin

all: ttyasc
ttyasc: $(objects)

static: all
static: LDFLAGS += -static

install:
	install -Dm755 -s ./ttyasc -t $(DESTDIR)$(PREFIX)$(bindir)

clean:
	$(RM) $(objects) $(objects:.o=.d) ttyasc

-include $(objects:.o=.d);

.PHONY: all clean static install
