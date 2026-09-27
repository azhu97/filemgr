#include "testing.hpp"
#include <cstdlib>
#include <fstream>
#include <unistd.h>
#include "ui.hpp"

namespace fs = std::filesystem;

void writeFile(const fs::path& p, const std::string& content) {
    fs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary) << content;
}

std::string readFile(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

int main(int argc, char* argv[]) {
    std::string filter = argc > 1 ? argv[1] : "";
    ui::configure(ui::Verbosity::Silent, false);
    const bool tty = isatty(STDOUT_FILENO);
    const std::string ok = tty ? "\033[32mok\033[0m  " : "ok  ";
    const std::string fail = tty ? "\033[31mFAIL\033[0m" : "FAIL";

    char tmpl[] = "/tmp/filemgr-test-XXXXXX";
    const char* base_dir = mkdtemp(tmpl);
    if (!base_dir) {
        std::cerr << "cannot create temp dir\n";
        return 1;
    }
    fs::path base = fs::path(base_dir);
    // Keep tests away from the real ~/.filemgr, ~/.config and ~/.Trash.
    setenv("FILEMGR_STATE_DIR", (base / "state").c_str(), 1);
    setenv("FILEMGR_CONFIG", (base / "config").c_str(), 1);
    setenv("FILEMGR_TRASH", (base / "trash").c_str(), 1);
    unsetenv("FILEMGR_ROOT");

    int passed = 0, failed = 0, index = 0;
    for (const auto& t : testing::registry()) {
        if (!filter.empty() && t.name.find(filter) == std::string::npos) continue;
        testing::currentTmp() = base / ("t" + std::to_string(index++));
        fs::create_directories(tmp());
        try {
            t.fn();
            passed++;
            std::cout << "  " << ok << " " << t.name << "\n";
        } catch (const testing::Failure& f) {
            failed++;
            std::cout << "  " << fail << " " << t.name << "\n      " << f.message << "\n";
        } catch (const std::exception& e) {
            failed++;
            std::cout << "  " << fail << " " << t.name << "\n      unexpected exception: " << e.what() << "\n";
        }
    }

    fs::remove_all(base);
    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed ? 1 : 0;
}
