// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "xtree/utils.h"

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <sstream>

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
               "  --git               Show Git status: M(odified), A(dded), "
               "D(eleted), R(enamed), C(opied), U(ntracked)\n"
               "  --du                Show directory sizes (total of all files "
               "inside)\n"
               "  --follow-links      Follow symbolic links\n"
               "  --stats             Show total file and line counts in the\n"
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

bool should_ignore(const fs::path &path, const Options &opts) {
  if (opts.ignorePatterns.empty())
    return false;
  std::string filename = path.filename().string();

  if (path.has_extension()) {
    std::string ext = path.extension().string();
    if (!ext.empty() && ext[0] == '.')
      ext = ext.substr(1);
    for (const auto &pat : opts.ignorePatterns) {
      if (ext == pat)
        return true;
    }
  }
  for (const auto &pat : opts.ignorePatterns) {
    if (filename == pat)
      return true;
  }
  return false;
}

std::vector<fs::directory_entry> get_filtered_entries(const fs::path &path, const Options &opts) {
  std::vector<fs::directory_entry> entries;
  entries.reserve(64);

  try {
    for (const auto &entry :
         fs::directory_iterator(path, fs::directory_options::skip_permission_denied)) {
      const auto &p = entry.path();
      const std::string fname = p.filename().string();

      if (!opts.showHidden && !fname.empty() && fname.front() == '.')
        continue;

      if (should_ignore(p, opts))
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
                           std::unordered_map<std::string, uintmax_t> &dirSizes) {
  uintmax_t total = 0;

  for (fs::recursive_directory_iterator
           it(root, opts.followSymlinks ? fs::directory_options::follow_directory_symlink
                                        : fs::directory_options::none),
       end;
       it != end; ++it) {
    try {
      if (it->is_regular_file())
        total += it->file_size();
    } catch (const std::exception &e) {
      std::cerr << "Warning: Cannot access file '" << it->path().string() 
                << "': " << e.what() << "\n";
    } catch (...) {
      std::cerr << "Warning: Unknown error accessing file '" 
                << it->path().string() << "'\n";
    }
  }

  dirSizes[root.string()] = total;
  return total;
}

void parse_ignore_patterns(const std::string &input, std::vector<std::string> &patterns) {
  patterns.clear();
  patterns.reserve(8);

  std::stringstream ss(input);
  std::string token;
  while (std::getline(ss, token, ',')) {
    size_t start = token.find_first_not_of(" \t\n\r");
    if (start == std::string::npos)
      continue;
    size_t end = token.find_last_not_of(" \t\n\r");

    if (start <= end) {
      patterns.emplace_back(token.substr(start, end - start + 1));
    }
  }
}

void compute_project_stats(const fs::path &path, const Options &opts, uintmax_t &fileCount,
                           uintmax_t &lineCount) {
  fileCount = 0;
  lineCount = 0;
  auto entries = get_filtered_entries(path, opts);

  for (const auto &entry : entries) {
    try {
      if (entry.is_directory() && (!fs::is_symlink(entry.path()) || opts.followSymlinks)) {
        uintmax_t fc = 0, lc = 0;
        compute_project_stats(entry.path(), opts, fc, lc);
        fileCount += fc;
        lineCount += lc;
      } else if (entry.is_regular_file()) {
        ++fileCount;
        std::ifstream in(entry.path());
        if (in) {
          lineCount += std::count(std::istreambuf_iterator<char>(in),
                                  std::istreambuf_iterator<char>(), '\n') +
                       1;
        }
      }
    } catch (const std::exception &e) {
      std::cerr << "Warning: Failed to process '" << entry.path().string() << "': " << e.what()
                << "\n";
    }
  }
}

} // namespace xtree
