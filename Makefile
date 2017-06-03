all:
	gcc -ggdb main.c -o main

.PHONY: clean
clean:
	rm main
