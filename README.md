# xtree

A simple command-line utility to display directory contents in a tree-like format, written in C++17.

## Features

· Recursively lists files and directories in a tree structure \
· Colorized output for directories (blue) and files (green) \
· Options to show hidden files, file sizes, and limit recursion depth \
· Display git status for tracked and untracked files \
· Show directory sizes (disk usage) \
· Handle symbolic links with optional following \
· Sort output: directories first, then files, both alphabetically \
· Robust error handling with informative messages \
· Cross-platform (uses C++17 filesystem library)

## Requirements

· C++17 compatible compiler (e.g., GCC 8+, Clang 7+, MSVC 2017+) \
· Standard C++ library with filesystem support

## Building and Installation

### Clone Git & cd

```bash
git clone https://github.com/C0dwiz/xtree && cd xtree
```

### Compile

```bash
g++ -std=c++17 -o xtree main.cpp
```

### Install system-wide (Linux / macOS)

```bash
sudo cp xtree /usr/local/bin/
```

After installation, you can run xtree from anywhere in your terminal.

### Install locally (without sudo)

```bash
mkdir -p ~/.local/bin
cp xtree ~/.local/bin/
```

Make sure ~/.local/bin is in your PATH.

### Usage

```bash
xtree [options] [directory]
```

If no directory is specified, the current working directory (.) is used.

# Options

## Option Description

```
Options:
  --help              Show this help message
  --all               Show hidden files (starting with dot)
  --size              Show file sizes
  --no-color          Disable colored output
  --depth N           Limit recursion depth (N levels)
  --ignore="p1, p2"   Ignore files with given extensions or folders with 
                      exact names (comma-separated)
  --git               Show Git status: M(odified), A(dded), D(eleted), 
                      R(enamed), C(opied), U(ntracked)
  --du                Show directory sizes (total of all files inside)
  --follow-links      Follow symbolic links (off by default)

If PATH is omitted, current directory is used.
```

# Examples

List current directory with default settings:

```bash
xtree
```

List a specific directory with hidden files and sizes:

```bash
xtree --all --size /home/user/projects
```

Limit output to 2 levels deep:

```bash
xtree --depth 2 /var/log
```

Show git status and directory sizes:

```bash
xtree --git --du
```

Ignore specific file types and follow symbolic links:

```bash
xtree --ignore="txt,log,tmp" --follow-links
```

Show directory without colors (useful for piping):

```bash
xtree --no-color --size --all
```

Sample Output

```
.
├── src
│   ├── main.cpp (5.2 KB)
│   └── utils.cpp (2.3 KB)
├── README.md (1.1 KB)
└── build (12.5 KB)
```

With git status:

```
.
├── src
│   ├── main.cpp (5.2 KB) (M)
│   └── utils.cpp (2.3 KB)
├── README.md (1.1 KB) (M)
└── new_file.txt (200 B) (U)
```

# License

This project is licensed under the Mozilla Public License Version 2.0. See the LICENSE file for details.
