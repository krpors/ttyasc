#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include <sys/time.h>

struct string {
	char* contents;
	uint32_t len;
	uint32_t cap;
};

struct string* string_init() {
	struct string* s = malloc(sizeof(struct string));
	s->cap = 20;
	s->len = 0;
	s->contents = calloc(s->cap, sizeof(char));
	return s;
}

void string_free(struct string* s) {
	if (s == NULL) {
		return;
	}

	if (s->contents != NULL) {
		free(s->contents);
	}

	free(s);
}

void string_append(struct string* s, char* what, int len) {
	if (s->len + len >= s->cap) {
		s->cap += len;
		s->cap *= 2;
		s->contents = realloc(s->contents, s->cap);
		if (s->contents == NULL) {
			perror("Unable to reallocate for string buffer)");
			exit(1);
		}
	}
	
	memcpy(s->contents + s->len, what, len);
	s->len += len;
	memset(s->contents + s->len, '\0', s->cap - s->len);
}

struct asciicast_frame {
	float delay;
	char* contents;
	struct asciicast_frame* next;
};

struct asciicast {
	int    version;
	int    width;
	int    height;
	float  duration;
	char*  command;
	char*  title;
	//char** env;

	struct asciicast_frame* frames;
};

struct asciicast* asciicast_init() {
	struct asciicast* ac = malloc(sizeof(struct asciicast));

	ac->version  = 1;
	ac->width    = 80; // XXX: cannot determine width from ttyrec
	ac->height   = 24; // XXX: cannot determine height from ttyrec
	ac->duration = 0;
	ac->command  = malloc(80 * sizeof(char));
	ac->title    = malloc(80 * sizeof(char));
	ac->frames   = NULL;

	return ac;
}


/*
 * This struct contains a header entry in a ttyrec file. A header contains three
 * little endian integers: seconds, microseconds, and the length of the payload.
 */
struct ttyrec_header {
	struct timeval tv; // timeval struct, with seconds and microseconds.
	uint32_t len;      // length of the payload.
};

/*
 * Calculates the difference between timevals t1 and t2, by subtracting the fields
 * of t2 with t1. A new timeval struct is returned.
 */
struct timeval diff(struct timeval t1, struct timeval t2) {
	struct timeval d;
	d.tv_sec  = t2.tv_sec  - t1.tv_sec;
	d.tv_usec = t2.tv_usec - t1.tv_usec;

	if (d.tv_usec < 0) {
		d.tv_sec--;
		d.tv_usec += 1000000;
	}
	return d;
}


/*
 * Reads a ttyrec header entry from the file `fp'. and stores the contents in the
 * struct `h'. Will return `0' if the end of file is reached, or 1 if otherwise.
 */
int read_ttyrec_header(FILE* fp, struct ttyrec_header* h) {
	uint32_t b[3];

	if ((fread(b, sizeof(uint32_t), 3, fp)) == 0) {
		return 0;
	}

	h->tv.tv_sec  = b[0];
	h->tv.tv_usec = b[1];
	h->len  = b[2];

	return 1;
}

void do_trick(struct string* s, char* what, int len) {
	char derp[6 + 1];
	for(int i = 0; i < len; i++) {
		char c = what[i];
		if (isprint(c)) {
			char filler[1];
			filler[0] = c;
			string_append(s, filler, 1);
		} else if(c == '\r') {
			string_append(s, "\\r", 2);
		} else if(c == '\n') {
			string_append(s, "\\n", 2);
		} else {
			snprintf(derp, 7, "\\u%04x", c);
			string_append(s, derp, 6);
		}
	}
	printf("Content len: %d, cap: %d, contents = \n", s->len, s->cap);
	printf("%s", s->contents);
	//write(STDOUT_FILENO, s->contents, s->len);
	printf("\n");
}

int main(int argc, char* argv[]) {
	FILE* f = fopen("ttyrecord", "r");
	if (f == NULL) {
		printf("Error opening file\n");
		exit(1);
	}

	fd_set rfds;

	bool first = true;

	float totaltime = 0.0f;

	struct timeval prev;
	prev.tv_sec = 0;
	prev.tv_usec = 0;

	struct ttyrec_header h;
	int x;
	while ((x = read_ttyrec_header(f, &h)) == 1) {
		char* contents = malloc(h.len * sizeof(char));
		fread(contents, sizeof(char), h.len, f);

		float timediff = 0.0f;
		if (!first) {
			struct timeval tvdiff = diff(prev, h.tv);
			timediff = tvdiff.tv_sec + (tvdiff.tv_usec / 1e6);
			totaltime += timediff;

			struct string* s = string_init();
			do_trick(s, contents, h.len);
			string_free(s);
			//select(1, &rfds, NULL, NULL, &tvdiff);
		} else {
			first = false;
		}

		printf("================================================\n");
		printf("Diff, %f\n", timediff);
		//write(STDOUT_FILENO, contents, h.len);

		prev = h.tv;
		free(contents);
	}

	fclose(f);

	printf("\nEND OF RECORDING\n");
	printf("Total time: %f\n", totaltime);
}
