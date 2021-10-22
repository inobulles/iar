#include <iar.h>

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/param.h> // for the MIN macro

// functions for opening / closing iar files

int iar_open_read(iar_file_t* self, const char* path) {
	self->fp = fopen(path, "rb");
	if (!self->fp) {
		fprintf(stderr, "ERROR Failed to open '%s' for reading\n", path);
		return 1;
	}

	self->absolute_path = realpath(path, (char*) 0);
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
	return 1;
}

int iar_open_write(iar_file_t* self, const char* path) {
	self->fp = fopen(path, "wb");
	if (!self->fp) {
		fprintf(stderr, "ERROR Failed to open '%s' for writing\n", path);
		return 1;
	}

	self->absolute_path = realpath(path, (char*) 0);
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
	uint64_t* node_offsets = (uint64_t*) malloc(node_offset_bytes);

	pread(self->fd, node_offsets, node_offset_bytes, parent->node_offsets_offset);

	uint64_t index = 0;
	for (; index < parent->node_count; index++) {
		iar_node_t child_node;
		pread(self->fd, &child_node, sizeof(child_node), node_offsets[index]);

		char* node_name = (char*) malloc(child_node.name_bytes);
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

int iar_read_node_name(iar_file_t* self, iar_node_t* node, char* buffer) {
	return pread(self->fd, buffer, node->name_bytes, node->name_offset) == -1;
}

int iar_read_node_content /* content not contents */ (iar_file_t* self, iar_node_t* node, char* buffer) {
	if (!node->is_dir) {
		pread(self->fd, buffer, node->data_bytes, node->data_offset);
		return 0;

	} else {
		fprintf(stderr, "ERROR Provided node is not a file and thus contains no data\n");
		return 1;
	}
}

int iar_map_node_content /* content not contents */ (iar_file_t* self, iar_node_t* node, void* address) {
	if (!node->is_dir) {
		if (mmap(address, node->data_bytes, PROT_READ, MAP_PRIVATE | MAP_FIXED, self->fd, node->data_offset) == MAP_FAILED) {
			fprintf(stderr, "ERROR Couldn't map file to memory (errno = %d)\n", errno);
			return 1;
		}

		return 0;

	} else {
		fprintf(stderr, "ERROR Provided node is not a file and thus contains no data\n");
		return 1;
	}
}

// functions for writing to iar files

int iar_write_header(iar_file_t* self) {
	return pwrite(self->fd, &self->header, sizeof(self->header), 0) == -1;
}

// functions for packing and unpacking iar files

static uint64_t pack_walk(iar_file_t* self, const char* path, const char* name); // return offset, -1 if failure, -2 if file to be ignored
static int unpack_walk(iar_file_t* self, const char* path, iar_node_t* node);

int iar_pack(iar_file_t* self, const char* path, const char* _name) {
	char* absolute_path = realpath(path, (char*) 0);
	char* name = (char*) _name;
	
	if (!name) { // a bit ugly but it gets the job done
		name = absolute_path;
		const char* __name = (const char*) name;

		name += strlen(absolute_path) - 1;
		for (; name >= __name && *name != '/'; name--);
	}

	self->current_offset = sizeof(self->header);
	int error = (self->header.root_node_offset = pack_walk(self, path, ++name)) == -1;

	free(absolute_path);
	return error;
}

int iar_unpack(iar_file_t* self, const char* path) {
	mkdir(path, 0700);
	return unpack_walk(self, path, &self->root_node);
}

// static functions

static uint64_t pack_walk(iar_file_t* self, const char* path, const char* name) { // return offset, -1 if failure, -2 if file to be ignored
	// make sure the file to be read is not our output (this can create infinite loops)
	
	char* absolute_path = realpath(path, (char*) 0);

	if (strcmp(absolute_path, self->absolute_path) == 0) {
		free(absolute_path);
		return -2;
	}

	free(absolute_path);
	
	// create node
	
	iar_node_t node;
	
	uint64_t offset = self->current_offset;
	self->current_offset += sizeof(node);

	// write node name

	node.name_bytes = strlen(name) + 1;
	node.name_offset = self->current_offset;

	pwrite(self->fd, name, node.name_bytes, node.name_offset);
	self->current_offset += node.name_bytes;

	// btw, the order of what comes where is not specified by the standard
	// as long as all offsets point to the right place, you've got nothing to worry about
	// so if you want, you can put the node data after the name (weirdo), whatever you want

	DIR* dp = opendir(path);

	if (!dp) { // handle files
		node.is_dir = 0;

		// open the file

		FILE* fp = fopen(path, "rb");
		if (!fp) {
			fprintf(stderr, "ERROR Failed to open '%s'\n", path);
			return -1;
		}

		// write the data

		node.data_offset = (self->current_offset & ~(self->header.page_bytes - 1)) + self->header.page_bytes;
		self->current_offset = node.data_offset;
		
		node.data_bytes = 0;
		uint8_t* block = (uint8_t*) malloc(IAR_MAX_READ_BLOCK_SIZE);

		while (!feof(fp)) {
			size_t bytes_read = fread(block, 1, IAR_MAX_READ_BLOCK_SIZE, fp);
			pwrite(self->fd, block, bytes_read, self->current_offset);

			node.data_bytes += bytes_read;
			self->current_offset += bytes_read;
		}

		free(block);
		fclose(fp);

		goto end;
	}

	// handle directories

	node.is_dir = 1;

	node.node_count = 0;
	uint64_t* node_offsets_buffer = (uint64_t*) 0;

	struct dirent* entry;

	extern struct dirent* __readdir(DIR* dp); // cf. 'main.c'

	while ((entry = __readdir(dp)) != (void*) 0) {
		if (!*entry->d_name) {
			fprintf(stderr, "FATAL Cf. 'main.c'\n");
			exit(-1);
		}

		if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
			char* path_buffer = (char*) malloc(strlen(path) + strlen(entry->d_name) + 2 /* strlen("/") + 1 */);
			sprintf(path_buffer, *path ? "%s/%s" : "%s%s", path, entry->d_name);

			uint64_t child_offset = pack_walk(self, path_buffer, entry->d_name);
			free(path_buffer);

			if (child_offset == -2) { // is to be ignored?
				continue;
			}
			
			node_offsets_buffer = (uint64_t*) realloc(node_offsets_buffer, (node.node_count + 1) * sizeof(uint64_t));

			if ((node_offsets_buffer[node.node_count++] = child_offset) == -1) {
				free(node_offsets_buffer);
				closedir(dp);
				return -1;
			}
		}
	}

	// write the offsets

	uint64_t node_offsets_bytes = node.node_count * sizeof(uint64_t);
	
	node.node_offsets_offset = self->current_offset;
	self->current_offset += node_offsets_bytes;

	pwrite(self->fd, node_offsets_buffer, node_offsets_bytes, node.node_offsets_offset);

	free(node_offsets_buffer);
	closedir(dp);

end:

	pwrite(self->fd, &node, sizeof(node), offset);
	return offset;
}

static int unpack_walk(iar_file_t* self, const char* path, iar_node_t* node) {
	int error = 0;

	// read name

	char* name = (char*) malloc(node->name_bytes);
	pread(self->fd, name, node->name_bytes, node->name_offset);

	// get full path

	char* path_buffer = (char*) malloc(strlen(path) + node->name_bytes + 1 /* -1 + strlen("/") + 1 */);
	sprintf(path_buffer, *path ? "%s/%s" : "%s%s", path, name);

	if (!node->is_dir) { // handle files
		// create file to write to

		FILE* fp = fopen(path_buffer, "wb");
		if (!fp) {
			fprintf(stderr, "ERROR Failed to open '%s' for writing\n", path_buffer);
			
			error = 1;
			goto end;
		}

		// write data to file

		uint8_t* block = (uint8_t*) malloc(IAR_MAX_READ_BLOCK_SIZE);
		uint64_t offset = node->data_offset;

		for (int64_t left = node->data_bytes; left > 0; left -= IAR_MAX_READ_BLOCK_SIZE) {
			size_t bytes_to_read = MIN((size_t) left, IAR_MAX_READ_BLOCK_SIZE);

			pread(self->fd, block, bytes_to_read, offset);
			fwrite(block, 1, bytes_to_read, fp);

			offset += bytes_to_read;
		}

		free(block);
		fclose(fp);
		
		goto end;
	}

	// handle directories
	// (first read all the node offsets)

	uint64_t node_offsets_bytes = node->node_count * sizeof(uint64_t);
	uint64_t* node_offsets = (uint64_t*) malloc(node_offsets_bytes);

	pread(self->fd, node_offsets, node_offsets_bytes, node->node_offsets_offset);

	// create directory to write in and loop through all the nodes in it

	mkdir(path_buffer, 0700);
	
	for (uint64_t i = 0; i < node->node_count; i++) {
		iar_node_t node;
		pread(self->fd, &node, sizeof(node), node_offsets[i]);

		if (unpack_walk(self, path_buffer, &node)) {
			free(node_offsets);

			error = 1;
			goto end;
		}
	}

	free(node_offsets);

end:

	free(name);
	free(path_buffer);

	return error;
}
