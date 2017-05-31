#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include <sys/time.h>

/*
 * ============================================================================
 * dynamic growable string buffer.
 * ============================================================================
 */

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

void string_append(struct string* s, const char* what, int len) {
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

/*
 * ============================================================================
 * asciicast frame functions.
 * ============================================================================
 */

/*
 * A single frame in asciicast format. Contains a pointer to a probable next 
 * frame, or NULL if this is the last one.
 */
struct asciicast_frame {
	float delay;
	struct string* contents;
	struct asciicast_frame* next;
};

struct asciicast_frame* asciicast_frame_init(float delay, const char* what, int len) {
	struct asciicast_frame* frame = malloc(sizeof(struct asciicast_frame));

	frame->delay    = delay;
	frame->next     = NULL;

	// Iterate over the `what' character array (can contain 0 bytes), and escape
	// and append accordingly into the string buffer.
	frame->contents = string_init();
	char derp[6 + 1];
	for(int i = 0; i < len; i++) {
		char c = what[i];
		if (isprint(c)) {
			if (c == '"') {
				// escape a double quote to prevent premature json termination.
				string_append(frame->contents, "\\\"", 2);
				continue;
			}
			if (c == '\\') {
				string_append(frame->contents, "\\\\", 2);
				continue;
			}

			// printable characters can be appended as-is.
			char filler[1];
			filler[0] = c;
			string_append(frame->contents, filler, 1);
		} else if (c == '\r') {
			// carriage returns can be escaped as \r
			string_append(frame->contents, "\\r", 2);
		} else if (c == '\n') {
			// linefeeds can be escaped as \n
			string_append(frame->contents, "\\n", 2);
		} else {
			// all other characters are assumed to be non-printable, and are escaped
			// using a \uxxxx sequence to adhere to JSON format.
			snprintf(derp, 7, "\\u%04x", c);
			string_append(frame->contents, derp, 6);
		}
	}

	//printf("Content len: %d, cap: %d, contents = \n", frame->contents->len, frame->contents->cap);
	//printf("%s", frame->contents->contents);
	//printf("\n");

	return frame;
}

void asciicast_frame_free(struct asciicast_frame* frame) {
	struct asciicast_frame* p = frame;
	while (p != NULL) {
		struct asciicast_frame* temp = p;
		p = p->next;
		string_free(temp->contents);
		printf("Freeing %f\n", temp->delay);
		free(temp);
	}
}

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
 * struct `h'. Will return `0' if the end of file is reached, or 1 if otherwise.
 * The seek pointer will be increased by calling this function.
 */
struct ttyrec_frame* ttyrec_frame_read(FILE* fp) {
	struct ttyrec_frame* f = malloc(sizeof(struct ttyrec_frame));

	uint32_t b[3];

	// first, read the three integers: the seconds, milliseconds and the size
	// of the ttyrec chunk.
	if ((fread(b, sizeof(uint32_t), 3, fp)) == 0) {
		return NULL;
	}

	f->tv.tv_sec  = b[0];
	f->tv.tv_usec = b[1];
	f->len        = b[2];
	f->buf        = malloc(f->len * sizeof(char));

	// Next, read f->len amount of bytes into f->buf.
	if((fread(f->buf, sizeof(char), f->len, fp)) == 0) {
		return NULL;
	}

	return f;
}

/*
 * Frees the frame, and the buffer contents.
 */
void ttyrec_frame_free(struct ttyrec_frame* f) {
	free(f->buf);
	free(f);
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

	fd_set rfds;

	bool first = true;

	float totaltime = 0.0f;

	struct timeval prev;
	prev.tv_sec = 0;
	prev.tv_usec = 0;

	struct asciicast_frame* frame_head = NULL;
	struct asciicast_frame* frame_current = NULL;

	while (true) {
		struct ttyrec_frame* h = ttyrec_frame_read(f);
		if (h == NULL) {
			break;
		}

		float timediff = 0.0f;
		if (!first) {
			struct timeval tvdiff = diff(&prev, &h->tv);
			timediff = tvdiff.tv_sec + (tvdiff.tv_usec / 1e6);
			totaltime += timediff;
			//select(1, &rfds, NULL, NULL, &tvdiff);
		} else {
			first = false;
		}

		struct asciicast_frame* next = asciicast_frame_init(timediff, h->buf, h->len);
		if (frame_current == NULL) {
			frame_head = next;
		} else {
			frame_current->next = next;
		}

		frame_current = next;

		//asciicast_frame_free(next);

		//printf("Diff, %f\n", timediff);
		//printf("================================================\n");
		//write(STDOUT_FILENO, contents, h.len);

		prev.tv_sec = h->tv.tv_sec;
		prev.tv_usec = h->tv.tv_usec;

		ttyrec_frame_free(h);
	}

	fclose(f);
	// 167920

	printf("{\n");

	struct asciicast_frame* p = frame_head;
	printf("\t\"version\": 1,\n");
	printf("\t\"width\": 80,\n");
	printf("\t\"height\": 24,\n");
	printf("\t\"duration\": 1,\n");
	printf("\t\"command\": 1,\n");
	printf("\t\"title\": 1,\n");
	printf("\t\"stdout\": [\n");
	while(p != NULL) {
		printf("\t\t[\n");
		printf("\t\t\t%f,\n", p->delay);
		printf("\t\t\t\"%s\"\n", p->contents->contents);

		if (p->next == NULL) {
			printf("\t\t]\n");
			break;
		}

		printf("\t\t],\n");
		p = p->next;
	}
	printf("\t]\n");
	printf("}");
}
