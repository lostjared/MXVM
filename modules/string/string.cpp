// string.cpp
#include<mxvm/instruct.hpp>
#include<mxvm/icode.hpp>
#include<cstring>
#include<cstdarg>
#include<cstdio>

extern "C" void mxvm_string_strlen(mxvm::Program *program, std::vector<mxvm::Operand> &operand) {
    if (operand.size() != 1) {
        throw mx::Exception("strlen requires a single file pointer/string argument.");
    }
    std::string &file_var = operand[0].op;
    if (!program->isVariable(file_var)) {
        throw mx::Exception("strlen argument must be a variable (file pointer).");
    }
    mxvm::Variable &file_v = program->getVariable(file_var);
    if (file_v.type != mxvm::VarType::VAR_POINTER && file_v.type != mxvm::VarType::VAR_STRING) {
        throw mx::Exception("strlen argument must be a pointer variable.");
    }

    if(file_v.type == mxvm::VarType::VAR_POINTER)
        mxvm::except_assert("strlen: pointer: " + file_v.var_name + " is null", (file_v.var_value.ptr_value != nullptr));

    const char *src = (file_v.type == mxvm::VarType::VAR_POINTER) ? reinterpret_cast<const char*>(file_v.var_value.ptr_value) : file_v.var_value.str_value.c_str();
    int64_t length = strlen(src);
    program->vars["%rax"].type = mxvm::VarType::VAR_INTEGER;
    program->vars["%rax"].var_value.type = mxvm::VarType::VAR_INTEGER;
    program->vars["%rax"].var_value.int_value = length;
}

extern "C" void mxvm_string_strcmp(mxvm::Program *program, std::vector<mxvm::Operand> &operand) {
    if (operand.size() != 2) {
        throw mx::Exception("strcmp requires two pointer/string arguments.");
    }
    std::string &var1 = operand[0].op;
    std::string &var2 = operand[1].op;

    if (!program->isVariable(var1) || !program->isVariable(var2)) {
        throw mx::Exception("strcmp arguments must be variables (pointer or string).");
    }

    mxvm::Variable &v1 = program->getVariable(var1);
    mxvm::Variable &v2 = program->getVariable(var2);

    const char *s1 = nullptr;
    const char *s2 = nullptr;

    if (v1.type == mxvm::VarType::VAR_POINTER) {
        mxvm::except_assert("strcmp: pointer: " + v1.var_name + " is null", (v1.var_value.ptr_value != nullptr));
        s1 = reinterpret_cast<const char*>(v1.var_value.ptr_value);
    } else if (v1.type == mxvm::VarType::VAR_STRING) {
        s1 = v1.var_value.str_value.c_str();
    } else {
        throw mx::Exception("strcmp first argument must be a pointer or string variable.");
    }

    if (v2.type == mxvm::VarType::VAR_POINTER) {
        mxvm::except_assert("strcmp: pointer: " + v2.var_name + " is null", (v2.var_value.ptr_value != nullptr));
        s2 = reinterpret_cast<const char*>(v2.var_value.ptr_value);
    } else if (v2.type == mxvm::VarType::VAR_STRING) {
        s2 = v2.var_value.str_value.c_str();
    } else {
        throw mx::Exception("strcmp second argument must be a pointer or string variable.");
    }

    int64_t cmp_result = strcmp(s1, s2);
    program->vars["%rax"].type = mxvm::VarType::VAR_INTEGER;
    program->vars["%rax"].var_value.type = mxvm::VarType::VAR_INTEGER;
    program->vars["%rax"].var_value.int_value = cmp_result;
}

extern "C" void mxvm_string_strncpy(mxvm::Program *program, std::vector<mxvm::Operand> &operand) {
    if (operand.size() != 3) {
        throw mx::Exception("strncpy requires destination, source, and length arguments.");
    }
    std::string &dest_var = operand[0].op;
    std::string &src_var = operand[1].op;
    int64_t n = operand[2].op_value;

    if (!program->isVariable(dest_var) || !program->isVariable(src_var)) {
        throw mx::Exception("strncpy arguments must be variables (pointer or string).");
    }

    mxvm::Variable &dest = program->getVariable(dest_var);
    mxvm::Variable &src = program->getVariable(src_var);


    if(program->isVariable(operand[2].op)) {
        mxvm::Variable &iv = program->getVariable(operand[2].op);
        n = iv.var_value.int_value;
    }
    if (dest.type != mxvm::VarType::VAR_POINTER && dest.type != mxvm::VarType::VAR_STRING && dest.var_value.buffer_size == 0) {
        throw mx::Exception("strncpy destination must be a pointer/string  buffer variable.");
    }

    if(dest.type != mxvm::VarType::VAR_STRING && static_cast<int64_t>(dest.var_value.buffer_size) > n) {
        throw mx::Exception("strncpy destination string must be large enogh:  " + std::to_string(dest.var_value.buffer_size) + " > " + std::to_string(n));
    }

    char *dptr = nullptr;

    if(dest.type == mxvm::VarType::VAR_POINTER) {
        mxvm::except_assert("strncpy: destination pointer is null", (dest.var_value.ptr_value != nullptr));
        dptr = reinterpret_cast<char*>(dest.var_value.ptr_value);
    }

    const char *sptr = nullptr;
    if (src.type == mxvm::VarType::VAR_POINTER) {
        mxvm::except_assert("strncpy: source pointer is null", (src.var_value.ptr_value != nullptr));
        sptr = reinterpret_cast<const char*>(src.var_value.ptr_value);
    } else if (src.type == mxvm::VarType::VAR_STRING) {
        sptr = src.var_value.str_value.c_str();
    } else {
        throw mx::Exception("strncpy source must be a pointer or string variable.");
    }

    if(dest.type == mxvm::VarType::VAR_POINTER) {
        strncpy(dptr, sptr, n);
    } else {
        dest.var_value.str_value = sptr;
    }
    program->vars["%rax"].type = mxvm::VarType::VAR_INTEGER;
    program->vars["%rax"].var_value.type = mxvm::VarType::VAR_INTEGER;
    program->vars["%rax"].var_value.int_value = n;
}

extern "C" void mxvm_string_strncat(mxvm::Program *program, std::vector<mxvm::Operand> &operand) {
    if (operand.size() != 3) {
        throw mx::Exception("strncat requires destination, source, and length arguments.");
    }
    std::string &dest_var = operand[0].op;
    std::string &src_var = operand[1].op;
    int64_t n = operand[2].op_value;

    if (!program->isVariable(dest_var) || !program->isVariable(src_var)) {
        throw mx::Exception("strncat arguments must be variables (pointer or string).");
    }

    mxvm::Variable &dest = program->getVariable(dest_var);
    mxvm::Variable &src = program->getVariable(src_var);

    if(program->isVariable(operand[2].op)) {
        mxvm::Variable &iv = program->getVariable(operand[2].op);
        n = iv.var_value.int_value;
    }
    
    if (dest.type != mxvm::VarType::VAR_POINTER && dest.type != mxvm::VarType::VAR_STRING && dest.var_value.buffer_size == 0) {
        throw mx::Exception("strncpy destination must be a pointer/string  buffer variable.");
    }
    if(dest.type != mxvm::VarType::VAR_STRING && static_cast<int64_t>(dest.var_value.buffer_size) > n) {
        throw mx::Exception("strncpy destination string must be large enogh:  " + std::to_string(dest.var_value.buffer_size) + " > " + std::to_string
        (n));
    }
    char *dptr = nullptr;
    if(dest.type == mxvm::VarType::VAR_POINTER) {
        mxvm::except_assert("strncat: destination pointer is null", (dest.var_value.ptr_value != nullptr));
        dptr = reinterpret_cast<char*>(dest.var_value.ptr_value);
    }

    const char *sptr = nullptr;
    if (src.type == mxvm::VarType::VAR_POINTER) {
        mxvm::except_assert("strncat: source pointer is null", (src.var_value.ptr_value != nullptr));
        sptr = reinterpret_cast<const char*>(src.var_value.ptr_value);
    } else if (src.type == mxvm::VarType::VAR_STRING) {
        sptr = src.var_value.str_value.c_str();
    } else {
        throw mx::Exception("strncat source must be a pointer or string variable.");
    }
    if(dest.type == mxvm::VarType::VAR_POINTER) {
        strncat(dptr, sptr, n);
    } else {
        dest.var_value.str_value += sptr;
    }

    program->vars["%rax"].type = mxvm::VarType::VAR_INTEGER;
    program->vars["%rax"].var_value.type = mxvm::VarType::VAR_INTEGER;
    program->vars["%rax"].var_value.int_value = n;
}

extern "C" void mxvm_string_snprintf(mxvm::Program *program, std::vector<mxvm::Operand> &operand) {
    if (operand.size() < 4) {
        throw mx::Exception("snprintf requires at least destination, format, and one argument.");
    }
    std::string &dest_var = operand[0].op;
    std::string &fmt_var = operand[2].op;
    int64_t n = operand[1].op_value;
    if (!program->isVariable(dest_var) || !program->isVariable(fmt_var)) {
        throw mx::Exception("snprintf destination and format must be variables.");
    }
    
    if(program->isVariable(operand[1].op)) {
        mxvm::Variable &iv = program->getVariable(operand[2].op);
        n = iv.var_value.int_value;
    }

    mxvm::except_assert("snprintf size is zero", n != 0);
    mxvm::Variable &dest = program->getVariable(dest_var);
    mxvm::Variable &fmt = program->getVariable(fmt_var);
    
    if (dest.type != mxvm::VarType::VAR_POINTER && dest.type != mxvm::VarType::VAR_STRING && dest.var_value.buffer_size == 0) {
        throw mx::Exception("strncpy destination must be a pointer/string  buffer variable.");
    }


    if(dest.type != mxvm::VarType::VAR_STRING && static_cast<int64_t>(dest.var_value.buffer_size) > n) {
        throw mx::Exception("strncpy destination string must be large enogh:  " + std::to_string(dest.var_value.buffer_size) + " > " + std::to_string(n));
    }

    if (fmt.type != mxvm::VarType::VAR_STRING) {
        throw mx::Exception("snprintf format must be a string variable.");
    }
    std::ostringstream oss;
    size_t argIndex = 3;
    const char* format = fmt.var_value.str_value.c_str();
    char buffer[4096];
    for (size_t i = 0; i < fmt.var_value.str_value.length(); ++i) {
        if (format[i] == '%' && i + 1 < fmt.var_value.str_value.length()) {
            size_t start = i;
            size_t j = i + 1;
            while (j < fmt.var_value.str_value.length() &&
                (format[j] == '-' || format[j] == '+' || format[j] == ' ' || format[j] == '#' || format[j] == '0' ||
                 (format[j] >= '0' && format[j] <= '9') || format[j] == '.' ||
                 format[j] == 'l' || format[j] == 'h' || format[j] == 'z' || format[j] == 'j' || format[j] == 't')) {
                ++j;
            }
            if (j < fmt.var_value.str_value.length() && format[j] == '%') {
                oss << '%';
                i = j;
                continue;
            }
            if (j >= fmt.var_value.str_value.length()) break;
            if (!std::isalpha(format[j])) {
                oss << format[i];
                continue;
            }
            std::string spec(format + start, format + j + 1);
            if (argIndex < operand.size()) {
                std::string &arg_var = operand[argIndex++].op;
                if (!program->isVariable(arg_var)) {
                    throw mx::Exception("snprintf argument must be a variable.");
                }
                mxvm::Variable &arg = program->getVariable(arg_var);
                if (arg.type == mxvm::VarType::VAR_INTEGER) {
                    std::snprintf(buffer, sizeof(buffer), spec.c_str(), arg.var_value.int_value);
                } else if (arg.type == mxvm::VarType::VAR_POINTER || arg.type == mxvm::VarType::VAR_EXTERN) {
                    std::snprintf(buffer, sizeof(buffer), spec.c_str(), arg.var_value.ptr_value);
                } else if (arg.type == mxvm::VarType::VAR_FLOAT) {
                    std::snprintf(buffer, sizeof(buffer), spec.c_str(), arg.var_value.float_value);
                } else if (arg.type == mxvm::VarType::VAR_STRING) {
                    std::snprintf(buffer, sizeof(buffer), spec.c_str(), arg.var_value.str_value.c_str());
                } else if (arg.type == mxvm::VarType::VAR_BYTE) {
                    std::snprintf(buffer, sizeof(buffer), spec.c_str(), arg.var_value.int_value);
                } else {
                    std::snprintf(buffer, sizeof(buffer), "%s", "(unsupported)");
                }
                oss << buffer;
            } else {
                oss << spec;
            }
            i = j;
        } else {
            oss << format[i];
        }
    }
    if(dest.type == mxvm::VarType::VAR_POINTER) {
        mxvm::except_assert("snpritnf dest pointer is null", dest.var_value.ptr_value != nullptr);
        strncpy(reinterpret_cast<char*>(dest.var_value.ptr_value), oss.str().c_str(), n);
    } else if(dest.type == mxvm::VarType::VAR_STRING) {
        dest.var_value.str_value = oss.str();
    }
    program->vars["%rax"].type = mxvm::VarType::VAR_INTEGER;
    program->vars["%rax"].var_value.type = mxvm::VarType::VAR_INTEGER;
    program->vars["%rax"].var_value.int_value = oss.str().length();
}

extern "C" void mxvm_string_strfind(mxvm::Program *program, std::vector<mxvm::Operand> &operand) {
    if (operand.size() != 3) {
        throw mx::Exception("strfind requires destination, source, and length arguments.");
    }
    std::string &dest_var = operand[0].op;
    std::string &src_var = operand[1].op;
    std::string bytes_value = operand[2].op;
    int64_t n = operand[2].op_value;

    if (!program->isVariable(dest_var) || !program->isVariable(src_var)) {
        throw mx::Exception("strfind arguments must be variables (pointer or string).");
    }
    mxvm::Variable &dest = program->getVariable(dest_var);
    mxvm::Variable &src = program->getVariable(src_var);
    if(program->isVariable(operand[2].op)) {
        mxvm::Variable &iv = program->getVariable(operand[2].op);
        n = iv.var_value.int_value;
    }
    const char *dptr = nullptr;
    if(dest.type == mxvm::VarType::VAR_POINTER) {
        mxvm::except_assert("stfind: arg1 pointer is null", (dest.var_value.ptr_value != nullptr));
        dptr = reinterpret_cast<char*>(dest.var_value.ptr_value);
    } else {
        dptr = dest.var_value.str_value.c_str();
    }
    const char *sptr = nullptr;
    if (src.type == mxvm::VarType::VAR_POINTER) {
        mxvm::except_assert("strfind: source pointer is null", (src.var_value.ptr_value != nullptr));
        sptr = reinterpret_cast<const char*>(src.var_value.ptr_value);
    } else if (src.type == mxvm::VarType::VAR_STRING) {
        sptr = src.var_value.str_value.c_str();
    } else {
        throw mx::Exception("strfind: arg2e must be a pointer or string variable.");
    }
    int64_t result = 0;
    auto pos = std::string(dptr).find(sptr, n);
    if(pos == std::string::npos) {
        result = -1;
    } else {
        result = pos;
    }
    program->vars["%rax"].type = mxvm::VarType::VAR_INTEGER;
    program->vars["%rax"].var_value.type = mxvm::VarType::VAR_INTEGER;
    program->vars["%rax"].var_value.int_value = result;
}

//long substr(char *dest, long size, const char *src, long pos,  long len)

extern "C" void mxvm_string_substr(mxvm::Program *program, std::vector<mxvm::Operand> &operand) {
    if (operand.size() != 5) {
        throw mx::Exception("sub requires position then length you used: " + std::to_string(operand.size()));
    }
    std::string &v_dest = operand[0].op;
    int64_t size = operand[1].op_value;
    std::string &v_src = operand[2].op;
    int64_t pos = operand[3].op_value;
    int64_t len = operand[4].op_value;;
    
    if(!program->isVariable(v_dest)) {
        throw mx::Exception("substr first argument requires variable\n");
    }

    if(!program->isVariable(v_src)) {
        throw mx::Exception("substr third argument requires variable\n");
    }

    
    if(program->isVariable(operand[1].op)) {
        mxvm::Variable &iv = program->getVariable(operand[1].op);
        size = iv.var_value.int_value;
    }

    if(program->isVariable(operand[3].op)) {
        mxvm::Variable &iv = program->getVariable(operand[3].op);
        pos = iv.var_value.int_value;
    }

    if(program->isVariable(operand[4].op)) {
        mxvm::Variable &iv = program->getVariable(operand[4].op);
        len = iv.var_value.int_value;
    }
    
    mxvm::Variable &v = program->getVariable(v_src);
    std::string s = v.var_value.str_value;
    std::string result = s.substr(pos, len);

    mxvm::Variable &d = program->getVariable(v_dest);
    if(d.type == mxvm::VarType::VAR_STRING && d.var_value.buffer_size > 0) {
        d.var_value.str_value = result;
    } else if(d.type == mxvm::VarType::VAR_POINTER) {
        strncpy((char*)d.var_value.ptr_value, result.c_str(), size);
    } else {
        
    }
    program->vars["%rax"].type = mxvm::VarType::VAR_INTEGER;
    program->vars["%rax"].var_value.type = mxvm::VarType::VAR_INTEGER;
    program->vars["%rax"].var_value.int_value = result.length();
}
