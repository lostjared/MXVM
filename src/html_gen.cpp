#include "mxvm/html_gen.hpp"
#include "mxvm/instruct.hpp"
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <cctype>
#include <sstream>

namespace {
    inline bool isIdentStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c=='_'; }
    inline bool isIdentChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c=='_'; }
    std::string lower(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
        return s;
    }
    std::string html_escape(const std::string &s) {
        std::string out;
        out.reserve(s.size()*12/10+8);
        for (char c: s) {
            switch (c) {
                case '&': out += "&amp;"; break;
                case '<': out += "&lt;"; break;
                case '>': out += "&gt;"; break;
                case '"': out += "&quot;"; break;
                case '\'': out += "&#39;"; break;
                case '\n': out += "\\n"; break;
                case '\t': out += "\\t"; break;
                default: out.push_back(c); break;
            }
        }
        return out;
    }
}

namespace mxvm {

    static std::unordered_set<std::string> buildInstructionSet() {
        std::unordered_set<std::string> s;
        for (const auto &name : IncType) {
            if (name == "NULL") continue;
            s.insert(lower(name));
        }
        return s;
    }

    static std::unordered_set<std::string> buildKeywordSet() {
        std::unordered_set<std::string> s;
        for (const auto &k : keywords) s.insert(lower(k));
        const char* extra[] = {
            "function","export","return","start","done","true","false","null",
            "string","int","byte","float","ptr","object","program","module","data","code","section"
        };
        for (auto *p : extra) s.insert(p);
        return s;
    }

    static const std::unordered_set<std::string> kInstructions = buildInstructionSet();
    static const std::unordered_set<std::string> kKeywords = buildKeywordSet();
    static const std::unordered_set<std::string> kTypes = []{
        return std::unordered_set<std::string>{"string","int","byte","float","ptr"};
    }();

    HTMLGen::HTMLGen(const std::string &text) : source(text) {}

    void HTMLGen::output(std::ostream &out, std::string filename) {
        out << "<!doctype html><html><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">";
        out << "<title>" << filename << " - [ MXVM Source ]</title>";
        out << "<style>";
        out << "body{background:#0b0b0b;color:#e6e6e6;margin:0;font:14px/1.5 ui-monospace,SFMono-Regular,Menlo,Monaco,Consolas,\"Liberation Mono\",\"Courier New\",monospace}";
        out << ".wrap{max-width:1200px;margin:0 auto;padding:24px}";
        out << "pre.code{white-space:pre-wrap;word-wrap:break-word;background:#111;padding:16px;border-radius:12px;border:1px solid #1f1f1f}";
        out << ".kw{color:#8be9fd;font-weight:600}";
        out << ".op{color:#ff79c6;font-weight:600}";
        out << ".id{color:#e6e6e6}";
        out << ".num{color:#bd93f9}";
        out << ".str{color:#f1fa8c}";
        out << ".com{color:#6a7a8c}";
        out << ".lbl{color:#50fa7b;font-weight:700}";
        out << ".typ{color:#ffa07a}";
        out << "</style></head><body><div class=\"wrap\"><pre class=\"code\"><code>";
        const std::string &s = source;
        size_t i = 0, n = s.size();
        while (i < n) {
            char c = s[i];
            if (c == '\r') { ++i; continue; }
            if (c == '\n') { out << "\n"; ++i; continue; }
            if (c == '\t') { out << "\t"; ++i; continue; }
            if (c == ' ')  { out << " "; ++i; continue; }

            if (c == '#') {
                size_t j = i;
                while (j < n && s[j] != '\n') ++j;
                std::string t = s.substr(i, j - i);
                out << "<span class=\"com\">" << html_escape(t) << "</span>";
                i = j;
                continue;
            }

            if (c == '"') {
                size_t j = i + 1;
                bool esc = false;
                while (j < n) {
                    if (!esc && s[j] == '"') { ++j; break; }
                    esc = (!esc && s[j] == '\\');
                    ++j;
                }
                std::string t = s.substr(i, j - i);
                out << "<span class=\"str\">" << html_escape(t) << "</span>";
                i = j;
                continue;
            }

            if (std::isdigit(static_cast<unsigned char>(c)) || (c=='-' && i+1<n && std::isdigit(static_cast<unsigned char>(s[i+1])))) {
                size_t j = i;
                if (s[j] == '-') ++j;
                if (j+1<n && s[j]=='0' && (s[j+1]=='x' || s[j+1]=='X')) {
                    j += 2;
                    while (j<n && std::isxdigit(static_cast<unsigned char>(s[j]))) ++j;
                } else {
                    while (j<n && std::isdigit(static_cast<unsigned char>(s[j]))) ++j;
                    if (j<n && s[j]=='.') { ++j; while (j<n && std::isdigit(static_cast<unsigned char>(s[j]))) ++j; }
                }
                std::string t = s.substr(i, j - i);
                out << "<span class=\"num\">" << html_escape(t) << "</span>";
                i = j;
                continue;
            }

            if (isIdentStart(c)) {
                size_t j = i + 1;
                while (j<n && isIdentChar(s[j])) ++j;
                bool is_label = (j<n && s[j] == ':');
                std::string ident = s.substr(i, j - i);
                std::string lid = lower(ident);
                if (is_label) {
                    out << "<span class=\"lbl\">" << html_escape(ident) << "</span>:";
                    i = j + 1;
                    continue;
                }
                if (kInstructions.count(lid)) {
                    out << "<span class=\"op\">" << html_escape(ident) << "</span>";
                } else if (kTypes.count(lid) || kKeywords.count(lid)) {
                    if (kTypes.count(lid)) out << "<span class=\"typ\">" << html_escape(ident) << "</span>";
                    else out << "<span class=\"kw\">" << html_escape(ident) << "</span>";
                } else {
                    out << "<span class=\"id\">" << html_escape(ident) << "</span>";
                }
                i = j;
                continue;
            }

            if (c == '{' || c == '}' || c == ',' || c == '(' || c == ')' || c == '[' || c == ']') {
                out << html_escape(std::string(1, c));
                ++i;
                continue;
            }

            out << html_escape(std::string(1, c));
            ++i;
        }
        out << "</code></pre></div></body></html>";
}

}
