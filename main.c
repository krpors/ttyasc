#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>

#include <sys/time.h>

struct header {
	struct timeval tv;
	uint32_t len;
};

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


int read_header(FILE* fp, struct header* h) {
	//uint32_t b[3];
	uint32_t b[3];

	if ((fread(b, sizeof(uint32_t), 3, fp)) == 0) {
		return 0;
	}

	h->tv.tv_sec  = b[0];
	h->tv.tv_usec = b[1];
	h->len  = b[2];

	return 1;
}

int main(int argc, char* argv[]) {
	FILE* f = fopen("ttyrecord", "r");
	if (f == NULL) {
		printf("Error opening file\n");
		exit(1);
	}

	fd_set rfds;

	bool first = true;

	struct timeval prev;
	prev.tv_sec = 0;
	prev.tv_usec = 0;

	struct header h;
	int x;
	while ((x = read_header(f, &h)) == 1) {
		//printf("header: %d, %6d, %5d\n", (uint32_t) h.tv.tv_sec, (uint32_t) h.tv.tv_usec, h.len);

		char* contents = malloc(h.len * sizeof(char));
		fread(contents, sizeof(char), h.len, f);

		if (!first) {
			struct timeval tvdiff = diff(prev, h.tv);

			select(1, &rfds, NULL, NULL, &tvdiff);
		}
		write(STDOUT_FILENO, contents, h.len);
		first = false;

		prev = h.tv;
		free(contents);

		// skip len bytes
		//fseek(f, h.len, SEEK_CUR);
	}

	fclose(f);

	printf("\nEND OF RECORDING\n");
}
