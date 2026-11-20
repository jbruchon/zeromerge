# zeromerge

**Zero Merge** combines files of the same size that have empty (zero, `0x00`) blocks by merging non-zero data where at least one file has zeroes, while aborting if both files contain different non-zero bytes at the same offset.

## Assumptions & Responsibility

`zeromerge` assumes `0x00` means empty or missing data—as produced by tools that zero-fill unallocated regions or unreadable sectors (such as torrent clients or rescue imaging tools). **It is the user's responsibility not to point `zeromerge` at files that are not zero-sparse.** Merging arbitrary files where `0x00` has non-sparse semantics or where two files differ in non-zero data can produce unintended results or abort on conflict.

## Use Cases

* **Incomplete torrents:** Combine partial copies of the same file where missing pieces are zero-filled, then force-recheck in your torrent client.
* **Data recovery:** Stitch together multiple zero-filled partial images or recovered files from failing media or interrupted transfers.

## Usage

### Single Merge

```
zeromerge [options] <file1> <file2> <output_file>

```

Merges two non-empty, non-hard-linked regular files of identical size into `<output_file>`.

### Recursive Mode (`-r`)

```
zeromerge -r [options] [path ...]

```

Recursively scans directories (defaulting to `.`), filters by size if `-s` is given, sorts files by size, and merges adjacent same-sized files via a temporary file—hard-linking on the same filesystem or copying across filesystems, and preserving permissions/ownership where supported.

## Options

* `-h`, `--help`: Show help and exit.
* `-v`, `-V`, `--version`: Show version and exit.
* `-r`, `--recursive`: Recursively scan and merge matching-size files.
* `-s SIZE[suffix]`: Minimum file size in recursive mode (supports raw bytes and size suffixes).
* `-C <0-18>`: Buffer chunk size index (`4096 << val`, `4 KiB` to `1 GiB`; default `16 MiB`).
* `-q`, `--quiet`: Suppress progress and status output.

## Building & SIMD

Build with `make` (requires `libjodycode`). On `x86_64` and `i686`, runtime detection selects SSE2, AVX2, or AVX-512 for buffer merging when supported, falling back to scalar code otherwise or when `NO_SIMD` / non-x86 targets are built.
