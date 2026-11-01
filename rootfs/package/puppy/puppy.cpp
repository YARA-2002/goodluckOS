#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static const char* kFontFile      = "/usr/share/fonts/Inter_24pt-Medium.ttf";
static const char* kAppsFiles[]   = {"/usr/share/puppy/apps.puppy", "/home/player/apps.puppy"};
static const char* kPuppyFiles[]   = {"/home/player/.local/share/applications"};
//static const char* kLaunchFile    = "/dev/shm/launch";
static const char* kStateFile     = "/dev/shm/launcher_state";
static const char* kAutoStartFile = "/home/player/autolaunch";
static const char* kThemeRoot = "/home/player/.config/puppy/themes/n64-unified";

constexpr int kScreenW              = 640;
constexpr int kScreenH              = 480;

constexpr int kCellWidth            = 192;
constexpr int kCellHeight           = 128;
constexpr int kGap                  = 32;
constexpr int kPitchX               = kCellWidth  + kGap;
constexpr int kPitchY               = kCellHeight + kGap;

// Screen position of the currently selected entry.
constexpr int kSelX                 = 64;
constexpr int kSelY                 = 164;

constexpr int kHeaderTextYMargin    = 9;
constexpr int kTextMargin           = 16;
constexpr int kBorder               = 3;

constexpr Uint32 kIdleCheckMs       = 60000;
constexpr Uint32 kRepeatDelayMs     = 350;  // holding a direction repeats it after this...
constexpr Uint32 kRepeatRateMs      = 60;   // ...and then this often
constexpr int kStickPress           = 16000; // left stick counts as a direction past about half travel...
constexpr int kStickRelease         = 8000;  // ...and lets go below about a quarter (axes span +-32767)
constexpr size_t kMaxCachedIcons    = 64;

constexpr SDL_Color kWhite  {255, 255, 255, 255};
constexpr SDL_Color kGrey   {170, 170, 170, 255};
constexpr SDL_Color kBlack  {0, 0, 0, 255};
constexpr SDL_Color kYellow {255, 205, 60, 255};
constexpr SDL_Color kTile   {40, 40, 44, 255};
constexpr SDL_Color kClear  {24, 24, 28, 255};

// Works for built-in archives and any user-defined category in apps.puppy.
static std::string categorySlug(const std::string& name) {
    std::string out;
    bool separator = false;
    for (unsigned char c : name) {
        if (std::isalnum(c)) {
            if (separator && !out.empty()) out.push_back('-');
            separator = false;
            out.push_back((char)std::tolower(c));
        } else {
            separator = true;
        }
    }
    return out.empty() ? "games" : out;
}
static SDL_Color categoryAccent(const std::string& name) {
    std::string n = name;
    for (char& c : n) c = (char)std::tolower((unsigned char)c);
    if (n.find("nintendo 64") != std::string::npos || n.find("n64") != std::string::npos)
        return {68, 153, 255, 255};
    if (n.find("playstation") != std::string::npos || n.find("psp") != std::string::npos)
        return {110, 174, 250, 255};
    if (n.find("sega") != std::string::npos || n.find("genesis") != std::string::npos)
        return {83, 193, 244, 255};
    if (n.find("game boy") != std::string::npos || n.find("gba") != std::string::npos)
        return {91, 213, 141, 255};
    if (n.find("super nintendo") != std::string::npos || n.find("snes") != std::string::npos)
        return {165, 135, 241, 255};
    if (n.find("nintendo") != std::string::npos || n.find("nes") != std::string::npos)
        return {240, 98, 93, 255};
    unsigned int hash = 2166136261u;
    for (unsigned char c : n) { hash ^= c; hash *= 16777619u; }
    constexpr SDL_Color palette[] = {
        {240, 98, 93, 255}, {91, 213, 141, 255},
        {68, 153, 255, 255}, {241, 205, 91, 255}
    };
    return palette[hash % 4];
}

// String helper functions
static std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) ++a;
    while (b > a && std::isspace((unsigned char)s[b - 1])) --b;
    return s.substr(a, b - a);
}

static std::string upper(std::string s) {
    for (auto& c : s) c = (char)std::toupper((unsigned char)c);
    return s;
}

static std::string lower(std::string s) {
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

// Converts semicolon-separated-list to an vector:
// "a;b;c;" -> {"a","b","c"}
static std::vector<std::string> splitList(const std::string& s) {
    std::vector<std::string> out;
    size_t start = 0;
    while (start <= s.size()) {
        size_t end = s.find(';', start);
        if (end == std::string::npos) end = s.size();
        std::string item = trim(s.substr(start, end - start));
        if (!item.empty()) out.push_back(item);
        start = end + 1;
    }
    return out;
}

// Single-quote a string for safe use in a shell command line.
static std::string shellQuote(const std::string& s) {
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    out += "'";
    return out;
}

static void drawSurface(SDL_Renderer* r, SDL_Surface* s, int x, int y, int maxW = 0) {
    SDL_Texture* t = SDL_CreateTextureFromSurface(r, s);
    if (!t) return;
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    int w = (maxW > 0) ? std::min(s->w, maxW) : s->w;   // clip, don't squash
    SDL_Rect src{0, 0, w, s->h};
    SDL_Rect dst{x, y, w, s->h};
    SDL_RenderCopy(r, t, &src, &dst);
    SDL_DestroyTexture(t);
}

static void drawText(SDL_Renderer* r, TTF_Font* font, const std::string& text, int x, int y, SDL_Color color, int maxW = 0) {
    if (!font || text.empty()) return;
    SDL_Surface* s = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (!s) return;
    drawSurface(r, s, x, y, maxW);
    SDL_FreeSurface(s);
}

static int textWidth(TTF_Font* font, const std::string& text) {
    int w = 0, h = 0;
    if (font && !text.empty()) TTF_SizeUTF8(font, text.c_str(), &w, &h);
    return w;
}

class IconCache {
public:
    struct Icon {
        SDL_Texture* tex = nullptr;
        int w = 0, h = 0;
    };

    IconCache(SDL_Renderer* r, const std::string& fallbackPath) : renderer(r) {
        fallback = load(fallbackPath);
        if (!fallback.tex) std::cerr << "Warning: missing fallback icon: " << fallbackPath << "\n";
    }

    ~IconCache() {
        for (auto& [path, slot] : slots) if (slot.icon.tex) SDL_DestroyTexture(slot.icon.tex);
        if (fallback.tex) SDL_DestroyTexture(fallback.tex);
    }

    IconCache(const IconCache&) = delete;
    IconCache& operator=(const IconCache&) = delete;

    Icon get(const std::string& path) {
        if (path.empty()) return fallback;

        auto it = slots.find(path);
        if (it != slots.end()) {
            it->second.lastUsed = ++clock;
            return it->second.icon.tex ? it->second.icon : fallback;
        }

        if (slots.size() >= kMaxCachedIcons) evictOldest();

        Slot slot;
        slot.icon = load(path);
        slot.lastUsed = ++clock;
        slots[path] = slot;
        return slot.icon.tex ? slot.icon : fallback;
    }

private:
    struct Slot {
        Icon icon;
        uint64_t lastUsed = 0;
    };

    SDL_Renderer* renderer;
    Icon fallback;
    std::map<std::string, Slot> slots;
    uint64_t clock = 0;

    Icon load(const std::string& path) {
        Icon icon;
        SDL_Surface* loaded = IMG_Load(path.c_str());
        if (!loaded) return icon;
        SDL_Surface* src = SDL_ConvertSurfaceFormat(loaded, SDL_PIXELFORMAT_RGBA32, 0);
        SDL_FreeSurface(loaded);
        if (!src) return icon;

        SDL_Surface* result = src;

        if (src->w > kCellWidth || src->h > kCellHeight) {
            // Show complete portrait box covers instead of cropping their artwork.
            const float scale = std::min((float)kCellWidth / src->w, (float)kCellHeight / src->h);
            const int w = std::max(1, (int)std::lround(src->w * scale));
            const int h = std::max(1, (int)std::lround(src->h * scale));
            SDL_Surface* out = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
            if (!out) { SDL_FreeSurface(src); return icon; }
            SDL_SetSurfaceBlendMode(src, SDL_BLENDMODE_NONE);
            SDL_SoftStretchLinear(src, nullptr, out, nullptr);
            SDL_FreeSurface(src);
            result = out;
        }

        icon.w = result->w;
        icon.h = result->h;
        icon.tex = SDL_CreateTextureFromSurface(renderer, result);
        SDL_FreeSurface(result);
        if (icon.tex) SDL_SetTextureBlendMode(icon.tex, SDL_BLENDMODE_BLEND);
        return icon;
    }

    void evictOldest() {
        auto oldest = slots.begin();
        for (auto it = slots.begin(); it != slots.end(); ++it)
            if (it->second.lastUsed < oldest->second.lastUsed) oldest = it;
        if (oldest->second.icon.tex) SDL_DestroyTexture(oldest->second.icon.tex);
        slots.erase(oldest);
    }
};

struct Entry {
    std::string category;
    std::string name;
    std::string description;
    bool terminal;
    std::string command;
    std::string iconPath;

    void render(SDL_Renderer* r, IconCache& icons, TTF_Font* badgeFont, int x, int y, bool selected, bool autostart, SDL_Color accent) const {
        SDL_Rect cell{x, y, kCellWidth, kCellHeight};
        SDL_SetRenderDrawColor(r, kTile.r, kTile.g, kTile.b, kTile.a);
        SDL_RenderFillRect(r, &cell);

        IconCache::Icon icon = icons.get(iconPath);
        if (icon.tex) {
            SDL_Rect dst{x + (kCellWidth - icon.w) / 2, y + (kCellHeight - icon.h) / 2, icon.w, icon.h};
            SDL_RenderCopy(r, icon.tex, nullptr, &dst);
        }

        if (selected) {
            SDL_SetRenderDrawColor(r, accent.r, accent.g, accent.b, accent.a);
            SDL_Rect strips[4] = {
                {x, y, kCellWidth, kBorder},                                // top
                {x, y + kCellHeight - kBorder, kCellWidth, kBorder},        // bottom
                {x, y, kBorder, kCellHeight},                               // left
                {x + kCellWidth - kBorder, y, kBorder, kCellHeight},        // right
            };
            SDL_RenderFillRects(r, strips, 4);
        }

        if (autostart && badgeFont) {
            static const std::string label = "autolaunch";
            const int padX = 6, padY = 2;
            int tw = textWidth(badgeFont, label);
            int th = TTF_FontHeight(badgeFont);
            SDL_Rect badge{x, y, tw + padX * 2, th + padY * 2};
            SDL_SetRenderDrawColor(r, accent.r, accent.g, accent.b, accent.a);
            SDL_RenderFillRect(r, &badge);
            drawText(r, badgeFont, label, badge.x + padX, badge.y + padY, kBlack);
        }
    }
};

struct Category {
    std::string name;
    std::vector<Entry> entries;
    int col = 0;
};

struct Model {
    std::vector<Category> categories;
    int row = 0;
    int& col() { return categories[row].col; }
    int  col() const { return categories[row].col; }
    int autoRow = -1, autoCol = -1;

    const Entry* selected() const {
        if (categories.empty()) return nullptr;
        const auto& entries = categories[row].entries;
        int c = categories[row].col;
        if (c < 0 || c >= (int)entries.size()) return nullptr;
        return &entries[c];
    }

    void move(int dx, int dy) {
        if (categories.empty()) return;
        row = std::clamp(row + dy, 0, (int)categories.size() - 1);
        Category& cat = categories[row];
        int last = std::max((int)cat.entries.size() - 1, 0);
        cat.col = std::clamp(cat.col + dx, 0, last);
    }

    bool isAutoStart(int r, int c) const { return r == autoRow && c == autoCol; }

    bool find(const std::string& category, const std::string& name, int& outRow, int& outCol) const {
        for (size_t r = 0; r < categories.size(); ++r) {
            if (categories[r].name != category) continue;
            for (size_t c = 0; c < categories[r].entries.size(); ++c) {
                if (categories[r].entries[c].name == name) {
                    outRow = (int)r;
                    outCol = (int)c;
                    return true;
                }
            }
        }
        return false;
    }
};

struct Archive {
    std::string name, description, command, defaultIcon;
    std::vector<std::string> dirs, exts, iconDirs;
};

struct Record {
    bool isArchive = false;
    Entry entry;
    Archive archive;

    std::string key() const {
        return isArchive ? "A\x1f" + archive.name
                         : "E\x1f" + entry.category + "\x1f" + entry.name;
    }
};

static void upsert(std::vector<Record>& records, const Record& rec) {
    const std::string k = rec.key();
    for (auto& existing : records) {
        if (existing.key() == k) {
            existing = rec;
            return;
        }
    }
    records.push_back(rec);
}

static void parseAppsFile(const std::string& path, std::vector<Record>& records) {
    std::ifstream in(path);
    if (!in) return;

    std::string section;
    std::map<std::string, std::string> kv;

    auto get = [&](const char* key) {
        auto it = kv.find(key);
        return it == kv.end() ? std::string() : it->second;
    };

    auto flush = [&]() {
        if (section == "ENTRY") {
            Record rec;
            rec.entry.category    = get("CATEGORY").empty() ? "Applications" : get("CATEGORY");
            rec.entry.name        = get("NAME");
            rec.entry.description = get("DESCRIPTION");
            rec.entry.terminal    = !get("TERMINAL").empty();
            rec.entry.command     = get("COMMAND");
            rec.entry.iconPath    = get("ICON");
            if (!rec.entry.name.empty() && !rec.entry.command.empty()) upsert(records, rec);
        } else if (section == "ARCHIVE") {
            Record rec;
            rec.isArchive = true;
            Archive& a = rec.archive;
            a.name        = get("NAME");
            a.description = get("DESCRIPTION");
            a.command     = get("COMMAND");
            a.defaultIcon = get("DEFAULT_ICON");
            a.dirs        = splitList(get("ENTRY_DIRECTORIES"));
            a.exts        = splitList(get("ENTRY_EXTENSIONS"));
            a.iconDirs    = splitList(get("ENTRY_ICONS_DIRECTORIES"));
            if (!a.name.empty() && !a.dirs.empty()) upsert(records, rec);
        }
        kv.clear();
    };

    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;

        if (line.front() == '[' && line.back() == ']') {
            flush();
            section = upper(trim(line.substr(1, line.size() - 2)));
            continue;
        }

        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        kv[upper(trim(line.substr(0, eq)))] = trim(line.substr(eq + 1));
    }
    flush();
}

static std::string buildArchiveCommand(const std::string& tmpl, const std::string& path) {
    const std::string quoted = shellQuote(path);
    if (tmpl.empty()) return quoted;

    std::string out = tmpl;
    bool replaced = false;
    size_t pos = 0;
    while ((pos = out.find("%s", pos)) != std::string::npos) {
        out.replace(pos, 2, quoted);
        pos += quoted.size();
        replaced = true;
    }
    if (!replaced) out += " " + quoted;
    return out;
}

static std::string findArchiveIcon(const Archive& a, const std::string& stem) {
    static const char* kExts[] = {"png", "jpg", "jpeg"};
    for (const auto& dir : a.iconDirs) {
        // Existing roms/<system>/icons remain supported.
        // Artwork from sibling roms/<system>/covers takes precedence.
        for (const auto& candidate : {fs::path(dir).parent_path() / "covers", fs::path(dir)}) {
            for (const char* ext : kExts) {
                fs::path p = candidate / (stem + "." + ext);
                std::error_code ec;
                if (fs::is_regular_file(p, ec)) return p.string();
            }
        }
    }
    return a.defaultIcon;
}

static std::vector<Entry> expandArchive(const Archive& a) {
    std::vector<std::string> exts;
    for (auto e : a.exts) {
        if (!e.empty() && e[0] == '.') e.erase(0, 1);
        exts.push_back(lower(e));
    }

    std::vector<Entry> out;
    for (const auto& dir : a.dirs) {
        std::error_code ec;
        for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
            std::error_code ec2;
            if (!it->is_regular_file(ec2)) continue;

            const fs::path& p = it->path();
            if (p.filename().string().empty() || p.filename().string()[0] == '.') continue;

            std::string ext = lower(p.extension().string());
            if (!ext.empty()) ext.erase(0, 1);
            if (!exts.empty() && std::find(exts.begin(), exts.end(), ext) == exts.end()) continue;

            Entry e;
            e.category    = a.name;
            e.name        = p.stem().string();
            e.description = a.description;
            e.command     = buildArchiveCommand(a.command, p.string());
            e.terminal    = false;
            e.iconPath    = findArchiveIcon(a, e.name);
            out.push_back(std::move(e));
        }
    }

    std::sort(out.begin(), out.end(), [](const Entry& l, const Entry& r) {
        return lower(l.name) < lower(r.name);
    });
    return out;
}

static void addToCategory(std::vector<Category>& cats, const Entry& e) {
    for (auto& c : cats) {
        if (c.name != e.category) continue;
        for (auto& existing : c.entries) {
            if (existing.name == e.name) { existing = e; return; }
        }
        c.entries.push_back(e);
        return;
    }
    cats.push_back({e.category, {e}});
}

static std::vector<Category> loadCatalog() {
    std::vector<Record> records;
    for (const char* path : kAppsFiles) parseAppsFile(path, records);

    //parse indiviual .puppy files in ~/.local/share/applications
    for (const auto& path : kPuppyFiles) {
        if (!fs::exists(path) || !fs::is_directory(path)) continue;
        for (const auto& entry : fs::directory_iterator(path)) {
            if (entry.path().extension() == ".puppy") {
                parseAppsFile(entry.path().string(), records);
            }
        }
    }

    std::vector<Category> cats;
    for (const auto& rec : records) {
        if (rec.isArchive) {
            for (const auto& e : expandArchive(rec.archive)) addToCategory(cats, e);
        } else {
            addToCategory(cats, rec.entry);
        }
    }
    return cats;
}

static void writeAutoStart(const Entry& e) {
    std::ofstream out(kAutoStartFile);
    if (out) out << "# " << e.category << "\t" << e.name << "\n" << e.command << "\n";
}

static void clearAutoStart() {
    std::error_code ec;
    fs::remove(kAutoStartFile, ec);
}

static bool readAutoStartId(std::string& category, std::string& name) {
    std::ifstream in(kAutoStartFile);
    std::string line;
    if (!in || !std::getline(in, line) || line.compare(0, 2, "# ") != 0) return false;
    size_t tab = line.find('\t', 2);
    if (tab == std::string::npos) return false;
    category = line.substr(2, tab - 2);
    name = line.substr(tab + 1);
    return true;
}

static void toggleAutoStart(Model& m) {
    const Entry* e = m.selected();
    if (!e) return;
    if (m.isAutoStart(m.row, m.col())) {
        clearAutoStart();
        m.autoRow = m.autoCol = -1;
    } else {
        writeAutoStart(*e);
        m.autoRow = m.row;
        m.autoCol = m.col();
    }
}

static void launch(const Model& m, const Entry& e) {
    // Save cursor position to file
    std::ofstream state(kStateFile);
    if (state) state << e.category << "\n" << e.name << "\n";

    std::ostringstream commandStream;
    commandStream << "doas appctl launch-application " << shellQuote(e.command);
    if (e.terminal) {
        commandStream << " 1";
    }
    commandStream << "\n";

    int exitCode = std::system(commandStream.str().c_str());
    (void)exitCode;
}

static void restoreCursor(Model& m) {
    std::ifstream in(kStateFile);
    std::string category, name;
    if (!in || !std::getline(in, category) || !std::getline(in, name)) return;
    int r, c;
    if (m.find(category, name, r, c)) { m.row = r; m.categories[r].col = c; }
}

static int readBattery() {
    std::error_code ec;
    for (fs::directory_iterator it("/sys/class/power_supply", ec), end; !ec && it != end; it.increment(ec)) {
        std::ifstream typeFile(it->path() / "type");
        std::string type;
        if (!(typeFile >> type) || type != "Battery") continue;
        std::ifstream capFile(it->path() / "capacity");
        int cap;
        if (capFile >> cap) return std::clamp(cap, 0, 100);
    }
    return -1; // no battery
}

class Ui {
public:
    Ui() {
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
            std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
            return;
        }
        sdlUp = true;
        IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
        TTF_Init();

        window = SDL_CreateWindow("Puppy Launcher", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, kScreenW, kScreenH, 0);
        if (!window) { std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n"; return; }

        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!renderer) { std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << "\n"; return; }

        SDL_RenderSetLogicalSize(renderer, kScreenW, kScreenH);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

        uiFont    = TTF_OpenFont(kFontFile, 22);
        titleFont = TTF_OpenFont(kFontFile, 34);
        descFont  = TTF_OpenFont(kFontFile, 20);
        badgeFont = TTF_OpenFont(kFontFile, 14);
        if (!uiFont || !titleFont || !descFont || !badgeFont)
            std::cerr << "Warning: could not load font " << kFontFile << "\n";

        overlay = IMG_LoadTexture(renderer, "/usr/share/puppy/assets/overlay.png");
        if (overlay) SDL_SetTextureBlendMode(overlay, SDL_BLENDMODE_BLEND);
        else std::cerr << "Warning: could not load /usr/share/puppy/assets/overlay.png\n";

        icons = std::make_unique<IconCache>(renderer, "/usr/share/puppy/assets/fallback.png");
        ok_ = true;
    }

    ~Ui() {
        icons.reset();
        if (overlay) SDL_DestroyTexture(overlay);
        for (auto& item : themeBackgrounds) if (item.second) SDL_DestroyTexture(item.second);
        for (auto& item : themeLogos) if (item.second) SDL_DestroyTexture(item.second);
        for (TTF_Font* f : {uiFont, titleFont, descFont, badgeFont}) if (f) TTF_CloseFont(f);
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        if (sdlUp) { TTF_Quit(); IMG_Quit(); SDL_Quit(); }
    }

    Ui(const Ui&) = delete;
    Ui& operator=(const Ui&) = delete;

    bool ok() const { return ok_; }

    void render(const Model& m, int battery) {
        const std::string activeCategory = m.categories.empty() ? std::string("Games") : m.categories[m.row].name;
        drawThemeBackground(activeCategory);

        for (int r = 0; r < (int)m.categories.size(); ++r) {
            int y = kSelY + (r - m.row) * kPitchY;
            if (y >= kScreenH || y + kCellHeight <= 0) continue;

            const auto& entries = m.categories[r].entries;
            int rowCol = m.categories[r].col;
            for (int c = 0; c < (int)entries.size(); ++c) {
                int x = kSelX + (c - rowCol) * kPitchX;
                if (x >= kScreenW || x + kCellWidth <= 0) continue;
                entries[c].render(renderer, *icons, badgeFont, x, y,
                                  r == m.row && c == rowCol, m.isAutoStart(r, c), categoryAccent(m.categories[r].name));
            }
        }

        if (overlay) SDL_RenderCopy(renderer, overlay, nullptr, nullptr);
        drawText_(m, battery);
        SDL_RenderPresent(renderer);
    }

private:
    bool sdlUp = false, ok_ = false;
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* overlay = nullptr;
    std::map<std::string, SDL_Texture*> themeBackgrounds;
    std::map<std::string, SDL_Texture*> themeLogos;
    TTF_Font *uiFont = nullptr, *titleFont = nullptr, *descFont = nullptr, *badgeFont = nullptr;
    std::unique_ptr<IconCache> icons;

    SDL_Texture* themeImage(std::map<std::string, SDL_Texture*>& cache, const std::string& path) {
        auto it = cache.find(path);
        if (it != cache.end()) return it->second;
        SDL_Texture* texture = IMG_LoadTexture(renderer, path.c_str());
        if (texture) SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        cache.emplace(path, texture);
        return texture;
    }

    void drawThemeBackground(const std::string& category) {
        const SDL_Color accent = categoryAccent(category);
        SDL_SetRenderDrawColor(renderer, kClear.r, kClear.g, kClear.b, kClear.a);
        SDL_RenderClear(renderer);
        for (int y = 0; y < kScreenH; y += 24) {
            const Uint8 v = (Uint8)(19 + y / 48);
            SDL_SetRenderDrawColor(renderer, v, v, (Uint8)(v + 9), 255);
            SDL_Rect stripe{0, y, kScreenW, 24};
            SDL_RenderFillRect(renderer, &stripe);
        }
        SDL_SetRenderDrawColor(renderer, accent.r, accent.g, accent.b, 255);
        SDL_Rect left{0, 0, 7, kScreenH};
        SDL_Rect top{0, 58, kScreenW, 3};
        SDL_RenderFillRect(renderer, &left);
        SDL_RenderFillRect(renderer, &top);
        const std::string p = std::string(kThemeRoot) + "/backgrounds/" + categorySlug(category) + ".png";
        if (SDL_Texture* bg = themeImage(themeBackgrounds, p)) {
            SDL_Rect full{0, 0, kScreenW, kScreenH};
            SDL_RenderCopy(renderer, bg, nullptr, &full);
            SDL_SetRenderDrawColor(renderer, 6, 9, 20, 130);
            SDL_RenderFillRect(renderer, &full);
        }
    }

    void drawText_(const Model& m, int battery) {
        const int maxW = kScreenW - 2 * kTextMargin;

        if (!m.categories.empty()) {
            const std::string& category = m.categories[m.row].name;
            const std::string p = std::string(kThemeRoot) + "/logos/" + categorySlug(category) + ".png";
            SDL_Texture* logo = themeImage(themeLogos, p);
            if (logo) {
                int w = 0, h = 0;
                if (SDL_QueryTexture(logo, nullptr, nullptr, &w, &h) == 0 && w > 0 && h > 0) {
                    const float scale = std::min(300.0f / w, 43.0f / h);
                    SDL_Rect dst{kTextMargin, 7, std::max(1, (int)(w * scale)), std::max(1, (int)(h * scale))};
                    SDL_RenderCopy(renderer, logo, nullptr, &dst);
                } else {
                    drawText(renderer, uiFont, category, kTextMargin, kHeaderTextYMargin, kWhite, maxW);
                }
            } else {
                drawText(renderer, uiFont, category, kTextMargin, kHeaderTextYMargin, kWhite, maxW);
            }
        }

        std::string s;
        if (battery >= 0) {
            s = std::to_string(battery);
        } else {
            s = "??";
        }
        drawText(renderer, uiFont, s, kScreenW - kTextMargin - textWidth(uiFont, s), kHeaderTextYMargin, kWhite);

        const Entry* e = m.selected();
        if (!e) return;

        const int bottom = kScreenH - kTextMargin;
        int descH = 0;
        if (descFont && !e->description.empty()) {
            SDL_Surface* s = TTF_RenderUTF8_Blended_Wrapped(descFont, e->description.c_str(), kGrey, maxW - 142);
            if (s) {
                descH = s->h;
                drawSurface(renderer, s, kTextMargin, bottom - descH);
                SDL_FreeSurface(s);
            }
        }
        if (titleFont) {
            int titleH = TTF_FontHeight(titleFont);
            drawText(renderer, titleFont, e->name, kTextMargin, bottom - descH - titleH, kWhite, maxW - 142);
        }
    }
};

enum class Action { None, Up, Down, Left, Right, Launch, ToggleAutoStart };

static Action actionFromKey(SDL_Keycode k) {
    switch (k) {
        case SDLK_UP:     return Action::Up;
        case SDLK_DOWN:   return Action::Down;
        case SDLK_LEFT:   return Action::Left;
        case SDLK_RIGHT:  return Action::Right;
        case SDLK_RETURN: return Action::Launch;
        case SDLK_SPACE:  return Action::ToggleAutoStart;
        default:          return Action::None;
    }
}

static bool isDirection(Action a) {
    return a == Action::Up || a == Action::Down || a == Action::Left || a == Action::Right;
}

static Action actionFromButton(Uint8 b) {
    switch (b) {
        case SDL_CONTROLLER_BUTTON_DPAD_UP:    return Action::Up;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  return Action::Down;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  return Action::Left;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return Action::Right;
        case SDL_CONTROLLER_BUTTON_A:          return Action::Launch;
        case SDL_CONTROLLER_BUTTON_Y:          return Action::ToggleAutoStart;
        default:                               return Action::None;
    }
}

// Maps the left stick onto the same Up/Down/Left/Right actions as the d-pad. `current` is the
// direction the stick is already holding (or None). A held direction is kept until the stick falls
// back towards centre, and only changes on a deliberate push, so a stick that wobbles sideways while
// springing back doesn't produce a stray move.
static Action actionFromStick(int x, int y, Action current) {
    int ax = std::abs(x), ay = std::abs(y);
    Action dir = ax >= ay ? (x < 0 ? Action::Left : Action::Right)
                          : (y < 0 ? Action::Up   : Action::Down);
    int pushed = std::max(ax, ay);
    if (isDirection(current)) {
        if (dir != current && pushed >= kStickPress) return dir;
        int along = current == Action::Left  ? -x
                  : current == Action::Right ?  x
                  : current == Action::Up    ? -y : y;
        return along >= kStickRelease ? current : Action::None;
    }
    return pushed >= kStickPress ? dir : Action::None;
}

int main() {
    Model model;
    model.categories = loadCatalog();
    if (model.categories.empty()) {
        std::cerr << "No entries found in apps.puppy files!\n";
        return 1;
    }

    restoreCursor(model);
    {
        std::string cat, name;
        if (readAutoStartId(cat, name)) model.find(cat, name, model.autoRow, model.autoCol);
    }

    Ui ui;
    if (!ui.ok()) return 1;

    SDL_GameController* pad = nullptr;
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) { pad = SDL_GameControllerOpen(i); break; }
    }

    bool running = true;
    bool dirty = true;
    int shownBattery = -2;
    Uint32 lastActivity = SDL_GetTicks();
    Action held = Action::None;
    Uint32 nextRepeat = 0;
    int stickX = 0, stickY = 0;
    Action stickDir = Action::None;

    while (running) {
        if (dirty) {
            shownBattery = readBattery();
            ui.render(model, shownBattery);
            dirty = false;
        }

        Uint32 now = SDL_GetTicks();
        Uint32 elapsed = now - lastActivity;
        int timeout = elapsed >= kIdleCheckMs ? 0 : (int)(kIdleCheckMs - elapsed);
        if (held != Action::None)
            timeout = std::min(timeout, (int)std::max<Sint32>(0, (Sint32)(nextRepeat - now)));

        SDL_Event ev;
        if (!SDL_WaitEventTimeout(&ev, timeout)) {
            now = SDL_GetTicks();
            if (held != Action::None && (Sint32)(now - nextRepeat) >= 0) {
                ev.type = SDL_USEREVENT;
                nextRepeat = now + kRepeatRateMs;
            } else {
                lastActivity = now;
                if (readBattery() != shownBattery) dirty = true;
                continue;
            }
        }

        do {
            Action action = Action::None;
            switch (ev.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_WINDOWEVENT:
                    if (ev.window.event == SDL_WINDOWEVENT_EXPOSED) dirty = true;
                    break;
                case SDL_USEREVENT:
                    action = held;
                    break;
                case SDL_KEYDOWN:
                    if (!ev.key.repeat) action = actionFromKey(ev.key.keysym.sym);
                    break;
                case SDL_CONTROLLERBUTTONDOWN:
                    action = actionFromButton(ev.cbutton.button);
                    break;
                case SDL_CONTROLLERAXISMOTION: {
                    if (ev.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX)      stickX = ev.caxis.value;
                    else if (ev.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) stickY = ev.caxis.value;
                    else break;
                    Action dir = actionFromStick(stickX, stickY, stickDir);
                    if (dir != stickDir) {
                        if (held == stickDir) held = Action::None; // the stick was driving the repeat
                        stickDir = dir;
                        action = dir;  // None on release; a direction acts like a d-pad press
                    }
                    break;
                }
                case SDL_KEYUP:
                    if (actionFromKey(ev.key.keysym.sym) == held) held = Action::None;
                    break;
                case SDL_CONTROLLERBUTTONUP:
                    if (actionFromButton(ev.cbutton.button) == held) held = Action::None;
                    break;
            }

            if (ev.type != SDL_USEREVENT && isDirection(action)) {
                held = action;
                nextRepeat = SDL_GetTicks() + kRepeatDelayMs;
            }

            switch (action) {
                case Action::Up:    model.move(0, -1); break;
                case Action::Down:  model.move(0, 1);  break;
                case Action::Left:  model.move(-1, 0); break;
                case Action::Right: model.move(1, 0);  break;
                case Action::ToggleAutoStart: toggleAutoStart(model); break;
                case Action::Launch:
                    held = Action::None;
                    if (const Entry* e = model.selected()) {
                        launch(model, *e);
                        //running = false;
                    }
                    break;
                case Action::None: break;
            }

            if (action != Action::None) {
                dirty = true;
                lastActivity = SDL_GetTicks();
            }
        } while (running && SDL_PollEvent(&ev));
    }

    if (pad) SDL_GameControllerClose(pad);
    return 0;
}
