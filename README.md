# xtree

A simple command-line utility to display directory contents in a tree-like format, written in C++17.

## Features

· Recursively lists files and directories in a tree structure \
· Colorized output for directories (blue) and files (green) \
· Options to show hidden files, file sizes, and limit depth \
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
--all - show hidden files (those starting with a dot).
--size - display file sizes in human-readable format (B, KB, MB, GB).
--no-color - disable colored output.
--depth <N> - limit recursion depth to N levels. -1 means unlimited.
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

Disable colors:
```bash
xtree --no-color
```

Sample Output
```
.
├── src
│   ├── main.cpp
│   └── utils.cpp (2.3 KB)
└── README.md (1.1 KB)
```

# License

This project is licensed under the Mozilla Public License Version 2.0. See the LICENSE file for details.
