// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include <string>
#include <vector>

namespace xtree {

struct Options {
  int maxDepth = -1;
  bool showHidden = false;
  bool showSize = false;
  bool showStats = false;
  bool useColor = true;
  bool followSymlinks = false;
  std::vector<std::string> ignorePatterns;
  bool gitStatus = false;
  bool diskUsage = false;
};

} // namespace xtree
