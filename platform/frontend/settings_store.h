#pragma once
// Shared by the OpenGL and Aurora menus. Comments, order and existing values
// in settings.txt and bindings.txt are preserved by both backends.
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <map>
#include <string>
#include <vector>
#include <strings.h>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
namespace sms_frontend::store {
inline std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

inline bool readLines(const std::string& path, std::vector<std::string>& out) {
    out.clear();
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return false;
    char buf[2048];
    while (fgets(buf, sizeof buf, f)) {
        std::string l = buf;
        while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
        out.push_back(l);
    }
    fclose(f);
    return true;
}

inline bool writeLines(const std::string& path, const std::vector<std::string>& lines) {
    std::string tmp = path + ".tmp";
    FILE* f = fopen(tmp.c_str(), "w");
    if (!f) return false;
    bool ok = true;
    for (const std::string& l : lines) if (fprintf(f, "%s\n", l.c_str()) < 0) ok = false;
    if (fclose(f) != 0) ok = false;
    if (!ok) { remove(tmp.c_str()); return false; }
#ifdef _WIN32
    return MoveFileExW(std::filesystem::u8path(tmp).c_str(), std::filesystem::u8path(path).c_str(),
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    return rename(tmp.c_str(), path.c_str()) == 0;
#endif
}

// settings.txt: `name = value` lines; a commented `# name = value` line is
// where a setting is written back when it was not set before.
struct SettingsFile {
    std::string path;
    std::vector<std::string> lines;
    std::map<std::string, std::string> values;

    static bool parse(const std::string& line, bool allowComment, std::string& key, std::string& val) {
        std::string s = trim(line);
        if (allowComment && !s.empty() && s[0] == '#') s = trim(s.substr(1));
        else if (s.empty() || s[0] == '#') return false;
        size_t eq = s.find('=');
        if (eq == std::string::npos) return false;
        key = trim(s.substr(0, eq));
        if (key.empty() || key.find(' ') != std::string::npos) return false;
        val = trim(s.substr(eq + 1));
        size_t c = val.find(" #");
        if (c != std::string::npos) val = trim(val.substr(0, c));
        return true;
    }
    void load() {
        values.clear();
        readLines(path, lines);
        for (const std::string& l : lines) {
            std::string k, v;
            if (parse(l, false, k, v)) values[k] = v;
        }
    }
    std::string get(const char* key, const char* def) const {
        auto it = values.find(key);
        return it == values.end() || it->second.empty() ? def : it->second;
    }
    void set(const char* key, const std::string& v) { values[key] = v; }
    bool save() {
        std::map<std::string, bool> done;
        for (std::string& l : lines) {  // active lines first
            std::string k, v;
            if (parse(l, false, k, v) && values.count(k) && !done[k]) {
                l = k + " = " + values[k];
                done[k] = true;
            }
        }
        for (std::string& l : lines) {  // then the commented examples
            std::string k, v;
            if (parse(l, true, k, v) && values.count(k) && !done[k] && trim(l)[0] == '#') {
                l = k + " = " + values[k];
                done[k] = true;
            }
        }
        bool header = false;
        for (auto& kv : values)
            if (!done[kv.first]) {
                if (!header) {
                    lines.push_back("");
                    lines.push_back("# Set from the launcher");
                    header = true;
                }
                lines.push_back(kv.first + " = " + kv.second);
            }
        return writeLines(path, lines);
    }
};

// ------------------------------------------------------------------ key bindings
const char* const kControls[] = {
    "A", "B", "X", "Y", "Z", "L", "R", "START",
    "DPAD_UP", "DPAD_DOWN", "DPAD_LEFT", "DPAD_RIGHT",
    "STICK_UP", "STICK_DOWN", "STICK_LEFT", "STICK_RIGHT",
    "CSTICK_UP", "CSTICK_DOWN", "CSTICK_LEFT", "CSTICK_RIGHT",
    "HALF_TILT", "QUIT",
};
const char* const kControlLabels[] = {
    "A button", "B button", "X button", "Y button", "Z button", "L trigger", "R trigger", "Start",
    "D-pad up", "D-pad down", "D-pad left", "D-pad right",
    "Stick up", "Stick down", "Stick left", "Stick right",
    "C-stick up", "C-stick down", "C-stick left", "C-stick right",
    "Half tilt (walk)", "Quit game",
};
const int kNumControls = int(sizeof kControls / sizeof kControls[0]);
const char* const kDefaultKeys[] = {
    "SPACE X", "LSHIFT RSHIFT C", "V", "F", "Z", "Q", "E", "ENTER",
    "1 KP_8", "2 KP_2", "3 KP_4", "4 KP_6",
    "UP W", "DOWN S", "LEFT A", "RIGHT D",
    "I", "K", "J", "L",
    "LCTRL", "ESCAPE",
};
// the names platform/pad understands, by SDL scancode
struct KeyName { const char* name; int code; };
const KeyName kKeyNames[] = {
    {"A", 4}, {"B", 5}, {"C", 6}, {"D", 7}, {"E", 8}, {"F", 9}, {"G", 10}, {"H", 11}, {"I", 12}, {"J", 13},
    {"K", 14}, {"L", 15}, {"M", 16}, {"N", 17}, {"O", 18}, {"P", 19}, {"Q", 20}, {"R", 21}, {"S", 22},
    {"T", 23}, {"U", 24}, {"V", 25}, {"W", 26}, {"X", 27}, {"Y", 28}, {"Z", 29}, {"1", 30}, {"2", 31},
    {"3", 32}, {"4", 33}, {"5", 34}, {"6", 35}, {"7", 36}, {"8", 37}, {"9", 38}, {"0", 39}, {"ENTER", 40},
    {"ESCAPE", 41}, {"BACKSPACE", 42}, {"TAB", 43}, {"SPACE", 44}, {"MINUS", 45}, {"EQUALS", 46},
    {"LBRACKET", 47}, {"RBRACKET", 48}, {"SEMICOLON", 51}, {"APOSTROPHE", 52}, {"COMMA", 54},
    {"PERIOD", 55}, {"SLASH", 56}, {"F1", 58}, {"F2", 59}, {"F3", 60}, {"F4", 61}, {"RIGHT", 79},
    {"LEFT", 80}, {"DOWN", 81}, {"UP", 82}, {"KP_DIVIDE", 84}, {"KP_MULTIPLY", 85}, {"KP_MINUS", 86},
    {"KP_PLUS", 87}, {"KP_ENTER", 88}, {"KP_1", 89}, {"KP_2", 90}, {"KP_3", 91}, {"KP_4", 92},
    {"KP_5", 93}, {"KP_6", 94}, {"KP_7", 95}, {"KP_8", 96}, {"KP_9", 97}, {"KP_0", 98}, {"LCTRL", 224},
    {"LSHIFT", 225}, {"LALT", 226}, {"RCTRL", 228}, {"RSHIFT", 229}, {"RALT", 230},
};
inline std::string keyName(int scancode) {
    for (const KeyName& k : kKeyNames)
        if (k.code == scancode) return k.name;
    return "#" + std::to_string(scancode);
}
struct BindingsFile {
    std::string path;
    std::vector<std::string> lines;
    std::string keys[kNumControls];

    void load() {
        for (int i = 0; i < kNumControls; i++) keys[i] = kDefaultKeys[i];
        readLines(path, lines);
        for (const std::string& l : lines) {
            std::string k, v;
            if (!SettingsFile::parse(l, false, k, v)) continue;
            for (int i = 0; i < kNumControls; i++)
                if (strcasecmp(k.c_str(), kControls[i]) == 0) keys[i] = v;
        }
    }
    // Updates the controls' lines in place (comments and order kept) and adds
    // a line for each changed control the file did not have.
    bool save() {
        std::vector<std::string> out = lines;
        bool present[kNumControls] = {};
        for (std::string& l : out) {
            std::string k, v;
            if (!SettingsFile::parse(l, false, k, v)) continue;
            for (int i = 0; i < kNumControls; i++)
                if (strcasecmp(k.c_str(), kControls[i]) == 0) {
                    if (v != keys[i]) l = std::string(kControls[i]) + " = " + keys[i];
                    present[i] = true;
                }
        }
        if (out.empty()) {
            out.push_back("# SMS port key bindings for controller 1 (written by the launcher).");
            out.push_back("# Format: CONTROL = KEY [KEY ...]; a line replaces that control's defaults.");
        }
        for (int i = 0; i < kNumControls; i++)
            if (!present[i] && keys[i] != kDefaultKeys[i]) out.push_back(std::string(kControls[i]) + " = " + keys[i]);
        if (out == lines) return true;  // unchanged: leave the file alone
        return writeLines(path, out);
    }
};

}
