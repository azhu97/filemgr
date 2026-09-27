#include "journal.hpp"
#include <sstream>
#include <stdexcept>

namespace {

std::string escape(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '\t': out += "\\t"; break;
            case '\n': out += "\\n"; break;
            default: out += c;
        }
    }
    return out;
}

std::string unescape(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            char n = s[++i];
            out += n == 't' ? '\t' : n == 'n' ? '\n' : n;
        } else {
            out += s[i];
        }
    }
    return out;
}

std::vector<std::string> splitTabs(const std::string& line) {
    std::vector<std::string> fields;
    std::string field;
    std::istringstream in(line);
    while (std::getline(in, field, '\t')) fields.push_back(field);
    return fields;
}

} // namespace

fs::path stateDirectory() {
    if (const char* dir = getenv("FILEMGR_STATE_DIR"); dir && *dir) return dir;
    const char* home = getenv("HOME");
    if (!home) throw std::runtime_error("Could not determine home directory.");
    return fs::path(home) / ".filemgr";
}

Journal::Journal(fs::path file) : file_(std::move(file)) {}

void Journal::beginRun(const std::string& command, const fs::path& root) {
    int last = 0;
    for (const auto& run : load()) last = std::max(last, run.id);
    run_id_ = last + 1;
    pending_header_ = "RUN\t" + std::to_string(run_id_) + "\t" + std::to_string(std::time(nullptr)) + "\t" +
                      escape(root.string()) + "\t" + escape(command);
}

void Journal::recordMove(const fs::path& from, const fs::path& to) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!pending_header_.empty()) {
        append(pending_header_);
        pending_header_.clear();
    }
    append("MOVE\t" + std::to_string(run_id_) + "\t" + escape(from.string()) + "\t" + escape(to.string()));
}

void Journal::markUndone(int run_id, int undo_run_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    append("UNDONE\t" + std::to_string(run_id) + "\t" + std::to_string(undo_run_id));
}

void Journal::append(const std::string& line) {
    fs::create_directories(file_.parent_path());
    std::ofstream out(file_, std::ios::app);
    if (!out) throw std::runtime_error("cannot write journal " + file_.string());
    out << line << "\n";
    out.flush();
}

std::vector<JournalRun> Journal::load() const {
    std::vector<JournalRun> runs;
    std::ifstream in(file_);
    std::string line;

    auto find = [&runs](int id) -> JournalRun* {
        for (auto it = runs.rbegin(); it != runs.rend(); ++it)
            if (it->id == id) return &*it;
        return nullptr;
    };

    while (std::getline(in, line)) {
        auto f = splitTabs(line);
        try {
            if (f.size() == 5 && f[0] == "RUN") {
                JournalRun run;
                run.id = std::stoi(f[1]);
                run.when = static_cast<std::time_t>(std::stoll(f[2]));
                run.root = unescape(f[3]);
                run.command = unescape(f[4]);
                runs.push_back(std::move(run));
            } else if (f.size() == 4 && f[0] == "MOVE") {
                if (auto* run = find(std::stoi(f[1])))
                    run->moves.emplace_back(unescape(f[2]), unescape(f[3]));
            } else if (f.size() == 3 && f[0] == "UNDONE") {
                if (auto* run = find(std::stoi(f[1]))) run->undone_by = std::stoi(f[2]);
            }
        } catch (const std::exception&) {
            // Skip malformed lines rather than refusing to read the whole journal.
        }
    }
    return runs;
}
