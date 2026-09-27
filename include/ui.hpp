#pragma once
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

// Terminal output: verbosity levels, optional ANSI color, and thread-safe
// printing (dedup reports from worker threads).
namespace ui {

// Silent suppresses everything, including errors (used by the unit tests).
enum class Verbosity { Silent, Quiet, Normal, Verbose };

void configure(Verbosity verbosity, bool color);
bool colorEnabled();
bool verbose();

// Normal-level message on stdout (suppressed by --quiet).
void info(const std::string& message);
// Only shown with --verbose.
void detail(const std::string& message);
// Always shown; errors and warnings go to stderr.
void warn(const std::string& message);
void error(const std::string& message);
// Summary line shown even in quiet mode.
void summary(const std::string& message);

// "  <verb> from -> to" line for a single file operation.
void action(const std::string& verb, const std::string& from, const std::string& to);

// Styling helpers; return the text unchanged when color is off.
std::string bold(const std::string& text);
std::string dim(const std::string& text);
std::string green(const std::string& text);
std::string yellow(const std::string& text);
std::string red(const std::string& text);
std::string cyan(const std::string& text);

// Human-readable byte count, e.g. 1536 -> "1.5 KB".
std::string humanSize(std::uintmax_t bytes);
// Pluralise: plural(3, "file") -> "3 files".
std::string plural(std::size_t count, const std::string& noun);

} // namespace ui
