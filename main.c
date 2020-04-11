
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <dirent.h>
#include <limits.h> // for PATH_MAX
#include <sys/stat.h> // for mkdir

#ifndef VERSION
	#define VERSION 0 // this is the *latest* version supported by this utility
#endif

static uint8_t verbose = 0;
static uint64_t version = VERSION;

#define MAGIC 0x1A4C1A4C1A4C1A4C

typedef struct {
	uint64_t magic;
	uint64_t version;
	uint64_t root_node_offset;
} iar_header_t;

typedef struct {
	uint64_t node_count;
	uint64_t node_offsets_offset;
	uint64_t name_offset;
	uint64_t data_bytes;
	uint64_t data_offset;
} iar_node_t;

static uint8_t* working_data = (uint8_t*) 0;
static uint64_t working_data_bytes = 0;

static uint64_t pack_walk(const char* path, const char* name) { // return offset, -1 if failure
	char path_buffer[PATH_MAX + 1];
	memset(path_buffer, 0, sizeof(path_buffer));
	
	// first, allocate space for storing the node itself
	
	uint64_t offset = working_data_bytes;
	working_data_bytes += sizeof(iar_node_t);
	working_data = (uint8_t*) realloc(working_data, working_data_bytes);
	iar_node_t* node = (iar_node_t*) (working_data + offset);
	
	// then, allocate space for storing the node name
	
	uint64_t name_length = strlen(name) + 1;
	node->name_offset = working_data_bytes;
	working_data_bytes += name_length;
	working_data = (uint8_t*) realloc(working_data, working_data_bytes);
	node = (iar_node_t*) (working_data + offset); // !!!don't forget to update this, since realloc is not guaranteed to stay in place!!!
	memcpy(working_data + node->name_offset, name, name_length);
	
	// btw, the order of what comes where is not specified by the standard
	// as long as all offsets point to the right place, you've got nothing to worry about
	// so if you want, you can put the node data after the name (weirdo), whatever you want
	
	DIR* dp = opendir(path);
	if (!dp) { // handle files
		node->node_count = 0; // we don't use these since we're dealing with a file
		node->node_offsets_offset = 0;
		
		FILE* fp = fopen(path, "r");
		if (!fp) {
			fprintf(stderr, "ERROR Failed to open `%s`\n", path);
			return -1;
		}
		
		if (verbose) {
			printf("Unpacking file at %s ...\n", path);
		}
		
		fseek(fp, 0, SEEK_END);
		node->data_bytes = ftell(fp);
		rewind(fp);
		
		// allocate space for all the data and write to it
		
		node->data_offset = working_data_bytes;
		working_data_bytes += node->data_bytes;
		working_data = (uint8_t*) realloc(working_data, working_data_bytes);
		node = (iar_node_t*) (working_data + offset);
		fread(working_data + working_data_bytes - node->data_bytes, node->data_bytes, 1, fp);
		
		fclose(fp);
		goto end;
	}
	
	if (verbose) {
		printf("Packing directory at %s ...\n", path);
	}
	
	// handle directories
	
	node->data_bytes = 0; // we don't use these since we're dealing with a directory
	node->data_offset = 0;
	
	node->node_count = 0;
	uint64_t* node_offsets_buffer = (uint64_t*) malloc(sizeof(uint64_t));
	
	struct dirent* entry;
	while ((entry = readdir(dp)) != (void*) 0) if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
		sprintf(path_buffer, *path ? "%s/%s" : "%s%s", path, entry->d_name);
		
		uint64_t child_offset = pack_walk(path_buffer, entry->d_name);
		node = (iar_node_t*) (working_data + offset);
		
		if ((node_offsets_buffer[node->node_count] = child_offset) == -1) {
			free(node_offsets_buffer); // needs to be freed to prevent memory leaks
			closedir(dp);
			return -1;
		}
		
		node_offsets_buffer = (uint64_t*) realloc(node_offsets_buffer, (++node->node_count + 1) * sizeof(uint64_t));
	}
	
	// now i can finally allocate space for the offsets
	
	uint64_t node_offsets_size = node->node_count * sizeof(uint64_t);
	node->node_offsets_offset = working_data_bytes;
	working_data_bytes += node_offsets_size;
	working_data = (uint8_t*) realloc(working_data, working_data_bytes);
	node = (iar_node_t*) (working_data + offset);
	memcpy(working_data + working_data_bytes - node_offsets_size, node_offsets_buffer, node_offsets_size);
	
	// now just return the offset for the parent and we're done
	
	free(node_offsets_buffer); // needs to be freed to prevent memory leaks
	closedir(dp);
	end: return offset;
}

static int unpack_walk(const char* path, iar_node_t* node) {
	char path_buffer[PATH_MAX + 1];
	sprintf(path_buffer, *path ? "%s/%s" : "%s%s", path, working_data + node->name_offset);
	
	if (node->data_offset) { // handle files
		if (verbose) {
			printf("Unpacking file at %s ...\n", path_buffer);
		}
		
		FILE* fp = fopen(path_buffer, "w");
		if (!fp) {
			fprintf(stderr, "ERROR Failed to open `%s`\n", path_buffer);
			return 1;
		}
		
		fwrite((const void*) (working_data + node->data_offset), node->data_bytes, 1, fp);
		fclose(fp);
		return 0;
	}
	
	// handle directories
	
	if (verbose) {
		printf("Unpacking directory at %s ...\n", path_buffer);
	}
	
	mkdir(path_buffer, 0700);
	for (int i = 0; i < node->node_count; i++) if (unpack_walk(path_buffer, (iar_node_t*) (working_data + ((uint64_t*) (working_data + node->node_offsets_offset))[i]))) {
		return 1;
	}
	
	return 0;
}

#define MODE_UNKNOWN 0
#define MODE_PACK 1
#define MODE_UNPACK 2

int main(int argc, char** argv) {
	if (argc == 1) {
		fprintf(stderr, "ERROR No arguments provided\n");
		return 1;
	}
	
	uint8_t mode = MODE_UNKNOWN;
	
	char* pack_output = "output.iar";
	char* unpack_output = "output";
	
	char* unpack_file = (char*) 0;
	char* pack_directory = (char*) 0;
	
	for (int i = 1; i < argc; i++) {
		if (strncmp(argv[i], "--", 2) == 0) { // argument is option
			char* option = argv[i] + 2;
			
			if (strcmp(option, "help") == 0) {
				printf("IAR command-line utility help\n");
				printf("`--help`: Print out help.\n");
				printf("`--version`: Print out maximum supported IAR version.\n");
				printf("`--pack [files]`: Pack the given files.\n");
				printf("`--unpack [IAR file]`: Unpack the given IAR archive file.\n");
				printf("`--output [output path]`: Output to the given destination path.\n");
				printf("`--verbose`: Give verbose output.\n");
				printf("`--use [version number]`: Use specific version number.\n");
				
				goto success_condition;
				
			} else if (strcmp(option, "verbose") == 0) {
				printf("Outputting verbosely ...\n");
				verbose = 1;
				
			} else if (strcmp(option, "use") == 0) {
				if ((version = atoll(argv[i++ + 1])) > VERSION) {
					fprintf(stderr, "ERROR Provided version number (%lu) is unsupported by this utility (this utility only supports versions up to %d)\n", version, VERSION);
					goto error_condition;
				}
				
			} else if (strcmp(option, "version") == 0) {
				printf("Command-line utility supports up to IAR version %d\n", VERSION);
				goto success_condition;
				
			} else if (strcmp(option, "output") == 0) {
				pack_output = unpack_output = argv[i++ + 1];
				
			} else if (strcmp(option, "pack") == 0) {
				if (mode == MODE_UNPACK) {
					fprintf(stderr, "ERROR `--unpack` has already been passed\n");
					goto error_condition;
				}
				
				mode = MODE_PACK;
				pack_directory = argv[i++ + 1];
				
			} else if (strcmp(option, "unpack") == 0) {
				if (mode == MODE_PACK) {
					fprintf(stderr, "ERROR `--pack` has already been passed\n");
					goto error_condition;
				}
				
				mode = MODE_UNPACK;
				unpack_file = argv[i++ + 1];
				
			} else {
				fprintf(stderr, "ERROR Option `--%s` is unknown. Run `iar --help` to see a list of available options\n", option);
				goto error_condition;
			}
			
		} else {
			fprintf(stderr, "ERROR Unexpected argument `%s`\n", argv[i]);
			goto error_condition;
		}
	}
	
	if (mode == MODE_PACK) {
		iar_header_t header = { .magic = MAGIC, .version = version };
		working_data_bytes = sizeof(header) + sizeof(iar_node_t);
		working_data = (uint8_t*) malloc(working_data_bytes);
		
		if (verbose) {
			printf("Walking through pack_directory (%s) ...\n", pack_directory);
		}
		
		if ((header.root_node_offset = pack_walk(pack_directory, pack_directory)) == -1) {
			goto error_condition; // no need for printing out error message; pack_walk will do it for us
		}
		
		if (verbose) {
			printf("Writing to pack_output (%s) ...\n", pack_output);
		}
		
		FILE* fp = fopen(pack_output, "w");
		if (!fp) {
			fprintf(stderr, "ERROR Couldn't output to `%s`\n", pack_output);
			goto error_condition;
		}
		
		memcpy(working_data, &header, sizeof(header));
		fwrite((const void*) working_data, working_data_bytes, 1, fp);
		fclose(fp);
		
	} else if (mode == MODE_UNPACK) {
		if (verbose) {
			printf("Reading unpack_file (%s) ...\n", unpack_file);
		}
				
		FILE* fp = fopen(unpack_file, "r");
		if (!fp) {
			fprintf(stderr, "ERROR Couldn't open `%s`\n", unpack_file);
			goto error_condition;
		}
		
		fseek(fp, 0, SEEK_END);
		working_data_bytes = ftell(fp);
		rewind(fp);
		
		working_data = (uint8_t*) malloc(working_data_bytes);
		fread(working_data, working_data_bytes, 1, fp);
		fclose(fp);
		
		if (verbose) {
			printf("Making sure header is valid ...\n");
		}
		
		iar_header_t* header = (iar_header_t*) working_data;
		if (header->magic != MAGIC) {
			fprintf(stderr, "ERROR Provided file is not a valid IAR file\n");
			goto error_condition;
		}
		
		if (header->version > VERSION) {
			fprintf(stderr, "ERROR Provided file is of an unsupported version (%lu, this utility only supports versions up to %d)\n", header->version, VERSION);
			goto error_condition;
		}
		
		mkdir(unpack_output, 0700);
		iar_node_t* root_node = (iar_node_t*) (working_data + header->root_node_offset);
		
		if (verbose) {
			printf("Walking through unpack_file ...\n");
		}
		
		for (int i = 0; i < root_node->node_count; i++) if (unpack_walk(unpack_output, (iar_node_t*) (working_data + ((uint64_t*) (working_data + root_node->node_offsets_offset))[i]))) { // we need to iterate over the root node ourselves since, unlike other directory nodes, its directory has already been made
			goto error_condition; // again, no need for printing out error message; unpack_walk will do it for us
		}
	}
	
	// tbh i dont't really care about freeing stuff
	// in all fairness, i'm pretty sure i can avoid memory leaks in such a simple program
	// also, it would hurt readability to handle all the freeing so eh
	
	success_condition:
	if (verbose) {
		printf("Program finished successfully\n");
	}
	
	return 0;
	
	error_condition:
	return 1;
}
