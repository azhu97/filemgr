// Standalone HTML rendering of `filemgr stats --html`. The page is fully
// self-contained (inline CSS/JS, no network) so it can be opened offline or
// shared as a single file. Colors follow a light/dark token set; every chart
// is a single series, so bars use one hue and values are labelled directly.
#include "stats.hpp"
#include <sstream>

namespace {

std::string esc(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default: out += c;
        }
    }
    return out;
}

std::string rel(const FolderStats& s, const fs::path& p) {
    fs::path r = p.lexically_relative(s.root);
    return (r.empty() || *r.begin() == "..") ? p.string() : r.string();
}

std::string formatDate(std::time_t t) {
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", std::localtime(&t));
    return buf;
}

// Horizontal bar chart: one row per bucket, bar length ∝ bytes.
// `keep_empty` keeps zero rows, for ordered buckets such as age bands.
void barChart(std::ostringstream& out, const std::string& title, const std::vector<StatBucket>& buckets,
              std::uintmax_t total, bool keep_empty = false) {
    std::uintmax_t max = 0;
    for (const auto& b : buckets) max = std::max(max, b.bytes);
    out << "<section class=\"card\"><h2>" << esc(title) << "</h2><div class=\"bars\" role=\"list\">";
    for (const auto& b : buckets) {
        if (b.count == 0 && !keep_empty) continue;
        double width = max ? 100.0 * b.bytes / max : 0;
        int pct = total ? static_cast<int>(100.0 * b.bytes / total + 0.5) : 0;
        std::string tip = b.name + " — " + ui::humanSize(b.bytes) + " · " + ui::plural(b.count, "file") + " · " +
                          std::to_string(pct) + "% of total";
        out << "<div class=\"row\" role=\"listitem\" data-tip=\"" << esc(tip) << "\">"
            << "<span class=\"label\">" << esc(b.name) << "</span>"
            << "<span class=\"track\"><span class=\"bar" << (b.count ? "" : " empty") << "\" style=\"width:" << width << "%\"></span>"
            << "<span class=\"value\">" << esc(ui::humanSize(b.bytes)) << "</span></span>"
            << "<span class=\"count\">" << b.count << "</span></div>";
    }
    out << "</div></section>";
}

void tile(std::ostringstream& out, const std::string& label, const std::string& value, const std::string& note) {
    out << "<div class=\"tile\"><div class=\"tile-label\">" << esc(label) << "</div><div class=\"tile-value\">"
        << esc(value) << "</div><div class=\"tile-note\">" << note << "</div></div>";
}

const char* kStyle = R"CSS(
:root {
  color-scheme: light;
  --surface-0: #f5f4f1; --surface-1: #fcfcfb; --border: #e4e2dc;
  --text-primary: #0b0b0b; --text-secondary: #52514e; --text-muted: #7a7974;
  --series-1: #2a78d6; --track: #efeee9; --accent-warn: #9a6400;
}
@media (prefers-color-scheme: dark) {
  :root:not([data-theme="light"]) {
    color-scheme: dark;
    --surface-0: #111110; --surface-1: #1a1a19; --border: #2e2e2c;
    --text-primary: #ffffff; --text-secondary: #c3c2b7; --text-muted: #8f8e86;
    --series-1: #3987e5; --track: #262624; --accent-warn: #e0a53a;
  }
}
:root[data-theme="dark"] {
  color-scheme: dark;
  --surface-0: #111110; --surface-1: #1a1a19; --border: #2e2e2c;
  --text-primary: #ffffff; --text-secondary: #c3c2b7; --text-muted: #8f8e86;
  --series-1: #3987e5; --track: #262624; --accent-warn: #e0a53a;
}
* { box-sizing: border-box; }
body { margin: 0; background: var(--surface-0); color: var(--text-primary);
  font: 15px/1.5 -apple-system, BlinkMacSystemFont, "Helvetica Neue", sans-serif; }
main { max-width: 1040px; margin: 0 auto; padding: 32px 16px 64px; }
header h1 { font-size: 26px; margin: 0 0 4px; letter-spacing: -0.01em; }
header p { margin: 0; color: var(--text-secondary); overflow-wrap: anywhere; }
code { font: 13px ui-monospace, Menlo, monospace; }
.tiles { display: grid; grid-template-columns: repeat(auto-fit, minmax(min(200px, 100%), 1fr)); gap: 12px; margin: 24px 0; }
.tile, .card { background: var(--surface-1); border: 1px solid var(--border); border-radius: 12px; }
.tile { padding: 16px 18px; min-width: 0; overflow-wrap: anywhere; }
.tile-label { color: var(--text-secondary); font-size: 13px; }
.tile-value { font-size: 28px; font-weight: 650; letter-spacing: -0.02em; margin: 2px 0; font-variant-numeric: tabular-nums; }
.tile-note { color: var(--text-muted); font-size: 13px; }
.grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(min(420px, 100%), 1fr)); gap: 12px; }
@media (max-width: 480px) {
  .tiles { grid-template-columns: 1fr 1fr; }
  .tile-value { font-size: 22px; }
  .row { grid-template-columns: minmax(64px, 32%) 1fr 32px; gap: 6px; }
  .card { padding: 14px; }
}
.card { padding: 18px 20px; margin-bottom: 12px; min-width: 0; }
.card h2 { font-size: 15px; margin: 0 0 12px; }
.bars { display: grid; gap: 2px; }
.row { display: grid; grid-template-columns: minmax(80px, 30%) 1fr 48px; align-items: center; gap: 10px;
  padding: 4px 6px; border-radius: 6px; cursor: default; }
.row:hover { background: var(--track); }
.label { color: var(--text-secondary); font-size: 13px; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.track { display: flex; align-items: center; gap: 8px; min-width: 0; }
.bar { display: block; height: 16px; min-width: 2px; background: var(--series-1); border-radius: 0 4px 4px 0; flex: none; max-width: calc(100% - 72px); }
.bar.empty { min-width: 0; }
.value { font-size: 13px; color: var(--text-primary); font-variant-numeric: tabular-nums; white-space: nowrap; }
.count { text-align: right; color: var(--text-muted); font-size: 12px; font-variant-numeric: tabular-nums; }
table { width: 100%; border-collapse: collapse; font-size: 13px; }
th { text-align: left; color: var(--text-muted); font-weight: 500; padding: 6px 8px; border-bottom: 1px solid var(--border); }
td { padding: 6px 8px; border-bottom: 1px solid var(--border); vertical-align: top; word-break: break-all; }
td.num, th.num { text-align: right; white-space: nowrap; word-break: normal; font-variant-numeric: tabular-nums; }
tr:last-child td { border-bottom: none; }
.muted { color: var(--text-muted); }
.warn { color: var(--accent-warn); font-weight: 600; }
.tip { position: fixed; pointer-events: none; background: var(--text-primary); color: var(--surface-1);
  padding: 6px 10px; border-radius: 6px; font-size: 12px; opacity: 0; transition: opacity .1s; z-index: 10; max-width: 320px; }
footer { margin-top: 24px; color: var(--text-muted); font-size: 12px; }
)CSS";

const char* kScript = R"JS(
(function () {
  var tip = document.createElement('div');
  tip.className = 'tip';
  tip.setAttribute('role', 'tooltip');
  document.body.appendChild(tip);
  document.querySelectorAll('[data-tip]').forEach(function (el) {
    el.addEventListener('mouseenter', function () { tip.textContent = el.dataset.tip; tip.style.opacity = 1; });
    el.addEventListener('mousemove', function (e) {
      var x = Math.min(e.clientX + 14, window.innerWidth - tip.offsetWidth - 8);
      tip.style.left = x + 'px'; tip.style.top = (e.clientY + 16) + 'px';
    });
    el.addEventListener('mouseleave', function () { tip.style.opacity = 0; });
  });
})();
)JS";

} // namespace

std::string renderHtmlReport(const FolderStats& s) {
    std::ostringstream out;
    out << "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
        << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
        << "<title>Folder Report</title><style>" << kStyle << "</style></head><body><main>";

    out << "<header><h1>" << esc(s.root.filename().string()) << " report</h1><p><code>" << esc(s.root.string())
        << "</code> · generated " << esc(formatDate(s.generated)) << "</p></header>";

    out << "<div class=\"tiles\">";
    tile(out, "Total size", ui::humanSize(s.total_bytes), esc(ui::plural(s.total_files, "file")));
    if (s.duplicates_scanned) {
        tile(out, "Duplicate copies", ui::humanSize(s.duplicate_bytes),
             esc(std::to_string(s.duplicate_files)) + " extra copies · <code>filemgr dedup</code>");
    }
    tile(out, "Untouched " + std::to_string(s.stale_days) + "+ days", ui::humanSize(s.stale_bytes),
         esc(ui::plural(s.stale_files, "file")) + " · <code>filemgr old</code>");
    tile(out, "Unsorted", std::to_string(s.unsorted_files), "top-level files · <code>filemgr sort</code>");
    out << "</div>";

    out << "<div class=\"grid\">";
    barChart(out, "By type", s.by_category, s.total_bytes);
    barChart(out, "By folder", s.by_location, s.total_bytes);
    out << "</div>";
    barChart(out, "By last modified", s.by_age, s.total_bytes, /*keep_empty=*/true);

    out << "<section class=\"card\"><h2>Largest files</h2><table><thead><tr><th>File</th>"
        << "<th class=\"num\">Size</th><th class=\"num\">Modified</th></tr></thead><tbody>";
    for (const auto& f : s.largest) {
        out << "<tr><td>" << esc(rel(s, f.path)) << "</td><td class=\"num\">" << esc(ui::humanSize(f.bytes))
            << "</td><td class=\"num muted\">" << esc(formatDate(f.modified).substr(0, 10)) << "</td></tr>";
    }
    out << "</tbody></table></section>";

    if (s.duplicates_scanned) {
        out << "<section class=\"card\"><h2>Duplicate groups</h2>";
        if (s.duplicates.empty()) {
            out << "<p class=\"muted\">No byte-identical duplicates found.</p>";
        } else {
            out << "<table><thead><tr><th>Kept copy</th><th>Other copies</th><th class=\"num\">Wasted</th>"
                << "</tr></thead><tbody>";
            for (std::size_t i = 0; i < s.duplicates.size() && i < 50; ++i) {
                const auto& g = s.duplicates[i];
                out << "<tr><td>" << esc(rel(s, g.paths.front())) << "</td><td>";
                const std::size_t shown = std::min<std::size_t>(g.paths.size(), 4);
                for (std::size_t j = 1; j < shown; ++j) {
                    out << (j > 1 ? "<br>" : "") << esc(rel(s, g.paths[j]));
                }
                if (g.paths.size() > shown) {
                    out << "<br><span class=\"muted\">+" << g.paths.size() - shown << " more</span>";
                }
                out << "</td><td class=\"num warn\">" << esc(ui::humanSize(g.wasted())) << "</td></tr>";
            }
            out << "</tbody></table>";
            if (s.duplicates.size() > 50) {
                out << "<p class=\"muted\">…and " << s.duplicates.size() - 50 << " more groups.</p>";
            }
        }
        out << "</section>";
    }

    out << "<footer>Generated by filemgr. Nothing was changed. Run the suggested commands with "
        << "<code>--dry-run</code> first to preview.</footer></main><script>" << kScript
        << "</script></body></html>\n";
    return out.str();
}
