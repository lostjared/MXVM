#include "icode.hpp"
#include <regex>
#include <unordered_set>
#include <algorithm>
#include <sstream>
#include <vector>
#include <string>

namespace pascal {
    static inline std::string rtrim_comment(const std::string &line) {
        auto h = line.find('#');
        auto s = line.find("//");
        size_t cut = std::min(h == std::string::npos ? line.size() : h,
                              s == std::string::npos ? line.size() : s);
        std::string out = line.substr(0, cut);
        while (!out.empty() && (unsigned char)out.back() <= ' ') out.pop_back();
        return out;
    }
    static inline std::string trim(const std::string &s) {
        size_t a = 0, b = s.size();
        while (a < b && (unsigned char)s[a] <= ' ') ++a;
        while (b > a && (unsigned char)s[b-1] <= ' ') --b;
        return s.substr(a, b-a);
    }

    static void locate_sections(const std::vector<std::string>& lines,
                                size_t& dataStart, size_t& dataEnd,
                                size_t& codeStart, size_t& codeEnd) {
        dataStart = dataEnd = codeStart = codeEnd = 0;
        for (size_t i = 0; i < lines.size(); ++i) {
            if (!dataStart && lines[i].find("section data {") != std::string::npos) {
                dataStart = i + 1;
                for (size_t j = dataStart; j < lines.size(); ++j) {
                    if (lines[j].find('}') != std::string::npos) { dataEnd = j - 1; break; }
                }
            }
            if (!codeStart && lines[i].find("section code {") != std::string::npos) {
                codeStart = i + 1;
                for (size_t j = codeStart; j < lines.size(); ++j) {
                    if (lines[j].find('}') != std::string::npos) { codeEnd = j - 1; break; }
                }
            }
            if (dataStart && dataEnd && codeStart && codeEnd) break;
        }
    }

    std::string mxvmOpt(const std::string &text) {
        std::vector<std::string> lines;
        {
            std::istringstream is(text);
            std::string ln;
            while (std::getline(is, ln)) lines.push_back(ln);
        }

        size_t dataStart = 0, dataEnd = 0, codeStart = 0, codeEnd = 0;
        locate_sections(lines, dataStart, dataEnd, codeStart, codeEnd);
        if (!codeStart) {
            std::string out;
            for (auto &ln : lines) out += ln + "\n";
            return out;
        }

        std::string codeSection;
        for (size_t i = codeStart; i <= codeEnd && i < lines.size(); ++i) {
            codeSection += lines[i];
            codeSection.push_back('\n');
        }

        std::unordered_set<std::string> usedVars = {"rax","fmt_int","fmt_str","fmt_chr","fmt_float","newline"};
        std::regex varUsagePattern("\\b([a-zA-Z_][a-zA-Z0-9_]*)\\b");
        for (auto it = std::sregex_iterator(codeSection.begin(), codeSection.end(), varUsagePattern);
             it != std::sregex_iterator(); ++it) {
            usedVars.insert((*it).str(1));
        }

        if (dataStart) {
            std::regex varDeclPattern("^\\s*(export)?\\s*(int|string|ptr|float|double)\\s+([a-zA-Z_][a-zA-Z0-9_]*)\\s*=");
            std::vector<size_t> eraseData;
            for (size_t i = dataStart; i <= dataEnd && i < lines.size(); ++i) {
                std::smatch m;
                if (std::regex_search(lines[i], m, varDeclPattern)) {
                    bool isExport = m[1].length() > 0;
                    std::string name = m[3];
                    if (!isExport && !usedVars.count(name)) eraseData.push_back(i);
                }
            }
            std::sort(eraseData.begin(), eraseData.end(), std::greater<size_t>());
            for (auto idx : eraseData) if (idx < lines.size()) lines.erase(lines.begin() + idx);
            locate_sections(lines, dataStart, dataEnd, codeStart, codeEnd);
            if (!codeStart) {
                std::string out;
                for (auto &ln : lines) out += ln + "\n";
                return out;
            }
        }

        size_t cStart = codeStart, cEnd = std::min(codeEnd, lines.size() ? lines.size()-1 : 0);
        std::vector<std::string> code;
        code.reserve(cEnd - cStart + 1);
        for (size_t i = cStart; i <= cEnd; ++i) code.push_back(lines[i]);

        std::vector<std::string> pass1;
        pass1.reserve(code.size());
        std::regex movPat("^\\s*(\\s*)mov\\s+([^,\\s]+)\\s*,\\s*([^\\s#;]+)\\s*(?:[;#].*)?$", std::regex::icase);

        for (size_t i = 0; i < code.size(); ) {
            std::string raw0 = code[i];
            std::string noCom0 = rtrim_comment(raw0);
            std::smatch m0;
            if (std::regex_match(noCom0, m0, movPat)) {
                std::string indent = m0[1].str();
                std::string dst0 = trim(m0[2].str());
                std::string src0 = trim(m0[3].str());
                if (dst0 == src0) { ++i; continue; }
                size_t k = i + 1;
                while (k < code.size() && rtrim_comment(code[k]).empty()) ++k;
                if (k < code.size()) {
                    std::string raw1 = code[k];
                    std::string noCom1 = rtrim_comment(raw1);
                    std::smatch m1;
                    if (std::regex_match(noCom1, m1, movPat)) {
                        std::string dst1 = trim(m1[2].str());
                        std::string src1 = trim(m1[3].str());
                        if (dst1 == src0 && src1 == dst0) {
                            pass1.push_back("\tmov " + dst0 + ", " + src0);
                            i = k + 1;
                            continue;
                        }
                    }
                }
                pass1.push_back(raw0);
                ++i;
                continue;
            }
            pass1.push_back(raw0);
            ++i;
        }

        std::vector<std::string> pass2;
        pass2.reserve(pass1.size());
        std::regex movDstPat("^\\s*mov\\s+([A-Za-z_][A-Za-z0-9_]*)\\s*,", std::regex::icase);

        for (size_t i = 0; i < pass1.size(); ++i) {
            std::string raw = pass1[i];
            std::string noCom = rtrim_comment(raw);
            std::smatch mm;
            if (std::regex_match(noCom, mm, movDstPat)) {
                std::string dst = mm[1].str();
                bool usedLater = false;
                std::regex word("\\b" + dst + "\\b");
                for (size_t j = i + 1; j < pass1.size(); ++j) {
                    std::string nxt = rtrim_comment(pass1[j]);
                    if (!nxt.empty() && std::regex_search(nxt, word)) { usedLater = true; break; }
                }
                if (!usedLater) continue;
            }
            pass2.push_back(raw);
        }

        std::vector<std::string> finalLines;
        finalLines.insert(finalLines.end(), lines.begin(), lines.begin() + cStart);
        finalLines.insert(finalLines.end(), pass2.begin(), pass2.end());
        if (cEnd + 1 < lines.size())
            finalLines.insert(finalLines.end(), lines.begin() + cEnd + 1, lines.end());

        std::string result;
        for (auto &ln : finalLines) result += ln + "\n";
        return result;
    }
}
