#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

struct chunk {
	uint32_t sec;
	uint32_t usec;
	uint32_t len;
	char* payload;

	struct chunk* next;
};

struct chunk* chunk_create() {
	struct chunk* e = malloc(sizeof(struct chunk));
	e->payload = NULL;
	e->next = NULL;
	return e;
}

void chunk_free(struct chunk* c) {
	while(c != NULL) {
		struct chunk* next = c->next;
		free(c->payload);
		free(c);
		c = next;
	}
}

const int CHUNK_HEADER_SIZE = 12;

int main(int argc, char* argv[]) {
	FILE* f = fopen("ttyrecord", "r");
	if (f == NULL) {
		printf("Error opening file\n");
		exit(1);
	}

	int i = 0;
	int chunkheader[CHUNK_HEADER_SIZE];

	bool newtoken = true;
	int read;

	// head of the linked list
	struct chunk* head = chunk_create();
	struct chunk* current = head;

	while ((read = fgetc(f)) != EOF) {
		chunkheader[i] = read;
		i++;

		if (i >= CHUNK_HEADER_SIZE) {
			// parse the header
			int sec  = (chunkheader[3]  << 24) | (chunkheader[2]  << 16) | (chunkheader[1] << 8) | (chunkheader[0]);
			int usec = (chunkheader[7]  << 24) | (chunkheader[6]  << 16) | (chunkheader[5] << 8) | (chunkheader[4]);
			int len  = (chunkheader[11] << 24) | (chunkheader[10] << 16) | (chunkheader[9] << 8) | (chunkheader[8]);
			i = 0;

			char* payload = malloc(len * sizeof(char));
			for (int x = 0; x < len; x++) {
				payload[x] = (char) fgetc(f);
			}

			current->sec = sec;
			current->usec = usec;
			current->len = len;
			current->payload = payload;

			struct chunk* next = chunk_create();
			current->next = next;
			current = next;
		}
	}

	fclose(f);

	struct chunk* iter = head;
	while (iter != NULL) {
		if (iter->len > 0) {
			printf("%s", iter->payload);
		}
		iter = iter->next;
	}

	chunk_free(head);

	printf("\nEND OF RECORDING\n");
}
