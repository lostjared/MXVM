#include "icode.hpp"
#include <regex>
#include <unordered_set>
#include <algorithm>

namespace pascal {
    std::string mxvmOpt(const std::string &text) {
        std::vector<std::string> lines;
        std::istringstream stream(text);
        std::string s;
        while(std::getline(stream, s)) {
            lines.push_back(s);
        }
        
        size_t dataStart = 0, dataEnd = 0, codeStart = 0;
        for (size_t i = 0; i < lines.size(); i++) {
            if (lines[i].find("section data {") != std::string::npos) {
                dataStart = i + 1;
            } else if (lines[i].find("section code {") != std::string::npos) {
                dataEnd = i - 1;
                codeStart = i + 1;
            }
        }
        
        if (dataStart == 0 || dataEnd == 0 || codeStart == 0) {
            return text; 
        }
        
        std::string codeSection;
        for (size_t i = codeStart; i < lines.size(); i++) {
            if (lines[i].find("}") != std::string::npos && 
                (i == lines.size() - 1 || lines[i+1].find("}") != std::string::npos)) {
                break;
            }
            codeSection += lines[i] + "\n";
        }
        
        std::unordered_set<std::string> usedVars;
        std::vector<size_t> linesToRemove;
        
        std::vector<std::string> systemVars = {
            "rax", "fmt_int", "fmt_str", "fmt_chr", "newline"
        };
        
        for (auto& sysVar : systemVars) {
            usedVars.insert(sysVar);
        }
        
        std::regex varUsagePattern("\\b([a-zA-Z_][a-zA-Z0-9_]*)\\b");
        auto words_begin = std::sregex_iterator(
            codeSection.begin(), codeSection.end(), varUsagePattern);
        auto words_end = std::sregex_iterator();
        
        for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
            std::smatch match = *i;
            usedVars.insert(match.str(1));
        }
        
        
        std::regex varDeclPattern("^\\s*(export)?\\s*(int|string|ptr|float|double)\\s+([a-zA-Z_][a-zA-Z0-9_]*)\\s*=");
        
        for (size_t i = dataStart; i <= dataEnd; i++) {
            std::smatch match;
            if (std::regex_search(lines[i], match, varDeclPattern)) {
                bool hasExport = match[1].length() > 0;
                std::string varName = match[3];
                if (!hasExport && usedVars.find(varName) == usedVars.end()) {
                    linesToRemove.push_back(i);
                }
            }
        }
        
        std::sort(linesToRemove.begin(), linesToRemove.end(), std::greater<size_t>());
        for (auto lineIdx : linesToRemove) {
            lines.erase(lines.begin() + lineIdx);
        }
        
        std::string result;
        for (const auto& line : lines) {
            result += line + "\n";
        }
        
        return result;
    }
}