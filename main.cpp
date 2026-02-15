// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

struct Options {
    int maxDepth = -1;
    bool showHidden = false;
    bool showSize = false;
    bool useColor = true;
};

std::string blue(const std::string& s, bool color) {
    return color ? "\033[1;34m" + s + "\033[0m" : s;
}

std::string green(const std::string& s, bool color) {
    return color ? "\033[1;32m" + s + "\033[0m" : s;
}

std::string gray(const std::string& s, bool color) {
    return color ? "\033[0;37m" + s + "\033[0m" : s;
}

std::string humanSize(uintmax_t size) {
    const char* units[] = {"B", "KB", "MB", "GB"};
    int i = 0;
    double dsize = size;
    while (dsize > 1024 && i < 3) {
        dsize /= 1024;
        ++i;
    }
    std::ostringstream out;
    out << std::fixed << std::setprecision(1) << dsize << units[i];
    return out.str();
}

void printTree(const fs::path& path, const Options& opts, int depth = 0, std::string prefix = "") {
    if (opts.maxDepth != -1 && depth > opts.maxDepth)
        return;

    std::vector<fs::directory_entry> entries;

    try {
        for (const auto& entry : fs::directory_iterator(path)) {
            std::string fname = entry.path().filename().string();
            if (!opts.showHidden && !fname.empty() && fname[0] == '.')
                continue;
            entries.push_back(entry);
        }
    } catch (...) {
        return;
    }

    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& entry = entries[i];
        bool isLast = (i == entries.size() - 1);

        std::cout << prefix;
        std::cout << (isLast ? "└── " : "├── ");

        std::string name = entry.path().filename().string();

        if (entry.is_directory()) {
            std::cout << blue(name, opts.useColor) << "\n";
            printTree(entry.path(), opts, depth + 1,
                      prefix + (isLast ? "    " : "│   "));
        } else {
            std::cout << green(name, opts.useColor);
            if (opts.showSize) {
                try {
                    auto size = entry.file_size();
                    std::cout << " " << gray("(" + humanSize(size) + ")", opts.useColor);
                } catch (...) {}
            }
            std::cout << "\n";
        }
    }
}

int main(int argc, char* argv[]) {
    Options opts;
    fs::path target = ".";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--all")
            opts.showHidden = true;
        else if (arg == "--size")
            opts.showSize = true;
        else if (arg == "--no-color")
            opts.useColor = false;
        else if (arg == "--depth" && i + 1 < argc)
            opts.maxDepth = std::stoi(argv[++i]);
        else
            target = arg;
    }

    std::cout << blue(target.string(), opts.useColor) << "\n";
    printTree(target, opts);

    return 0;
}
