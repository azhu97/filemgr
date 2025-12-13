#include "file_upload.hpp"

namespace fs = std::filesystem;

void uploadFolder(std::string folder_name) {
    std::string download_path = downloadPath();
    fs::path full_path = fs::path(download_path) / folder_name;

    if (!fs::exists(full_path)) {
        std::cout << folder_name << " does not exist in Downloads." << std::endl;
        return;
    }

    if (!fs::is_directory(full_path)) {
        std::cout << folder_name << " is not a directory." << std::endl;
        return;
    }

    std::cout << "Uploading folder: " << folder_name << std::endl;

    std::string command = "rclone copy \"" + full_path.string() + "\" gdrive:/" + folder_name;
    command += " --progress";

    int result = std::system(command.c_str());
    if (result != 0) {
        std::cout << "Failed to upload folder: " << folder_name << std::endl;
    } else {
        std::cout << "Successfully uploaded folder: " << folder_name << std::endl;
    }
}