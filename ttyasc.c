#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include <sys/time.h>

/*
 * Calculates the difference between timevals t1 and t2, by subtracting the fields
 * of t2 with t1. A new timeval struct is returned on the stack.
 */
struct timeval diff(const struct timeval* t1, const struct timeval* t2) {
	struct timeval d;

	d.tv_sec  = t2->tv_sec  - t1->tv_sec;
	d.tv_usec = t2->tv_usec - t1->tv_usec;

	if (d.tv_usec < 0) {
		d.tv_sec--;
		d.tv_usec += 1000000;
	}
	return d;
}

/*
 * ============================================================================
 * ttyrec structures and functions.
 * ============================================================================
 */

/*
 * This struct contains a header entry in a ttyrec file. A header contains three
 * little endian integers: seconds, microseconds, and the length of the payload.
 */
struct ttyrec_frame {
	struct   timeval tv; // timeval struct, with seconds and microseconds.
	uint32_t len;        // length of the payload.
	char*    buf;        // contents
};

/*
 * Reads a ttyrec header entry from the file `fp'. and stores the contents in the
 * struct `f'. Will return `0' if the end of file is reached, or 1 if otherwise.
 * The seek pointer will be increased by calling this function.
 */
int ttyrec_frame_read(FILE* fp, struct ttyrec_frame* f) {
	uint32_t b[3];

	// first, read the three integers: the seconds, milliseconds and the size
	// of the ttyrec chunk.
	if ((fread(b, sizeof(uint32_t), 3, fp)) == 0) {
		return 0;
	}

	f->tv.tv_sec  = b[0];
	f->tv.tv_usec = b[1];
	f->len        = b[2];
	f->buf        = malloc(f->len * sizeof(char));

	// Next, read f->len amount of bytes into f->buf.
	if((fread(f->buf, sizeof(char), f->len, fp)) == 0) {
		return 0;
	}

	return 1;
}

/*
 * Frees the frame, and the buffer contents.
 */
void ttyrec_frame_free(struct ttyrec_frame* f) {
	if (f->buf != NULL) {
		free(f->buf);
		f->buf = NULL;
	}
	if (f != NULL) {
		free(f);
		f = NULL;
	}
}

void ttyrec_frame_write(const struct ttyrec_frame* f) {
	printf("\t\t\t\"");
	for(int i = 0; i < f->len; i++) {
		char c = f->buf[i];
		if (isprint(c)) {
			if (c == '"') {
				// escape a double quote to prevent premature json termination.
				printf("\\\"");
				continue;
			}
			if (c == '\\') {
				printf("\\\\");
				continue;
			}

			// printable characters can be appended as-is.
			printf("%c", c);
		} else if (c == '\r') {
			// carriage returns can be escaped as \r
			printf("\\r");
		} else if (c == '\n') {
			// linefeeds can be escaped as \n
			printf("\\n");
		} else {
			// all other characters are assumed to be non-printable, and are escaped
			// using a \uxxxx sequence to adhere to JSON format.
			printf("\\u%04x", c);
		}
	}
	printf("\"");
}

/*
 * Entry point!
 */
int main(int argc, char* argv[]) {
	FILE* f = fopen("ttyrecord", "r");
	if (f == NULL) {
		printf("Error opening file\n");
		exit(1);
	}

	bool first = true;

	float totaltime = 0.0f;

	struct timeval prev;
	prev.tv_sec = 0;
	prev.tv_usec = 0;

	// Poor man's JSON serializer. Just keeping it simple, stupid.
	printf("{\n");

	printf("\t\"version\": 1,\n");
	printf("\t\"width\": 80,\n");
	printf("\t\"height\": 24,\n");
	printf("\t\"duration\": 1,\n");
	printf("\t\"command\": 1,\n");
	printf("\t\"title\": 1,\n");
	printf("\t\"stdout\": [\n");
	while (true) {
		struct ttyrec_frame* h = malloc(sizeof(struct ttyrec_frame));

		if (ttyrec_frame_read(f, h) <= 0) {
			// no more frames to read.
			// TODO: im not happy with the malloc/free part. It looks a bit...
			// fishy to say the least. Malloc inside loop, but if nothing
			// can be read anymore, free it? All because I want to contain
			// the information inside a struct! Too much OOP has clouded
			// my mind... And too lazy right now to fix it so here you go.
			ttyrec_frame_free(h);
			printf("\n");
			break;
		}

		float timediff = 0.0f;
		if (!first) {
			struct timeval tvdiff = diff(&prev, &h->tv);
			timediff = tvdiff.tv_sec + (tvdiff.tv_usec / 1e6);
			totaltime += timediff;

			printf(",\n");
		} else {
			first = false;
		}

		printf("\t\t[\n");
		printf("\t\t\t%f,\n", timediff);
		ttyrec_frame_write(h);
		printf("\n\t\t]");

		prev.tv_sec = h->tv.tv_sec;
		prev.tv_usec = h->tv.tv_usec;

		ttyrec_frame_free(h);
	}
	printf("\t]\n");
	printf("}");

	fclose(f);
}
