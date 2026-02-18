// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include <cstdio>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>

namespace fs = std::filesystem;

constexpr int BUFFER_SIZE = 128;
constexpr int SIZE_UNIT = 1024;
constexpr int MAX_UNITS = 3;
constexpr const char *SIZE_UNITS[] = {"B", "KB", "MB", "GB"};

struct Options
{
  int maxDepth = -1;
  bool showHidden = false;
  bool showSize = false;
  bool useColor = true;
  bool followSymlinks = false;
  std::vector<std::string> ignorePatterns;
  bool gitStatus = false;
  bool diskUsage = false;
};

void printHelp()
{
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

std::string blue(const std::string &s, bool color)
{
  return color ? "\033[1;34m" + s + "\033[0m" : s;
}

std::string green(const std::string &s, bool color)
{
  return color ? "\033[1;32m" + s + "\033[0m" : s;
}

std::string gray(const std::string &s, bool color)
{
  return color ? "\033[0;37m" + s + "\033[0m" : s;
}

std::string humanSize(uintmax_t size)
{
  int i = 0;
  double dsize = size;
  while (dsize > SIZE_UNIT && i < MAX_UNITS)
  {
    dsize /= SIZE_UNIT;
    ++i;
  }
  std::ostringstream out;
  out << std::fixed << std::setprecision(1) << dsize << SIZE_UNITS[i];
  return out.str();
}

std::string normalizePath(const std::string &p)
{
  std::string res = p;
#ifdef _WIN32
  std::replace(res.begin(), res.end(), '\\', '/');
#endif
  while (!res.empty() && res.back() == '/')
  {
    res.pop_back();
  }
  return res;
}

bool shouldIgnore(const fs::path &path, const Options &opts)
{
  if (opts.ignorePatterns.empty())
    return false;
  std::string filename = path.filename().string();

  if (path.has_extension())
  {
    std::string ext = path.extension().string();
    if (!ext.empty() && ext[0] == '.')
      ext = ext.substr(1);
    for (const auto &pat : opts.ignorePatterns)
    {
      if (ext == pat)
        return true;
    }
  }
  for (const auto &pat : opts.ignorePatterns)
  {
    if (filename == pat)
      return true;
  }
  return false;
}

std::vector<fs::directory_entry> getFilteredEntries(const fs::path &path,
                                                    const Options &opts)
{
  std::vector<fs::directory_entry> entries;
  try
  {
    for (const auto &entry : fs::directory_iterator(path))
    {
      std::string fname = entry.path().filename().string();
      if (!opts.showHidden && !fname.empty() && fname[0] == '.')
        continue;
      if (shouldIgnore(entry.path(), opts))
        continue;
      if (!opts.followSymlinks && fs::is_symlink(entry.path()))
        continue;
      entries.push_back(entry);
    }
  }
  catch (const std::exception &e)
  {
    std::cerr << "Warning: Failed to read directory '" << path.string()
              << "': " << e.what() << "\n";
  }

  std::sort(entries.begin(), entries.end(),
            [](const fs::directory_entry &a, const fs::directory_entry &b)
            {
              bool a_is_dir = a.is_directory();
              bool b_is_dir = b.is_directory();
              if (a_is_dir != b_is_dir)
                return a_is_dir;
              return a.path().filename() < b.path().filename();
            });

  return entries;
}

uintmax_t computeDirSize(const fs::path &path, const Options &opts,
                         std::unordered_map<std::string, uintmax_t> &dirSizes)
{
  uintmax_t total = 0;
  auto entries = getFilteredEntries(path, opts);
  for (const auto &entry : entries)
  {
    if (entry.is_directory() && (!fs::is_symlink(entry.path()) || opts.followSymlinks))
    {
      total += computeDirSize(entry.path(), opts, dirSizes);
    }
    else if (entry.is_regular_file())
    {
      try
      {
        total += entry.file_size();
      }
      catch (const std::exception &e)
      {
        std::cerr << "Warning: Failed to get size of '" << entry.path().string()
                  << "': " << e.what() << "\n";
      }
    }
  }
  dirSizes[path.string()] = total;
  return total;
}

bool getGitStatus(const fs::path &target, fs::path &repo_root,
                  std::unordered_map<std::string, char> &fileStatus,
                  std::unordered_map<std::string, char> &dirStatus)
{
  fs::path cur = target;
  while (true)
  {
    if (fs::exists(cur / ".git") && fs::is_directory(cur / ".git"))
    {
      repo_root = cur;
      break;
    }
    if (cur == cur.root_path() || cur == cur.parent_path())
    {
      return false;
    }
    cur = cur.parent_path();
  }

  std::string cmd = "git -C \"" + repo_root.string() + "\" status --porcelain";
  std::unique_ptr<FILE, int (*)(FILE *)> pipe(popen(cmd.c_str(), "r"), pclose);
  if (!pipe)
  {
    std::cerr << "Warning: Failed to execute git command\n";
    return false;
  }

  std::string result;
  char buffer[BUFFER_SIZE];
  while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr)
  {
    result += buffer;
  }

  std::istringstream iss(result);
  std::string line;
  while (std::getline(iss, line))
  {
    if (line.empty())
      continue;
    char status = '?';
    std::string path;
    if (line.compare(0, 2, "??") == 0)
    {
      status = 'U';
      path = line.substr(3);
    }
    else
    {
      char x = line[0];
      char y = line[1];
      status = (y != ' ') ? y : x;
      path = line.substr(3);
      size_t arrow = path.find(" -> ");
      if (arrow != std::string::npos)
      {
        path = path.substr(arrow + 4);
      }
    }
    path.erase(0, path.find_first_not_of(" \t"));
    path.erase(path.find_last_not_of(" \t") + 1);
    if (!path.empty())
    {
      fileStatus[path] = status;
    }
  }

  auto priority = [](char c)
  {
    switch (c)
    {
    case 'M':
      return 5;
    case 'A':
      return 4;
    case 'D':
      return 3;
    case 'R':
      return 2;
    case 'C':
      return 1;
    case 'U':
      return 0;
    default:
      return -1;
    }
  };

  for (const auto &[file, st] : fileStatus)
  {
    std::string fullPath = file;
    while (true)
    {
      size_t pos = fullPath.find_last_of('/');
      if (pos == std::string::npos)
      {
        fullPath = "";
      }
      else
      {
        fullPath = fullPath.substr(0, pos);
      }
      auto it = dirStatus.find(fullPath);
      if (it == dirStatus.end() || priority(st) > priority(it->second))
      {
        dirStatus[fullPath] = st;
      }
      if (fullPath.empty())
        break;
    }
  }

  return true;
}

void printTree(const fs::path &path, const Options &opts,
               const std::unordered_map<std::string, uintmax_t> &dirSizes,
               const fs::path &repo_root,
               const std::unordered_map<std::string, char> *fileStatus,
               const std::unordered_map<std::string, char> *dirStatus,
               int depth = 0, std::string prefix = "")
{
  if (opts.maxDepth != -1 && depth > opts.maxDepth)
    return;

  auto entries = getFilteredEntries(path, opts);

  for (size_t i = 0; i < entries.size(); ++i)
  {
    const auto &entry = entries[i];
    bool isLast = (i == entries.size() - 1);

    std::cout << prefix;
    std::cout << (isLast ? "└── " : "├── ");

    std::string name = entry.path().filename().string();

    if (entry.is_directory())
    {
      std::cout << blue(name, opts.useColor);

      if (opts.diskUsage)
      {
        auto it = dirSizes.find(entry.path().string());
        if (it != dirSizes.end())
        {
          std::cout << " "
                    << gray("(" + humanSize(it->second) + ")", opts.useColor);
        }
      }

      if (fileStatus && dirStatus && !repo_root.empty())
      {
        fs::path rel = fs::relative(entry.path(), repo_root);
        std::string key = normalizePath(rel.generic_string());
        if (key == ".")
          key = "";
        auto it = dirStatus->find(key);
        if (it != dirStatus->end())
        {
          std::cout << " "
                    << gray("(" + std::string(1, it->second) + ")",
                            opts.useColor);
        }
      }
      std::cout << "\n";

      printTree(entry.path(), opts, dirSizes, repo_root, fileStatus, dirStatus,
                depth + 1, prefix + (isLast ? "    " : "│   "));
    }
    else
    {
      std::cout << green(name, opts.useColor);

      if (opts.showSize)
      {
        try
        {
          auto size = entry.file_size();
          std::cout << " " << gray("(" + humanSize(size) + ")", opts.useColor);
        }
        catch (const std::exception &e)
        {
          std::cerr << "Warning: Failed to get size of '" << entry.path().string()
                    << "': " << e.what() << "\n";
        }
      }

      if (fileStatus && !repo_root.empty())
      {
        fs::path rel = fs::relative(entry.path(), repo_root);
        std::string key = normalizePath(rel.generic_string());
        if (key == ".")
          key = "";
        auto it = fileStatus->find(key);
        if (it != fileStatus->end())
        {
          std::cout << " "
                    << gray("(" + std::string(1, it->second) + ")",
                            opts.useColor);
        }
      }
      std::cout << "\n";
    }
  }
}

void parseIgnorePatterns(const std::string &input,
                         std::vector<std::string> &patterns)
{
  std::stringstream ss(input);
  std::string token;
  while (std::getline(ss, token, ','))
  {
    token.erase(0, token.find_first_not_of(" \t\n\r"));
    token.erase(token.find_last_not_of(" \t\n\r") + 1);
    if (!token.empty())
    {
      patterns.push_back(token);
    }
  }
}

int main(int argc, char *argv[])
{
  for (int i = 1; i < argc; ++i)
  {
    std::string arg = argv[i];
    if (arg == "--help")
    {
      printHelp();
      return 0;
    }
  }

  Options opts;
  fs::path target = ".";

  for (int i = 1; i < argc; ++i)
  {
    std::string arg = argv[i];

    if (arg == "--all")
    {
      opts.showHidden = true;
    }
    else if (arg == "--size")
    {
      opts.showSize = true;
    }
    else if (arg == "--no-color")
    {
      opts.useColor = false;
    }
    else if (arg == "--follow-links")
    {
      opts.followSymlinks = true;
    }
    else if (arg == "--depth" && i + 1 < argc)
    {
      opts.maxDepth = std::stoi(argv[++i]);
    }
    else if (arg == "--git")
    {
      opts.gitStatus = true;
    }
    else if (arg == "--du")
    {
      opts.diskUsage = true;
    }
    else if (arg.rfind("--ignore=", 0) == 0)
    {
      parseIgnorePatterns(arg.substr(9), opts.ignorePatterns);
    }
    else if (arg == "--ignore" && i + 1 < argc)
    {
      parseIgnorePatterns(argv[++i], opts.ignorePatterns);
    }
    else
    {
      target = arg;
    }
  }

  fs::path repo_root;
  std::unordered_map<std::string, char> fileStatus, dirStatus;
  bool gitOk = false;
  if (opts.gitStatus)
  {
    gitOk = getGitStatus(target, repo_root, fileStatus, dirStatus);
    if (!gitOk)
    {
      std::cerr << "Not a git repository (or any parent). Ignoring --git.\n";
    }
  }

  std::unordered_map<std::string, uintmax_t> dirSizes;
  if (opts.diskUsage)
  {
    computeDirSize(target, opts, dirSizes);
  }

  std::cout << blue(target.string(), opts.useColor) << "\n";
  printTree(target, opts, dirSizes, gitOk ? repo_root : fs::path(),
            gitOk ? &fileStatus : nullptr, gitOk ? &dirStatus : nullptr);

  return 0;
}