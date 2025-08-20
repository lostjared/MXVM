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
        std::regex re_mem_to_reg(R"(^\s*mov[a-z]*\s+([a-zA-Z0-9_]+(?:\(%rip\))?)\s*,\s*(%[a-z0-9]+)\s*(?:[#;].*)?$)", std::regex::icase);
        std::regex re_reg_to_mem(R"(^\s*mov[a-z]*\s+(%[a-z0-9]+)\s*,\s*([a-zA-Z0-9_]+(?:\(%rip\))?)\s*(?:[#;].*)?$)", std::regex::icase);
        std::regex re_reg_to_reg(R"(^\s*mov[a-z]*\s+(%[a-z0-9]+)\s*,\s*(%[a-z0-9]+)\s*(?:[#;].*)?$)", std::regex::icase);
        std::regex re_modifies_reg(R"(^\s*(?:add|sub|mul|imul|div|idiv|and|or|xor|shl|shr|sal|sar|not|neg)[a-z]*\s+.*,\s*(%[a-z0-9]+)\s*(?:[#;].*)?$)", std::regex::icase);
        std::regex re_call_instr(R"(^\s*(?:call|jmp|je|jne|jz|jnz|jg|jge|jl|jle|ja|jae|jb|jbe)[a-z]*\s+)", std::regex::icase);

        std::vector<std::string> out;
        out.reserve(lines.size());

        struct ValueInfo {
            std::string location;  
            bool valid = true;     
        };
        std::unordered_map<std::string, ValueInfo> reg_contents;  
        std::unordered_map<std::string, ValueInfo> mem_contents;  

        
        auto invalidate_tracking = [&]() {
            reg_contents.clear();
            mem_contents.clear();
        };

        for (size_t i = 0; i < lines.size(); ++i) {
            const std::string& line = lines[i];
            std::smatch m;

        
            if (is_label(line) || std::regex_search(line, re_call_instr)) {
                out.push_back(line);
                invalidate_tracking();
                continue;
            }

        
            std::smatch m_mod;
            if (std::regex_match(line, m_mod, re_modifies_reg)) {
                const std::string reg = m_mod[1].str();
                reg_contents.erase(reg);  
                out.push_back(line);
                continue;
            }

            
            std::smatch m_load;
            if (std::regex_match(line, m_load, re_mem_to_reg)) {
                const std::string mem = m_load[1].str();
                const std::string reg = m_load[2].str();
                
            
                auto it_mem = mem_contents.find(mem);
                if (it_mem != mem_contents.end() && it_mem->second.valid) {
                    const std::string& source_reg = it_mem->second.location;
                    
            
                    if (source_reg != reg) {
                        out.push_back("\tmovq " + source_reg + ", " + reg);
                        reg_contents[reg] = {mem, true};
                    }
            
                    continue;
                }
                
            
                out.push_back(line);
                reg_contents[reg] = {mem, true};
                continue;
            }

            
            std::smatch m_store;
            if (std::regex_match(line, m_store, re_reg_to_mem)) {
                const std::string reg = m_store[1].str();
                const std::string mem = m_store[2].str();
                
            
                mem_contents[mem] = {reg, true};
                
            
                out.push_back(line);
                
            
                if (i + 1 < lines.size()) {
                    std::smatch m_next;
                    if (std::regex_match(lines[i+1], m_next, re_mem_to_reg)) {
                        const std::string next_mem = m_next[1].str();
                        const std::string next_reg = m_next[2].str();
                        
                        if (next_mem == mem) {
                            if (next_reg != reg) {
                                out.push_back("\tmovq " + reg + ", " + next_reg);
                                reg_contents[next_reg] = {mem, true};
                            }
                            i++;  
                            continue;
                        }
                    }
                }
                continue;
            }

            
            std::smatch m_reg_move;
            if (std::regex_match(line, m_reg_move, re_reg_to_reg)) {
                const std::string src_reg = m_reg_move[1].str();
                const std::string dst_reg = m_reg_move[2].str();
                
            
                if (src_reg == dst_reg) {
                    continue;
                }
                
            
                auto it_src = reg_contents.find(src_reg);
                if (it_src != reg_contents.end()) {
                    reg_contents[dst_reg] = it_src->second;
                } else {
                    reg_contents[dst_reg] = {src_reg, true};
                }
                
                out.push_back(line);
                continue;
            }

            
            if (std::regex_match(line, m, re_cmp_zero)) {
                out.push_back("\ttest " + m[1].str() + ", " + m[1].str());
                invalidate_tracking();
                continue;
            }
            
            if (std::regex_match(line, m, re_add_one)) {
                out.push_back("\tinc " + m[1].str());
                invalidate_tracking();
                continue;
            }
            
            if (std::regex_match(line, m, re_sub_one)) {
                out.push_back("\tdec " + m[1].str());
                invalidate_tracking();
                continue;
            }
            
            if (std::regex_match(line, m, re_mov_imm) && m[1].str() == "0") {
                out.push_back("\txor " + m[2].str() + ", " + m[2].str());
                invalidate_tracking();
                continue;
            }
            
            
            if (i + 2 < lines.size()) {
                std::smatch m1, m2, m3;
                std::regex re_store_load_store(
                    R"(^\s*movq?\s+(%[a-z0-9]+)\s*,\s*([a-zA-Z0-9_]+(?:\(%rip\))?)\s*(?:[#;].*)?$)",
                    std::regex::icase);
                    
                if (std::regex_match(line, m1, re_store_load_store)) {
                    const std::string reg1 = m1[1].str();
                    const std::string mem1 = m1[2].str();
                    
            
                    std::string escaped_mem1 = std::regex_replace(mem1, std::regex(R"([\$\(\)])"), "\\$&");
                    std::regex load_pattern(R"(^\s*movq?\s+)" + escaped_mem1 + 
                                           R"(\s*,\s*(%[a-z0-9]+)\s*(?:[#;].*)?$)", std::regex::icase);
                    
                    if (std::regex_match(lines[i+1], m2, load_pattern)) {
                        const std::string reg2 = m2[1].str();
                        
            
                        std::regex store_pattern(R"(^\s*movq?\s+)" + reg2 + 
                                                R"(\s*,\s*([a-zA-Z0-9_]+(?:\(%rip\))?)\s*(?:[#;].*)?$)", std::regex::icase);
                        
                        if (std::regex_match(lines[i+2], m3, store_pattern)) {
                            const std::string mem2 = m3[1].str();
                            
            
                            out.push_back(lines[i]);  
                            
                            
                            std::string optimized_store = "\tmovq " + reg1 + ", " + mem2;
                            out.push_back(optimized_store);
                            
                            i += 2;  
                            continue;
                        }
                    }
                }
            }
            
            
            if (i + 1 < lines.size()) {
                std::smatch m1, m2;
                std::regex re_store_reload(
                    R"(^\s*movq?\s+(%[a-z0-9]+)\s*,\s*([a-zA-Z0-9_]+(?:\(%rip\))?)\s*(?:[#;].*)?$)",
                    std::regex::icase);
                    
                if (std::regex_match(line, m1, re_store_reload)) {
                    const std::string reg = m1[1].str();
                    const std::string mem = m1[2].str();
                    
            
                    std::string escaped_mem = std::regex_replace(mem, std::regex(R"([\$\(\)])"), "\\$&");
                    std::regex reload_pattern(R"(^\s*movq?\s+)" + escaped_mem + 
                                             R"(\s*,\s*)" + reg + R"(\s*(?:[#;].*)?$)", std::regex::icase);
                    
                    if (std::regex_match(lines[i+1], reload_pattern)) {
                        out.push_back(lines[i]); 
                        i += 1;  
                        continue;
                    }
                }
            }
            invalidate_tracking();
            out.push_back(line);
        }
        return out;
    }


    static std::vector<std::string> x64_opt_core_lines(const std::vector<std::string>& lines) {
        std::regex re_mov_imm(R"(^\s*mov[a-z]*\s+\$(-?\d+)\s*,\s*(%[a-z0-9]+)\s*(?:[#;].*)?$)", std::regex::icase);
        std::regex re_bin_op(R"(^\s*((?:add|sub|and|or|xor|cmp)[a-z]*)\s+(\%[a-z0-9]+|\$-?[0-9]+)\s*,\s*(\%[a-z0-9]+)\s*(?:([#;].*))?$)", std::regex::icase);
        std::regex re_mov_reg(R"(^\s*mov[a-z]*\s+(%[a-z0-9]+)\s*,\s*(%[a-z0-9]+)\s*(?:[#;].*)?$)", std::regex::icase);
        std::regex re_add_one(R"(^\s*add[a-z]*\s+\$1\s*,\s*(%[a-z0-9]+)\s*(?:[#;].*)?$)", std::regex::icase);
        std::regex re_sub_one(R"(^\s*sub[a-z]*\s+\$1\s*,\s*(%[a-z0-9]+)\s*(?:[#;].*)?$)", std::regex::icase);
        std::regex re_mul_pow2(R"(^\s*(?:imul|mul)[a-z]*\s+\$(\d+)\s*,\s*(%[a-z0-9]+)(?:\s*,\s*(%[a-z0-9]+))?\s*(?:[#;].*)?$)", std::regex::icase);
        std::regex re_cmp_zero(R"(^\s*cmp[a-z]*\s+\$0\s*,\s*(%[a-z0-9]+)\s*(?:[#;].*)?$)", std::regex::icase);
        std::regex re_mem_load_store(R"(^\s*mov[a-z]*\s+([^%][^,]+)\s*,\s*(%[a-z0-9]+)\s*(?:[#;].*)?$)", std::regex::icase);
        std::regex re_mem_store(R"(^\s*mov[a-z]*\s+(%[a-z0-9]+)\s*,\s*([^%][^,]+)\s*(?:[#;].*)?$)", std::regex::icase);

        std::regex re_sub_rsp(R"(^\s*sub\s+\$([0-9]+)\s*,\s*%rsp\s*(?:[#;].*)?$)", std::regex::icase);
        std::regex re_add_rsp(R"(^\s*add\s+\$([0-9]+)\s*,\s*%rsp\s*(?:[#;].*)?$)", std::regex::icase);

        auto is_label = [](const std::string& line) {
            static const std::regex re(R"(^\s*(?:[A-Za-z_.$][\w.$]*|\d+)\s*:)"); 
            return std::regex_search(line, re);
        };
        auto has_call_or_ret = [](const std::string& s){
            return s.find(" call ") != std::string::npos ||
                s.find("\tcall ") != std::string::npos ||
                s.find(" ret") != std::string::npos ||
                s.find("\tret") != std::string::npos;
        };
        auto touches_rsp = [](const std::string& s){
            return s.find("%rsp") != std::string::npos || s.find("(%rsp") != std::string::npos;
        };

        std::vector<std::string> out;
        out.reserve(lines.size());
        std::vector<int> frame_bytes;

        struct ValueInfo { std::string location; bool valid = true; };
        std::unordered_map<std::string, ValueInfo> reg_contents;
        std::unordered_map<std::string, ValueInfo> mem_contents;
        auto invalidate_tracking = [&]() {
            reg_contents.clear();
            mem_contents.clear();
        };

        std::regex re_mem_to_reg(R"(^\s*movq?\s+([a-zA-Z0-9_]+(?:\(%rip\))?)\s*,\s*(%[a-z0-9]+)\s*(?:[#;].*)?$)", std::regex::icase);
        std::regex re_reg_to_mem(R"(^\s*movq?\s+(%[a-z0-9]+)\s*,\s*([a-zA-Z0-9_]+(?:\(%rip\))?)\s*(?:[#;].*)?$)", std::regex::icase);
        std::regex re_reg_to_reg(R"(^\s*movq?\s+(%[a-z0-9]+)\s*,\s*(%[a-z0-9]+)\s*(?:[#;].*)?$)", std::regex::icase);
        std::regex re_modifies_reg(R"(^\s*(?:add|sub|mul|imul|div|idiv|and|or|xor|shl|shr|sal|sar|not|neg)[a-z]*\s+.*,\s*(%[a-z0-9]+)\s*(?:[#;].*)?$)", std::regex::icase);
        std::regex re_call_instr(R"(^\s*(?:call|jmp|je|jne|jz|jnz|jg|jge|jl|jle|ja|jae|jb|jbe)[a-z]*\s+)", std::regex::icase);

        for (size_t i = 0; i < lines.size(); ++i) {
            const std::string line = lines[i];

            if (is_label(line)) {
                out.push_back(line);
                invalidate_tracking();
                continue;
            }

            {
                std::smatch m;
                if (std::regex_match(line, m, re_sub_rsp)) {
                    frame_bytes.push_back(std::stoi(m[1].str()));
                    out.push_back(line);
                    continue;
                }
            }

            if (!frame_bytes.empty()) {
                out.push_back(line);
                std::smatch m;
                if (std::regex_match(line, m, re_add_rsp)) {
                    int n = std::stoi(m[1].str());
                    if (!frame_bytes.empty() && frame_bytes.back() == n) frame_bytes.pop_back();
                }
                continue;
            }

            if (has_call_or_ret(line) || touches_rsp(line) || std::regex_search(line, re_call_instr)) {
                out.push_back(line);
                invalidate_tracking();
                continue;
            }

            {
                std::smatch m_mod;
                if (std::regex_match(line, m_mod, re_modifies_reg)) {
                    const std::string reg = m_mod[1].str();
                    reg_contents.erase(reg);
                    out.push_back(line);
                    continue;
                }
            }

            /*
            {
                std::smatch m_load;
                if (std::regex_match(line, m_load, re_mem_to_reg)) {
                    const std::string mem = m_load[1].str();
                    const std::string reg = m_load[2].str();

                    auto it_mem = mem_contents.find(mem);
                    if (it_mem != mem_contents.end() && it_mem->second.valid) {
                        const std::string& source_reg = it_mem->second.location;
                        if (source_reg != reg) {
                            out.push_back("\tmovq " + source_reg + ", " + reg);
                            reg_contents[reg] = {mem, true};
                        }
                        continue;
                    }

                    out.push_back(line);
                    reg_contents[reg] = {mem, true};
                    continue;
                }
            } */

            {
                std::smatch m_store;
                if (std::regex_match(line, m_store, re_reg_to_mem)) {
                    const std::string reg = m_store[1].str();
                    const std::string mem = m_store[2].str();

                    mem_contents[mem] = {reg, true};
                    out.push_back(line);

                    if (i + 1 < lines.size()) {
                        std::smatch m_next;
                        if (std::regex_match(lines[i+1], m_next, re_mem_to_reg)) {
                            const std::string next_mem = m_next[1].str();
                            const std::string next_reg = m_next[2].str();
                            if (next_mem == mem) {
                                if (next_reg != reg) {
                                    out.push_back("\tmovq " + reg + ", " + next_reg);
                                    reg_contents[next_reg] = {mem, true};
                                }
                                i++;
                                continue;
                            }
                        }
                    }
                    continue;
                }
            }

            {
                std::smatch m_reg_move;
                if (std::regex_match(line, m_reg_move, re_reg_to_reg)) {
                    const std::string src_reg = m_reg_move[1].str();
                    const std::string dst_reg = m_reg_move[2].str();
                    if (src_reg == dst_reg) continue;

                    auto it_src = reg_contents.find(src_reg);
                    if (it_src != reg_contents.end()) reg_contents[dst_reg] = it_src->second;
                    else reg_contents[dst_reg] = {src_reg, true};

                    out.push_back(line);
                    continue;
                }
            }

            {
                std::smatch m;
                if (std::regex_match(line, m, re_cmp_zero)) {
                    out.push_back("\ttest " + m[1].str() + ", " + m[1].str());
                    continue;
                }
            }

            {
                std::smatch m;
                if (std::regex_match(line, m, re_add_one)) { out.push_back("\tinc " + m[1].str()); continue; }
                if (std::regex_match(line, m, re_sub_one)) { out.push_back("\tdec " + m[1].str()); continue; }
            }

            {
                std::smatch m;
                if (std::regex_match(line, m, re_mul_pow2)) {
                    int imm = std::stoi(m[1].str());
                    const std::string src = m[2].str();
                    const std::string dst = (m.size() > 3 && !m[3].str().empty()) ? m[3].str() : src;
                    if (imm == 3 || imm == 5 || imm == 9) {
                        if (src != dst) out.push_back("\tmov " + src + ", " + dst);
                        if (imm == 3)      out.push_back("\tlea (" + dst + "," + dst + ",2), " + dst);
                        else if (imm == 5) out.push_back("\tlea (" + dst + "," + dst + ",4), " + dst);
                        else               out.push_back("\tlea (" + dst + "," + dst + ",8), " + dst);
                        continue;
                    }
                }
            }

            {
                std::smatch m;
                if (std::regex_match(line, m, re_mov_imm)) {
                    const std::string imm = m[1].str();
                    const std::string reg = m[2].str();
                    if (imm == "0") { out.push_back("\txor " + reg + ", " + reg); continue; }

                    if (i + 1 < lines.size()) {
                        std::smatch m2;
                        if (std::regex_match(lines[i + 1], m2, re_bin_op)) {
                            const std::string op_full = m2[1].str();
                            const std::string src     = m2[2].str();
                            const std::string dst     = m2[3].str();
                            const std::string cmt     = (m2.size() >= 5) ? m2[4].str() : "";
                            if (src == reg &&
                                (op_full.rfind("cmp",0)==0 || op_full.rfind("add",0)==0 || op_full.rfind("sub",0)==0 ||
                                op_full.rfind("and",0)==0 || op_full.rfind("or",0)==0  || op_full.rfind("xor",0)==0)) {
                                out.push_back("\t" + op_full + " $" + imm + ", " + dst + cmt);
                                ++i;
                                continue;
                            }
                        }
                    }
                }
            }

            {
                std::smatch m;
                if (std::regex_match(line, m, re_mov_reg)) {
                    const std::string src_reg = m[1].str();
                    const std::string dst_reg = m[2].str();
                    if (i + 1 < lines.size()) {
                        std::smatch m2;
                        if (std::regex_match(lines[i + 1], m2, re_bin_op)) {
                            const std::string op_full = m2[1].str();
                            const std::string op_src  = m2[2].str();
                            const std::string op_dst  = m2[3].str();
                            const std::string cmt     = (m2.size() >= 5) ? m2[4].str() : "";
                            if (op_src == dst_reg) {
                                out.push_back("\t" + op_full + " " + src_reg + ", " + op_dst + cmt);
                                ++i;
                                continue;
                            }
                        }
                    }
                }
            }

            std::regex re_store_load_store(
                R"(^\s*movq?\s+(%[a-z0-9]+)\s*,\s*([a-zA-Z0-9_]+(?:\(%rip\))?)\s*(?:[#;].*)?$)",
                std::regex::icase);

            if (i + 2 < lines.size()) {
                std::smatch m1, m2, m3;
                if (std::regex_match(lines[i], m1, re_store_load_store)) {
                    const std::string reg1 = m1[1].str();
                    const std::string mem1 = m1[2].str();

                    std::string escaped_mem1 = std::regex_replace(mem1, std::regex(R"([\$\(\)])"), "\\$&");
                    std::regex load_pattern(R"(^\s*movq?\s+)" + escaped_mem1 +
                                            R"(\s*,\s*(%[a-z0-9]+)\s*(?:[#;].*)?$)", std::regex::icase);

                    if (std::regex_match(lines[i+1], m2, load_pattern)) {
                        const std::string reg2 = m2[1].str();

                        std::regex store_pattern(R"(^\s*movq?\s+)" + reg2 +
                                                R"(\s*,\s*([a-zA-Z0-9_]+(?:\(%rip\))?)\s*(?:[#;].*)?$)", std::regex::icase);

                        if (std::regex_match(lines[i+2], m3, store_pattern)) {
                            const std::string mem2 = m3[1].str();
                            out.push_back(lines[i]);
                            std::string optimized_store = "\tmovq " + reg1 + ", " + mem2;
                            out.push_back(optimized_store);
                            i += 2;
                            continue;
                        }
                    }
                }
            }

            {
                std::unordered_map<std::string, std::string> reg_values;

                std::regex re_store_load(
                    R"(^\s*movq?\s+(%[a-z0-9]+)\s*,\s*([a-zA-Z0-9_]+(?:\(%rip\))?)\s*(?:[#;].*)?$)",
                    std::regex::icase);

                std::smatch store_match;
                if (std::regex_match(lines[i], store_match, re_store_load)) {
                    const std::string reg = store_match[1].str();
                    const std::string mem = store_match[2].str();
                    reg_values[mem] = reg;
                    out.push_back(lines[i]);
                    continue;
                }

                std::regex re_load(
                    R"(^\s*movq?\s+([a-zA-Z0-9_]+(?:\(%rip\))?)\s*,\s*(%[a-z0-9]+)\s*(?:[#;].*)?$)",
                    std::regex::icase);

                std::smatch load_match;
                if (std::regex_match(lines[i], load_match, re_load)) {
                    const std::string mem = load_match[1].str();
                    const std::string reg = load_match[2].str();

                    auto it = reg_values.find(mem);
                    if (it != reg_values.end()) {
                        if (it->second != reg) {
                            out.push_back("\tmovq " + it->second + ", " + reg);
                        }
                        continue;
                    }

                    out.push_back(lines[i]);
                    continue;
                }
            }

            {
                if (i + 1 < lines.size()) {
                    std::smatch m1, m2;
                    std::regex re_store_reload(
                        R"(^\s*movq?\s+(%[a-z0-9]+)\s*,\s*([a-zA-Z0-9_]+(?:\(%rip\))?)\s*(?:[#;].*)?$)",
                        std::regex::icase);

                    if (std::regex_match(lines[i], m1, re_store_reload)) {
                        const std::string reg = m1[1].str();
                        const std::string mem = m1[2].str();

                        std::string escaped_mem = std::regex_replace(mem, std::regex(R"([\$\(\)])"), "\\$&");
                        std::regex reload_pattern(R"(^\s*movq?\s+)" + escaped_mem +
                                                R"(\s*,\s*)" + reg + R"(\s*(?:[#;].*)?$)", std::regex::icase);

                        if (std::regex_match(lines[i+1], reload_pattern)) {
                            out.push_back(lines[i]);
                            i += 1;
                            continue;
                        }

                        if (i + 2 < lines.size() && std::regex_match(lines[i+1], reload_pattern)) {
                            std::smatch m3;
                            std::regex re_load_other(
                                R"(^\s*movq?\s+([a-zA-Z0-9_]+(?:\(%rip\))?)\s*,\s*(%[a-z0-9]+)\s*(?:[#;].*)?$)",
                                std::regex::icase);

                            if (std::regex_match(lines[i+2], m3, re_load_other)) {
                                const std::string load_mem = m3[1].str();
                                const std::string load_reg = m3[2].str();
                                out.push_back(lines[i]);
                                out.push_back("\tmovq " + reg + ", " + load_reg);
                                i += 2;
                                continue;
                            }
                        }
                    }
                }
            }

            invalidate_tracking();
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
    
            std::regex re_single_stdout(R"(^\s*movq?\s+_stdout\(%rip\)\s*,\s*(%\w+)\s*(?:[#;].*)?$)", std::regex::icase);
            if (std::regex_search(line, m, re_single_stdout)) {
                std::string r = m[1].str();  
                out.push_back("\tmovq ___stdoutp@GOTPCREL(%rip), %rax");
                out.push_back("\tmovq (%rax), " + r);
                continue;
            }
            std::regex re_single_stderr(R"(^\s*movq?\s+_stderr\(%rip\)\s*,\s*(%\w+)\s*(?:[#;].*)?$)", std::regex::icase);
            if (std::regex_search(line, m, re_single_stderr)) {
                std::string r = m[1].str();  
                out.push_back("\tmovq ___stderrp@GOTPCREL(%rip), %rax");
                out.push_back("\tmovq (%rax), " + r);
                continue;
            }
            std::regex re_single_stdin(R"(^\s*movq?\s+_stdin\(%rip\)\s*,\s*(%\w+)\s*(?:[#;].*)?$)", std::regex::icase);
            if (std::regex_search(line, m, re_single_stdin)) {
                std::string r = m[1].str();  
                out.push_back("\tmovq ___stdinp@GOTPCREL(%rip), %rax");
                out.push_back("\tmovq (%rax), " + r);
                continue;
            }

            line = darwin_prefix_calls(line, macos_functions);
            out.push_back(line);
        }

        return out;
    }

  static std::vector<std::string> opt_x64_windows_lines(const std::vector<std::string> &lines) {
        static const std::regex add_rx(R"(^\s*addq?\s+\$([0-9]+|0x[0-9a-fA-F]+)\s*,\s*%rsp\s*(?:#.*)?$)");
        static const std::regex sub_rx(R"(^\s*subq?\s+\$([0-9]+|0x[0-9a-fA-F]+)\s*,\s*%rsp\s*(?:#.*)?$)");

        auto parse_imm = [](const std::string &s) -> unsigned long long {
            return std::stoull(s, nullptr, 0);
        };

        std::vector<std::string> out;
        out.reserve(lines.size());

        for (size_t i = 0; i < lines.size(); ) {
            const std::string &a = lines[i];

            std::smatch ma, mb;
            bool a_is_add = std::regex_match(a, ma, add_rx);
            bool a_is_sub = !a_is_add && std::regex_match(a, ma, sub_rx);

            if ((a_is_add || a_is_sub) && (i + 1) < lines.size()) {
                const std::string &b = lines[i + 1];
                bool b_is_add = std::regex_match(b, mb, add_rx);
                bool b_is_sub = !b_is_add && std::regex_match(b, mb, sub_rx);

                if ((a_is_add && b_is_sub) || (a_is_sub && b_is_add)) {
                    unsigned long long ia = parse_imm(ma[1].str());
                    unsigned long long ib = parse_imm(mb[1].str());
                    if (ia == ib) { i += 2; continue; }
                }
            }

            out.push_back(a);
            ++i;
        }

        return out;
    }

    static std::vector<std::string> opt_linux_lines(const std::vector<std::string>& lines) {
        return lines;
    }

    std::string Program::gen_optimize(const std::string &code, const Platform &platform_name) {
        std::vector<std::string> lines;
        {
            std::istringstream stream(code);
            std::string s;
            while (std::getline(stream, s)) lines.push_back(s);
        }

        std::vector<std::string> final_lines;

        if(platform == Platform::WINX64) {
             final_lines = x64_opt_core_lines(lines);
             auto lines = opt_x64_windows_lines(final_lines);
             return join_lines(lines);
        }
        else {
            std::vector<std::string> core  = opt_core_lines(lines);
            std::vector<std::string> final_lines =
            (platform_name == Platform::DARWIN) ? opt_darwin_lines(core)
                                           : opt_linux_lines(core);
            return join_lines(final_lines);
        }
    }
}
