#include "file_upload.hpp"
#include <spawn.h>
#include <sys/wait.h>
#include <vector>

namespace fs = std::filesystem;

extern char** environ;

int uploadFolder(const Context& ctx, const std::string& folder_name, const std::string& remote) {
    fs::path full_path = ctx.root / folder_name;

    if (!fs::exists(full_path)) {
        ui::error(folder_name + " does not exist in " + ctx.root.string());
        return 1;
    }
    if (!fs::is_directory(full_path)) {
        ui::error(folder_name + " is not a directory");
        return 1;
    }

    std::string target = remote + ":/" + fs::path(folder_name).filename().string();
    std::vector<std::string> args = {"rclone", "copy", full_path.string(), target, "--progress"};

    std::string shown;
    for (const auto& a : args) shown += (shown.empty() ? "" : " ") + a;
    if (ctx.dry_run) {
        ui::info("Would run: " + shown);
        return 0;
    }
    ui::info("Uploading " + ui::bold(folder_name) + " to " + target);
    ui::detail("running: " + shown);

    // Spawn rclone directly (no shell), so folder names with quotes or other
    // shell metacharacters are passed through safely.
    std::vector<char*> argv;
    for (auto& a : args) argv.push_back(a.data());
    argv.push_back(nullptr);

    pid_t pid;
    int rc = posix_spawnp(&pid, "rclone", nullptr, nullptr, argv.data(), environ);
    if (rc != 0) {
        ui::error("could not run rclone (install it with: brew install rclone)");
        return 1;
    }
    int status = 0;
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        ui::error("upload of " + folder_name + " failed");
        return 1;
    }
    ui::summary("Uploaded " + folder_name);
    return 0;
}
