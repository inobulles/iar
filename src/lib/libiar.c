#define __STDC_WANT_LIB_EXT2__ 1 // ISO/IEC TR 24731-2:2010 standard library extensions

#if __linux__
	#define _GNU_SOURCE
#endif

#include <iar.h>

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/param.h> // for the MIN macro

#if !defined(WITHOUT_JSON)
	#include "json.h"

	typedef struct json_value_s json_value_t;
	typedef struct json_string_s json_str_t;
	typedef struct json_object_s json_obj_t;
	typedef struct json_object_element_s json_member_t;
#endif

// functions for opening / closing iar files

int iar_open_read(iar_file_t* self, const char* path) {
	self->fp = fopen(path, "rb");

	if (!self->fp) {
		fprintf(stderr, "ERROR Failed to open '%s' for reading\n", path);
		return -1;
	}

	self->absolute_path = realpath(path, NULL);
	self->fd = fileno(self->fp);

	pread(self->fd, &self->header, sizeof(self->header), 0); // read the iar header

	if (self->header.magic != IAR_MAGIC) {
		fprintf(stderr, "ERROR '%s' is not a valid IAR file (magic = 0x%lx)\n", path, self->header.magic);
		goto error;
	}

	if (self->header.version > IAR_VERSION) {
		fprintf(stderr, "ERROR '%s' is of an unsupported version (%lu) (latest supported version is %lu)\n", path, self->header.version, IAR_VERSION);
		goto error;
	}

	pread(self->fd, &self->root_node, sizeof(self->root_node), self->header.root_node_offset); // read root node
	return 0;

error:

	fclose(self->fp);
	return -1;
}

int iar_open_write(iar_file_t* self, const char* path) {
	self->fp = fopen(path, "wb");

	if (!self->fp) {
		fprintf(stderr, "ERROR Failed to open '%s' for writing\n", path);
		return -1;
	}

	self->absolute_path = realpath(path, NULL);
	self->fd = fileno(self->fp);

	// set defaults (these field can obviously be set after this function has been called)

	self->header.magic = IAR_MAGIC;
	self->header.version = IAR_VERSION;
	self->header.page_bytes = IAR_DEFAULT_PAGE_BYTES;

	return 0;
}

void iar_close(iar_file_t* self) {
	free(self->absolute_path);
	fclose(self->fp);
}

// functions for reading iar files

uint64_t iar_find_node(iar_file_t* self, iar_node_t* node, const char* name, iar_node_t* parent) {
	uint64_t node_offset_bytes = parent->node_count * sizeof(uint64_t);
	uint64_t* node_offsets = malloc(node_offset_bytes);

	pread(self->fd, node_offsets, node_offset_bytes, parent->node_offsets_offset);

	uint64_t index = 0;

	for (; index < parent->node_count; index++) {
		iar_node_t child_node;
		pread(self->fd, &child_node, sizeof(child_node), node_offsets[index]);

		char* node_name = malloc(child_node.name_bytes);
		pread(self->fd, node_name, child_node.name_bytes, child_node.name_offset);

		if (strncmp(name, node_name, child_node.name_bytes) == 0) {
			memcpy(node, &child_node, sizeof(child_node));
			goto found;
		}
	}

	return -1;

found:

	return index;
}

int iar_read_node_name(iar_file_t* self, iar_node_t* node, char* buf) {
	return pread(self->fd, buf, node->name_bytes, node->name_offset) == -1;
}

int iar_read_node_content /* content not contents */ (iar_file_t* self, iar_node_t* node, char* buf) {
	if (node->is_dir) {
		fprintf(stderr, "ERROR Provided node is not a file and thus contains no data\n");
		return -1;
	}

	pread(self->fd, buf, node->data_bytes, node->data_offset);
	return 0;
}

int iar_map_node_content /* content not contents */ (iar_file_t* self, iar_node_t* node, void* address) {
	if (node->is_dir) {
		fprintf(stderr, "ERROR Provided node is not a file and thus contains no data\n");
		return -1;
	}


	if (mmap(address, node->data_bytes, PROT_READ, MAP_PRIVATE | MAP_FIXED, self->fd, node->data_offset) == MAP_FAILED) {
		fprintf(stderr, "ERROR Couldn't map file to memory (%s)\n", strerror(errno));
		return -1;
	}

	return 0;
}

// functions for writing to iar files

int iar_write_header(iar_file_t* self) {
	return pwrite(self->fd, &self->header, sizeof(self->header), 0) == -1;
}

// functions for packing and unpacking iar files
// TODO 'uint64_t' vs 'int' for return types?

static uint64_t pack_walk(iar_file_t* self, const char* path, const char* name); // return offset, -1 if failure, -2 if file to be ignored
static int unpack_walk(iar_file_t* self, const char* path, iar_node_t* node);

#if !defined(WITHOUT_JSON)
	static uint64_t pack_json_walk(iar_file_t* self, json_value_t* member, const char* name); // return offset, -1 if failure, -2 if file to be ignored
#endif

// get the last bit of path & use that as a name (if user doesn't specify a name himself)
// a bit ugly but it gets the job done

static inline char* __iar_pack_gen_name(const char* path, const char* _name) {
	if (_name) {
		return strdup(++_name); // so we have something to free
	}

	char* abs_path = realpath(path, NULL);
	char* name = abs_path + strlen(abs_path) - 1; // go to end of absolute path

	for (; name >= abs_path && *name != '/'; name--); // move backwards until we find a path delimiter or we reach the beginning of the absolute path

	name = strdup(++name);
	free(abs_path);

	return name;
}

int iar_pack(iar_file_t* self, const char* path, const char* _name) {
	char* name = __iar_pack_gen_name(path, _name);

	// walk

	self->current_offset = sizeof(self->header);
	int error = (self->header.root_node_offset = pack_walk(self, path, name)) == -1ull;

	free(name);
	return -error;
}

#if !defined(WITHOUT_JSON)

int iar_pack_json(iar_file_t* self, const char* path, const char* _name) {
	char* name = __iar_pack_gen_name(path, _name);

	int rv = -1;

	FILE* fp = fopen(path, "rb");

	if (!fp) {
		fprintf(stderr, "ERROR Failed to open '%s'\n", path);
		goto error;
	}

	fseek(fp, 0, SEEK_END);
	size_t bytes = ftell(fp);

	char* raw = malloc(bytes + 1);
	raw[bytes] = '\0'; // just to be sure

	rewind(fp);
	fread(raw, 1, bytes, fp);

	fclose(fp);

	json_value_t* json = json_parse(raw, strlen(raw));

	if (!json) {
		fprintf(stderr, "ERROR Failed to parse JSON\n");
		goto error_json;
	}

	self->current_offset = sizeof(self->header);
	self->header.root_node_offset = pack_json_walk(self, json, name);

	if (self->header.root_node_offset == -1ull) {
		goto error_json;
	}

	rv = 0; // success

error_json:

	free(json);

error:

	free(name);
	return rv;
}

#endif

int iar_unpack(iar_file_t* self, const char* path) {
	mkdir(path, 0700);
	return unpack_walk(self, path, &self->root_node);
}

// static functions

#define NODE_OFFSET(node) \
	(node).data_offset = (self->current_offset & ~(self->header.page_bytes - 1)) + self->header.page_bytes; \
	self->current_offset = (node).data_offset;

#define WRITE_NODE_OFFSETS(node, node_offsets_buf) \
	uint64_t node_offsets_bytes = (node).node_count * sizeof *(node_offsets_buf); \
	\
	(node).node_offsets_offset = self->current_offset; \
	self->current_offset += node_offsets_bytes; \
	\
	pwrite(self->fd, (node_offsets_buf), node_offsets_bytes, (node).node_offsets_offset);

static inline uint64_t __create_node(iar_file_t* self, iar_node_t* node, const char* name) {
	// create node

	uint64_t offset = self->current_offset;
	self->current_offset += sizeof *node;

	// write node name

	node->name_bytes = strlen(name) + 1;
	node->name_offset = self->current_offset;

	pwrite(self->fd, name, node->name_bytes, node->name_offset);
	self->current_offset += node->name_bytes;

	return offset;
}

static inline int __pack_stream_node(iar_file_t* self, iar_node_t* node, const char* path) {
	// open the file

	FILE* fp = fopen(path, "rb");

	if (!fp) {
		fprintf(stderr, "ERROR Failed to open '%s'\n", path);
		return -1;
	}

	// write the data

	NODE_OFFSET(*node)
	node->data_bytes = 0;

	uint8_t* block = malloc(IAR_MAX_READ_BLOCK_SIZE);

	while (!feof(fp)) {
		size_t bytes_read = fread(block, 1, IAR_MAX_READ_BLOCK_SIZE, fp);
		pwrite(self->fd, block, bytes_read, self->current_offset);

		node->data_bytes += bytes_read;
		self->current_offset += bytes_read;
	}

	free(block);
	fclose(fp);

	return 0;
}

static uint64_t pack_walk(iar_file_t* self, const char* path, const char* name) { // return offset, -1 if failure, -2 if file to be ignored
	// make sure the file to be read is not our output (this can create infinite loops)

	char* absolute_path = realpath(path, NULL);

	if (strcmp(absolute_path, self->absolute_path) == 0) {
		free(absolute_path);
		return -2;
	}

	free(absolute_path);

	iar_node_t node;
	uint64_t offset = __create_node(self, &node, name);

	// btw, the order of what comes where is not specified by the standard
	// so long as all offsets point to the right place, you've got nothing to worry about
	// so if you want, you can put the node data after the name (weirdo), whatever you want

	DIR* dp = opendir(path);

	if (!dp) { // handle files
		node.is_dir = 0;

		if (__pack_stream_node(self, &node, path) < 0) {
			return -1;
		}

		goto end;
	}

	// handle directories

	node.is_dir = 1;

	node.node_count = 0;
	uint64_t* node_offsets_buf = NULL;

	struct dirent* entry;

	while ((entry = readdir(dp)) != NULL) {
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
			continue;
		}

		char* path_buf = malloc(strlen(path) + strlen(entry->d_name) + 2 /* strlen("/") + 1 */);
		sprintf(path_buf, *path ? "%s/%s" : "%s%s", path, entry->d_name);

		uint64_t child_offset = pack_walk(self, path_buf, entry->d_name);
		free(path_buf);

		if (child_offset == -2ull) { // is to be ignored?
			continue;
		}

		if (child_offset == -1ull) {
			if (node_offsets_buf) {
				free(node_offsets_buf);
			}

			closedir(dp);
			return -1; // propagate error
		}

		node_offsets_buf = realloc(node_offsets_buf, (node.node_count + 1) * sizeof *node_offsets_buf);
		node_offsets_buf[node.node_count++] = child_offset;
	}

	// write the offsets

	WRITE_NODE_OFFSETS(node, node_offsets_buf)
	free(node_offsets_buf);

	closedir(dp);

end:

	pwrite(self->fd, &node, sizeof node, offset);
	return offset;
}

static int unpack_walk(iar_file_t* self, const char* path, iar_node_t* node) {
	int rv = -1;

	// read name

	char* name = malloc(node->name_bytes);
	pread(self->fd, name, node->name_bytes, node->name_offset);

	// get full path

	char* path_buf = malloc(strlen(path) + node->name_bytes + 1 /* -1 + strlen("/") + 1 */);
	sprintf(path_buf, *path ? "%s/%s" : "%s%s", path, name);

	if (!node->is_dir) { // handle files
		// create file to write to

		FILE* fp = fopen(path_buf, "wb");

		if (!fp) {
			fprintf(stderr, "ERROR Failed to open '%s' for writing\n", path_buf);
			goto error;
		}

		// write data to file

		uint8_t* block = malloc(IAR_MAX_READ_BLOCK_SIZE);
		uint64_t offset = node->data_offset;

		for (int64_t left = node->data_bytes; left > 0; left -= IAR_MAX_READ_BLOCK_SIZE) {
			size_t bytes_to_read = MIN((size_t) left, IAR_MAX_READ_BLOCK_SIZE);

			pread(self->fd, block, bytes_to_read, offset);
			fwrite(block, 1, bytes_to_read, fp);

			offset += bytes_to_read;
		}

		free(block);
		fclose(fp);

		goto success;
	}

	// handle directories
	// (first read all the node offsets)

	uint64_t node_offsets_bytes = node->node_count * sizeof(uint64_t);
	uint64_t* node_offsets = malloc(node_offsets_bytes);

	pread(self->fd, node_offsets, node_offsets_bytes, node->node_offsets_offset);

	// create directory to write in and loop through all the nodes in it

	mkdir(path_buf, 0700);

	for (uint64_t i = 0; i < node->node_count; i++) {
		iar_node_t node;
		pread(self->fd, &node, sizeof node, node_offsets[i]);

		if (unpack_walk(self, path_buf, &node)) {
			free(node_offsets);
			goto error;
		}
	}

	free(node_offsets);

success:

	rv = 0;

error:

	free(name);
	free(path_buf);

	return rv;
}

#if !defined(WITHOUT_JSON)

static uint64_t pack_json_walk(iar_file_t* self, json_value_t* member, const char* name) {
	size_t type = member->type;
	void* payload = member->payload;

	// create node

	iar_node_t node;
	uint64_t offset = __create_node(self, &node, name);

	// handle "files"

	if (type == json_type_string) {
		node.is_dir = 0;
		json_str_t* _str = payload;

		size_t len = _str->string_size;
		char* str = (void*) _str->string;

		// check if we're dealing with a path, and stream the file to our node if that's the case

		#define JSON_IAR_PATH_PREFIX "__IAR_PATH__::"
		const size_t JSON_IAR_PATH_PREFIX_LEN = strlen(JSON_IAR_PATH_PREFIX);

		if (strncmp(str, JSON_IAR_PATH_PREFIX, JSON_IAR_PATH_PREFIX_LEN) == 0) {
			str += JSON_IAR_PATH_PREFIX_LEN;
			len -= JSON_IAR_PATH_PREFIX_LEN;

			if (__pack_stream_node(self, &node, str) < 0) {
				return -1;
			}

			goto end;
		}

		// write string data

		NODE_OFFSET(node)
		node.data_bytes = len; // includes NULL-byte

		pwrite(self->fd, str, node.data_bytes, self->current_offset);
		self->current_offset += node.data_bytes;

		goto end;
	}

	// handle "directories"
	// go through all object elements (keys) linked list

	if (type != json_type_object) {
		fprintf(stderr, "WARNING JSON member type is not an object or string; will be ignored\n");
		return -2;
	}

	node.is_dir = 1;

	node.node_count = 0;
	uint64_t* node_offsets_buf = NULL;

	json_obj_t* obj = payload;

	for (json_member_t* child = obj->start; child; child = child->next) {
		uint64_t child_offset = pack_json_walk(self, child->value, child->name->string);

		if (child_offset == -2ull) { // is to be ignored?
			continue;
		}

		if (child_offset == -1ull) {
			if (node_offsets_buf) {
				free(node_offsets_buf);
			}

			return -1; // propagate error
		}

		node_offsets_buf = realloc(node_offsets_buf, (node.node_count + 1) * sizeof *node_offsets_buf);
		node_offsets_buf[node.node_count++] = child_offset;
	}

	// write the offsets

	WRITE_NODE_OFFSETS(node, node_offsets_buf)
	free(node_offsets_buf);

end:

	pwrite(self->fd, &node, sizeof node, offset);
	return offset;
}

#endif
