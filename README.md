## zeromerge

### Combine files with zeroed blocks into one unified file

Author: Jody Bruchon <jody@jodybruchon.com>

License: GNU GPL v2 (see LICENSE file for details)

Copyright (C) 2020-2026 Jody Bruchon and contributors

This program merges two files that are supposed to be identical but have missing data in one or both files that manifests as zeroed blocks. This is commonly seen when the same file exists in multiple torrents but each individual torrent is not fully seeded. For example, if a torrent stops at 99.9% completion but the last block is not available, but another torrent is available with that same block fully available, the two partial file downloads can be merged with this tool and force-rechecked to fill in the data gap and recover the full file.

The program will abort if the file sizes differ or if any of the file data that mismatches is not all zeroes.

### Usage

  zeromerge [-C ##] [-q] file1 file2 outfile
  zeromerge [-C ##] [-q] -r [directories/files...]

### Options

* **`-C ##`**: Set I/O chunk size index (0 = 4K, doubles up to 18 = 1GiB; default 12 = 16MiB).
* **`-r, --recursive`**: Recursively scan paths, group files by size, merge matches, and link/copy.
* **`-q, --quiet`**: Suppress all progress and completion output.
* **`-h, --help`**: Display help and exit.
* **`-v, --version`**: Output version information and exit.

### Please consider financially supporting continued development!

One-time or recurring donations are greatly appreciated, but not required. Use PayPal, Ko-Fi, etc. links at jodybruchon.com
