# iar
The source code for the IAR (Inobulles ARchive) file format command-line utility.

## Usage

Here is a list of all the command-line arguments you can pass to `iar` and what they do:

`--help`: Print out help.

`--version`: Print out version.

`--pack [files]`: Pack the given files.

`--unpack [IAR file]`: Unpack the given IAR archive file.

`--output [output path]`: Output to the given destination path.

`--verbose`: Give verbose output.

## Building

On Linux, build with

```$ sh build.sh```

This will install necessary libraries (Debian), compile the utility and move the binary to /bin.
