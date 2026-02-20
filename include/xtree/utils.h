// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "xtree/options.h"
#include <filesystem>
#include <string>
#include <vector>
#include <unordered_map>

namespace xtree {

namespace fs = std::filesystem;

void print_help();

std::string color_blue(const std::string &s, bool color);
std::string color_green(const std::string &s, bool color);
std::string color_gray(const std::string &s, bool color);

std::string color_red(const std::string &s, bool color);
std::string color_yellow(const std::string &s, bool color);

std::string human_size(uintmax_t size);
std::string normalize_path(const std::string &p);

bool should_ignore(const fs::path &path, const Options &opts);
std::vector<fs::directory_entry> get_filtered_entries(const fs::path &path,
                                                    const Options &opts);

uintmax_t compute_dir_size(const fs::path &path, const Options &opts,
                         std::unordered_map<std::string, uintmax_t> &dirSizes);

void compute_project_stats(const fs::path &path, const Options &opts,
                          uintmax_t &fileCount, uintmax_t &lineCount);

void parse_ignore_patterns(const std::string &input,
                         std::vector<std::string> &patterns);

} // namespace xtree

inline void printHelp() { xtree::print_help(); }
