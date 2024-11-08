# iar

The source code for the IAR (Inobulles ARchive) file format library and command-line utility.

## Building

With [Bob the Builder v0.1.0](https://github.com/inobulles/bob) installed:

```console
bob test install
```

## Command-line arguments

Here is a list of all the command-line arguments you can pass to `iar` and what they do:

### --version

Print out latest supported IAR version (`IAR_VERSION`).

### --pack [file or directory path]

Pack the given file or directory.

### --unpack [IAR file path]

Unpack the given IAR file.

### --json [JSON file path]

Pack the given JSON file.
Strings will act as files, & objects as directories.
Any other type will emit a warning and be ignored.
Note that this option is unavailable if you compile with `WITHOUT_JSON`.

### --output [output path]

Output to the given destination path.

### --align [page size in bytes]

Use a specific page size in bytes for alignment (default is `IAR_DEFAULT_PAGE_BYTES`, which is 4096 by default).
Pass `1` to disable page alignment.

## Compilation options

Here is a list of all the compilation options you can compile the IAR library and command-line utility with and what they do:

### IAR_VERSION

Set the latest supported IAR version (default is 1, as that's the latest current standard).

### IAR_DEFAULT_PAGE_BYTES

Set the default page size in bytes for alignment (default is 4096 bytes, or 4 KiB).

### IAR_MAX_READ_BLOCK_SIZE

Set the maximum read block size in bytes to be allocated (default is 65536 bytes, or 64 KiB, which is the minimum size the C99 standard guarantees `malloc` supports).
Higher values mean better performance with large files at the expense of higher RAM usage.

### WITHOUT_JSON

Compile without support for packing JSON files.
Also disables the `--json` flag in the command-line utility for obvious reasons.
