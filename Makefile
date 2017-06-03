all:
	gcc -ggdb ttyasc.c -o ttyasc

static:
	gcc ttyasc.c -static -o ttyasc

.PHONY: clean
clean:
	rm ttyasc
