#pragma once
#include "config.hpp"
#include "utils.hpp"

// `filemgr config [show|path|init|edit]`: inspect or create the config file.
int configCommand(const Config& config, const fs::path& config_path,
                  const std::string& action, bool force);
