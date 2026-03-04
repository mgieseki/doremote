// This is a modified version of file examples/cxx-api.cxx which is part of replxx.
// https://github.com/AmokHuginnsson/replxx

#include <vector>
#include <unordered_map>
#include <map>
#include <string>
#include <regex>
#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <utility>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>

#include <replxx.hxx>
#include "util.h"
#include "DoremoteWrapper.hpp"

using Replxx = replxx::Replxx;
using namespace replxx::color;

class Tick {
    typedef std::vector<char32_t> keys_t;
    std::thread _thread;
    int _tick;
    int _promptState;
    bool _alive;
    keys_t _keys;
    bool _tickMessages;
    bool _promptFan;
    Replxx& _replxx;
public:
    Tick(Replxx& replxx_, std::string const& keys_, bool tickMessages_, bool promptFan_)
        : _thread()
        , _tick(0)
        , _promptState(0)
        , _alive(false)
        , _keys(keys_.begin(), keys_.end())
        , _tickMessages(tickMessages_)
        , _promptFan(promptFan_)
        , _replxx(replxx_) {
    }
    void start() {
        _alive = true;
        _thread = std::thread(&Tick::run, this);
    }
    void stop() {
        _alive = false;
        _thread.join();
    }
    void run() {
        std::string s;
        static char const PROMPT_STATES[] = "-\\|/";
        while (_alive) {
            if (_tickMessages) {
                _replxx.print("%d\n", _tick);
            }
            if (_tick < static_cast<int>(_keys.size())) {
                _replxx.emulate_key_press(_keys[_tick]);
            }
            if (! _tickMessages && ! _promptFan && (_tick >= static_cast<int>(_keys.size()))) {
                break;
            }
            if (_promptFan) {
                for (int i(0); i < 4; ++ i) {
                    char prompt[] = "\x1b[1;32mreplxx\x1b[0m[ ]> ";
                    prompt[18] = PROMPT_STATES[_promptState % 4];
                    ++ _promptState;
                    _replxx.set_prompt(prompt);
                    std::this_thread::sleep_for(std::chrono::milliseconds(250));
                }
            } else {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            ++ _tick;
        }
    }
};

typedef std::vector<std::pair<std::string, Replxx::Color>> syntax_highlight_t;
typedef std::unordered_map<std::string, Replxx::Color> keyword_highlight_t;

static bool eq(std::string const& l, std::string const& r, size_t s, bool ic) {
    if (l.length() < s) {
        return false;
    }
    if (r.length() < s) {
        return false;
    }
    bool same(true);
    for (int i(0); same && (i < s); ++ i) {
        same = (ic && (towlower(l[i]) == towlower(r[i]))) || (l[i] == r[i]);
    }
    return same;
}

static std::vector<std::string> extract_params (std::string const &paramstr) {
    std::vector<std::string> params;
    std::istringstream is(paramstr);
    std::string param;
    while (std::getline(is, param, ',')) {
        if (param[0] == '[') {
            param = param.substr(1);
            param.pop_back();
        }
        params.emplace_back(param);
    }
    std::sort(params.begin(), params.end());
    return params;
}

Replxx::completions_t hook_completion(std::string const& context, int& contextLen, const std::map<std::string, std::string> &commands, bool ignoreCase) {
    Replxx::completions_t completions;
    int utf8ContextLen(context_len(context.c_str()));
    int prefixLen(static_cast<int>(context.length()) - utf8ContextLen);
    if ((prefixLen > 0) && (context[prefixLen - 1] == '\\')) {
        -- prefixLen;
        ++ utf8ContextLen;
    }
    contextLen = utf8str_codepoint_len(context.c_str() + prefixLen, utf8ContextLen);

    Replxx::Color c(Replxx::Color::DEFAULT);
    std::string prefix { context.substr(prefixLen) };
    size_t qmpos = context.find('?');
    size_t eqpos = context.rfind('=');
    size_t cmpos = context.rfind(',');
    if (qmpos == std::string::npos) {
        for (const auto &[cmd, params] : commands) {
            bool lowerCasePrefix(std::none_of(prefix.begin(), prefix.end(), iswupper));
            if (eq(cmd, prefix, static_cast<int>(prefix.size()), ignoreCase && lowerCasePrefix))
                completions.emplace_back(cmd.c_str(), c);
        }
    }
    else if (eqpos == std::string::npos || (cmpos != std::string::npos && cmpos > eqpos)) {
        std::string cmd = context.substr(0, qmpos);
        auto it = commands.find(cmd);
        if (it != commands.end()) {
            bool lowerCasePrefix(std::none_of(prefix.begin(), prefix.end(), iswupper));
            for (const std::string &param : extract_params(it->second)) {
                if (eq(param, prefix, prefix.size(), ignoreCase && lowerCasePrefix))
                    completions.emplace_back(param.c_str(), c);
            }
        }
    }
    return completions;
}

Replxx::hints_t hook_hint(std::string const& context, int& contextLen, Replxx::Color& color, const std::map<std::string, std::string> &commands, bool ignoreCase) {
    Replxx::hints_t hints;
    int utf8ContextLen(context_len(context.c_str()));
    int prefixLen(static_cast<int>(context.length()) - utf8ContextLen);
    contextLen = utf8str_codepoint_len(context.c_str() + prefixLen, utf8ContextLen);
    std::string prefix { context.substr(prefixLen) };
    size_t qmpos = context.find('?');
    size_t eqpos = context.rfind('=');
    size_t cmpos = context.rfind(',');
    if (qmpos == std::string::npos) {
        if (!prefix.empty()) {
            bool lowerCasePrefix(std::none_of(prefix.begin(), prefix.end(), iswupper));
            for (const auto &[cmd, params] : commands) {
                if (eq(cmd, prefix, prefix.size(), ignoreCase && lowerCasePrefix))
                    hints.emplace_back(cmd.c_str());
            }
        }
    }
    else if (eqpos == std::string::npos || (cmpos != std::string::npos && cmpos > eqpos)) {
        std::string cmd = context.substr(0, qmpos);
        auto it = commands.find(cmd);
        if (it != commands.end()) {
            bool lowerCasePrefix(std::none_of(prefix.begin(), prefix.end(), iswupper));
            for (const std::string &param : extract_params(it->second)) {
                if (eq(param, prefix, prefix.size(), ignoreCase && lowerCasePrefix))
                    hints.emplace_back(param.c_str());
            }
        }
    }

    // set hint color to green if single match found
    if (hints.size() == 1) {
        color = Replxx::Color::GREEN;
    }

    return hints;
}

inline bool is_kw(char ch) {
    return isalnum(ch) || (ch == '_');
}

void hook_color(std::string const& context, Replxx::colors_t& colors, syntax_highlight_t const& regex_color, keyword_highlight_t const& word_color) {
    // highlight matching regex sequences
    for (auto const& e : regex_color) {
        size_t pos {0};
        std::string str = context;
        std::smatch match;

        while(std::regex_search(str, match, std::regex(e.first))) {
            std::string c{ match[0] };
            std::string prefix(match.prefix().str());
            pos += utf8str_codepoint_len(prefix.c_str(), static_cast<int>(prefix.length()));
            int len(utf8str_codepoint_len(c.c_str(), static_cast<int>(c.length())));

            for (int i = 0; i < len; ++i) {
                colors.at(pos + i) = e.second;
            }

            pos += len;
            str = match.suffix();
        }
    }
    bool inWord(false);
    int wordStart(0);
    int wordEnd(0);
    int colorOffset(0);
    auto dohl = [&](int i) {
        inWord = false;
        std::string intermission(context.substr(wordEnd, wordStart - wordEnd));
        colorOffset += utf8str_codepoint_len(intermission.c_str(), intermission.length());
        int wordLen(i - wordStart);
        std::string keyword(context.substr(wordStart, wordLen));
        bool bold(false);
        if (keyword.substr(0, 5) == "bold_") {
            keyword = keyword.substr(5);
            bold = true;
        }
        bool underline(false);
        if (keyword.substr(0, 10) == "underline_") {
            keyword = keyword.substr(10);
            underline = true;
        }
        keyword_highlight_t::const_iterator it(word_color.find(keyword));
        Replxx::Color color = Replxx::Color::DEFAULT;
        if (it != word_color.end()) {
            color = it->second;
        }
        if (bold) {
            color = replxx::color::bold(color);
        }
        if (underline) {
            color = replxx::color::underline(color);
        }
        for (int k(0); k < wordLen; ++ k) {
            Replxx::Color& c(colors.at(colorOffset + k));
            if (color != Replxx::Color::DEFAULT) {
                c = color;
            }
        }
        colorOffset += wordLen;
        wordEnd = i;
    };
    for (int i(0); i < static_cast<int>(context.length()); ++ i) {
        if (!inWord) {
            if (is_kw(context[i])) {
                inWord = true;
                wordStart = i;
            }
        } else if (inWord && !is_kw(context[i])) {
            dohl(i);
        }
        if (context[i] != '_' && ispunct(context[i])) {
            wordStart = i;
            dohl(i + 1);
        }
    }
    if (inWord) {
        dohl(context.length());
    }
}

void hook_modify(std::string& currentInput_, int&, Replxx* rx) {
    char prompt[64];
    snprintf(prompt, 64, "\x1b[1;32mdoremote\x1b[0m[%zu]> ", currentInput_.length());
    rx->set_prompt(prompt);
}

Replxx::ACTION_RESULT message(Replxx& replxx, std::string s, char32_t) {
    replxx.invoke(Replxx::ACTION::CLEAR_SELF, 0);
    replxx.print("%s\n", s.c_str());
    replxx.invoke(Replxx::ACTION::REPAINT, 0);
    return (Replxx::ACTION_RESULT::CONTINUE);
}

static void bind_keys (Replxx &rx) {
    using namespace std::placeholders;
    // showcase key bindings
    rx.bind_key_internal(Replxx::KEY::BACKSPACE,                    "delete_character_left_of_cursor");
    rx.bind_key_internal(Replxx::KEY::DELETE,                       "delete_character_under_cursor");
    rx.bind_key_internal(Replxx::KEY::LEFT,                         "move_cursor_left");
    rx.bind_key_internal(Replxx::KEY::RIGHT,                        "move_cursor_right");
    rx.bind_key_internal(Replxx::KEY::UP,                           "line_previous");
    rx.bind_key_internal(Replxx::KEY::DOWN,                         "line_next");
    rx.bind_key_internal(Replxx::KEY::meta(Replxx::KEY::UP),        "history_previous");
    rx.bind_key_internal(Replxx::KEY::meta(Replxx::KEY::DOWN),      "history_next");
    rx.bind_key_internal(Replxx::KEY::PAGE_UP,                      "history_first");
    rx.bind_key_internal(Replxx::KEY::PAGE_DOWN,                    "history_last");
    rx.bind_key_internal(Replxx::KEY::HOME,                         "move_cursor_to_begining_of_line");
    rx.bind_key_internal(Replxx::KEY::END,                          "move_cursor_to_end_of_line");
    rx.bind_key_internal(Replxx::KEY::TAB,                          "complete_line");
    rx.bind_key_internal(Replxx::KEY::control(Replxx::KEY::LEFT),   "move_cursor_one_word_left");
    rx.bind_key_internal(Replxx::KEY::control(Replxx::KEY::RIGHT),  "move_cursor_one_word_right");
    rx.bind_key_internal(Replxx::KEY::control(Replxx::KEY::UP),     "hint_previous");
    rx.bind_key_internal(Replxx::KEY::control(Replxx::KEY::DOWN),   "hint_next");
    rx.bind_key_internal(Replxx::KEY::control(Replxx::KEY::ENTER),  "commit_line");
    rx.bind_key_internal(Replxx::KEY::control('R'),                 "history_incremental_search");
    rx.bind_key_internal(Replxx::KEY::control('W'),                 "kill_to_begining_of_word");
    rx.bind_key_internal(Replxx::KEY::control('U'),                 "kill_to_begining_of_line");
    rx.bind_key_internal(Replxx::KEY::control('K'),                 "kill_to_end_of_line");
    rx.bind_key_internal(Replxx::KEY::control('Y'),                 "yank");
    rx.bind_key_internal(Replxx::KEY::control('L'),                 "clear_screen");
    rx.bind_key_internal(Replxx::KEY::control('D'),                 "send_eof");
    rx.bind_key_internal(Replxx::KEY::control('C'),                 "abort_line");
    rx.bind_key_internal(Replxx::KEY::control('T'),                 "transpose_characters");
#ifndef _WIN32
    rx.bind_key_internal(Replxx::KEY::control('V'),                 "verbatim_insert");
    rx.bind_key_internal(Replxx::KEY::control('Z'),                 "suspend");
#endif
    rx.bind_key_internal(Replxx::KEY::meta(Replxx::KEY::BACKSPACE), "kill_to_whitespace_on_left");
    rx.bind_key_internal(Replxx::KEY::meta('p'),                    "history_common_prefix_search");
    rx.bind_key_internal(Replxx::KEY::meta('n'),                    "history_common_prefix_search");
    rx.bind_key_internal(Replxx::KEY::meta('d'),                    "kill_to_end_of_word");
    rx.bind_key_internal(Replxx::KEY::meta('y'),                    "yank_cycle");
    rx.bind_key_internal(Replxx::KEY::meta('u'),                    "uppercase_word");
    rx.bind_key_internal(Replxx::KEY::meta('l'),                    "lowercase_word");
    rx.bind_key_internal(Replxx::KEY::meta('c'),                    "capitalize_word");
    rx.bind_key_internal('a',                                       "insert_character");
    rx.bind_key_internal(Replxx::KEY::INSERT,                       "toggle_overwrite_mode");
    rx.bind_key(Replxx::KEY::F1, std::bind(&message, std::ref(rx), "<F1>", _1));
    rx.bind_key(Replxx::KEY::F2, std::bind(&message, std::ref(rx), "<F2>", _1));
    rx.bind_key(Replxx::KEY::F3, std::bind(&message, std::ref(rx), "<F3>", _1));
    rx.bind_key(Replxx::KEY::F4, std::bind(&message, std::ref(rx), "<F4>", _1));
    rx.bind_key(Replxx::KEY::F5, std::bind(&message, std::ref(rx), "<F5>", _1));
    rx.bind_key(Replxx::KEY::F6, std::bind(&message, std::ref(rx), "<F6>", _1));
    rx.bind_key(Replxx::KEY::F7, std::bind(&message, std::ref(rx), "<F7>", _1));
    rx.bind_key(Replxx::KEY::F8, std::bind(&message, std::ref(rx), "<F8>", _1));
    rx.bind_key(Replxx::KEY::F9, std::bind(&message, std::ref(rx), "<F9>", _1));
    rx.bind_key(Replxx::KEY::F10, std::bind(&message, std::ref(rx), "<F10>", _1));
    rx.bind_key(Replxx::KEY::F11, std::bind(&message, std::ref(rx), "<F11>", _1));
    rx.bind_key(Replxx::KEY::F12, std::bind(&message, std::ref(rx), "<F12>", _1));
    rx.bind_key(Replxx::KEY::shift(Replxx::KEY::F1), std::bind(&message, std::ref(rx), "<S-F1>", _1));
    rx.bind_key(Replxx::KEY::shift(Replxx::KEY::F2), std::bind(&message, std::ref(rx), "<S-F2>", _1));
    rx.bind_key(Replxx::KEY::shift(Replxx::KEY::F3), std::bind(&message, std::ref(rx), "<S-F3>", _1));
    rx.bind_key(Replxx::KEY::shift(Replxx::KEY::F4), std::bind(&message, std::ref(rx), "<S-F4>", _1));
    rx.bind_key(Replxx::KEY::shift(Replxx::KEY::F5), std::bind(&message, std::ref(rx), "<S-F5>", _1));
    rx.bind_key(Replxx::KEY::shift(Replxx::KEY::F6), std::bind(&message, std::ref(rx), "<S-F6>", _1));
    rx.bind_key(Replxx::KEY::shift(Replxx::KEY::F7), std::bind(&message, std::ref(rx), "<S-F7>", _1));
    rx.bind_key(Replxx::KEY::shift(Replxx::KEY::F8), std::bind(&message, std::ref(rx), "<S-F8>", _1));
    rx.bind_key(Replxx::KEY::shift(Replxx::KEY::F9), std::bind(&message, std::ref(rx), "<S-F9>", _1));
    rx.bind_key(Replxx::KEY::shift(Replxx::KEY::F10), std::bind(&message, std::ref(rx), "<S-F10>", _1));
    rx.bind_key(Replxx::KEY::shift(Replxx::KEY::F11), std::bind(&message, std::ref(rx), "<S-F11>", _1));
    rx.bind_key(Replxx::KEY::shift(Replxx::KEY::F12), std::bind(&message, std::ref(rx), "<S-F12>", _1));
    rx.bind_key(Replxx::KEY::control(Replxx::KEY::F1), std::bind(&message, std::ref(rx), "<C-F1>", _1));
    rx.bind_key(Replxx::KEY::control(Replxx::KEY::F2), std::bind(&message, std::ref(rx), "<C-F2>", _1));
    rx.bind_key(Replxx::KEY::control(Replxx::KEY::F3), std::bind(&message, std::ref(rx), "<C-F3>", _1));
    rx.bind_key(Replxx::KEY::control(Replxx::KEY::F4), std::bind(&message, std::ref(rx), "<C-F4>", _1));
    rx.bind_key(Replxx::KEY::control(Replxx::KEY::F5), std::bind(&message, std::ref(rx), "<C-F5>", _1));
    rx.bind_key(Replxx::KEY::control(Replxx::KEY::F6), std::bind(&message, std::ref(rx), "<C-F6>", _1));
    rx.bind_key(Replxx::KEY::control(Replxx::KEY::F7), std::bind(&message, std::ref(rx), "<C-F7>", _1));
    rx.bind_key(Replxx::KEY::control(Replxx::KEY::F8), std::bind(&message, std::ref(rx), "<C-F8>", _1));
    rx.bind_key(Replxx::KEY::control(Replxx::KEY::F9), std::bind(&message, std::ref(rx), "<C-F9>", _1));
    rx.bind_key(Replxx::KEY::control(Replxx::KEY::F10), std::bind(&message, std::ref(rx), "<C-F10>", _1));
    rx.bind_key(Replxx::KEY::control(Replxx::KEY::F11), std::bind(&message, std::ref(rx), "<C-F11>", _1));
    rx.bind_key(Replxx::KEY::control(Replxx::KEY::F12), std::bind(&message, std::ref(rx), "<C-F12>", _1));
    rx.bind_key(Replxx::KEY::shift(Replxx::KEY::TAB), std::bind(&message, std::ref(rx), "<S-Tab>", _1));
    rx.bind_key(Replxx::KEY::control(Replxx::KEY::HOME), std::bind(&message, std::ref(rx), "<C-Home>", _1));
    rx.bind_key(Replxx::KEY::shift(Replxx::KEY::HOME), std::bind(&message, std::ref(rx), "<S-Home>", _1));
    rx.bind_key(Replxx::KEY::control(Replxx::KEY::END), std::bind(&message, std::ref(rx), "<C-End>", _1));
    rx.bind_key(Replxx::KEY::shift(Replxx::KEY::END), std::bind(&message, std::ref(rx), "<S-End>", _1));
    rx.bind_key(Replxx::KEY::control(Replxx::KEY::PAGE_UP), std::bind(&message, std::ref(rx), "<C-PgUp>", _1));
    rx.bind_key(Replxx::KEY::control(Replxx::KEY::PAGE_DOWN), std::bind(&message, std::ref(rx), "<C-PgDn>", _1));
    rx.bind_key(Replxx::KEY::shift(Replxx::KEY::LEFT), std::bind(&message, std::ref(rx), "<S-Left>", _1));
    rx.bind_key(Replxx::KEY::shift(Replxx::KEY::RIGHT), std::bind(&message, std::ref(rx), "<S-Right>", _1));
    rx.bind_key(Replxx::KEY::shift(Replxx::KEY::UP), std::bind(&message, std::ref(rx), "<S-Up>", _1));
    rx.bind_key(Replxx::KEY::shift(Replxx::KEY::DOWN), std::bind(&message, std::ref(rx), "<S-Down>", _1));
    rx.bind_key(Replxx::KEY::meta('\r'), std::bind(&message, std::ref(rx), "<M-Enter>", _1));
}

static void add_basic_commands (std::map<std::string,std::string> &doricoCommands) {
    std::vector<std::string> commands {
        "history", "quit", "exit",
        "connect", "disconnect"
    };
    for (const auto &cmd : commands)
        doricoCommands.emplace(cmd, "");
}

static void confirmation_callback () {
    std::cout << "Trying to connect to Dorico. Please approve the confirmation dialog in Dorico.\n";
}

int main(int argc_, char** argv_) {
    std::map<std::string,std::string> doricoCommands;
    add_basic_commands(doricoCommands);

    // highlight specific words
    // a regex string, and a color
    // the order matters, the last match will take precedence
    using cl = Replxx::Color;
    keyword_highlight_t word_color {
        // single chars
        {"`", cl::BRIGHTCYAN},
        {"'", cl::BRIGHTBLUE},
        {"\"", cl::BRIGHTBLUE},
        {"-", cl::BRIGHTBLUE},
        {"+", cl::BRIGHTBLUE},
        {"=", cl::BRIGHTBLUE},
        {"/", cl::BRIGHTBLUE},
        {"*", cl::BRIGHTBLUE},
        {"^", cl::BRIGHTBLUE},
        {".", cl::BRIGHTMAGENTA},
        {"(", cl::BRIGHTMAGENTA},
        {")", cl::BRIGHTMAGENTA},
        {"[", cl::BRIGHTMAGENTA},
        {"]", cl::BRIGHTMAGENTA},
        {"{", cl::BRIGHTMAGENTA},
        {"}", cl::BRIGHTMAGENTA},

        // color keywords
        {"color_black", cl::BLACK},
        {"color_red", cl::RED},
        {"color_green", cl::GREEN},
        {"color_brown", cl::BROWN},
        {"color_blue", cl::BLUE},
        {"color_magenta", cl::MAGENTA},
        {"color_cyan", cl::CYAN},
        {"color_lightgray", cl::LIGHTGRAY},
        {"color_gray", cl::GRAY},
        {"color_brightred", cl::BRIGHTRED},
        {"color_brightgreen", cl::BRIGHTGREEN},
        {"color_yellow", cl::YELLOW},
        {"color_brightblue", cl::BRIGHTBLUE},
        {"color_brightmagenta", cl::BRIGHTMAGENTA},
        {"color_brightcyan", cl::BRIGHTCYAN},
        {"color_white", cl::WHITE},

        // commands
//        {"help", cl::BRIGHTMAGENTA},
        {"history", cl::BRIGHTMAGENTA},
        {"quit", cl::BRIGHTMAGENTA},
        {"exit", cl::BRIGHTMAGENTA},
        {"connect", cl::BRIGHTMAGENTA},
        {"disconnect", cl::BRIGHTMAGENTA},
//        {"clear", cl::BRIGHTMAGENTA},
//        {"prompt", cl::BRIGHTMAGENTA},
    };
    syntax_highlight_t regex_color {
        // numbers
        {"[\\-|+]{0,1}[0-9]+", cl::YELLOW}, // integers
        {"[\\-|+]{0,1}[0-9]*\\.[0-9]+", cl::YELLOW}, // decimals

        // strings
        {"\".*?\"", cl::BRIGHTGREEN}, // double quotes
        {"\'.*?\'", cl::BRIGHTGREEN}, // single quotes
        {R"(\bk[A-Z][A-Za-z]+\b)", cl::BLUE},
    };
    static int const MAX_LABEL_NAME(32);
    char label[MAX_LABEL_NAME];
    for (int r(0); r < 6; ++ r) {
        for (int g(0); g < 6; ++ g) {
            for (int b(0); b < 6; ++ b) {
                snprintf(label, MAX_LABEL_NAME, "rgb%d%d%d", r, g, b);
                word_color.insert(std::make_pair(label, replxx::color::rgb666(r, g, b)));
                for (int br(0); br < 6; ++ br) {
                    for (int bg(0); bg < 6; ++ bg) {
                        for (int bb(0); bb < 6; ++ bb) {
                            snprintf(label, MAX_LABEL_NAME, "fg%d%d%dbg%d%d%d", r, g, b, br, bg, bb);
                            word_color.insert(
                                std::make_pair(
                                    label,
                                    rgb666(r, g, b) | replxx::color::bg(rgb666(br, bg, bb))
                               )
                           );
                        }
                    }
                }
            }
        }
    }
    for (int gs(0); gs < 24; ++ gs) {
        snprintf(label, MAX_LABEL_NAME, "gs%d", gs);
        word_color.insert(std::make_pair(label, grayscale(gs)));
        for (int bgs(0); bgs < 24; ++ bgs) {
            snprintf(label, MAX_LABEL_NAME, "gs%dgs%d", gs, bgs);
            word_color.insert(std::make_pair(label, grayscale(gs) | bg(grayscale(bgs))));
        }
    }
    Replxx::Color colorCodes[] = {
        Replxx::Color::BLACK, Replxx::Color::RED, Replxx::Color::GREEN, Replxx::Color::BROWN, Replxx::Color::BLUE,
        Replxx::Color::CYAN, Replxx::Color::MAGENTA, Replxx::Color::LIGHTGRAY, Replxx::Color::GRAY, Replxx::Color::BRIGHTRED,
        Replxx::Color::BRIGHTGREEN, Replxx::Color::YELLOW, Replxx::Color::BRIGHTBLUE, Replxx::Color::BRIGHTCYAN,
        Replxx::Color::BRIGHTMAGENTA, Replxx::Color::WHITE
    };
    for (Replxx::Color bg : colorCodes) {
        for (Replxx::Color fg : colorCodes) {
            snprintf(label, MAX_LABEL_NAME, "c_%d_%d", static_cast<int>(fg), static_cast<int>(bg));
            word_color.insert(std::make_pair(label, fg | replxx::color::bg(bg)));
        }
    }

    bool tickMessages(false);
    bool promptFan(false);
    bool promptInCallback(false);
    bool indentMultiline(false);
    bool bracketedPaste(false);
    bool ignoreCase(false);
    std::string keys;
    std::string prompt;
    int hintDelay(0);
#if 0
    while (argc_ > 1) {
        -- argc_;
        ++ argv_;
        switch ((*argv_)[0]) {
            case ('m'): tickMessages = true; break;
            case ('F'): promptFan = true; break;
            case ('P'): promptInCallback = true; break;
            case ('I'): indentMultiline = true; break;
            case ('i'): ignoreCase = true; break;
            case ('k'): keys = (*argv_) + 1; break;
            case ('d'): hintDelay = std::stoi((*argv_) + 1); break;
            case ('h'): examples.push_back((*argv_) + 1); break;
            case ('p'): prompt = (*argv_) + 1; break;
            case ('B'): bracketedPaste = true; break;
        }
    }
#endif

    // init the repl
    Replxx rx;
    Tick tick(rx, keys, tickMessages, promptFan);
    rx.install_window_change_handler();

    // the path to the history file
    std::string history_file_path {".doremote_history"};

    // load the history file if it exists
    /* scope for ifstream object for auto-close */ {
        std::ifstream history_file(history_file_path.c_str());
        rx.history_load(history_file);
    }

    // set the max history size
    rx.set_max_history_size(128);

    // set the max number of hint rows to show
    rx.set_max_hint_rows(10);

    // set the callbacks
    using namespace std::placeholders;
    rx.set_completion_callback(std::bind(&hook_completion, _1, _2, std::cref(doricoCommands), ignoreCase));
    rx.set_highlighter_callback(std::bind(&hook_color, _1, _2, cref(regex_color), cref(word_color)));
    rx.set_hint_callback(std::bind(&hook_hint, _1, _2, _3, std::cref(doricoCommands), ignoreCase));
    if (promptInCallback) {
        rx.set_modify_callback(std::bind(&hook_modify, _1, _2, &rx));
    }

    // other api calls
    rx.set_word_break_characters(" \n\t.,-%!;:=*~^'\"/?<>|[](){}");
    rx.set_completion_count_cutoff(128);
    rx.set_hint_delay(hintDelay);
    rx.set_double_tab_completion(false);
    rx.set_complete_on_empty(true);
    rx.set_beep_on_ambiguous_completion(false);
    rx.set_no_color(false);
    rx.set_indent_multiline(indentMultiline);
    if (bracketedPaste) {
        rx.enable_bracketed_paste();
    }
    rx.set_ignore_case(ignoreCase);

    bind_keys(rx);

    // display initial welcome message
    std::cout
        << "Welcome to the Doremote Test Client.\n"
        << "Press TAB to view a list of the available commands.\n"
        << "Type 'quit' or 'exit' to leave the program.\n\n";

    const std::string defaultPrompt = "\x1b[1;32mdoremote\x1b[0m> ";
    // set the repl prompt
    if (prompt.empty()) {
        prompt = defaultPrompt;
    }

    // main repl loop
    if (! keys.empty() || tickMessages || promptFan) {
        tick.start();
    }

    DoremoteWrapper doremote;
    doremote.setConfirmationCallback(&confirmation_callback);
    for (;;) {
        // display the prompt and retrieve input from the user
        char const* cinput{ nullptr };

        do {
            cinput = rx.input(prompt);
        } while ((cinput == nullptr) && (errno == EAGAIN));

        if (cinput == nullptr) {
            break;
        }

        // change cinput into a std::string
        // easier to manipulate
        std::string input {cinput};

        if (doremote.terminated()) {
            std::cout << "connection to Dorico terminated\n";
            prompt = defaultPrompt;
            doricoCommands.clear();
            add_basic_commands(doricoCommands);
            doremote.disconnect();
            doremote.setTerminated(false);
        }
        if (input.empty()) {
            // user hit enter on an empty line
        }
        else if (input.compare(0, 5, "quit") == 0 || input.compare(0, 5, "exit") == 0) {
            // exit the repl
            rx.history_add(input);
            if (doremote.connected())
                std::cout << "disconnected from Dorico\n";
            break;
        }
        if (input.compare(0, 8, "history") == 0) {
            // display the current history
            Replxx::HistoryScan hs(rx.history_scan());
            for (int i(0); hs.next(); ++ i) {
                std::cout << std::setw(4) << i << ": " << hs.get().text() << "\n";
            }
            rx.history_add(input);
        }
        else if (input.compare(0, 6, "clear") == 0) {
            // clear the screen
            rx.clear_screen();
            rx.history_add(input);
        }
        else if (input.compare(0, 8, "connect") == 0) {
            switch (doremote.connect()) {
                case DOREMOTE_CONNECTED: {
                    DoricoAppInfo info = doremote.getDoricoAppInfo();
                    std::string doricoVersion = info.variant + std::string(" ") + info.number;
                    std::cout
                        << "connected to Dorico " << doricoVersion
                        << " (session token: " << doremote.sessionToken() << ")\n";
                    doricoCommands = doremote.getCommands();
                    add_basic_commands(doricoCommands);
                    prompt = "\x1b[1;32mDorico " + doricoVersion + "\x1b[0m> ";
                    break;
                }
                case DOREMOTE_CONNECTION_DENIED:
                    std::cout << "connection denied\n";
                    break;
                case DOREMOTE_OFFLINE:
                    std::cout << "can't connect to Dorico (it's probably not running)\n";
                    break;
                default: ;
            }
        }
        else if (input.compare(0, 11, "disconnect") == 0) {
            if (!doremote.connected())
                std::cout << "not connected to Dorico\n";
            else {
                doremote.disconnect();
                std::cout << "disconnected from Dorico\n";
                doricoCommands.clear();
                add_basic_commands(doricoCommands);
                prompt = defaultPrompt;
            }
        }
        else {
            if (!input.empty()) {
                if (!doremote.connected())
                    std::cout << "unknown command\n";
                else {
                    std::string json = doremote.sendCommand(input);
                    if (!json.empty())
                        std::cout << json << "\n";
                }
            }
        }
        rx.history_add(input);
    }
    if (! keys.empty() || tickMessages || promptFan) {
        tick.stop();
    }

    // save the history
    rx.history_sync(history_file_path);
    if (bracketedPaste) {
        rx.disable_bracketed_paste();
    }
    if (bracketedPaste || promptInCallback || promptFan) {
        std::cout << "\n" << prompt;
    }
    return 0;
}
