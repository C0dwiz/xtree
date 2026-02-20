// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "xtree/printer.h"
#include "xtree/utils.h"

#include <iostream>

namespace xtree {

namespace fs = std::filesystem;

void print_tree(const fs::path &path, const Options &opts,
               const std::unordered_map<std::string, uintmax_t> &dirSizes,
               const fs::path &repo_root,
               const std::unordered_map<std::string, FileGitInfo> *fileStatus,
               const std::unordered_map<std::string, char> *dirStatus,
               int depth, std::string prefix)
{
  if (opts.maxDepth != -1 && depth > opts.maxDepth)
    return;

  auto entries = get_filtered_entries(path, opts);
  const bool hasGitInfo = fileStatus && dirStatus && !repo_root.empty();
  const fs::path repo_root_path = repo_root;

  for (size_t i = 0; i < entries.size(); ++i)
  {
    const auto &entry = entries[i];
    bool isLast = (i == entries.size() - 1);

    std::cout << prefix;
    std::cout << (isLast ? "└── " : "├── ");

    std::string name = entry.path().filename().string();

    if (entry.is_directory())
    {
      std::cout << color_blue(name, opts.useColor);

      if (opts.diskUsage)
      {
        auto it = dirSizes.find(entry.path().string());
        if (it != dirSizes.end())
          std::cout << " " << color_gray("(" + human_size(it->second) + ")", opts.useColor);
      }

      if (hasGitInfo)
      {
        fs::path rel = fs::relative(entry.path(), repo_root_path);
        std::string key = normalize_path(rel.generic_string());
        if (key == ".") key = "";
        auto it = dirStatus->find(key);
        if (it != dirStatus->end())
          std::cout << " " << color_gray("(" + std::string(1, it->second) + ")", opts.useColor);
      }
      std::cout << "\n";

      print_tree(entry.path(), opts, dirSizes, repo_root_path, fileStatus, dirStatus,
            depth + 1, prefix + (isLast ? "    " : "│   "));
    }
    else
    {
      std::string key;
      if (hasGitInfo) {
        fs::path rel = fs::relative(entry.path(), repo_root_path);
        key = normalize_path(rel.generic_string());
        if (key == ".") key = "";
      }

      if (fileStatus && !key.empty()) {
        auto itfs = fileStatus->find(key);
        if (itfs != fileStatus->end()) {
          const FileGitInfo &fi = itfs->second;
          if (fi.ignored) {
            std::cout << color_gray(name, opts.useColor);
          } else if (fi.x != ' ' && fi.x != '?') {
            std::cout << color_yellow(name, opts.useColor);
          } else if (fi.y != ' ' && fi.y != '?') {
            std::cout << color_red(name, opts.useColor);
          } else {
            std::cout << color_green(name, opts.useColor);
          }
        } else {
          std::cout << color_green(name, opts.useColor);
        }
      } else {
        std::cout << color_green(name, opts.useColor);
      }

      if (opts.showSize)
      {
        try
        {
          auto size = entry.file_size();
          std::cout << " " << color_gray("(" + human_size(size) + ")", opts.useColor);
        }
        catch (const std::exception &e)
        {
          std::cerr << "Warning: Failed to get size of '" << entry.path().string()
                    << "': " << e.what() << "\n";
        }
      }

      if (hasGitInfo)
      {
        fs::path rel = fs::relative(entry.path(), repo_root_path);
        std::string key = normalize_path(rel.generic_string());
        if (key == ".") key = "";
        auto it = fileStatus->find(key);
        if (it != fileStatus->end()) {
          const FileGitInfo &fi = it->second;
          std::string statusStr(1, fi.status);
          if (fi.ignored)
            std::cout << " " << color_gray("(" + statusStr + ")", opts.useColor);
          else if (fi.x != ' ' && fi.x != '?')
            std::cout << " " << color_yellow("(" + statusStr + ")", opts.useColor);
          else if (fi.y != ' ' && fi.y != '?')
            std::cout << " " << color_red("(" + statusStr + ")", opts.useColor);
          else
            std::cout << " " << color_gray("(" + statusStr + ")", opts.useColor);

          if (!fi.author.empty() || !fi.date.empty()) {
            std::string meta;
            if (!fi.author.empty()) {
              meta = fi.author;
              if (!fi.date.empty()) meta += ", " + fi.date;
            } else if (!fi.date.empty()) {
              meta = fi.date;
            }
            if (!meta.empty())
              std::cout << " " << color_gray("(" + meta + ")", opts.useColor);
          }
        }
      }
      std::cout << "\n";
    }
  }
}

} // namespace xtree
