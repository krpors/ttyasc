CPPFLAGS = -DNDEBUG
CFLAGS = -std=c99 -Wall -Wextra -pedantic -O3 -MMD -MP
#LDFLAGS = -O3

objects := ttyasc.o

all: ttyasc
ttyasc: $(objects)

static: all
static: LDFLAGS += -static

clean:
	$(RM) $(objects) $(objects:.o=.d) ttyasc

-include $(objects:.o=.d);

.PHONY: all clean
