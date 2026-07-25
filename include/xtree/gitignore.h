// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace xtree {

namespace fs = std::filesystem;

// A single compiled pattern from a .gitignore file.
struct GitIgnoreRule {
  std::vector<std::string> segments; // pattern split on '/'
  bool negation = false;             // leading '!'
  bool dirOnly = false;              // trailing '/'
};

// Self-contained .gitignore engine. Parses .gitignore files lazily as the tree
// is traversed and caches the compiled rules per directory. No dependency on an
// installed `git` binary, so it works on any platform.
//
// Matching semantics follow gitignore(5): later rules win, non-anchored
// patterns match at any depth, '**' spans directories, trailing '/' restricts a
// pattern to directories, and a leading '!' re-includes a match.
class GitIgnore {
public:
  explicit GitIgnore(const fs::path &root);

  // Returns true if `path` (which must live under the root passed to the
  // constructor) is ignored. `isDir` selects directory-only rules.
  bool is_ignored(const fs::path &path, bool isDir) const;

private:
  const std::vector<GitIgnoreRule> &layer_for(const std::string &dir) const;

  std::string boundary_; // normalized repository root; matching stops here
  mutable std::unordered_map<std::string, std::vector<GitIgnoreRule>> cache_;
  mutable std::unordered_map<std::string, bool> result_cache_;
};

} // namespace xtree
