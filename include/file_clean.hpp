#pragma once
#include "config.hpp"
#include "utils.hpp"

// `filemgr clean`: applies the [rule ...] sections from the config. Only
// files filemgr manages are considered (top level, category folders,
// DUPLICATES); each file gets the first rule it matches. `only` limits the
// run to rules with these names (case-insensitive). With `list`, prints the
// rules and how many files each currently matches instead.
int cleanWithRules(const Context& ctx, const std::vector<std::string>& only, bool list);

// Where "trash" sends files: $FILEMGR_TRASH, or ~/.Trash.
fs::path trashDirectory();
