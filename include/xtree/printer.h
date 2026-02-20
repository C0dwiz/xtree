// printer.h
#pragma once

#include "xtree/options.h"
#include <filesystem>
#include <string>
#include <unordered_map>

#include "xtree/git.h"

namespace xtree {

namespace fs = std::filesystem;

void print_tree(const fs::path &path, const Options &opts,
               const std::unordered_map<std::string, uintmax_t> &dirSizes,
               const fs::path &repo_root,
               const std::unordered_map<std::string, FileGitInfo> *fileStatus,
               const std::unordered_map<std::string, char> *dirStatus,
               int depth = 0, std::string prefix = "");

} // namespace xtree
