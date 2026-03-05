// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace xtree {

struct FileGitInfo {
  char x = ' ';
  char y = ' ';
  char status = '?';
  bool ignored = false;
  std::string author;
  std::string date;
};

bool get_git_status(const std::filesystem::path &target, std::filesystem::path &repo_root,
                    std::unordered_map<std::string, FileGitInfo> &fileStatus,
                    std::unordered_map<std::string, char> &dirStatus,
                    std::vector<std::string> &branches);

} // namespace xtree