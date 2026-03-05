// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "xtree/git.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace xtree {

namespace fs = std::filesystem;

namespace {

struct ParsedStatusEntry {
  std::string path;
  FileGitInfo info;
};

std::string trim(const std::string &s) {
  size_t start = s.find_first_not_of(" \t\n\r");
  if (start == std::string::npos)
    return "";
  size_t end = s.find_last_not_of(" \t\n\r");
  return s.substr(start, end - start + 1);
}

std::string run_command_capture(const std::vector<std::string> &args, bool merge_stderr = false) {
  if (args.empty())
    return {};

  int pipefd[2];
  if (pipe(pipefd) != 0)
    return {};

  const pid_t pid = fork();
  if (pid < 0) {
    close(pipefd[0]);
    close(pipefd[1]);
    return {};
  }

  if (pid == 0) {
    dup2(pipefd[1], STDOUT_FILENO);
    if (merge_stderr)
      dup2(pipefd[1], STDERR_FILENO);
    close(pipefd[0]);
    close(pipefd[1]);

    std::vector<char *> argv;
    argv.reserve(args.size() + 1);
    for (const auto &arg : args)
      argv.push_back(const_cast<char *>(arg.c_str()));
    argv.push_back(nullptr);

    execvp(argv[0], argv.data());
    _exit(127);
  }

  close(pipefd[1]);

  std::string res;
  res.reserve(4096);
  std::array<char, 1024> buffer{};
  ssize_t bytes_read;
  while ((bytes_read = read(pipefd[0], buffer.data(), buffer.size())) > 0)
    res.append(buffer.data(), static_cast<size_t>(bytes_read));

  close(pipefd[0]);

  int status = 0;
  if (waitpid(pid, &status, 0) < 0)
    return {};

  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
    return {};

  return res;
}

bool resolve_repo_root(const std::filesystem::path &target, std::filesystem::path &repo_root) {
  const std::string out =
      run_command_capture({"git", "-C", target.string(), "rev-parse", "--show-toplevel"}, true);
  const std::string root = trim(out);
  if (root.empty())
    return false;
  repo_root = fs::path(root);
  return true;
}

void collect_branches(const std::string &repo_path, std::vector<std::string> &branches) {
  branches.clear();
  branches.reserve(16);

  const std::string output =
      run_command_capture({"git", "-C", repo_path, "branch", "--all", "--no-color"});

  std::istringstream stream(output);
  std::string line;
  while (std::getline(stream, line)) {
    std::string branch = trim(line);
    if (branch.empty())
      continue;
    if (branch.front() == '*') {
      branch.erase(0, 1);
      branch = trim(branch);
    }
    if (!branch.empty())
      branches.push_back(std::move(branch));
  }
}

std::optional<ParsedStatusEntry> parse_status_line(const std::string &line) {
  if (line.size() < 3)
    return std::nullopt;

  ParsedStatusEntry entry;

  if (line.compare(0, 2, "??") == 0) {
    entry.info.status = 'U';
    entry.info.x = ' ';
    entry.info.y = '?';
    entry.path = line.substr(3);
  } else if (line.compare(0, 2, "!!") == 0) {
    entry.info.status = 'I';
    entry.info.ignored = true;
    entry.path = line.substr(3);
  } else {
    entry.info.x = line[0];
    entry.info.y = line[1];
    entry.info.status = (entry.info.y != ' ') ? entry.info.y : entry.info.x;
    entry.path = line.substr(3);

    const size_t arrow_pos = entry.path.find(" -> ");
    if (arrow_pos != std::string::npos)
      entry.path = entry.path.substr(arrow_pos + 4);
  }

  entry.path = trim(entry.path);
  if (entry.path.empty())
    return std::nullopt;

  return entry;
}

void collect_file_status(const std::string &repo_path,
                         std::unordered_map<std::string, FileGitInfo> &file_status) {
  file_status.clear();
  file_status.reserve(128);

  const std::string output =
      run_command_capture({"git", "-C", repo_path, "status", "--porcelain=1", "--ignored"});

  std::istringstream stream(output);
  std::string line;
  while (std::getline(stream, line)) {
    const auto parsed = parse_status_line(line);
    if (!parsed.has_value())
      continue;
    file_status[parsed->path] = parsed->info;
  }
}

void fill_last_commit_metadata(const std::string &repo_path,
                               std::unordered_map<std::string, FileGitInfo> &file_status) {
  for (auto &kv : file_status) {
    if (kv.second.ignored)
      continue;

    const std::string output = run_command_capture(
        {"git", "-C", repo_path, "log", "-1", "--format=%an|%ad", "--date=short", "--", kv.first});
    if (output.empty())
      continue;

    const std::string meta = trim(output);
    const size_t delimiter = meta.find('|');
    if (delimiter == std::string::npos)
      continue;

    kv.second.author = meta.substr(0, delimiter);
    kv.second.date = trim(meta.substr(delimiter + 1));
    if (kv.second.date.size() > 10)
      kv.second.date.resize(10);
  }
}

int status_priority(char c) {
  switch (c) {
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
  case 'I':
    return -2;
  default:
    return -1;
  }
}

void build_directory_status(const std::unordered_map<std::string, FileGitInfo> &file_status,
                            std::unordered_map<std::string, char> &dir_status) {
  dir_status.clear();

  for (const auto &pair : file_status) {
    const char status = pair.second.status;
    std::string full_path = pair.first;

    while (true) {
      const size_t pos = full_path.find_last_of('/');
      if (pos == std::string::npos)
        full_path.clear();
      else
        full_path = full_path.substr(0, pos);

      auto it = dir_status.find(full_path);
      if (it == dir_status.end() || status_priority(status) > status_priority(it->second))
        dir_status[full_path] = status;

      if (full_path.empty())
        break;
    }
  }
}

} // namespace

bool get_git_status(const std::filesystem::path &target, std::filesystem::path &repo_root,
                    std::unordered_map<std::string, FileGitInfo> &fileStatus,
                    std::unordered_map<std::string, char> &dirStatus,
                    std::vector<std::string> &branches) {
  if (!resolve_repo_root(target, repo_root))
    return false;

  const std::string repo_path = repo_root.string();

  collect_branches(repo_path, branches);
  collect_file_status(repo_path, fileStatus);
  fill_last_commit_metadata(repo_path, fileStatus);
  build_directory_status(fileStatus, dirStatus);

  return true;
}

} // namespace xtree
