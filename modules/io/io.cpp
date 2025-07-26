#include<mxvm/instruct.hpp>
#include<mxvm/icode.hpp>
#include<vector>
#include<string>

extern "C" mxvm::Operand mxvm_io_fopen(mxvm::Program *program, std::vector<mxvm::Operand> &operand) {
    if(operand.size() == 2) {
        std::string &filename = operand[0].op;
        std::string &mode = operand[1].op;
        if(program->isVariable(filename)) {
            mxvm::Variable &v = program->getVariable(filename);
            if(v.type != mxvm::VarType::VAR_STRING) {
                throw mx::Exception("Requires string variable type for fopen.\n");
            }
            filename = v.var_value.str_value;
        } else {
            throw mx::Exception("Variable required for fopen.");
        }
        if(program->isVariable(mode)) {
            mxvm::Variable &v = program->getVariable(mode);
            if(v.type != mxvm::VarType::VAR_STRING) {
                throw mx::Exception("Requires string variable type for fopen.\n");
            }
            mode = v.var_value.str_value;
        } else {
            throw mx::Exception("Variable required for fopen.");
        }
        program->vars["%rax"].type = mxvm::VarType::VAR_POINTER;
        program->vars["%rax"].var_name = "%rax";
        program->vars["%rax"].var_value.ptr_value = (void*)fopen(filename.c_str(), mode.c_str());
        mxvm::Operand o;
        o.op = "%rax";
        return o;
    } else {
        throw mx::Exception("Error invalid types for fopen.\n");
    }
    return mxvm::Operand();
}

extern "C" mxvm::Operand mxvm_io_fprintf(mxvm::Program *program, std::vector<mxvm::Operand> &operand) {
    if(operand.size() < 2) {
        throw mx::Exception("Requires at least two arguments for fprintf.");
     }
     if(program->isVariable(operand[0].op)) {
        std::vector<mxvm::Variable *> v;
        for(size_t i = 2; i < operand.size(); ++i) {
            if(program->isVariable(operand[i].op)) {
                mxvm::Variable &varval = program->getVariable(operand[i].op);
                v.push_back(&varval);
            } else {
                if(operand[i].type == mxvm::OperandType::OP_CONSTANT) {
                    mxvm::Variable val = program->createTempVariable(mxvm::VarType::VAR_INTEGER,  operand[i].op);
                    v.push_back(&val);
                }
            }
        }
        if(program->isVariable(operand[1].op)) {
            mxvm::Variable &fmtv = program->getVariable(operand[1].op);
            if(fmtv.type != mxvm::VarType::VAR_STRING) {
                throw mx::Exception("fprintf requires format to be a string.");
            }
            std::string value = program->printFormatted(fmtv.var_value.str_value, v, false);
            mxvm::Variable &vx = program->getVariable(operand[0].op);
            if(vx.type == mxvm::VarType::VAR_POINTER || vx.type == mxvm::VarType::VAR_EXTERN) {
                FILE *fptr = reinterpret_cast<FILE *>(vx.var_value.ptr_value);
                fprintf(fptr, "%s", value.c_str());
            }
        } else {
            throw mx::Exception("fprintf requires format to be a variable");
        }
    } else {
        throw mx::Exception("fprintf requires first argument to be valid variable found: " + operand[0].op);
    }
    return mxvm::Operand();
}


extern "C" mxvm::Operand mxvm_io_fclose(mxvm::Program *program, std::vector<mxvm::Operand> &operand) {
    if (operand.size() != 1) {
        throw mx::Exception("fclose requires a single file pointer argument.");
    }
    std::string &file_var = operand[0].op;
    if (!program->isVariable(file_var)) {
        throw mx::Exception("fclose argument must be a variable (file pointer).");
    }
    mxvm::Variable &file_v = program->getVariable(file_var);
    if (file_v.type != mxvm::VarType::VAR_POINTER) {
        throw mx::Exception("fclose argument must be a pointer variable.");
    }
    FILE *fp = reinterpret_cast<FILE*>(file_v.var_value.ptr_value);
    int result = fclose(fp);
    program->vars["%rax"].type = mxvm::VarType::VAR_INTEGER;
    program->vars["%rax"].var_value.type = mxvm::VarType::VAR_INTEGER;
    program->vars["%rax"].var_value.int_value = result;
    mxvm::Operand o;
    o.op = "%rax";
    return o;
}

extern "C" mxvm::Operand mxvm_io_fsize(mxvm::Program *program, std::vector<mxvm::Operand> &operand) {
    if (operand.size() != 1) {
        throw mx::Exception("fsize requires a single file pointer argument.");
    }
    std::string &file_var = operand[0].op;
    if (!program->isVariable(file_var)) {
        throw mx::Exception("fsize argument must be a variable pointer.");
    }
    mxvm::Variable &file_v = program->getVariable(file_var);
    if (file_v.type != mxvm::VarType::VAR_POINTER) {
        throw mx::Exception("fsize argument must be a pointer variable.");
    }
    FILE *fptr = reinterpret_cast<FILE*>(file_v.var_value.ptr_value);
    if(fptr == NULL) {
        throw mx::Exception("Error pointer handle must not be null");
    }
    fseek(fptr, 0, SEEK_END);
    size_t result = ftell(fptr);
    fseek(fptr, 0, SEEK_SET);
    program->vars["%rax"].type = mxvm::VarType::VAR_INTEGER;
    program->vars["%rax"].var_value.type = mxvm::VarType::VAR_INTEGER;
    program->vars["%rax"].var_value.int_value = result;
    mxvm::Operand o;
    o.op = "%rax";
    return o;
}


extern "C" mxvm::Operand mxvm_io_fread(mxvm::Program *program, std::vector<mxvm::Operand> &operand) {
    if (operand.size() != 4) {
        throw mx::Exception("fread requires four arguments.");
    }
    std::string &buf_var = operand[0].op;
    if (!program->isVariable(buf_var)) {
        throw mx::Exception("fread argument must be a variable pointer.");
    }
    mxvm::Variable &buf_v = program->getVariable(buf_var);
    if (buf_v.type != mxvm::VarType::VAR_POINTER) {
        throw mx::Exception("fread argument must be a pointer variable.");
    }

    mxvm::Variable vsize, vcount, file_ptr;
    vsize = program->variableFromOperand(operand[1]);
    vcount = program->variableFromOperand(operand[2]);
    file_ptr = program->variableFromOperand(operand[3]);
 
    if(vsize.type != mxvm::VarType::VAR_INTEGER || vcount.type != mxvm::VarType::VAR_INTEGER) {
        throw mx::Exception("Argument type mismatch expected integer for fread");
    }
    if(file_ptr.type != mxvm::VarType::VAR_POINTER && file_ptr.type != mxvm::VarType::VAR_EXTERN) {
        throw mx::Exception("Final argument for fread requires pointer");
    }
    FILE *fptr = reinterpret_cast<FILE*>(file_ptr.var_value.ptr_value);
    size_t result = fread(buf_v.var_value.ptr_value, vsize.var_value.int_value, vcount.var_value.int_value, fptr); 
    program->vars["%rax"].type = mxvm::VarType::VAR_INTEGER;
    program->vars["%rax"].var_value.type = mxvm::VarType::VAR_INTEGER;
    program->vars["%rax"].var_value.int_value = result;
    mxvm::Operand o;
    o.op = "%rax";
    return o;
}

extern "C" mxvm::Operand mxvm_io_fwrite(mxvm::Program *program, std::vector<mxvm::Operand> &operand) {
    if (operand.size() != 4) {
        throw mx::Exception("fwrite requires four arguments.");
    }
    std::string &buf_var = operand[0].op;
    if (!program->isVariable(buf_var)) {
        throw mx::Exception("fwrite argument must be a variable pointer.");
    }
    mxvm::Variable &buf_v = program->getVariable(buf_var);
    if (buf_v.type != mxvm::VarType::VAR_POINTER) {
        throw mx::Exception("fwrite argument must be a pointer variable.");
    }

    mxvm::Variable vsize, vcount, file_ptr;
    vsize = program->variableFromOperand(operand[1]);
    vcount = program->variableFromOperand(operand[2]);
    file_ptr = program->variableFromOperand(operand[3]);

    if(vsize.type != mxvm::VarType::VAR_INTEGER || vcount.type != mxvm::VarType::VAR_INTEGER) {
        throw mx::Exception("Argument type mismatch expected integer for fwrite");
    }
    if(file_ptr.type != mxvm::VarType::VAR_POINTER && file_ptr.type != mxvm::VarType::VAR_EXTERN) {
        throw mx::Exception("Final argument for fwrite requires pointer");
    }
    FILE *fptr = reinterpret_cast<FILE*>(file_ptr.var_value.ptr_value);
    size_t result = fwrite(buf_v.var_value.ptr_value, vsize.var_value.int_value, vcount.var_value.int_value, fptr); 
    program->vars["%rax"].type = mxvm::VarType::VAR_INTEGER;
    program->vars["%rax"].var_value.type = mxvm::VarType::VAR_INTEGER;
    program->vars["%rax"].var_value.int_value = result;
    mxvm::Operand o;
    o.op = "%rax";
    return o;
}


extern "C" mxvm::Operand mxvm_io_fseek(mxvm::Program *program, std::vector<mxvm::Operand> &operand) {
    if (operand.size() != 3) {
        throw mx::Exception("fseek requires three arguments got: " + std::to_string(operand.size()));
    }
    std::string &buf_var = operand[0].op;
    if (!program->isVariable(buf_var)) {
        throw mx::Exception("fseek argument must be a variable found: " + buf_var);
    }
    mxvm::Variable &buf_v = program->getVariable(buf_var);
    if (buf_v.type != mxvm::VarType::VAR_POINTER) {
        throw mx::Exception("fseek argument must be a pointer variable.");
    }
 
    mxvm::Variable vsize, vcount, file_ptr;
    file_ptr =  program->variableFromOperand(operand[0]);
    vsize = program->variableFromOperand(operand[1]);
    vcount = program->variableFromOperand(operand[2]);
         
    if(vsize.type != mxvm::VarType::VAR_INTEGER || vcount.type != mxvm::VarType::VAR_INTEGER) {
        throw mx::Exception("Argument type mismatch expec  ted integer for fread");
    }
    if(file_ptr.type != mxvm::VarType::VAR_POINTER && file_ptr.type != mxvm::VarType::VAR_EXTERN) {
        throw mx::Exception("Final argument for fread requires pointer");
    }
    FILE *fptr = reinterpret_cast<FILE*>(file_ptr.var_value.ptr_value);
    int result = fseek(fptr, vsize.var_value.int_value, vcount.var_value.int_value);
    program->vars["%rax"].type = mxvm::VarType::VAR_INTEGER;
    program->vars["%rax"].var_value.type = mxvm::VarType::VAR_INTEGER;
    program->vars["%rax"].var_value.int_value = result;
    mxvm::Operand o;
    o.op = "%rax";
    return o;
}