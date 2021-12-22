#if !defined(__IAR__SRC_IAR_H)
	#define __IAR__SRC_IAR_H

#include <stdio.h>
#include <stdint.h>

// iar macros

#define IAR_MAGIC 0x1A4C1A4C1A4C1A4C

#if !defined(IAR_VERSION)
	#define IAR_VERSION 1lu // *latest* supported version
#endif

#if !defined(IAR_DEFAULT_PAGE_BYTES)
	#define IAR_DEFAULT_PAGE_BYTES 4096lu
#endif

#if !defined(IAR_MAX_READ_BLOCK_SIZE)
	#define IAR_MAX_READ_BLOCK_SIZE 0x10000 // 64 KiB
#endif

// iar data structures

typedef struct {
	uint64_t magic;
	uint64_t version;
	uint64_t root_node_offset;
	uint64_t page_bytes;
} iar_header_t;

typedef struct {
	uint64_t is_dir;

	uint64_t name_bytes; // the name is always null-terminated, so this is (strlen(name) + 1)
	uint64_t name_offset;

	union {
		uint64_t node_count;
		uint64_t data_bytes;
	};

	union {
		uint64_t node_offsets_offset;
		uint64_t data_offset;
	};
} iar_node_t;

// functions for opening / closing iar files

typedef struct {
	char* absolute_path;

	FILE* fp;
	int fd;

	iar_header_t header;
	iar_node_t root_node;

	uint64_t current_offset;
} iar_file_t;

int iar_open_read(iar_file_t* self, const char* path);
int iar_open_write(iar_file_t* self, const char* path);

void iar_close(iar_file_t* self);

// functions for reading iar files

uint64_t iar_find_node(iar_file_t* self, iar_node_t* node, const char* name, iar_node_t* parent); // return the index of found file or -1 if nothing found
int iar_read_node_name(iar_file_t* self, iar_node_t* node, char* buffer);

int iar_read_node_content /* content not contents */ (iar_file_t* self, iar_node_t* node, char* buffer);
int iar_map_node_content /* content not contents */ (iar_file_t* self, iar_node_t* node, void* address);

// functions for writing to iar files

int iar_write_header(iar_file_t* self);

// functions for packing and unpacking iar files

int iar_pack(iar_file_t* self, const char* path, const char* name); // if no name is passed (NULL), the name will automatically be generated from the path
int iar_unpack(iar_file_t* self, const char* path);

#if !defined(WITHOUT_JSON)
	int iar_pack_json(iar_file_t* self, const char* path, const char* name); // for the name, see above
#endif

#endif
