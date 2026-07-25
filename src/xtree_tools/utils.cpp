// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "xtree/utils.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <sstream>

#include <cstdio>
#include <cstring>
#include <system_error>

namespace xtree {

namespace fs = std::filesystem;

constexpr const char *ANSI_RESET = "\033[0m";
constexpr const char *ANSI_BLUE = "\033[1;34m";
constexpr const char *ANSI_GREEN = "\033[1;32m";
constexpr const char *ANSI_GRAY = "\033[0;37m";
constexpr const char *ANSI_RED = "\033[1;31m";
constexpr const char *ANSI_YELLOW = "\033[1;33m";

void print_help() {
  std::cout << "Usage: xtree [OPTIONS] [PATH]\n"
               "Display directory tree with optional features.\n"
               "\n"
               "Options:\n"
               "  --help              Show this help message\n"
               "  --all               Show hidden files (starting with dot)\n"
               "  --size              Show file sizes\n"
               "  --no-color          Disable colored output\n"
               "  --depth N           Limit recursion depth (N levels)\n"
               "  --ignore=\"p1, p2\"   Ignore files with given extensions or "
               "folders with exact names (comma-separated)\n"
               "  --gitignore         Ignore files and folders matched by .gitignore\n"
               "  --git               Show Git status: M(odified), A(dded), "
               "D(eleted), R(enamed), C(opied), U(ntracked)\n"
               "  --du                Show directory sizes (total of all files "
               "inside)\n"
               "  --follow-links      Follow symbolic links\n"
               "  --stats             Show total file and line counts\n"
               "\n"
               "If PATH is omitted, current directory is used.\n"
               "\n"
               "Examples:\n"
               "  xtree\n"
               "  xtree --all --size /home/user\n"
               "  xtree --ignore=\"txt,json\" --git --du\n"
               "  xtree --depth 2 --size --no-color\n"
               "  xtree --all --du /var\n";
}

std::string color_blue(const std::string &s, bool color) {
  return color ? ANSI_BLUE + s + ANSI_RESET : s;
}

std::string color_green(const std::string &s, bool color) {
  return color ? ANSI_GREEN + s + ANSI_RESET : s;
}

std::string color_gray(const std::string &s, bool color) {
  return color ? ANSI_GRAY + s + ANSI_RESET : s;
}

std::string color_red(const std::string &s, bool color) {
  return color ? ANSI_RED + s + ANSI_RESET : s;
}

std::string color_yellow(const std::string &s, bool color) {
  return color ? ANSI_YELLOW + s + ANSI_RESET : s;
}

std::string human_size(uintmax_t size) {
  if (size == 0)
    return "0B";

  constexpr const char *units[] = {"B", "K", "M", "G", "T", "P"};
  constexpr int max_units = sizeof(units) / sizeof(units[0]);

  double dsize = static_cast<double>(size);
  int unit_index = 0;

  while (dsize >= 1024.0 && unit_index < max_units - 1) {
    dsize /= 1024.0;
    ++unit_index;
  }

  char buffer[32];
  int len = snprintf(buffer, sizeof(buffer), "%.1f%s", dsize, units[unit_index]);
  return std::string(buffer, len);
}

std::string normalize_path(const std::string &p) {
  std::string res = p;
#ifdef _WIN32
  std::replace(res.begin(), res.end(), '\\', '/');
#endif
  while (!res.empty() && res.back() == '/')
    res.pop_back();
  return res;
}

bool should_ignore(const fs::path &path, const Options &opts, const GitIgnore *gitIgnore,
                   bool isDir) {
  if (gitIgnore && gitIgnore->is_ignored(path, isDir))
    return true;
  if (opts.ignorePatterns.empty())
    return false;
  std::string filename = path.filename().string();
  std::transform(filename.begin(), filename.end(), filename.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

  if (path.has_extension()) {
    std::string ext = path.extension().string();
    if (!ext.empty() && ext[0] == '.')
      ext = ext.substr(1);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    for (const auto &pat : opts.ignorePatterns) {
      if (ext == pat || (pat.size() > 1 && pat.front() == '.' && ext == pat.substr(1)))
        return true;
    }
  }
  for (const auto &pat : opts.ignorePatterns) {
    if (filename == pat)
      return true;
  }
  return false;
}

std::vector<fs::directory_entry> get_filtered_entries(const fs::path &path, const Options &opts,
                                                      const GitIgnore *gitIgnore) {
  std::vector<fs::directory_entry> entries;
  entries.reserve(64);

  try {
    for (const auto &entry :
         fs::directory_iterator(path, fs::directory_options::skip_permission_denied)) {
      const auto &p = entry.path();
      const std::string fname = p.filename().string();

      if (!opts.showHidden && !fname.empty() && fname.front() == '.')
        continue;

      if (should_ignore(p, opts, gitIgnore, entry.is_directory()))
        continue;

      if (!opts.followSymlinks && entry.is_symlink())
        continue;

      entries.emplace_back(entry);
    }
  } catch (const std::exception &e) {
    std::cerr << "Warning: Cannot read directory: " << path << " (" << e.what() << ")\n";
  }

  std::sort(entries.begin(), entries.end(),
            [](const fs::directory_entry &a, const fs::directory_entry &b) {
              const bool a_dir = a.is_directory();
              const bool b_dir = b.is_directory();

              if (a_dir != b_dir)
                return a_dir > b_dir;

              return a.path().filename() < b.path().filename();
            });

  return entries;
}

uintmax_t compute_dir_size(const fs::path &root, const Options &opts,
                           std::unordered_map<std::string, uintmax_t> &dirSizes,
                           const GitIgnore *gitIgnore) {
  uintmax_t total = 0;

  std::error_code ec;
  const auto options = fs::directory_options::skip_permission_denied |
                       (opts.followSymlinks ? fs::directory_options::follow_directory_symlink
                                            : fs::directory_options::none);

  fs::recursive_directory_iterator it(root, options, ec), end;
  if (ec) {
    std::cerr << "Warning: Cannot read directory: " << root << " (" << ec.message() << ")\n";
    return 0;
  }

  for (; it != end; it.increment(ec)) {
    if (ec) {
      std::cerr << "Warning: Cannot access entry: " << it->path().string() << " (" << ec.message()
                << ")\n";
      ec.clear();
      continue;
    }

    const fs::path p = it->path();
    const std::string fname = p.filename().string();

    if (!opts.showHidden && !fname.empty() && fname.front() == '.') {
      if (it->is_directory(ec) && !ec)
        it.disable_recursion_pending();
      ec.clear();
      continue;
    }

    if (!opts.followSymlinks && it->is_symlink(ec) && !ec) {
      if (it->is_directory(ec) && !ec)
        it.disable_recursion_pending();
      ec.clear();
      continue;
    }

    if (it->is_directory(ec)) {
      if (ec) {
        ec.clear();
      } else if (should_ignore(p, opts, gitIgnore, true)) {
        it.disable_recursion_pending();
      }
      continue;
    }

    if (!it->is_regular_file(ec) || ec) {
      ec.clear();
      continue;
    }

    if (should_ignore(p, opts, gitIgnore, false)) {
      continue;
    }

    const uintmax_t sz = it->file_size(ec);
    if (ec) {
      std::cerr << "Warning: Cannot access file '" << p.string() << "': " << ec.message() << "\n";
      ec.clear();
      continue;
    }

    total += sz;
    fs::path cur = p.parent_path();
    while (!cur.empty() && cur != cur.root_path()) {
      dirSizes[cur.string()] += sz;
      if (cur == root)
        break;
      cur = cur.parent_path();
    }
  }

  dirSizes.emplace(root.string(), 0);
  return total;
}

void parse_ignore_patterns(const std::string &input, std::vector<std::string> &patterns) {
  if (patterns.capacity() < 8)
    patterns.reserve(8);

  std::stringstream ss(input);
  std::string token;
  while (std::getline(ss, token, ',')) {
    size_t start = token.find_first_not_of(" \t\n\r");
    if (start == std::string::npos)
      continue;
    size_t end = token.find_last_not_of(" \t\n\r");

    if (start <= end) {
      std::string pat = token.substr(start, end - start + 1);
      std::transform(pat.begin(), pat.end(), pat.begin(),
                     [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
      if (!pat.empty() && std::find(patterns.begin(), patterns.end(), pat) == patterns.end())
        patterns.emplace_back(std::move(pat));
    }
  }
}

inline uintmax_t count_lines_fast(const fs::path& filepath) {
  // rb flag for ignore \r\n in windows
  FILE* file = std::fopen(filepath.string().c_str(), "rb");
  if (!file) return 0;

  // 128kb for l1/l2 cache
  constexpr size_t BUFFER_SIZE = 131072;
  char buffer[BUFFER_SIZE];

  uintmax_t lines = 0;
  size_t bytes_read;
  bool is_empty = true;
  bool ends_with_newline = true;

  while ((bytes_read = std::fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
    is_empty = false;
    char* p = buffer;
    char* end = buffer + bytes_read;
    ends_with_newline = (*(end - 1) == '\n');

    while ((p = static_cast<char*>(std::memchr(p, '\n', end - p))) != nullptr) {
      ++lines;
      ++p;
    }
  }
  std::fclose(file);

  if (!is_empty && !ends_with_newline) {
    ++lines;
  }

  return lines;
}

void compute_project_stats(const fs::path &root, const Options &opts, uintmax_t &fileCount,
                           uintmax_t &lineCount, const GitIgnore *gitIgnore) {
  fileCount = 0;
  lineCount = 0;

  std::error_code ec;
  const auto options = fs::directory_options::skip_permission_denied | (opts.followSymlinks ? fs::directory_options::follow_directory_symlink : fs::directory_options::none);

  fs::recursive_directory_iterator it(root, options, ec), end;
  if (ec) {
    std::cerr << "Warning: Cannot read directory for stats: " << root << " (" << ec.message() << ")\n";
    return;
  }

  for (; it != end; it.increment(ec)) {
    if (ec) {
      ec.clear();
      continue;
    }

    const fs::path p = it->path();
    const std::string fname = p.filename().string();

    if (!opts.showHidden && !fname.empty() && fname.front() == '.') {
      if (it->is_directory(ec) && !ec) {
        it.disable_recursion_pending();
      }
      ec.clear();
      continue;
    }

    if (it->is_directory(ec)) {
      if (!ec && should_ignore(p, opts, gitIgnore, true)) {
        it.disable_recursion_pending();
      }
      ec.clear();
      continue;
    }

    if (!it->is_regular_file(ec) || ec) {
      ec.clear();
      continue;
    }

    if (should_ignore(p, opts, gitIgnore, false)) {
      continue;
    }

    ++fileCount;
    lineCount += count_lines_fast(p);
  }
}
} // namespace xtree
