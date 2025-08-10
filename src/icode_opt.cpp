#include "mxvm/icode.hpp"
#include <string>
#include <vector>
#include <sstream>
#include <regex>
#include <cctype>
#include <unordered_set>

namespace mxvm {

    static std::string join_lines(const std::vector<std::string>& v) {
        std::ostringstream os;
        for (size_t i = 0; i < v.size(); ++i) {
            os << v[i];
            if (i + 1 < v.size()) os << '\n';
        }
        return os.str();
    }

    static inline std::string trim(std::string s) {
        auto issp = [](int c){ return std::isspace(c); };
        while (!s.empty() && issp(s.front())) s.erase(s.begin());
        while (!s.empty() && issp(s.back())) s.pop_back();
        return s;
    }

    static bool is_label(const std::string& line) {
        static const std::regex re_label(R"(^\s*(?:[A-Za-z_.$][\w.$]*|\d+)\s*:)");
        return std::regex_search(line, re_label);
    }

    static std::vector<std::string> opt_core_lines(const std::vector<std::string>& lines) {
        std::regex re_mov_imm(R"(^\s*mov[a-z]*\s+\$(-?\d+)\s*,\s*(%[a-z0-9]+)\s*(?:[#;].*)?$)", std::regex::icase);
        std::regex re_bin_op(R"(^\s*((?:add|sub|and|or|xor|cmp)[a-z]*)\s+(\%[a-z0-9]+|\$-?[0-9]+)\s*,\s*(\%[a-z0-9]+)\s*(?:([#;].*))?$)", std::regex::icase);
        std::regex re_mov_reg(R"(^\s*mov[a-z]*\s+(%[a-z0-9]+)\s*,\s*(%[a-z0-9]+)\s*(?:[#;].*)?$)", std::regex::icase);
        std::regex re_add_one(R"(^\s*add[a-z]*\s+\$1\s*,\s*(%[a-z0-9]+)\s*(?:[#;].*)?$)", std::regex::icase);
        std::regex re_sub_one(R"(^\s*sub[a-z]*\s+\$1\s*,\s*(%[a-z0-9]+)\s*(?:[#;].*)?$)", std::regex::icase);
        std::regex re_mul_pow2(R"(^\s*(?:imul|mul)[a-z]*\s+\$(\d+)\s*,\s*(%[a-z0-9]+)(?:\s*,\s*(%[a-z0-9]+))?\s*(?:[#;].*)?$)", std::regex::icase);
        std::regex re_cmp_zero(R"(^\s*cmp[a-z]*\s+\$0\s*,\s*(%[a-z0-9]+)\s*(?:[#;].*)?$)", std::regex::icase);
        std::regex re_mem_load_store(R"(^\s*mov[a-z]*\s+([^%][^,]+)\s*,\s*(%[a-z0-9]+)\s*(?:[#;].*)?$)", std::regex::icase);
        std::regex re_mem_store(R"(^\s*mov[a-z]*\s+(%[a-z0-9]+)\s*,\s*([^%][^,]+)\s*(?:[#;].*)?$)", std::regex::icase);
        std::regex re_mov_rax_to_mem(R"(^\s*mov[a-z]*\s+%rax\s*,\s*([^%][^,]+)\s*(?:[#;].*)?$)", std::regex::icase);
        std::regex re_mov_mem_to_rax(R"(^\s*mov[a-z]*\s+([^%][^,]+)\s*,\s*%rax\s*(?:[#;].*)?$)", std::regex::icase);

        std::vector<std::string> out;
        out.reserve(lines.size());

        for (size_t i = 0; i < lines.size(); ++i) {
            if (is_label(lines[i])) {
                out.push_back(lines[i]);
                continue;
            }

            std::smatch m;

            if (i + 1 < lines.size() && std::regex_match(lines[i], m, re_mem_store)) {
                const std::string store_reg = trim(m[1].str());
                const std::string store_mem = trim(m[2].str());
                std::smatch m2;
                if (std::regex_match(lines[i + 1], m2, re_mem_load_store)) {
                    const std::string load_mem = trim(m2[1].str());
                    const std::string load_reg = trim(m2[2].str());
                    if (store_reg == load_reg && store_mem == load_mem) {
                        out.push_back(lines[i]); 
                        ++i;                     
                        continue;
                    }
                }
            }

            if (std::regex_match(lines[i], m, re_cmp_zero)) {
                const std::string reg = m[1].str();
                out.push_back("\ttest " + reg + ", " + reg);
                continue;
            }

            if (std::regex_match(lines[i], m, re_mov_mem_to_rax)) {
                const std::string mem_loc = m[1].str();
                if (i + 1 < lines.size()) {
                    std::smatch m2;
                    if (std::regex_match(lines[i + 1], m2, re_mov_rax_to_mem)) {
                        const std::string store_mem = m2[1].str();
                        out.push_back("\tpushq %rax");
                        out.push_back("\tmovq " + mem_loc + ", %rax");
                        out.push_back("\tmovq %rax, " + store_mem);
                        out.push_back("\tpopq %rax");
                        i++;
                        continue;
                    }
                }
            }

            if (std::regex_match(lines[i], m, re_mul_pow2)) {
                const int imm = std::stoi(m[1].str());
                const std::string src_reg = m[2].str();
                const std::string dst_reg = m.size() > 3 && !m[3].str().empty() ? m[3].str() : src_reg;
                if (imm == 3 || imm == 5 || imm == 9) {
                    if (src_reg != dst_reg) out.push_back("\tmov " + src_reg + ", " + dst_reg);
                    if (imm == 3) {
                        out.push_back("\tlea (" + dst_reg + "," + dst_reg + ",2), " + dst_reg);
                    } else if (imm == 5) {
                        out.push_back("\tlea (" + dst_reg + "," + dst_reg + ",4), " + dst_reg);
                    } else {
                        out.push_back("\tlea (" + dst_reg + "," + dst_reg + ",8), " + dst_reg);
                    }
                    continue;
                }
            }

            if (std::regex_match(lines[i], m, re_mem_load_store)) {
                const std::string mem_loc = m[1].str();
                const std::string reg = m[2].str();
                if (i + 1 < lines.size()) {
                    std::smatch m2;
                    if (std::regex_match(lines[i + 1], m2, re_mem_store)) {
                        const std::string store_reg = m2[1].str();
                        const std::string store_mem = m2[2].str();
                        if (reg == store_reg && trim(mem_loc) == trim(store_mem)) {
                            ++i; 
                            out.push_back(lines[i]); 
                            continue;
                        }
                    }
                }
            }

            if (std::regex_match(lines[i], m, re_add_one)) {
                const std::string reg = m[1].str();
                out.push_back("\tinc " + reg);
                continue;
            }
            if (std::regex_match(lines[i], m, re_sub_one)) {
                const std::string reg = m[1].str();
                out.push_back("\tdec " + reg);
                continue;
            }

            if (std::regex_match(lines[i], m, re_mov_imm)) {
                const std::string imm = m[1].str();
                const std::string reg = m[2].str();
                if (imm == "0") {
                    out.push_back("\txor " + reg + ", " + reg);
                    continue;
                }
                if (i + 1 < lines.size()) {
                    std::smatch m2;
                    if (std::regex_match(lines[i + 1], m2, re_bin_op)) {
                        const std::string op_full = m2[1].str();
                        const std::string src = m2[2].str();
                        const std::string dst = m2[3].str();
                        const std::string comment = m2.size() >= 5 ? m2[4].str() : "";
                        if (src == reg && (
                            op_full.rfind("cmp",0)==0 || op_full.rfind("add",0)==0 || op_full.rfind("sub",0)==0 ||
                            op_full.rfind("and",0)==0 || op_full.rfind("or",0)==0  || op_full.rfind("xor",0)==0)) {
                            out.push_back("\t" + op_full + " $" + imm + ", " + dst + comment);
                            ++i;
                            continue;
                        }
                    }
                }
            }

            if (std::regex_match(lines[i], m, re_mov_reg)) {
                const std::string src_reg = m[1].str();
                const std::string dst_reg = m[2].str();
                if (i + 1 < lines.size()) {
                    std::smatch m2;
                    if (std::regex_match(lines[i + 1], m2, re_bin_op)) {
                        const std::string op_full = m2[1].str();
                        const std::string op_src = m2[2].str();
                        const std::string op_dst = m2[3].str();
                        const std::string comment = m2.size() >= 5 ? m2[4].str() : "";
                        if (op_src == dst_reg) {
                            out.push_back("\t" + op_full + " " + src_reg + ", " + op_dst + comment);
                            ++i;
                            continue;
                        }
                    }
                }
            }

            out.push_back(lines[i]);
        }

        return out;
    }


    static std::string darwin_prefix_calls(const std::string& line,
                                           const std::unordered_set<std::string>& macos_functions) {
        static const std::regex re_call(R"(\b(call|jmp)\s+([_A-Za-z][_A-Za-z0-9]*)\b)");
        std::string result;
        result.reserve(line.size() + 16);

        size_t last = 0;
        for (std::sregex_iterator it(line.begin(), line.end(), re_call), end; it != end; ++it) {
            const auto& m = *it;
            const auto pos = static_cast<size_t>(m.position());
            const auto len = static_cast<size_t>(m.length());
            const std::string fn = m[2].str();

            result.append(line, last, pos - last);

            std::string piece = m.str(); 
            if (fn == "main" || macos_functions.count(fn)) {
                const auto fnpos = piece.rfind(fn);
                if (fnpos != std::string::npos) piece.replace(fnpos, fn.size(), "_" + fn);
            }
            result += piece;

            last = pos + len;
        }
        result.append(line, last, std::string::npos);
        return result;
    }

    static std::vector<std::string> opt_darwin_lines(const std::vector<std::string>& lines) {
        std::vector<std::string> out;
        out.reserve(lines.size());

        std::regex re_stdin_access(R"(movq?\s+stdin\(%rip\)\s*,\s*(%\w+))", std::regex::icase);
        std::regex re_stdout_access(R"(movq?\s+stdout\(%rip\)\s*,\s*(%\w+))", std::regex::icase);
        std::regex re_stderr_access(R"(movq?\s+stderr\(%rip\)\s*,\s*(%\w+))", std::regex::icase);
        std::regex re_global_main(R"(^\s*\.(?:global|globl)\s+main\s*$)");
        std::regex re_main_label(R"(^\s*main\s*:\s*$)");
        std::regex re_global_function(R"(^\s*\.(?:global|globl)\s+([_a-zA-Z][_a-zA-Z0-9]*)(?:\s|$))");
        std::regex re_function_label(R"(^([_a-zA-Z][_a-zA-Z0-9]*)\s*:\s*$)");

        std::unordered_set<std::string> macos_functions;

        for (const auto& raw : lines) {
            std::string line = raw;

            if (std::regex_search(line, re_global_main)) {
                out.push_back("\t.globl _main");
                continue;
            }
            if (std::regex_search(line, re_main_label)) {
                out.push_back("_main:");
                continue;
            }

            std::smatch m_func;
            if (std::regex_search(line, m_func, re_global_function)) {
                std::string fn = m_func[1].str();
                if (fn != "main") {
                    macos_functions.insert(fn);
                    out.push_back("\t.globl _" + fn);
                }
                continue;
            }
            if (std::regex_search(line, m_func, re_function_label)) {
                std::string fn = m_func[1].str();
                if (fn != "main" && macos_functions.count(fn)) {
                    out.push_back("_" + fn + ":");
                } else {
                    out.push_back(line);
                }
                continue;
            }

            std::smatch m;
            if (std::regex_search(line, m, re_stdin_access)) {
                std::string r = m[1].str();
                out.push_back("\tmovq ___stdinp@GOTPCREL(%rip), %rax");
                out.push_back("\tmovq (%rax), " + r);
                continue;
            }
            if (std::regex_search(line, m, re_stdout_access)) {
                std::string r = m[1].str();
                out.push_back("\tmovq ___stdoutp@GOTPCREL(%rip), %rax");
                out.push_back("\tmovq (%rax), " + r);
                continue;
            }
            if (std::regex_search(line, m, re_stderr_access)) {
                std::string r = m[1].str();
                out.push_back("\tmovq ___stderrp@GOTPCREL(%rip), %rax");
                out.push_back("\tmovq (%rax), " + r);
                continue;
            }
    
            // Also catch references using standard variable names with single underscore
            std::regex re_single_stdout(R"(_stdout\(%rip\))", std::regex::icase);
            if (std::regex_search(line, m, re_single_stdout)) {
                std::string r = m[1].str();
                out.push_back("\tmovq ___stdoutp@GOTPCREL(%rip), %rax");
                out.push_back("\tmovq (%rax), " + r);
                continue;
            }

            line = darwin_prefix_calls(line, macos_functions);
            out.push_back(line);
        }

        return out;
    }


    static std::vector<std::string> opt_linux_lines(const std::vector<std::string>& lines) {
        return lines;
    }

    std::string Program::gen_optimize(const std::string &code, const Platform &platform) {
        std::vector<std::string> lines;
        {
            std::istringstream stream(code);
            std::string s;
            while (std::getline(stream, s)) lines.push_back(s);
        }
        if (lines.empty()) return "";

        auto core = opt_core_lines(lines);

        std::vector<std::string> final_lines =
            (platform == Platform::DARWIN) ? opt_darwin_lines(core)
                                           : opt_linux_lines(core);

        return join_lines(final_lines);
    }
}
