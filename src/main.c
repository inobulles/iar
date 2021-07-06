#include <iar.h>

#include <string.h>
#include <stdlib.h>

typedef enum {
	MODE_UNKNOWN,
	MODE_PACK,
	MODE_UNPACK,
} mode_t;

int main(int argc, char** argv) {
	if (argc == 1) {
		fprintf(stderr, "ERROR No arguments provided\n");
		return 1;
	}
	
	mode_t mode = MODE_UNKNOWN;
	uint64_t page_bytes = IAR_DEFAULT_PAGE_BYTES;
	
	char* pack_output = "output.iar";
	char* unpack_output = "output";
	
	char* unpack_file = (char*) 0;
	char* pack_directory = (char*) 0;
	
	for (int i = 1; i < argc; i++) {
		if (strncmp(argv[i], "--", 2) == 0) { // argument is option
			char* option = argv[i] + 2;
			
			if (strcmp(option, "align") == 0) {
				if ((page_bytes = atoll(argv[++i])) < 1) {
					fprintf(stderr, "ERROR Provided page size (%lu) is too small\n", page_bytes);
					return 1;
				}
				
			} else if (strcmp(option, "version") == 0) {
				printf("Command-line utility supports up to IAR version %lu\n", IAR_VERSION);
				return 0;
				
			} else if (strcmp(option, "output") == 0) {
				pack_output = unpack_output = argv[++i];
				
			} else if (strcmp(option, "pack") == 0) {
				if (mode == MODE_UNPACK) {
					fprintf(stderr, "ERROR '--unpack' has already been passed\n");
					return 1;
				}
				
				mode = MODE_PACK;
				pack_directory = argv[++i];
				
			} else if (strcmp(option, "unpack") == 0) {
				if (mode == MODE_PACK) {
					fprintf(stderr, "ERROR '--pack' has already been passed\n");
					return 1;
				}
				
				mode = MODE_UNPACK;
				unpack_file = argv[++i];
				
			} else {
				fprintf(stderr, "ERROR Option '--%s' is unknown. Check README.md or go to https://github.com/inobulles/iar/blob/master/README.md to see a list of available options\n", option);
				return 1;
			}
			
		} else {
			fprintf(stderr, "ERROR Unexpected argument '%s'\n", argv[i]);
			return 1;
		}
	}
	
	iar_file_t iar;

	if (mode == MODE_PACK) {
		if (iar_open_write(&iar, pack_output)) {
			return 1;
		}

		iar.header.page_bytes = page_bytes;
		
		if (iar_pack(&iar, pack_directory, (const char*) 0)) {
			iar_close(&iar);
			return 1;
		}

		iar_write_header(&iar);
		iar_close(&iar);
		
	} else if (mode == MODE_UNPACK) {
		if (iar_open_read(&iar, unpack_file)) {
			return 1;
		}

		if (iar_unpack(&iar, unpack_output)) {
			iar_close(&iar);
			return 1;
		}

		iar_close(&iar);
	}

	return 0;
}
