#pragma once
#include "utils.hpp"
#include <string>
#include <filesystem>
#include <iostream>
#include <cstdlib>

int uploadFolder(const Context& ctx, const std::string& folder_name, const std::string& remote);