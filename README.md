# Torrent to JSON (t2j)

A simple C program that decodes and prints bencoded files (e.g. torrent files) as JSON.

## Building the binary

```
$ ./build.sh         # with optimizations
$ ./build.sh warn    # with warnings and optimizations
$ ./build.sh dev     # only warnings

$ ./build.sh tests   # build and run the binary through the 'tests' folder
```

## Usage

```
USAGE:
    t2j [OPTIONS] FILE
	t2j -h
	
OPTIONS:
	-b Print binary data (otherwise marked as [BLOB] in the output).
	-i Print the info hash as part of the output (sha1 hash of the 'info' field).
	-x Print binary in hexadecimal as "0x0A0x0B0x0C (etc.)".
```

**Examples:**

```
$ t2j movie.torrent
$ t2j movie.torrent | python3 -m json.tool  # e.g. format the output with python

$ t2j -i movie.torrent                      # include the 'info_hash' in the output
$ t2j -b -x movie.torrent                   # print the binary data as hexadecimal (i.e. from the 'pieces' field)
```

## TODO

- [ ] Being able to select a single field to be output, e.g. `t2j -f "info.name"` for the name of the torrent
- [ ] Read from STDIN
- [ ] .bat file for compilation on windows


