#include <iar.h>

#include <string.h>
#include <stdlib.h>

typedef enum {
	MODE_UNKNOWN,
	MODE_PACK,
	MODE_UNPACK,
	MODE_JSON,
} mode_t;

// there seems to be a bug with the 'getdirentries' syscall (554) in the FreeBSD kernel (refer to 'sys/sys/kern/vfs_syscalls.c')
// this syscall is used by 'readdir', and, consequentially, libiar
// for now, the solution seems to be to call a wrapper to 'readdir' from somewhere else, i.e. here

#include <dirent.h>

struct dirent* __readdir(DIR* dp) {
	return readdir(dp);
}

int main(int argc, char** argv) {
	if (argc == 1) {
		fprintf(stderr, "ERROR No arguments provided\n");
		return -1;
	}
	
	mode_t mode = MODE_UNKNOWN;
	uint64_t page_bytes = IAR_DEFAULT_PAGE_BYTES;
	
	char* pack_output = "output.iar";
	char* unpack_output = "output";
	
	char* unpack_file = NULL;
	char* pack_dir = NULL;
	char* pack_json = NULL;
	
	for (int i = 1; i < argc; i++) {
		if (strncmp(argv[i], "--", 2)) {
			fprintf(stderr, "ERROR Unexpected argument '%s'\n", argv[i]);
			return -1;
		}

		char* option = argv[i] + 2;
		
		if (strcmp(option, "align") == 0) {
			page_bytes = atoll(argv[++i]);
			
			if (page_bytes < 1) {
				fprintf(stderr, "ERROR Provided page size (%lu) is too small\n", page_bytes);
				return -1;
			}
		}
		
		else if (strcmp(option, "version") == 0) {
			printf("Command-line utility supports up to IAR version %lu\n", IAR_VERSION);
			return 0;
		}
		
		else if (strcmp(option, "output") == 0) {
			pack_output = unpack_output = argv[++i];
		}
		
		else if (strcmp(option, "pack") == 0) {
			if (mode == MODE_UNPACK) {
				fprintf(stderr, "ERROR '--unpack' has already been passed\n");
				return -1;
			}

			if (mode == MODE_JSON) {
				fprintf(stderr, "ERROR '--json' has already been passed\n");
				return -1;
			}
			
			mode = MODE_PACK;
			pack_dir = argv[++i];
		}

		else if (strcmp(option, "unpack") == 0) {
			if (mode == MODE_PACK) {
				fprintf(stderr, "ERROR '--pack' has already been passed\n");
				return -1;
			}

			if (mode == MODE_JSON) {
				fprintf(stderr, "ERROR '--json' has already been passed\n");
				return -1;
			}

			mode = MODE_UNPACK;
			unpack_file = argv[++i];
		}

		else if (strcmp(option, "json") == 0) {
			if (mode == MODE_PACK) {
				fprintf(stderr, "ERROR '--pack' has already been passed\n");
				return -1;
			}

			if (mode == MODE_UNPACK) {
				fprintf(stderr, "ERROR '--unpack' has already been passed\n");
				return -1;
			}

			mode = MODE_JSON;

		}
		
		else {
			fprintf(stderr, "ERROR Option '--%s' is unknown. Check README.md or go to https://github.com/inobulles/iar/blob/master/README.md to see a list of available options\n", option);
			return -1;
		}
	}
	
	iar_file_t iar = {
		.header = {
			.page_bytes = page_bytes,
		},
	};

	int rv = -1;

	if (mode == MODE_PACK) {
		if (iar_open_write(&iar, pack_output) < 0) {
			goto error_open;
		}

		if (iar_pack(&iar, pack_dir, NULL) < 0) {
			goto error;
		}

		iar_write_header(&iar);
	}
	
	else if (mode == MODE_UNPACK) {
		if (iar_open_read(&iar, unpack_file) < 0) {
			goto error_open;
		}

		if (iar_unpack(&iar, unpack_output) < 0) {
			goto error;
		}
	}

	// else if (mode == MODE_JSON) {
	// 	if (iar_open_write(&iar, pack_output) < 0) {
	// 		goto error_open;
	// 	}

	// 	if (iar_pack_json(&iar, pack_json, NULL) < 0) {
	// 		goto error;
	// 	}

	// 	iar_write_header(&iar);
	// }

	rv = 0; // success

error:

	iar_close(&iar);

error_open:

	return rv;
}
