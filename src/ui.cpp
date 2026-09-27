#include "ui.hpp"
#include <cstdio>
#include <iostream>
#include <mutex>

namespace ui {

namespace {
Verbosity g_verbosity = Verbosity::Normal;
bool g_color = false;
std::mutex g_mutex;

std::string wrap(const char* code, const std::string& text) {
    if (!g_color) return text;
    return std::string("\033[") + code + "m" + text + "\033[0m";
}

void print(std::ostream& os, const std::string& message) {
    std::lock_guard<std::mutex> lock(g_mutex);
    os << message << "\n";
}
} // namespace

void configure(Verbosity verbosity, bool color) {
    g_verbosity = verbosity;
    g_color = color;
}

bool colorEnabled() { return g_color; }
bool verbose() { return g_verbosity == Verbosity::Verbose; }

void info(const std::string& message) {
    if (g_verbosity >= Verbosity::Normal) print(std::cout, message);
}

void detail(const std::string& message) {
    if (g_verbosity == Verbosity::Verbose) print(std::cout, dim(message));
}

void warn(const std::string& message) {
    if (g_verbosity != Verbosity::Silent) print(std::cerr, yellow("warning: ") + message);
}
void error(const std::string& message) {
    if (g_verbosity != Verbosity::Silent) print(std::cerr, red("error: ") + message);
}
void summary(const std::string& message) {
    if (g_verbosity != Verbosity::Silent) print(std::cout, bold(message));
}

void action(const std::string& verb, const std::string& from, const std::string& to) {
    info("  " + cyan(verb) + " " + from + dim(" -> ") + to);
}

std::string bold(const std::string& text) { return wrap("1", text); }
std::string dim(const std::string& text) { return wrap("2", text); }
std::string green(const std::string& text) { return wrap("32", text); }
std::string yellow(const std::string& text) { return wrap("33", text); }
std::string red(const std::string& text) { return wrap("31", text); }
std::string cyan(const std::string& text) { return wrap("36", text); }

std::string humanSize(std::uintmax_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    double size = static_cast<double>(bytes);
    int unit = 0;
    while (size >= 1024 && unit < 4) {
        size /= 1024;
        unit++;
    }
    char buf[32];
    if (unit == 0) {
        snprintf(buf, sizeof(buf), "%ju B", bytes);
    } else {
        snprintf(buf, sizeof(buf), "%.1f %s", size, units[unit]);
    }
    return buf;
}

std::string plural(std::size_t count, const std::string& noun) {
    return std::to_string(count) + " " + noun + (count == 1 ? "" : "s");
}

} // namespace ui
