#include "icode.hpp"

namespace pascal {

        std::unordered_set<std::string> sdlFunctions = {
            "sdl_init", "sdl_quit", "sdl_create_window", "sdl_destroy_window", "sdl_set_window_title",
            "sdl_set_window_position", "sdl_get_window_size", "sdl_set_window_fullscreen", "sdl_set_window_icon",
            "sdl_create_renderer", "sdl_destroy_renderer", "sdl_set_draw_color", "sdl_clear", "sdl_present",
            "sdl_get_renderer_output_size", "sdl_draw_point", "sdl_draw_line", "sdl_draw_rect", "sdl_fill_rect",
            "sdl_poll_event", "sdl_get_event_type", "sdl_get_key_code", "sdl_get_mouse_x", "sdl_get_mouse_y",
            "sdl_get_mouse_button", "sdl_get_mouse_buttons", "sdl_get_relative_mouse_x", "sdl_get_relative_mouse_y",
            "sdl_get_relative_mouse_buttons", "sdl_create_rgb_surface", "sdl_free_surface", "sdl_blit_surface",
            "sdl_get_mouse_state", "sdl_get_relative_mouse_state", "sdl_get_keyboard_state", "sdl_is_key_pressed",
            "sdl_get_num_keys", "sdl_set_clipboard_text", "sdl_get_clipboard_text", "sdl_show_cursor",
            "sdl_create_texture", "sdl_destroy_texture", "sdl_load_texture", "sdl_render_texture",
            "sdl_update_texture", "sdl_lock_texture", "sdl_unlock_texture", "sdl_get_ticks", "sdl_delay",
            "sdl_open_audio", "sdl_close_audio", "sdl_pause_audio", "sdl_load_wav", "sdl_free_wav",
            "sdl_queue_audio", "sdl_get_queued_audio_size", "sdl_clear_queued_audio", "sdl_init_text",
            "sdl_quit_text", "sdl_load_font", "sdl_draw_text", "sdl_create_render_target", "sdl_set_render_target",
            "sdl_destroy_render_target", "sdl_present_scaled", "sdl_present_stretched"
        };

        
        std::unordered_set<std::string> stdFunctions = {
            "argc", "argv", "set_program_args", "free_program_args", "abs", "fabs",
            "sqrt", "pow", "sin", "cos", "tan", "floor", "ceil", "rand", "srand",
            "malloc", "calloc", "free", "toupper", "tolower", "isalpha", "isdigit",
            "isspace", "atoi", "atof", "exit", "system", "memcpy", "memcmp",
            "memmove", "memset", "exp", "exp2", "log", "log10", "log2", "fmod",
            "atan2", "asin", "acos", "atan", "sinh", "cosh", "tanh", "hypot",
            "round", "trunc", "float_to_int", "int_to_float", "halt"
        };
    

        bool BuiltinFunctionHandler::generateWithResult(CodeGenVisitor& visitor, const std::string& funcName, const std::vector<std::unique_ptr<ASTNode>>& arguments) { return false; }

        bool IOFunctionHandler::canHandle(const std::string& funcName) const {
            return funcName == "writeln" || funcName == "write" || funcName == "readln";
        }

        void IOFunctionHandler::generate(CodeGenVisitor& visitor, const std::string& funcName, 
                        const std::vector<std::unique_ptr<ASTNode>>& arguments) {
            if (funcName == "writeln" || funcName == "write") {
                for(auto &arg : arguments) {
                    ASTNode *a = arg.get();
                    
                    if (auto numNode = dynamic_cast<NumberNode*>(a)) {
                        if (numNode->isReal || visitor.isRealNumber(numNode->value)) {
                            visitor.usedStrings.insert("fmt_float");
                            std::string v = visitor.eval(a);
                            visitor.emit2("print", "fmt_float", v);
                            if (visitor.isReg(v)) visitor.freeReg(v);
                        } else {
                            visitor.usedStrings.insert("fmt_int");
                            std::string v = visitor.eval(a);
                            visitor.emit2("print", "fmt_int", v);
                            if (visitor.isReg(v)) visitor.freeReg(v);
                        }
                    }
                    else if (auto strNode = dynamic_cast<StringNode*>(a)) {
                        if (strNode->value.length() == 1) {
                            visitor.usedStrings.insert("fmt_chr");
                            std::string v = visitor.eval(a);
                            visitor.emit2("print", "fmt_chr", v);
                            if (visitor.isReg(v)) visitor.freeReg(v);
                        } else {
                            visitor.usedStrings.insert("fmt_str");
                            std::string v = visitor.eval(a);
                            visitor.emit2("print", "fmt_str", v);
                            if (visitor.isReg(v)) visitor.freeReg(v);
                        }
                    }
                    else if (auto varNode = dynamic_cast<VariableNode*>(a)) {
                        auto varType = visitor.getVarType(varNode->name);
                        if (varType == CodeGenVisitor::VarType::CHAR) {
                            visitor.usedStrings.insert("fmt_chr");
                            std::string v = visitor.eval(a);
                            visitor.emit2("print", "fmt_chr", v);
                            if (visitor.isReg(v)) visitor.freeReg(v);
                        } 
                        else if (varType == CodeGenVisitor::VarType::STRING || visitor.isStringVar(varNode->name)) {
                            visitor.usedStrings.insert("fmt_str");
                            std::string v = visitor.eval(a);
                            visitor.emit2("print", "fmt_str", v);
                            if (visitor.isReg(v)) visitor.freeReg(v);
                        }
                        else if (varType == CodeGenVisitor::VarType::DOUBLE) {
                            visitor.usedStrings.insert("fmt_float");
                            std::string v = visitor.eval(a);
                            visitor.emit2("print", "fmt_float", v);
                            if (visitor.isReg(v)) visitor.freeReg(v);
                        }
                        else {
                            visitor.usedStrings.insert("fmt_int");
                            std::string v = visitor.eval(a);
                            visitor.emit2("print", "fmt_int", v);
                            if (visitor.isReg(v)) visitor.freeReg(v);
                        }
                    }
                    else {
                        std::string v = visitor.eval(a);
                        if (visitor.isFloatReg(v) || v.find("real_const_") != std::string::npos) {
                            visitor.usedStrings.insert("fmt_float");
                            visitor.emit2("print", "fmt_float", v);
                        } else {
                            CodeGenVisitor::VarType exprType = visitor.getExpressionType(a);
                            if (exprType == CodeGenVisitor::VarType::DOUBLE) {
                                visitor.usedStrings.insert("fmt_float");
                                visitor.emit2("print", "fmt_float", v);
                            } else {
                                visitor.usedStrings.insert("fmt_int");
                                visitor.emit2("print", "fmt_int", v);
                            }
                        }
                        
                        if (visitor.isReg(v)) visitor.freeReg(v);
                    }
                }

                if (funcName == "writeln") {
                    visitor.usedStrings.insert("newline");
                    visitor.emit1("print", "newline");
                }
            }
            else if (funcName == "readln") {
                if (arguments.empty()) {
                    visitor.emit1("getline", "input_buffer");
                    return;
                }
                
                for (auto &arg : arguments) {
                    if (auto varNode = dynamic_cast<VariableNode*>(arg.get())) {
                        std::string varName = varNode->name;
                        int slot = visitor.newSlotFor(varName);
                        std::string memLoc = visitor.slotVar(slot);
                        
                        auto varType = visitor.getVarType(varName);
                        visitor.emit1("getline", "input_buffer");
                        
                        if (varType == CodeGenVisitor::VarType::DOUBLE) {
                            visitor.emit2("to_float", memLoc, "input_buffer");
                        }
                        else if (varType == CodeGenVisitor::VarType::INT) {
                            visitor.emit2("to_int", memLoc, "input_buffer");
                        }
                        
                        visitor.recordLocation(varName, {CodeGenVisitor::ValueLocation::MEMORY, memLoc});
                    } 
                    else {
                        int lineNum = arg->getLineNumber();
                        throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                                ": readln argument must be a variable");
                    }
                }
            }
        }

        bool StdFunctionHandler::canHandle(const std::string& funcName) const  {
            return stdFunctions.find(funcName) != stdFunctions.end();
        }

        void StdFunctionHandler::generate(CodeGenVisitor& visitor, const std::string& funcName, 
                                    const std::vector<std::unique_ptr<ASTNode>>& arguments)  {
            visitor.usedModules.insert("std");
            
            int lineNum = arguments.empty() ? 1 : arguments[0]->getLineNumber();
            
            std::vector<std::string> args;
            for (auto& arg : arguments) {
                args.push_back(visitor.eval(arg.get()));
            }

            if (funcName == "srand") {
                if (arguments.size() != 1) 
                    throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                            ": srand requires 1 argument");
                visitor.emit_invoke("srand", args);
            }
            else if (funcName == "free") {
                if (arguments.size() != 1) 
                    throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                            ": free requires 1 argument");
                visitor.emit_invoke("free", args);
            }
            else if (funcName == "halt") {
                if (arguments.size() != 1) 
                    throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                            ": halt requires 1 argument");
                visitor.emit("exit " + args[0]);
            }
            
            for (const std::string& arg : args) {
                if (visitor.isReg(arg)) visitor.freeReg(arg);
            }
        }

        bool StdFunctionHandler::generateWithResult(CodeGenVisitor& visitor, const std::string& funcName,
                                                    const std::vector<std::unique_ptr<ASTNode>>& arguments)  {
            visitor.usedModules.insert("std");
            
            int lineNum = arguments.empty() ? 1 : arguments[0]->getLineNumber();
            
            std::unordered_set<std::string> floatMathFuncs = {
                "sin", "cos", "tan", "asin", "acos", "atan", "sinh", "cosh", "tanh",
                "exp", "log", "log10", "sqrt", "pow", "fmod", "ceil", "floor", "fabs",
                "round", "trunc", "hypot"
            };

            if (floatMathFuncs.find(funcName) != floatMathFuncs.end()) {
                int requiredArgs = (funcName == "pow" || funcName == "fmod" || funcName == "hypot") ? 2 : 1;
                if (static_cast<int>(arguments.size()) != requiredArgs)
                    throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                            ": " + funcName + " requires " + std::to_string(requiredArgs) + " argument(s)");

                std::vector<std::string> args;
                args.reserve(arguments.size());

                for (const auto& argNode : arguments) {
                    std::string argLocation;
                    bool isLiteral = false;

                    if (auto numNode = dynamic_cast<NumberNode*>(argNode.get())) {
                        isLiteral = true;
                        std::string constName = visitor.generateRealConstantName();
                        if (numNode->value.find('.') == std::string::npos) {
                            visitor.realConstants[constName] = numNode->value + ".0";
                        } else {
                            visitor.realConstants[constName] = numNode->value;
                        }
                        visitor.usedRealConstants.insert(constName);
                        argLocation = constName;
                    } 
                    else if (auto unaryNode = dynamic_cast<UnaryOpNode*>(argNode.get())) {
                        if (unaryNode->operator_ == UnaryOpNode::MINUS) {
                            if (auto numNode = dynamic_cast<NumberNode*>(unaryNode->operand.get())) {
                                isLiteral = true;
                                std::string constName = visitor.generateRealConstantName();
                                std::string val = "-" + numNode->value;
                                if (val.find('.') == std::string::npos) {
                                    visitor.realConstants[constName] = val + ".0";
                                } else {
                                    visitor.realConstants[constName] = val;
                                }
                                visitor.usedRealConstants.insert(constName);
                                argLocation = constName;
                            }
                        }
                    }
                    
                    if (!isLiteral) {
                        argLocation = visitor.eval(argNode.get());
                    }

                    std::string floatReg = visitor.allocFloatReg();
                    visitor.emit3("mov", floatReg, argLocation);
                    args.push_back(floatReg);
                    if (!isLiteral && visitor.isReg(argLocation)) {
                        visitor.freeReg(argLocation);
                    }
                }

                std::string resultReg = visitor.allocFloatReg();
                visitor.emit_invoke(funcName, args);
                visitor.emit("return " + resultReg); 
                visitor.pushValue(resultReg);

                for (const std::string& fr : args) {
                    if (visitor.isFloatReg(fr)) {
                        visitor.freeFloatReg(fr);
                    }
                }
                return true;
            }
            else if (funcName == "abs") {
                if (arguments.size() != 1) 
                    throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                            ": abs requires 1 argument");
                std::string a = visitor.eval(arguments[0].get());
                
                std::string resultReg = visitor.allocReg();
                visitor.emit_invoke("abs", {a});
                visitor.emit("return " + resultReg); 
                visitor.pushValue(resultReg);
                if (visitor.isReg(a)) visitor.freeReg(a);
                return true;
            } else if (funcName == "rand") {
                if (arguments.size() != 0) 
                    throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                            ": rand requires 0 arguments");
                visitor.emit_invoke("rand", {});
                std::string resultReg = visitor.allocReg();
                visitor.emit("return " + resultReg); 
                visitor.pushValue(resultReg);
                return true;
            }

            return false;
        }

    bool SDLFunctionHandler::canHandle(const std::string& funcName) const {
        return funcName == "sdl_init" || funcName == "sdl_quit" ||
               funcName == "sdl_create_window" || funcName == "sdl_destroy_window" ||
               funcName == "sdl_create_renderer" || funcName == "sdl_destroy_renderer" ||
               funcName == "sdl_set_draw_color" || funcName == "sdl_clear" || funcName == "sdl_present" ||
               funcName == "sdl_draw_point" || funcName == "sdl_draw_line" || funcName == "sdl_draw_rect" ||
               funcName == "sdl_fill_rect" || funcName == "sdl_poll_event" || funcName == "sdl_get_event_type" ||
               funcName == "sdl_get_key_code" || funcName == "sdl_get_mouse_x" || funcName == "sdl_get_mouse_y" ||
               funcName == "sdl_get_mouse_button" || funcName == "sdl_get_mouse_state" ||
               funcName == "sdl_get_relative_mouse_state" || funcName == "sdl_get_mouse_buttons" ||
               funcName == "sdl_get_relative_mouse_x" || funcName == "sdl_get_relative_mouse_y" ||
               funcName == "sdl_get_relative_mouse_buttons" || funcName == "sdl_is_key_pressed" ||
               funcName == "sdl_get_keyboard_state" || funcName == "sdl_get_num_keys" ||
               funcName == "sdl_set_window_title" || funcName == "sdl_set_window_position" ||
               funcName == "sdl_get_window_size" || funcName == "sdl_set_window_fullscreen" ||
               funcName == "sdl_set_window_icon" || funcName == "sdl_get_renderer_output_size" ||
               funcName == "sdl_show_cursor" || funcName == "sdl_create_rgb_surface" ||
               funcName == "sdl_free_surface" || funcName == "sdl_blit_surface" ||
               funcName == "sdl_set_clipboard_text" || funcName == "sdl_get_clipboard_text" ||
               funcName == "sdl_create_texture" || funcName == "sdl_destroy_texture" ||
               funcName == "sdl_load_texture" || funcName == "sdl_render_texture" ||
               funcName == "sdl_get_ticks" || funcName == "sdl_delay" ||
               funcName == "sdl_open_audio" || funcName == "sdl_close_audio" || funcName == "sdl_pause_audio" ||
               funcName == "sdl_load_wav" || funcName == "sdl_free_wav" || funcName == "sdl_queue_audio" ||
               funcName == "sdl_get_queued_audio_size" || funcName == "sdl_clear_queued_audio" ||
               funcName == "sdl_update_texture" || funcName == "sdl_lock_texture" || funcName == "sdl_unlock_texture" ||
               funcName == "sdl_init_text" || funcName == "sdl_quit_text" || funcName == "sdl_load_font" ||
               funcName == "sdl_draw_text" || funcName == "sdl_create_render_target" ||
               funcName == "sdl_set_render_target" || funcName == "sdl_destroy_render_target" ||
               funcName == "sdl_present_scaled" || funcName == "sdl_present_stretched";
    }

    void SDLFunctionHandler::generate(CodeGenVisitor& visitor, const std::string& funcName, const std::vector<std::unique_ptr<ASTNode>>& arguments) {
        visitor.usedModules.insert("sdl");
        
        int lineNum = arguments.empty() ? 1 : arguments[0]->getLineNumber();
        
        std::vector<std::string> args;
        args.reserve(arguments.size());
        for (auto& arg : arguments) args.push_back(visitor.eval(arg.get()));

        auto freeArgs = [&](){
            for (const std::string& a : args) if (visitor.isReg(a)) visitor.freeReg(a);
        };

        if (funcName == "sdl_init") {
            if (arguments.size() != 0) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_init requires 0 arguments");
            visitor.emit_invoke("init", {});
        }
        else if (funcName == "sdl_quit") {
            if (arguments.size() != 0) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_quit requires 0 arguments");
            visitor.emit_invoke("quit", {});
        }
        else if (funcName == "sdl_destroy_window") {
            if (arguments.size() != 1) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_destroy_window requires 1 argument (window_id)");
            visitor.emit_invoke("destroy_window", args);
        }
        else if (funcName == "sdl_destroy_renderer") {
            if (arguments.size() != 1) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_destroy_renderer requires 1 argument (renderer_id)");
            visitor.emit_invoke("destroy_renderer", args);
        }
        else if (funcName == "sdl_set_window_title") {
            if (arguments.size() != 2) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_set_window_title requires 2 arguments (window_id, title)");
            visitor.emit_invoke("set_window_title", args);
        }
        else if (funcName == "sdl_set_window_position") {
            if (arguments.size() != 3) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_set_window_position requires 3 arguments (window_id, x, y)");
            visitor.emit_invoke("set_window_position", args);
        }
        else if (funcName == "sdl_get_window_size") {
            if (arguments.size() != 3) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_get_window_size requires 3 arguments (window_id, w_var, h_var)");
            visitor.emit_invoke("get_window_size", args);
        }
        else if (funcName == "sdl_set_window_fullscreen") {
            if (arguments.size() != 2) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_set_window_fullscreen requires 2 arguments (window_id, fullscreen)");
            visitor.emit_invoke("set_window_fullscreen", args);
        }
        else if (funcName == "sdl_set_window_icon") {
            if (arguments.size() != 2) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_set_window_icon requires 2 arguments (window_id, path)");
            visitor.emit_invoke("set_window_icon", args);
        }
        else if (funcName == "sdl_set_draw_color") {
            if (arguments.size() != 5) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_set_draw_color requires 5 arguments (renderer_id, r, g, b, a)");
            visitor.emit_invoke("set_draw_color", args);
        }
        else if (funcName == "sdl_clear") {
            if (arguments.size() != 1) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_clear requires 1 argument (renderer_id)");
            visitor.emit_invoke("clear", args);
        }
        else if (funcName == "sdl_present") {
            if (arguments.size() != 1) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_present requires 1 argument (renderer_id)");
            visitor.emit_invoke("present", args);
        }
        else if (funcName == "sdl_draw_point") {
            if (arguments.size() != 3) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_draw_point requires 3 arguments (renderer_id, x, y)");
            visitor.emit_invoke("draw_point", args);
        }
        else if (funcName == "sdl_draw_line") {
            if (arguments.size() != 5) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_draw_line requires 5 arguments (renderer_id, x1, y1, x2, y2)");
            visitor.emit_invoke("draw_line", args);
        }
        else if (funcName == "sdl_draw_rect") {
            if (arguments.size() != 5) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_draw_rect requires 5 arguments (renderer_id, x, y, w, h)");
            visitor.emit_invoke("draw_rect", args);
        }
        else if (funcName == "sdl_fill_rect") {
            if (arguments.size() != 5) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_fill_rect requires 5 arguments (renderer_id, x, y, w, h)");
            visitor.emit_invoke("fill_rect", args);
        }
        else if (funcName == "sdl_get_mouse_state") {
            if (arguments.size() != 2) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_get_mouse_state requires 2 arguments (x_var, y_var)");
            visitor.emit_invoke("get_mouse_state", args);
        }
        else if (funcName == "sdl_get_relative_mouse_state") {
            if (arguments.size() != 2) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_get_relative_mouse_state requires 2 arguments (x_var, y_var)");
            visitor.emit_invoke("get_relative_mouse_state", args);
        }
        else if (funcName == "sdl_get_keyboard_state") {
            if (arguments.size() != 1) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_get_keyboard_state requires 1 argument (numkeys_var)");
            visitor.emit_invoke("get_keyboard_state", args);
        }
        else if (funcName == "sdl_show_cursor") {
            if (arguments.size() != 1) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_show_cursor requires 1 argument (show)");
            visitor.emit_invoke("show_cursor", args);
        }
        else if (funcName == "sdl_free_surface") {
            if (arguments.size() != 1) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_free_surface requires 1 argument (surf_ptr)");
            visitor.emit_invoke("free_surface", args);
        }
        else if (funcName == "sdl_set_clipboard_text") {
            if (arguments.size() != 1) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_set_clipboard_text requires 1 argument (text)");
            visitor.emit_invoke("set_clipboard_text", args);
        }
        else if (funcName == "sdl_destroy_texture") {
            if (arguments.size() != 1) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_destroy_texture requires 1 argument (texture_id)");
            visitor.emit_invoke("destroy_texture", args);
        }
        else if (funcName == "sdl_render_texture") {
            if (arguments.size() != 10) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_render_texture requires 10 arguments (renderer_id, texture_id, src_x, src_y, src_w, src_h, dst_x, dst_y, dst_w, dst_h)");
            visitor.emit_invoke("render_texture", args);
        }
        else if (funcName == "sdl_delay") {
            if (arguments.size() != 1) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_delay requires 1 argument (ms)");
            visitor.emit_invoke("delay", args);
        }
        else if (funcName == "sdl_close_audio") {
            if (arguments.size() != 0) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_close_audio requires 0 arguments");
            visitor.emit_invoke("close_audio", {});
        }
        else if (funcName == "sdl_pause_audio") {
            if (arguments.size() != 1) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_pause_audio requires 1 argument (pause_on)");
            visitor.emit_invoke("pause_audio", args);
        }
        else if (funcName == "sdl_free_wav") {
            if (arguments.size() != 1) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_free_wav requires 1 argument (audio_buf)");
            visitor.emit_invoke("free_wav", args);
        }
        else if (funcName == "sdl_queue_audio") {
            if (arguments.size() != 2) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_queue_audio requires 2 arguments (data, len)");
            visitor.emit_invoke("queue_audio", args);
        }
        else if (funcName == "sdl_clear_queued_audio") {
            if (arguments.size() != 0) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_clear_queued_audio requires 0 arguments");
            visitor.emit_invoke("clear_queued_audio", {});
        }
        else if (funcName == "sdl_update_texture") {
            if (arguments.size() != 3) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_update_texture requires 3 arguments (texture_id, pixels, pitch)");
            visitor.emit_invoke("update_texture", args);
        }
        else if (funcName == "sdl_unlock_texture") {
            if (arguments.size() != 1) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_unlock_texture requires 1 argument (texture_id)");
            visitor.emit_invoke("unlock_texture", args);
        }
        else if (funcName == "sdl_quit_text") {
            if (arguments.size() != 0) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_quit_text requires 0 arguments");
            visitor.emit_invoke("quit_text", {});
        }
        else if (funcName == "sdl_draw_text") {
            if (arguments.size() != 9) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_draw_text requires 9 arguments (renderer_id, font_id, text, x, y, r, g, b, a)");
            visitor.emit_invoke("draw_text", args);
        }
        else if (funcName == "sdl_set_render_target") {
            if (arguments.size() != 2) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_set_render_target requires 2 arguments (renderer_id, target_id)");
            visitor.emit_invoke("set_render_target", args);
        }
        else if (funcName == "sdl_destroy_render_target") {
            if (arguments.size() != 1) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_destroy_render_target requires 1 argument (target_id)");
            visitor.emit_invoke("destroy_render_target", args);
        }
        else if (funcName == "sdl_present_scaled") {
            if (arguments.size() != 4) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_present_scaled requires 4 arguments (renderer_id, target_id, src_width, src_height)");
            visitor.emit_invoke("present_scaled", args);
        }
        else if (funcName == "sdl_present_stretched") {
            if (arguments.size() != 4) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_present_stretched requires 4 arguments (renderer_id, target_id, dst_width, dst_height)");
            visitor.emit_invoke("present_stretched", args);
        }
        else if (funcName == "sdl_lock_texture") {
            if (arguments.size() != 3) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_lock_texture requires 3 arguments (texture_id, pixels_var, pitch_var)");
            visitor.emit_invoke("lock_texture", args);
        }

        freeArgs();
    }

    bool SDLFunctionHandler::generateWithResult(CodeGenVisitor& visitor, const std::string& funcName, const std::vector<std::unique_ptr<ASTNode>>& arguments) {
        visitor.usedModules.insert("sdl");
        
        int lineNum = arguments.empty() ? 1 : arguments[0]->getLineNumber();
        
        std::vector<std::string> args;
        args.reserve(arguments.size());
        for (auto& arg : arguments) args.push_back(visitor.eval(arg.get()));

        auto freeArgs = [&](){
            for (const std::string& a : args) if (visitor.isReg(a)) visitor.freeReg(a);
        };

        auto emitCallWithReturn = [&](const std::string& name)->std::string {
            std::string dst = visitor.allocReg();
            visitor.emit_invoke(name, args);
            visitor.emit("return " + dst);
            return dst;
        };

        if (funcName == "sdl_init") {
            if (arguments.size() != 0) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_init requires 0 arguments");
            std::string r = emitCallWithReturn("init");
            visitor.pushValue(r);
            return true;
        }
        else if (funcName == "sdl_create_window") {
            if (arguments.size() != 6) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_create_window requires 6 arguments (title, x, y, w, h, flags)");
            std::string r = emitCallWithReturn("create_window");
            visitor.pushValue(r);
            freeArgs();
            return true;
        }
        else if (funcName == "sdl_create_renderer") {
            if (arguments.size() != 3) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_create_renderer requires 3 arguments (window_id, index, flags)");
            std::string r = emitCallWithReturn("create_renderer");
            visitor.pushValue(r);
            freeArgs();
            return true;
        }
        else if (funcName == "sdl_poll_event") {
            if (arguments.size() != 0) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_poll_event requires 0 arguments");
            std::string r = emitCallWithReturn("poll_event");
            visitor.pushValue(r);
            return true;
        }
        else if (funcName == "sdl_get_event_type") {
            if (arguments.size() != 0) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_get_event_type requires 0 arguments");
            std::string r = emitCallWithReturn("get_event_type");
            visitor.pushValue(r);
            return true;
        }
        else if (funcName == "sdl_get_key_code") {
            if (arguments.size() != 0) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_get_key_code requires 0 arguments");
            std::string r = emitCallWithReturn("get_key_code");
            visitor.pushValue(r);
            return true;
        }
        else if (funcName == "sdl_get_mouse_x") {
            if (arguments.size() != 0) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_get_mouse_x requires 0 arguments");
            std::string r = emitCallWithReturn("get_mouse_x");
            visitor.pushValue(r);
            return true;
        }
        else if (funcName == "sdl_get_mouse_y") {
            if (arguments.size() != 0) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_get_mouse_y requires 0 arguments");
            std::string r = emitCallWithReturn("get_mouse_y");
            visitor.pushValue(r);
            return true;
        }
        else if (funcName == "sdl_get_mouse_button") {
            if (arguments.size() != 0) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_get_mouse_button requires 0 arguments");
            std::string r = emitCallWithReturn("get_mouse_button");
            visitor.pushValue(r);
            return true;
        }
        else if (funcName == "sdl_get_mouse_buttons") {
            if (arguments.size() != 0) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_get_mouse_buttons requires 0 arguments");
            std::string r = emitCallWithReturn("get_mouse_buttons");
            visitor.pushValue(r);
            return true;
        }
        else if (funcName == "sdl_get_relative_mouse_x") {
            if (arguments.size() != 0) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_get_relative_mouse_x requires 0 arguments");
            std::string r = emitCallWithReturn("get_relative_mouse_x");
            visitor.pushValue(r);
            return true;
        }
        else if (funcName == "sdl_get_relative_mouse_y") {
            if (arguments.size() != 0) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_get_relative_mouse_y requires 0 arguments");
            std::string r = emitCallWithReturn("get_relative_mouse_y");
            visitor.pushValue(r);
            return true;
        }
        else if (funcName == "sdl_get_relative_mouse_buttons") {
            if (arguments.size() != 0) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_get_relative_mouse_buttons requires 0 arguments");
            std::string r = emitCallWithReturn("get_relative_mouse_buttons");
            visitor.pushValue(r);
            return true;
        }
        else if (funcName == "sdl_is_key_pressed") {
            if (arguments.size() != 1) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_is_key_pressed requires 1 argument (scancode)");
            std::string r = emitCallWithReturn("is_key_pressed");
            visitor.pushValue(r);
            freeArgs();
            return true;
        }
        else if (funcName == "sdl_get_num_keys") {
            if (arguments.size() != 0) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_get_num_keys requires 0 arguments");
            std::string r = emitCallWithReturn("get_num_keys");
            visitor.pushValue(r);
            return true;
        }
        else if (funcName == "sdl_get_clipboard_text") {
            if (arguments.size() != 0) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_get_clipboard_text requires 0 arguments");
            std::string r = emitCallWithReturn("get_clipboard_text");
            visitor.pushValue(r);
            return true;
        }
        else if (funcName == "sdl_create_rgb_surface") {
            if (arguments.size() != 3) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_create_rgb_surface requires 3 arguments (width, height, depth)");
            std::string r = emitCallWithReturn("create_rgb_surface");
            visitor.pushValue(r);
            freeArgs();
            return true;
        }
        else if (funcName == "sdl_blit_surface") {
            if (arguments.size() != 4) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_blit_surface requires 4 arguments (src_ptr, dst_ptr, x, y)");
            std::string r = emitCallWithReturn("blit_surface");
            visitor.pushValue(r);
            freeArgs();
            return true;
        }
        else if (funcName == "sdl_create_texture") {
            if (arguments.size() != 5) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_create_texture requires 5 arguments (renderer_id, format, access, w, h)");
            std::string r = emitCallWithReturn("create_texture");
            visitor.pushValue(r);
            freeArgs();
            return true;
        }
        else if (funcName == "sdl_load_texture") {
            if (arguments.size() != 2) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_load_texture requires 2 arguments (renderer_id, file_path)");
            std::string r = emitCallWithReturn("load_texture");
            visitor.pushValue(r);
            freeArgs();
            return true;
        }
        else if (funcName == "sdl_get_ticks") {
            if (arguments.size() != 0) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_get_ticks requires 0 arguments");
            std::string r = emitCallWithReturn("get_ticks");
            visitor.pushValue(r);
            return true;
        }
        else if (funcName == "sdl_open_audio") {
            if (arguments.size() != 4) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_open_audio requires 4 arguments (freq, format, channels, samples)");
            std::string r = emitCallWithReturn("open_audio");
            visitor.pushValue(r);
            freeArgs();
            return true;
        }
        else if (funcName == "sdl_load_wav") {
            if (arguments.size() != 4) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_load_wav requires 4 arguments (file_path, audio_buf_var, audio_len_var, audio_spec_var)");
            std::string r = emitCallWithReturn("load_wav");
            visitor.pushValue(r);
            freeArgs();
            return true;
        }
        else if (funcName == "sdl_get_queued_audio_size") {
            if (arguments.size() != 0) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_get_queued_audio_size requires 0 arguments");
            std::string r = emitCallWithReturn("get_queued_audio_size");
            visitor.pushValue(r);
            return true;
        }
        else if (funcName == "sdl_init_text") {
            if (arguments.size() != 0) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_init_text requires 0 arguments");
            std::string r = emitCallWithReturn("init_text");
            visitor.pushValue(r);
            return true;
        }
        else if (funcName == "sdl_load_font") {
            if (arguments.size() != 2) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_load_font requires 2 arguments (file, ptsize)");
            std::string r = emitCallWithReturn("load_font");
            visitor.pushValue(r);
            freeArgs();
            return true;
        }
        else if (funcName == "sdl_create_render_target") {
            if (arguments.size() != 3) 
                throw std::runtime_error("Error on line " + std::to_string(lineNum) + 
                                        ": sdl_create_render_target requires 3 arguments (renderer_id, width, height)");
            std::string r = emitCallWithReturn("create_render_target");
            visitor.pushValue(r);
            freeArgs();
            return true;
        }

        return false;
    }
}