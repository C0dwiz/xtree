// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "xtree/git.h"
#include "xtree/options.h"
#include "xtree/printer.h"
#include "xtree/utils.h"

#include <filesystem>
#include <functional>
#include <iostream>
#include <unordered_map>

namespace fs = std::filesystem;

int main(int argc, char *argv[]) {
  using namespace xtree;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--help") {
      print_help();
      return 0;
    }
  }

  Options opts;
  fs::path target = ".";

  const std::unordered_map<std::string, std::function<void(Options &, int &, int, char **)>>
      option_handlers = {
          {"--all", [](Options &o, int &, int, char **) { o.showHidden = true; }},
          {"--size", [](Options &o, int &, int, char **) { o.showSize = true; }},
          {"--no-color", [](Options &o, int &, int, char **) { o.useColor = false; }},
          {"--follow-links", [](Options &o, int &, int, char **) { o.followSymlinks = true; }},
          {"--git", [](Options &o, int &, int, char **) { o.gitStatus = true; }},
          {"--stats", [](Options &o, int &, int, char **) { o.showStats = true; }},
          {"--du", [](Options &o, int &, int, char **) { o.diskUsage = true; }},
          {"--depth",
           [](Options &o, int &i, int argc, char **argv) {
             if (i + 1 < argc)
               o.maxDepth = std::stoi(argv[++i]);
           }},
          {"--ignore", [](Options &o, int &i, int argc, char **argv) {
             if (i + 1 < argc)
               parse_ignore_patterns(argv[++i], o.ignorePatterns);
           }}};

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];

    if (arg.rfind("--ignore=", 0) == 0) {
      parse_ignore_patterns(arg.substr(9), opts.ignorePatterns);
      continue;
    }

    auto it = option_handlers.find(arg);
    if (it != option_handlers.end()) {
      it->second(opts, i, argc, argv);
    } else {
      target = arg;
    }
  }

  fs::path repo_root;
  std::unordered_map<std::string, FileGitInfo> fileStatus;
  std::unordered_map<std::string, char> dirStatus;
  std::vector<std::string> branches;
  bool gitOk = false;
  if (opts.gitStatus) {
    gitOk = get_git_status(target, repo_root, fileStatus, dirStatus, branches);
    if (!gitOk)
      std::cerr << "Not a git repository (or any parent). Ignoring --git.\n";
    else if (!branches.empty()) {
      std::cout << "Branches: ";
      for (size_t i = 0; i < branches.size(); ++i) {
        if (i)
          std::cout << ", ";
        std::cout << branches[i];
      }
      std::cout << "\n";
    }
  }

  std::unordered_map<std::string, uintmax_t> dirSizes;
  if (opts.diskUsage)
    compute_dir_size(target, opts, dirSizes);

  std::cout << color_blue(target.string(), opts.useColor) << "\n";
  print_tree(target, opts, dirSizes, gitOk ? repo_root : fs::path(), gitOk ? &fileStatus : nullptr,
             gitOk ? &dirStatus : nullptr);

  if (opts.showStats) {
    uintmax_t files = 0, lines = 0;
    compute_project_stats(target, opts, files, lines);
    std::ostringstream ss;
    ss << "Files: " << files << ", Lines: " << lines;
    std::cout << color_gray(ss.str(), opts.useColor) << "\n";
  }

  return 0;
}