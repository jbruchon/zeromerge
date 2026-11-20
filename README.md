# zeromerge

**Zero Merge** combines files of the same size that have empty (zero, `0x00`) blocks by merging non-zero data where at least one file has zeroes, while aborting if both files contain different non-zero bytes at the same offset.

If you find this software useful, please consider donating to support its continued development! Visit [jodybruchon.com](https://www.jodybruchon.com/) for donation links and more of my projects.

## Assumptions & Responsibility

`zeromerge` assumes `0x00` means empty or missing data—as produced by tools that pre-allocate zeroed files or zero-fill unallocated regions and unreadable sectors. **It is the user's responsibility not to point `zeromerge` at files that are not zero-sparse.** Merging arbitrary files where `0x00` has non-sparse semantics or where two files differ in non-zero data can produce unintended results or abort on conflict.

## Use Cases

* **Incomplete torrents:** Combine partial copies of the same file where missing pieces are zero-filled, then force-recheck in your torrent client.
* **Data & media recovery:** Stitch together multiple zero-filled partial images, scratched optical disc passes, failing drive dumps, or flaky flash/EEPROM reads where different passes recovered different sectors.
* **Segmented or interrupted downloads:** Merge partial outputs from multi-pass downloaders, mirror fetches, or resume scripts that write byte ranges into zero-padded target files.
* **Parallel worker & checkpoint outputs:** Combine fixed-size zero-initialized output files written independently by multiple workers or interrupted batch runs without lock contention.

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

## License

Copyright (C) 2020-2026 by Jody Bruchon `<jody@jodybruchon.com>`. Distributed under the GNU General Public License v2.
