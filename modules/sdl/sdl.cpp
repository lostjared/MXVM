#include<mxvm/instruct.hpp>
#include<mxvm/icode.hpp>
#include<cstring>
#include<cstdarg>
#include<cstdio>
extern "C" {
#include"mx_sdl.h"
}
// SDL_Init
extern "C" void mxvm_sdl_init(mxvm::Program *program, std::vector<mxvm::Operand> &operand) {
    int64_t result = init();
    program->vars["%rax"].type = mxvm::VarType::VAR_INTEGER;
    program->vars["%rax"].var_value.type = mxvm::VarType::VAR_INTEGER;
    program->vars["%rax"].var_value.int_value = result;
}

// SDL_Quit
extern "C" void mxvm_sdl_quit(mxvm::Program *program, std::vector<mxvm::Operand> &operand) {
   quit();
}

// SDL_CreateWindow
extern "C" void mxvm_sdl_create_window(mxvm::Program *program, std::vector<mxvm::Operand> &operand) {
    if (operand.size() != 6) {
        throw mx::Exception("sdl_create_window requires 6 arguments (title, x, y, w, h, flags).");
    }
    
    std::string title;
    if (program->isVariable(operand[0].op)) {
        mxvm::Variable &var = program->getVariable(operand[0].op);
        if (var.type == mxvm::VarType::VAR_STRING) {
            title = var.var_value.str_value;
        } else if (var.type == mxvm::VarType::VAR_POINTER) {
            title = std::string(reinterpret_cast<const char*>(var.var_value.ptr_value));
        }
    } else {
        title = operand[0].op;
    }
    
    int64_t x = program->isVariable(operand[1].op) ? 
                program->getVariable(operand[1].op).var_value.int_value : operand[1].op_value;
    int64_t y = program->isVariable(operand[2].op) ? 
                program->getVariable(operand[2].op).var_value.int_value : operand[2].op_value;
    int64_t w = program->isVariable(operand[3].op) ? 
                program->getVariable(operand[3].op).var_value.int_value : operand[3].op_value;
    int64_t h = program->isVariable(operand[4].op) ? 
                program->getVariable(operand[4].op).var_value.int_value : operand[4].op_value;
    int64_t flags = program->isVariable(operand[5].op) ? 
                    program->getVariable(operand[5].op).var_value.int_value : operand[5].op_value;
    
    int64_t result =create_window(title.c_str(), x, y, w, h, flags);
    program->vars["%rax"].type = mxvm::VarType::VAR_INTEGER;
    program->vars["%rax"].var_value.type = mxvm::VarType::VAR_INTEGER;
    program->vars["%rax"].var_value.int_value = result;
}

// SDL_DestroyWindow
extern "C" void mxvm_sdl_destroy_window(mxvm::Program *program, std::vector<mxvm::Operand> &operand) {
    if (operand.size() != 1) {
        throw mx::Exception("sdl_destroy_window requires 1 argument (window_id).");
    }
    
    int64_t window_id = program->isVariable(operand[0].op) ? 
                        program->getVariable(operand[0].op).var_value.int_value : operand[0].op_value;
    
   destroy_window(window_id);
}

// SDL_CreateRenderer
extern "C" void mxvm_sdl_create_renderer(mxvm::Program *program, std::vector<mxvm::Operand> &operand) {
    if (operand.size() != 3) {
        throw mx::Exception("sdl_create_renderer requires 3 arguments (window_id, index, flags).");
    }
    
    int64_t window_id = program->isVariable(operand[0].op) ? 
                        program->getVariable(operand[0].op).var_value.int_value : operand[0].op_value;
    int64_t index = program->isVariable(operand[1].op) ? 
                    program->getVariable(operand[1].op).var_value.int_value : operand[1].op_value;
    int64_t flags = program->isVariable(operand[2].op) ? 
                    program->getVariable(operand[2].op).var_value.int_value : operand[2].op_value;
    
    int64_t result = create_renderer(window_id, index, flags);
    program->vars["%rax"].type = mxvm::VarType::VAR_INTEGER;
    program->vars["%rax"].var_value.type = mxvm::VarType::VAR_INTEGER;
    program->vars["%rax"].var_value.int_value = result;
}

// SDL_DestroyRenderer
extern "C" void mxvm_sdl_destroy_renderer(mxvm::Program *program, std::vector<mxvm::Operand> &operand) {
    if (operand.size() != 1) {
        throw mx::Exception("sdl_destroy_renderer requires 1 argument (renderer_id).");
    }
    
    int64_t renderer_id = program->isVariable(operand[0].op) ? 
                          program->getVariable(operand[0].op).var_value.int_value : operand[0].op_value;
    
   destroy_renderer(renderer_id);
}

// SDL_PollEvent
extern "C" void mxvm_sdl_poll_event(mxvm::Program *program, std::vector<mxvm::Operand> &operand) {
    int64_t result = poll_event();
    program->vars["%rax"].type = mxvm::VarType::VAR_INTEGER;
    program->vars["%rax"].var_value.type = mxvm::VarType::VAR_INTEGER;
    program->vars["%rax"].var_value.int_value = result;
}

// SDL_GetEventType
extern "C" void mxvm_sdl_get_event_type(mxvm::Program *program, std::vector<mxvm::Operand> &operand) {
    int64_t result =get_event_type();
    program->vars["%rax"].type = mxvm::VarType::VAR_INTEGER;
    program->vars["%rax"].var_value.type = mxvm::VarType::VAR_INTEGER;
    program->vars["%rax"].var_value.int_value = result;
}

// SDL_GetKeyCode
extern "C" void mxvm_sdl_get_key_code(mxvm::Program *program, std::vector<mxvm::Operand> &operand) {
    int64_t result = get_key_code();
    program->vars["%rax"].type = mxvm::VarType::VAR_INTEGER;
    program->vars["%rax"].var_value.type = mxvm::VarType::VAR_INTEGER;
    program->vars["%rax"].var_value.int_value = result;
}

// SDL_GetMouseX
extern "C" void mxvm_sdl_get_mouse_x(mxvm::Program *program, std::vector<mxvm::Operand> &operand) {
    int64_t result = get_mouse_x();
    program->vars["%rax"].type = mxvm::VarType::VAR_INTEGER;
    program->vars["%rax"].var_value.type = mxvm::VarType::VAR_INTEGER;
    program->vars["%rax"].var_value.int_value = result;
}

// SDL_GetMouseY
extern "C" void mxvm_sdl_get_mouse_y(mxvm::Program *program, std::vector<mxvm::Operand> &operand) {
    int64_t result = get_mouse_y();
    program->vars["%rax"].type = mxvm::VarType::VAR_INTEGER;
    program->vars["%rax"].var_value.type = mxvm::VarType::VAR_INTEGER;
    program->vars["%rax"].var_value.int_value = result;
}

// SDL_GetMouseButton
extern "C" void mxvm_sdl_get_mouse_button(mxvm::Program *program, std::vector<mxvm::Operand> &operand) {
    int64_t result = get_mouse_button();
    program->vars["%rax"].type = mxvm::VarType::VAR_INTEGER;
    program->vars["%rax"].var_value.type = mxvm::VarType::VAR_INTEGER;
    program->vars["%rax"].var_value.int_value = result;
}

// SDL_SetDrawColor
extern "C" void mxvm_sdl_set_draw_color(mxvm::Program *program, std::vector<mxvm::Operand> &operand) {
    if (operand.size() != 5) {
        throw mx::Exception("sdl_set_draw_color requires 5 arguments (renderer_id, r, g, b, a).");
    }
    
    int64_t renderer_id = program->isVariable(operand[0].op) ? 
                          program->getVariable(operand[0].op).var_value.int_value : operand[0].op_value;
    int64_t r = program->isVariable(operand[1].op) ? 
                program->getVariable(operand[1].op).var_value.int_value : operand[1].op_value;
    int64_t g = program->isVariable(operand[2].op) ? 
                program->getVariable(operand[2].op).var_value.int_value : operand[2].op_value;
    int64_t b = program->isVariable(operand[3].op) ? 
                program->getVariable(operand[3].op).var_value.int_value : operand[3].op_value;
    int64_t a = program->isVariable(operand[4].op) ? 
                program->getVariable(operand[4].op).var_value.int_value : operand[4].op_value;
    
   set_draw_color(renderer_id, r, g, b, a);
}

// SDL_Clear
extern "C" void mxvm_sdl_clear(mxvm::Program *program, std::vector<mxvm::Operand> &operand) {
    if (operand.size() != 1) {
        throw mx::Exception("sdl_clear requires 1 argument (renderer_id).");
    }
    
    int64_t renderer_id = program->isVariable(operand[0].op) ? 
                          program->getVariable(operand[0].op).var_value.int_value : operand[0].op_value;
    
   clear(renderer_id);
}

// SDL_Present
extern "C" void mxvm_sdl_present(mxvm::Program *program, std::vector<mxvm::Operand> &operand) {
    if (operand.size() != 1) {
        throw mx::Exception("sdl_present requires 1 argument (renderer_id).");
    }
    
    int64_t renderer_id = program->isVariable(operand[0].op) ? 
                          program->getVariable(operand[0].op).var_value.int_value : operand[0].op_value;
    
   present(renderer_id);
}

// SDL_DrawPoint
extern "C" void mxvm_sdl_draw_point(mxvm::Program *program, std::vector<mxvm::Operand> &operand) {
    if (operand.size() != 3) {
        throw mx::Exception("sdl_draw_point requires 3 arguments (renderer_id, x, y).");
    }
    
    int64_t renderer_id = program->isVariable(operand[0].op) ? 
                          program->getVariable(operand[0].op).var_value.int_value : operand[0].op_value;
    int64_t x = program->isVariable(operand[1].op) ? 
                program->getVariable(operand[1].op).var_value.int_value : operand[1].op_value;
    int64_t y = program->isVariable(operand[2].op) ? 
                program->getVariable(operand[2].op).var_value.int_value : operand[2].op_value;
    
   draw_point(renderer_id, x, y);
}

// SDL_DrawLine
extern "C" void mxvm_sdl_draw_line(mxvm::Program *program, std::vector<mxvm::Operand> &operand) {
    if (operand.size() != 5) {
        throw mx::Exception("sdl_draw_line requires 5 arguments (renderer_id, x1, y1, x2, y2).");
    }
    
    int64_t renderer_id = program->isVariable(operand[0].op) ? 
                          program->getVariable(operand[0].op).var_value.int_value : operand[0].op_value;
    int64_t x1 = program->isVariable(operand[1].op) ? 
                 program->getVariable(operand[1].op).var_value.int_value : operand[1].op_value;
    int64_t y1 = program->isVariable(operand[2].op) ? 
                 program->getVariable(operand[2].op).var_value.int_value : operand[2].op_value;
    int64_t x2 = program->isVariable(operand[3].op) ? 
                 program->getVariable(operand[3].op).var_value.int_value : operand[3].op_value;
    int64_t y2 = program->isVariable(operand[4].op) ? 
                 program->getVariable(operand[4].op).var_value.int_value : operand[4].op_value;
    
   draw_line(renderer_id, x1, y1, x2, y2);
}

// SDL_DrawRect
extern "C" void mxvm_sdl_draw_rect(mxvm::Program *program, std::vector<mxvm::Operand> &operand) {
    if (operand.size() != 5) {
        throw mx::Exception("sdl_draw_rect requires 5 arguments (renderer_id, x, y, w, h).");
    }
    
    int64_t renderer_id = program->isVariable(operand[0].op) ? 
                          program->getVariable(operand[0].op).var_value.int_value : operand[0].op_value;
    int64_t x = program->isVariable(operand[1].op) ? 
                program->getVariable(operand[1].op).var_value.int_value : operand[1].op_value;
    int64_t y = program->isVariable(operand[2].op) ? 
                program->getVariable(operand[2].op).var_value.int_value : operand[2].op_value;
    int64_t w = program->isVariable(operand[3].op) ? 
                program->getVariable(operand[3].op).var_value.int_value : operand[3].op_value;
    int64_t h = program->isVariable(operand[4].op) ? 
                program->getVariable(operand[4].op).var_value.int_value : operand[4].op_value;
    
   draw_rect(renderer_id, x, y, w, h);
}

// SDL_FillRect
extern "C" void mxvm_sdl_fill_rect(mxvm::Program *program, std::vector<mxvm::Operand> &operand) {
    if (operand.size() != 5) {
        throw mx::Exception("sdl_fill_rect requires 5 arguments (renderer_id, x, y, w, h).");
    }
    
    int64_t renderer_id = program->isVariable(operand[0].op) ? 
                          program->getVariable(operand[0].op).var_value.int_value : operand[0].op_value;
    int64_t x = program->isVariable(operand[1].op) ? 
                program->getVariable(operand[1].op).var_value.int_value : operand[1].op_value;
    int64_t y = program->isVariable(operand[2].op) ? 
                program->getVariable(operand[2].op).var_value.int_value : operand[2].op_value;
    int64_t w = program->isVariable(operand[3].op) ? 
                program->getVariable(operand[3].op).var_value.int_value : operand[3].op_value;
    int64_t h = program->isVariable(operand[4].op) ? 
                program->getVariable(operand[4].op).var_value.int_value : operand[4].op_value;
    
   fill_rect(renderer_id, x, y, w, h);
}

// SDL_CreateTexture
extern "C" void mxvm_sdl_create_texture(mxvm::Program *program, std::vector<mxvm::Operand> &operand) {
    if (operand.size() != 5) {
        throw mx::Exception("sdl_create_texture requires 5 arguments (renderer_id, format, access, w, h).");
    }
    
    int64_t renderer_id = program->isVariable(operand[0].op) ? 
                          program->getVariable(operand[0].op).var_value.int_value : operand[0].op_value;
    int64_t format = program->isVariable(operand[1].op) ? 
                     program->getVariable(operand[1].op).var_value.int_value : operand[1].op_value;
    int64_t access = program->isVariable(operand[2].op) ? 
                     program->getVariable(operand[2].op).var_value.int_value : operand[2].op_value;
    int64_t w = program->isVariable(operand[3].op) ? 
                program->getVariable(operand[3].op).var_value.int_value : operand[3].op_value;
    int64_t h = program->isVariable(operand[4].op) ? 
                program->getVariable(operand[4].op).var_value.int_value : operand[4].op_value;
    
    int64_t result =create_texture(renderer_id, format, access, w, h);
    program->vars["%rax"].type = mxvm::VarType::VAR_INTEGER;
    program->vars["%rax"].var_value.type = mxvm::VarType::VAR_INTEGER;
    program->vars["%rax"].var_value.int_value = result;
}

// SDL_DestroyTexture
extern "C" void mxvm_sdl_destroy_texture(mxvm::Program *program, std::vector<mxvm::Operand> &operand) {
    if (operand.size() != 1) {
        throw mx::Exception("sdl_destroy_texture requires 1 argument (texture_id).");
    }
    
    int64_t texture_id = program->isVariable(operand[0].op) ? 
                         program->getVariable(operand[0].op).var_value.int_value : operand[0].op_value;
    
   destroy_texture(texture_id);
}

// SDL_LoadTexture
extern "C" void mxvm_sdl_load_texture(mxvm::Program *program, std::vector<mxvm::Operand> &operand) {
    if (operand.size() != 2) {
        throw mx::Exception("sdl_load_texture requires 2 arguments (renderer_id, file_path).");
    }
    
    int64_t renderer_id = program->isVariable(operand[0].op) ? 
                          program->getVariable(operand[0].op).var_value.int_value : operand[0].op_value;
    
    std::string file_path;
    if (program->isVariable(operand[1].op)) {
        mxvm::Variable &var = program->getVariable(operand[1].op);
        if (var.type == mxvm::VarType::VAR_STRING) {
            file_path = var.var_value.str_value;
        } else if (var.type == mxvm::VarType::VAR_POINTER) {
            file_path = std::string(reinterpret_cast<const char*>(var.var_value.ptr_value));
        }
    } else {
        file_path = operand[1].op;
    }
    
    int64_t result =load_texture(renderer_id, file_path.c_str());
    program->vars["%rax"].type = mxvm::VarType::VAR_INTEGER;
    program->vars["%rax"].var_value.type = mxvm::VarType::VAR_INTEGER;
    program->vars["%rax"].var_value.int_value = result;
}

// SDL_RenderTexture
extern "C" void mxvm_sdl_render_texture(mxvm::Program *program, std::vector<mxvm::Operand> &operand) {
    if (operand.size() != 10) {
        throw mx::Exception("sdl_render_texture requires 10 arguments (renderer_id, texture_id, src_x, src_y, src_w, src_h, dst_x, dst_y, dst_w, dst_h).");
    }
    
    int64_t renderer_id = program->isVariable(operand[0].op) ? 
                          program->getVariable(operand[0].op).var_value.int_value : operand[0].op_value;
    int64_t texture_id = program->isVariable(operand[1].op) ? 
                         program->getVariable(operand[1].op).var_value.int_value : operand[1].op_value;
    int64_t src_x = program->isVariable(operand[2].op) ? 
                    program->getVariable(operand[2].op).var_value.int_value : operand[2].op_value;
    int64_t src_y = program->isVariable(operand[3].op) ? 
                    program->getVariable(operand[3].op).var_value.int_value : operand[3].op_value;
    int64_t src_w = program->isVariable(operand[4].op) ? 
                    program->getVariable(operand[4].op).var_value.int_value : operand[4].op_value;
    int64_t src_h = program->isVariable(operand[5].op) ? 
                    program->getVariable(operand[5].op).var_value.int_value : operand[5].op_value;
    int64_t dst_x = program->isVariable(operand[6].op) ? 
                    program->getVariable(operand[6].op).var_value.int_value : operand[6].op_value;
    int64_t dst_y = program->isVariable(operand[7].op) ? 
                    program->getVariable(operand[7].op).var_value.int_value : operand[7].op_value;
    int64_t dst_w = program->isVariable(operand[8].op) ? 
                    program->getVariable(operand[8].op).var_value.int_value : operand[8].op_value;
    int64_t dst_h = program->isVariable(operand[9].op) ? 
                    program->getVariable(operand[9].op).var_value.int_value : operand[9].op_value;
    
   render_texture(renderer_id, texture_id, src_x, src_y, src_w, src_h, dst_x, dst_y, dst_w, dst_h);
}

// SDL_OpenAudio
extern "C" void mxvm_sdl_open_audio(mxvm::Program *program, std::vector<mxvm::Operand> &operand) {
    if (operand.size() != 4) {
        throw mx::Exception("sdl_open_audio requires 4 arguments (frequency, format, channels, samples).");
    }
    
    int64_t frequency = program->isVariable(operand[0].op) ? 
                        program->getVariable(operand[0].op).var_value.int_value : operand[0].op_value;
    int64_t format = program->isVariable(operand[1].op) ? 
                     program->getVariable(operand[1].op).var_value.int_value : operand[1].op_value;
    int64_t channels = program->isVariable(operand[2].op) ? 
                       program->getVariable(operand[2].op).var_value.int_value : operand[2].op_value;
    int64_t samples = program->isVariable(operand[3].op) ? 
                      program->getVariable(operand[3].op).var_value.int_value : operand[3].op_value;
    
    int64_t result =open_audio(frequency, format, channels, samples);
    program->vars["%rax"].type = mxvm::VarType::VAR_INTEGER;
    program->vars["%rax"].var_value.type = mxvm::VarType::VAR_INTEGER;
    program->vars["%rax"].var_value.int_value = result;
}

// SDL_CloseAudio
extern "C" void mxvm_sdl_close_audio(mxvm::Program *program, std::vector<mxvm::Operand> &operand) {
   close_audio();
}

// SDL_PauseAudio
extern "C" void mxvm_sdl_pause_audio(mxvm::Program *program, std::vector<mxvm::Operand> &operand) {
    if (operand.size() != 1) {
        throw mx::Exception("sdl_pause_audio requires 1 argument (pause_on).");
    }
    
    int64_t pause_on = program->isVariable(operand[0].op) ? 
                       program->getVariable(operand[0].op).var_value.int_value : operand[0].op_value;
    
   pause_audio(pause_on);
}

// SDL_GetTicks
extern "C" void mxvm_sdl_get_ticks(mxvm::Program *program, std::vector<mxvm::Operand> &operand) {
    int64_t result =get_ticks();
    program->vars["%rax"].type = mxvm::VarType::VAR_INTEGER;
    program->vars["%rax"].var_value.type = mxvm::VarType::VAR_INTEGER;
    program->vars["%rax"].var_value.int_value = result;
}

// SDL_Delay
extern "C" void mxvm_sdl_delay(mxvm::Program *program, std::vector<mxvm::Operand> &operand) {
    if (operand.size() != 1) {
        throw mx::Exception("sdl_delay requires 1 argument (ms).");
    }
    
    int64_t ms = program->isVariable(operand[0].op) ? 
                 program->getVariable(operand[0].op).var_value.int_value : operand[0].op_value;
    
   delay(ms);
}