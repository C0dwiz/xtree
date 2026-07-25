// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "xtree/gitignore.h"

#include <fstream>
#include <string>
#include <vector>

namespace xtree {

namespace fs = std::filesystem;

namespace {

std::string normalize_path(const fs::path &path) {
  return path.lexically_normal().generic_string();
}

// Match one path component. '*' and '?' do not cross '/', and '[...]' is a
// character class. Iterative backtracking avoids recursion on long filenames.
bool match_segment(const char *pat, size_t pn, const char *str, size_t sn) {
  size_t p = 0;
  size_t s = 0;
  size_t star_p = std::string::npos;
  size_t star_s = 0;

  while (s < sn) {
    if (p < pn && (pat[p] == '?' || pat[p] == str[s])) {
      ++p;
      ++s;
    } else if (p < pn && pat[p] == '\\' && p + 1 < pn && pat[p + 1] == str[s]) {
      p += 2;
      ++s;
    } else if (p < pn && pat[p] == '*') {
      star_p = p++;
      star_s = s;
    } else if (p < pn && pat[p] == '[') {
      size_t q = p + 1;
      bool negated = false;
      if (q < pn && (pat[q] == '!' || pat[q] == '^')) {
        negated = true;
        ++q;
      }

      bool matched = false;
      bool first = true;
      while (q < pn && (pat[q] != ']' || first)) {
        first = false;
        const char low = pat[q];
        if (q + 2 < pn && pat[q + 1] == '-' && pat[q + 2] != ']') {
          const char high = pat[q + 2];
          if (str[s] >= low && str[s] <= high)
            matched = true;
          q += 3;
        } else {
          if (str[s] == low)
            matched = true;
          ++q;
        }
      }

      if (q < pn && pat[q] == ']' && matched != negated) {
        p = q + 1;
        ++s;
      } else if (star_p != std::string::npos) {
        p = star_p + 1;
        s = ++star_s;
      } else {
        return false;
      }
    } else if (star_p != std::string::npos) {
      p = star_p + 1;
      s = ++star_s;
    } else {
      return false;
    }
  }

  while (p < pn && pat[p] == '*')
    ++p;
  return p == pn;
}

// Match path segments. A "**" segment consumes zero or more components.
bool match_segments(const std::vector<std::string> &pattern, size_t pattern_index,
                    const std::vector<std::string> &path, size_t path_index) {
  while (pattern_index < pattern.size()) {
    if (pattern[pattern_index] == "**") {
      while (pattern_index + 1 < pattern.size() && pattern[pattern_index + 1] == "**")
        ++pattern_index;
      if (pattern_index + 1 == pattern.size())
        return true;
      for (size_t i = path_index; i <= path.size(); ++i) {
        if (match_segments(pattern, pattern_index + 1, path, i))
          return true;
      }
      return false;
    }

    if (path_index >= path.size() ||
        !match_segment(pattern[pattern_index].data(), pattern[pattern_index].size(),
                       path[path_index].data(), path[path_index].size()))
      return false;
    ++pattern_index;
    ++path_index;
  }
  return path_index == path.size();
}

std::vector<std::string> split_path(const std::string &path) {
  std::vector<std::string> parts;
  size_t start = 0;
  for (size_t i = 0; i <= path.size(); ++i) {
    if (i == path.size() || path[i] == '/') {
      if (i > start)
        parts.emplace_back(path.substr(start, i - start));
      start = i + 1;
    }
  }
  return parts;
}

bool compile_rule(std::string line, GitIgnoreRule &rule) {
  if (!line.empty() && line.back() == '\r')
    line.pop_back();
  if (line.empty() || line.front() == '#')
    return false;

  size_t end = line.size();
  while (end > 0 && line[end - 1] == ' ') {
    if (end >= 2 && line[end - 2] == '\\')
      break;
    --end;
  }
  line.resize(end);
  if (line.empty())
    return false;

  rule.negation = false;
  rule.dirOnly = false;
  size_t pos = 0;
  if (line.front() == '!') {
    rule.negation = true;
    pos = 1;
  } else if (line.front() == '\\' && line.size() > 1 &&
             (line[1] == '#' || line[1] == '!')) {
    pos = 1;
  }

  std::string body = line.substr(pos);
  if (body.empty())
    return false;
  if (body.back() == '/') {
    rule.dirOnly = true;
    body.pop_back();
    if (body.empty())
      return false;
  }

  const bool anchored = body.find('/') != std::string::npos;
  if (!body.empty() && body.front() == '/')
    body.erase(0, 1);
  rule.segments = split_path(body);
  if (rule.segments.empty())
    return false;
  if (!anchored)
    rule.segments.insert(rule.segments.begin(), "**");
  return true;
}

} // namespace

GitIgnore::GitIgnore(const fs::path &root)
    : boundary_(normalize_path(fs::absolute(root).lexically_normal())) {
  fs::path current = fs::absolute(root).lexically_normal();
  if (!fs::is_directory(current))
    current = current.parent_path();

  while (!current.empty()) {
    std::error_code ec;
    if (fs::is_directory(current / ".git", ec) || fs::is_regular_file(current / ".git", ec)) {
      boundary_ = normalize_path(current);
      break;
    }
    const fs::path parent = current.parent_path();
    if (parent == current)
      break;
    current = parent;
  }
}

const std::vector<GitIgnoreRule> &GitIgnore::layer_for(const std::string &dir) const {
  auto it = cache_.find(dir);
  if (it != cache_.end())
    return it->second;

  std::vector<GitIgnoreRule> rules;
  const fs::path ignore_file = fs::path(dir) / ".gitignore";
  std::ifstream input(ignore_file, std::ios::in | std::ios::binary);
  if (input) {
    std::string line;
    while (std::getline(input, line)) {
      GitIgnoreRule rule;
      if (compile_rule(line, rule))
        rules.emplace_back(std::move(rule));
    }
  }

  auto inserted = cache_.emplace(dir, std::move(rules));
  return inserted.first->second;
}

bool GitIgnore::is_ignored(const fs::path &path, bool isDir) const {
  const fs::path absolute = fs::absolute(path).lexically_normal();
  const std::string full = normalize_path(absolute);
  const std::string cache_key = full + (isDir ? "/" : "");
  auto cached = result_cache_.find(cache_key);
  if (cached != result_cache_.end())
    return cached->second;

  if (full == boundary_) {
    result_cache_[cache_key] = false;
    return false;
  }
  const bool boundary_is_root = !boundary_.empty() && boundary_.back() == '/';
  if (full.size() <= boundary_.size() || full.compare(0, boundary_.size(), boundary_) != 0 ||
      (!boundary_is_root && full[boundary_.size()] != '/')) {
    result_cache_[cache_key] = false;
    return false;
  }

  std::vector<std::string> directories;
  fs::path current = absolute.parent_path();
  while (true) {
    const std::string directory = normalize_path(current);
    directories.push_back(directory);
    if (directory == boundary_)
      break;
    const fs::path parent = current.parent_path();
    if (parent == current)
      break;
    current = parent;
  }

  bool ignored = false;
  for (auto it = directories.rbegin(); it != directories.rend(); ++it) {
    const std::string &base = *it;
    if (full.size() <= base.size() || full.compare(0, base.size(), base) != 0)
      continue;

    const size_t separator = (!base.empty() && base.back() == '/') ? base.size() : base.size() + 1;
    if (full.size() <= separator)
      continue;
    const std::string relative = full.substr(separator);
    const auto &rules = layer_for(base);
    if (rules.empty())
      continue;

    const std::vector<std::string> relative_parts = split_path(relative);
    for (const GitIgnoreRule &rule : rules) {
      if (rule.dirOnly && !isDir)
        continue;
      if (match_segments(rule.segments, 0, relative_parts, 0))
        ignored = !rule.negation;
    }
  }

  result_cache_[cache_key] = ignored;
  return ignored;
}

} // namespace xtree
