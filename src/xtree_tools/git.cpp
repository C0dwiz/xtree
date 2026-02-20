// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "xtree/git.h"

#include <cstdio>
#include <memory>
#include <sstream>
#include <string>

namespace xtree {

namespace fs = std::filesystem;

static bool findRepoRoot(fs::path cur, fs::path &repo_root)
{
  while (true)
  {
    if (fs::exists(cur / ".git") && fs::is_directory(cur / ".git"))
    {
      repo_root = cur;
      return true;
    }
    if (cur == cur.root_path() || cur == cur.parent_path())
      return false;
    cur = cur.parent_path();
  }
}

static std::string runCommandCapture(const std::string &cmd)
{
  std::unique_ptr<FILE, int (*)(FILE *)> pipe(popen(cmd.c_str(), "r"), pclose);
  if (!pipe)
    return {};
  
  std::string res;
  res.reserve(4096);
  char buffer[1024];
  size_t bytes_read;
  while ((bytes_read = fread(buffer, 1, sizeof(buffer), pipe.get())) > 0)
    res.append(buffer, bytes_read);
  return res;
}

static std::string trim(const std::string &s)
{
  size_t start = s.find_first_not_of(" \t\n\r");
  if (start == std::string::npos) return "";
  size_t end = s.find_last_not_of(" \t\n\r");
  return s.substr(start, end - start + 1);
}

bool get_git_status(const fs::path &target, fs::path &repo_root,
                  std::unordered_map<std::string, FileGitInfo> &fileStatus,
                  std::unordered_map<std::string, char> &dirStatus,
                  std::vector<std::string> &branches)
{
  if (!findRepoRoot(target, repo_root))
    return false;

  const std::string repoPath = repo_root.string();
  branches.reserve(16);

  std::string brcmd = "git -C \"" + repoPath + "\" branch --all --no-color";
  std::string brout = runCommandCapture(brcmd);
  std::istringstream brss(brout);
  std::string brline;
  while (std::getline(brss, brline))
  {
    if (brline.empty()) continue;
    
    std::string b = trim(brline);
    if (!b.empty() && b[0] == '*') b = b.substr(1);
    if (!b.empty()) branches.push_back(b);
  }

  std::string cmd = "git -C \"" + repoPath + "\" status --porcelain";
  std::string result = runCommandCapture(cmd);

  std::istringstream iss(result);
  std::string line;
  fileStatus.reserve(128);
  while (std::getline(iss, line))
  {
    if (line.empty())
      continue;
    FileGitInfo info;
    std::string path;
    if (line.size() >= 2 && line.compare(0, 2, "??") == 0)
    {
      info.status = 'U';
      info.x = ' ';
      info.y = '?';
      path = line.substr(3);
    }
    else if (line.size() >= 3)
    {
      info.x = line[0];
      info.y = line[1];
      info.status = (info.y != ' ') ? info.y : info.x;
      path = line.substr(3);
      size_t arrow = path.find(" -> ");
      if (arrow != std::string::npos)
        path = path.substr(arrow + 4);
    }
    path = trim(path);
    if (!path.empty())
      fileStatus[std::move(path)] = info;
  }

 
  std::string igncmd = "git -C \"" + repoPath + "\" ls-files --others -i --exclude-standard";
  std::string ignout = runCommandCapture(igncmd);
  std::istringstream ignss(ignout);
  while (std::getline(ignss, line))
  {
    line = trim(line);
    if (line.empty()) continue;
    
    auto it = fileStatus.find(line);
    if (it == fileStatus.end()) {
      FileGitInfo info;
      info.ignored = true;
      info.status = 'I';
      fileStatus[line] = std::move(info);
    } else {
      it->second.ignored = true;
      it->second.status = 'I';
    }
  }

  std::vector<std::string> filePaths;
  filePaths.reserve(fileStatus.size());
  for (const auto &kv : fileStatus)
    filePaths.push_back(kv.first);

  constexpr size_t BATCH_SIZE = 50;
  for (size_t i = 0; i < filePaths.size(); i += BATCH_SIZE) {
    size_t end = std::min(i + BATCH_SIZE, filePaths.size());
    std::string fileList;
    for (size_t j = i; j < end; ++j) {
      if (!fileList.empty()) fileList += " ";
      fileList += "\"" + filePaths[j] + "\"";
    }
    
    std::string logcmd = "git -C \"" + repoPath + "\" log -1 --format='%an|%ad' --date=short -- " + fileList + " 2>/dev/null";
    std::string out = runCommandCapture(logcmd);
    
    if (out.empty() || out.find('\n') != std::string::npos) {
      for (size_t j = i; j < end; ++j) {
        std::string singlecmd = "git -C \"" + repoPath + "\" log -1 --format='%an|%ad' --date=short -- \"" + filePaths[j] + "\" 2>/dev/null";
        std::string singleout = runCommandCapture(singlecmd);
        
        if (!singleout.empty()) {
          size_t pipe = singleout.find('|');
          if (pipe != std::string::npos) {
            std::string author = singleout.substr(0, pipe);
            std::string date = singleout.substr(pipe + 1);
            date = trim(date);
            
            if (date.size() >= 10)
              date = date.substr(0, 10);
            
            auto it = fileStatus.find(filePaths[j]);
            if (it != fileStatus.end()) {
              it->second.author = author;
              it->second.date = date;
            }
          }
        }
      }
    } else if (!out.empty()) {
      size_t pipe = out.find('|');
      if (pipe != std::string::npos) {
        std::string author = out.substr(0, pipe);
        std::string date = out.substr(pipe + 1);
        date = trim(date);
        
        if (date.size() >= 10)
          date = date.substr(0, 10);
        
        for (size_t j = i; j < end; ++j) {
          auto it = fileStatus.find(filePaths[j]);
          if (it != fileStatus.end()) {
            it->second.author = author;
            it->second.date = date;
          }
        }
      }
    }
  }

  auto priority = [](char c)
  {
    switch (c)
    {
    case 'M': return 5;
    case 'A': return 4;
    case 'D': return 3;
    case 'R': return 2;
    case 'C': return 1;
    case 'U': return 0;
    case 'I': return -2;
    default: return -1;
    }
  };

  
  for (const auto &pair : fileStatus)
  {
    const std::string &file = pair.first;
    char st = pair.second.status;
    std::string fullPath = file;
    while (true)
    {
      size_t pos = fullPath.find_last_of('/');
      if (pos == std::string::npos)
        fullPath = "";
      else
        fullPath = fullPath.substr(0, pos);
      auto it = dirStatus.find(fullPath);
      if (it == dirStatus.end() || priority(st) > priority(it->second))
        dirStatus[fullPath] = st;
      if (fullPath.empty())
        break;
    }
  }

  return true;
}

} // namespace xtree
