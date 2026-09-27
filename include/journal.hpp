#pragma once
#include <ctime>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

// Append-only log of every file move filemgr makes, grouped into runs, so a
// run can be listed (`history`) and reversed (`undo`).
//
// File format (one record per line, fields tab-separated, paths escaped):
//   RUN    <id> <epoch-seconds> <root> <command>
//   MOVE   <id> <from> <to>
//   UNDONE <id> <undo-run-id>
// A RUN line is only written once the run's first move is recorded, so
// commands that change nothing leave no trace.

struct JournalRun {
    int id = 0;
    std::time_t when = 0;
    fs::path root;
    std::string command;
    std::vector<std::pair<fs::path, fs::path>> moves;  // (from, to), in order
    int undone_by = 0;                                 // id of the undo run, or 0
};

class Journal {
public:
    explicit Journal(fs::path file);

    // Starts a new run; nothing is written until the first recordMove().
    void beginRun(const std::string& command, const fs::path& root);
    // Thread-safe. Flushed immediately so a crash never loses a record.
    void recordMove(const fs::path& from, const fs::path& to);
    void markUndone(int run_id, int undo_run_id);

    // Id of the current run (valid after beginRun).
    int currentRunId() const { return run_id_; }

    std::vector<JournalRun> load() const;
    const fs::path& file() const { return file_; }

private:
    void append(const std::string& line);

    fs::path file_;
    std::mutex mutex_;
    int run_id_ = 0;
    std::string pending_header_;
};

// Location of filemgr's state: $FILEMGR_STATE_DIR, or ~/.filemgr.
fs::path stateDirectory();
