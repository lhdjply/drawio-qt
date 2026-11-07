#pragma once

#include "app/Args.h"

namespace dq {

/// Command line export mode (`drawio-qt -x -f png -o out in.drawio`).
namespace CliExport {

/// Runs the export; returns the process exit code (0 success, 1 failure).
int run(const CliOptions &options);

} // namespace CliExport
} // namespace dq
