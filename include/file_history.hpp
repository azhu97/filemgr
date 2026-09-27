#pragma once
#include "journal.hpp"
#include "utils.hpp"

// Lists recent runs from the journal, or every move of run `run_id` when non-zero.
int showHistory(const Journal& journal, int limit, int run_id);

// Reverses run `run_id` (or the most recent run not yet undone when 0),
// moving each file back to where it came from in reverse order.
int undoRun(const Context& ctx, int run_id);
