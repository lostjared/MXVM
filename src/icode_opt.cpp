#include "mxvm/icode.hpp"
#include <string>
#include <vector>
#include <sstream>
#include <regex>
#include <cctype>

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

    std::string Program::gen_optimize(const std::string &code) {
        std::vector<std::string> lines;
        std::istringstream stream(code);
        std::string s;
        while (std::getline(stream, s)) lines.push_back(s);
        if (lines.empty()) return "";

        
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
        std::regex re_store_reg(R"(^\s*mov[a-z]*\s+(%[a-z0-9]+)\s*,\s*([^#;]+?)\s*(?:[#;].*)?$)", std::regex::icase);
        std::regex re_load_same_reg(R"(^\s*mov[a-z]*\s+([^#;]+?)\s*,\s*(%[a-z0-9]+)\s*(?:[#;].*)?$)", std::regex::icase);

        std::vector<std::string> out;
        out.reserve(lines.size());

        for (size_t i = 0; i < lines.size(); ++i) {
            if (is_label(lines[i])) {
                out.push_back(lines[i]);
                continue;
            }

            std::smatch m;

            
            if (i + 1 < lines.size() && std::regex_match(lines[i], m, re_store_reg)) {
                const std::string reg1 = trim(m[1].str());
                const std::string mem1 = trim(m[2].str());

                std::smatch m2;
                if (std::regex_match(lines[i + 1], m2, re_load_same_reg)) {
                    const std::string mem2 = trim(m2[1].str());
                    const std::string reg2 = trim(m2[2].str());

                    if (reg1 == reg2 && mem1 == mem2) {
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
                    if (src_reg != dst_reg) {
                        out.push_back("\tmov " + src_reg + ", " + dst_reg);
                    }
                    
                    if (imm == 3) {
                        out.push_back("\tlea (" + dst_reg + "," + dst_reg + ",2), " + dst_reg);
                    } else if (imm == 5) {
                        out.push_back("\tlea (" + dst_reg + "," + dst_reg + ",4), " + dst_reg);
                    } else if (imm == 9) {
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
                                op_full.starts_with("cmp") ||
                                op_full.starts_with("add") ||
                                op_full.starts_with("sub") ||
                                op_full.starts_with("and") ||
                                op_full.starts_with("or") ||
                                op_full.starts_with("xor"))) {
                            
                            const std::string rewritten = "\t" + op_full + " $" + imm + ", " + dst + comment;
                            out.push_back(rewritten);
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
                            const std::string rewritten = "\t" + op_full + " " + src_reg + ", " + op_dst + comment;
                            out.push_back(rewritten);
                            ++i; 
                            continue;
                        }
                    }
                }
            }
            out.push_back(lines[i]);
        }

        return join_lines(out);
    }
}