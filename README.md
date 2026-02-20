# xtree

> xtree — a focused, fast and maintainable C++17 command-line utility that
> prints directory contents in a compact, human-readable tree format.

—

Table of Contents

- [About](#about)
- [Features](#features)
- [Requirements](#requirements)
- [Quick Start](#quick-start)
- [Build](#build)
- [Usage](#usage)
- [Options](#options)
- [Examples](#examples)
- [Development](#development)
- [Packaging](#packaging)
- [License](#license)
- [Contributing](#contributing)

## About

`xtree` is designed for developers and CI systems that need a concise view of a
project tree with optional metadata such as file sizes, Git status and simple
project statistics. The codebase is modular, follows modern C++ idioms, and is
set up for reproducible builds and automated testing.

## Features

- Recursive directory tree with clear ASCII branches
- Colorized output (configurable) for directories, files and status flags
- Optional file sizes and directory disk-usage (DU)
- Git integration: show file status (M/A/D/R/C/U), ignored files, branches,
 and last commit author/date
- Project statistics option (`--stats`) to report total files and total lines
- Respect `.gitignore`-style exclusions via `--ignore`
- Cross-platform (uses std::filesystem) and small dependency surface

## Requirements

- A C++17-capable compiler (GCC 8+, Clang 7+, MSVC 2017+)
- CMake >= 3.10 (recommended for full build, tests and packaging)

## Quick Start

Clone the repository and perform a typical CMake build:

```bash
git clone https://github.com/C0dwiz/xtree.git
cd xtree
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

Alternatively, a simple Makefile is provided for quick builds:

```bash
make -j$(nproc)
```

## Build (details)

Run tests after building:

```bash
ctest --test-dir build --output-on-failure
```

Install the binary system-wide:

```bash
sudo cmake --install build --prefix /usr/local
```

Or install to a local user prefix:

```bash
cmake --install build --prefix "$HOME/.local"
```

## Usage

```text
xtree [OPTIONS] [PATH]
```

If PATH is omitted, `.` (current directory) is used.

## Options

The most commonly used options are listed below; run `xtree --help` for the
complete set.

| Option | Description |
|---|---|
| `--help` | Show help message |
| `--all` | Show hidden files (dotfiles) |
| `--size` | Display file sizes next to files |
| `--du` | Show directory sizes (summed file sizes) |
| `--no-color` | Disable ANSI color codes in output |
| `--depth N` | Limit recursion to N levels |
| `--ignore="ext, name"` | Ignore files by extension or exact name (comma-separated) |
| `--git` | Enable Git-aware output (status, branches, author/date) |
| `--follow-links` | Follow symbolic links when descending directories |
| `--stats` | Print total number of files and total lines at the end |

## Examples

Show current tree with sizes and hidden files:

```bash
./build/xtree --all --size
```

Show Git statuses, skip `build/` and `.git/`, and print project stats:

```bash
./build/xtree --git --ignore="build,.git" --stats
```

Limit output to two directory levels and disable color:

```bash
./build/xtree --depth 2 --no-color
```

## Development

- Code formatting: a `.clang-format` file is present. Format tracked source
 files with:

```bash
git ls-files '*.cpp' '*.cc' '*.c' '*.hpp' '*.hh' '*.h' | xargs clang-format -style=file -i
```

- Tests are integrated via CTest. Run `ctest` after building.
- The project provides a minimal Makefile and a full CMake build that enables
 testing and packaging via CPack.

## Packaging

After configuring the build directory, use CPack inside `build/` to generate
archives or native packages (TGZ/DEB):

```bash
cd build
cpack
```

The resulting artifacts will be placed in the `build/` directory.

## License

This project is distributed under the Mozilla Public License v2.0. See the
`LICENSE` file for details.

## Contributing

Contributions are welcome. Please:

1. Fork the repository and create a clear, focused branch for your change.
2. Run code formatting and tests locally before opening a pull request.
3. Write a brief description of the change and the rationale in the PR.

Maintainers will review PRs and request changes where necessary.
