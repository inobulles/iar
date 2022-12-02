// C compilation

var cc = CC.new()

cc.add_opt("-I/usr/local/include")
cc.add_opt("-Isrc")
cc.add_opt("-fPIC")
cc.add_opt("-std=c99")
cc.add_opt("-Wall")
cc.add_opt("-Wextra")
cc.add_opt("-Werror")

var lib_src = File.list("src/lib")
	.where { |path| path.endsWith(".c") }

var cmd_src = File.list("src/cmd")
	.where { |path| path.endsWith(".c") }

var src = lib_src.toList + cmd_src.toList

src
	.each { |path| cc.compile(path) }

// create static & dynamic libraries

var linker = Linker.new(cc)

linker.archive(lib_src.toList, "libiar.a")
linker.link(lib_src.toList, [], "libiar.so", true)

// create command-line frontend

linker.link(cmd_src.toList, ["iar"], "iar")

// running

class Runner {
	static run(args) {
		return File.exec("iar", args)
	}
}

// installation map
// TODO for headers, we should have a special way to simply move certain files (i.e. 'iar.h' in this case) to the output directory

var prefix = "/usr/local" // TODO way to discriminate between OS' - on Linux distros, this would usually be simply "/usr" instead

var install = {
	"iar":       "%(prefix)/bin/iar",
	"libiar.a":  "%(prefix)/lib/libiar.a",
	"libiar.so": "%(prefix)/lib/libiar.so",
}

// testing

class Tests {
	static version {
		return File.exec("iar", ["--version"])
	}

	static pack {
		return -1
	}

	static json {
		return -1
	}
}

var tests = ["version", "pack", "json"]
