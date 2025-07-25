#include<mxvm/mxvm.hpp>
#include"argz.hpp"
#include<iostream>
#include<fstream>
#include<sstream>
#include<string>
#include<string_view>
#include<memory>
#include<filesystem>
#include<csignal>

enum class vm_action { null_action = 0, translate , interpret };
enum class vm_target { x86_64_linux };

struct Args {
    std::string source_file;
    std::string output_file;
    std::string module_path;
    vm_action action = vm_action::null_action;
    vm_target target = vm_target::x86_64_linux;
};

void process_arguments(Args *args);
void action_translate(std::string_view input, std::string_view mod_path, std::string_view output, vm_target &target);
int action_interpret(std::string_view input, std::string_view mod_path);
void translate_x64_linux(std::string_view input, std::string_view mod_path, std::string_view output);

Args proc_args(int argc, char **argv) {
    Args args;
    mx::Argz<std::string> argz(argc, argv);

    argz.addOptionSingleValue('o', "output file")
    .addOptionSingleValue('a', "action")
    .addOptionDoubleValue(128, "action", "action to take [translate,  interpret]")
    .addOptionSingleValue('t', "target")
    .addOptionDoubleValue(129, "target", "output target")
    .addOptionSingle('d', "debug mode")
    .addOptionDouble(130, "debug", "debug mode")
    .addOptionSingle('i', "instruction mode")
    .addOptionDouble(131, "instruction", "instruction output on")
    .addOptionSingle('h', "Help")
    .addOptionDouble(132, "help", "Help output")
    .addOptionDouble(133, "html", "debug html")
    .addOptionSingle('D', "print html")
    .addOptionSingleValue('p', "path")
    .addOptionDoubleValue(134, "path", "module path")
    ;

    if(argc == 1) {
        argz.help(std::cout);
        exit(EXIT_SUCCESS);
    }

    mx::Argument<std::string> arg;
    int value = 0;
    try {
        while((value = argz.proc(arg)) != -1) {
            switch(value) {
                case 134:
                case 'p':
                    args.module_path = arg.arg_value;
                break;
                case 133:
                case 'D':
                    mxvm::html_mode = true;
                    break;
                case 'h':
                case 132:
                    argz.help(std::cout);
                    exit(EXIT_SUCCESS);
                break;
                case 'i':
                case 131:
                    mxvm::instruct_mode = true;
                break;
                case 'd':
                case 130:
                    mxvm::debug_mode = true;
                break;
                case 'o':
                    args.output_file = arg.arg_value;
                break;
                case 'a':
                case 128:
                    if(arg.arg_value == "translate") {
                        args.action = vm_action::translate;
                    } else if(arg.arg_value == "interpret") {
                        args.action = vm_action::interpret;
                    } else {
                        throw mx::ArgException<std::string>("Error invalid action value");
                    }
                break;
                case 't':
                case 129:
                // set target
                break;
                default:
                case '-':
                    args.source_file = arg.arg_value;
                break;
            }
        }
    } catch(mx::ArgException<std::string> &exec) {
        std::cerr << "Sytnax Error: "<< exec.text() << "\n";
        exit(EXIT_FAILURE);
    }

    if(args.source_file.empty()) {
        std::cerr << "Error source file required..\n";
        exit(EXIT_FAILURE);
    }
    if(args.module_path.empty()) {
        std::cerr << "Errror: please set module path.\n";
        exit(EXIT_FAILURE);
    }
    return args;
}


int main(int argc, char **argv) {
    Args args = proc_args(argc, argv);


    process_arguments(&args);
    return 0;
}

void process_arguments(Args *args) { 
    int exitCode = 0;
    if(!std::filesystem::is_regular_file(args->source_file) || !std::filesystem::exists(args->source_file)) {
        std::cerr << "Error: input file: " << args->source_file << " does not exist or is not regular file.\n";
        exit(EXIT_FAILURE);
    }
    if(args->action == vm_action::translate) {
        action_translate(args->source_file, args->module_path, args->output_file, args->target);
    } else if(args->action == vm_action::interpret && !args->source_file.empty()) {
        exitCode = action_interpret(args->source_file, args->module_path);
    } else if(args->action == vm_action::null_action && !args->source_file.empty()) {
        exitCode = action_interpret(args->source_file, args->module_path);
    } else {
        std::cerr << "Error invalid action/command\n";
        exit(EXIT_FAILURE);
    }
    if(mxvm::debug_mode) {
        std::cout << "Program exited with: " << exitCode << "\n";
    }
    exit(exitCode);
}

void action_translate(std::string_view input, std::string_view mod_path, std::string_view output, vm_target &target) {
    switch(target) {
        case vm_target::x86_64_linux:
            translate_x64_linux(input, mod_path, output);
        break;
    }
}

void translate_x64_linux(std::string_view input, std::string_view mod_path, std::string_view output) {
    try {
        std::string input_file(input);
        std::fstream file;
        file.open(input_file, std::ios::in);
        if(!file.is_open()) {
            throw mx::Exception("Error could not open file: " + input_file);
        }
        std::ostringstream stream;
        stream << file.rdbuf();
        file.close();
        mxvm::Parser parser(stream.str());
        parser.scan();
        std::unique_ptr<mxvm::Program> program(new mxvm::Program());
        parser.module_path = std::string(mod_path);
        if(parser.generateProgramCode(mxvm::Mode::MODE_COMPILE, program)) {
            std::string output_file(output);
            std::string program_name = output_file.empty() ? program->name + ".s" : output_file;
            std::fstream file;
            file.open(program_name, std::ios::out);
            if(file.is_open()) {
                program->generateCode(file);
                file.close();
                std::cout << "translated: " << program_name << "\n";
            }
        } else {
            std::cerr << "Error: Failed to generate intermediate code.\n";
            exit(EXIT_FAILURE);
        }
    } catch(const mx::Exception &e) {
        std::cerr << "Exception: " << e.what() << "\n";
    } catch(const std::runtime_error &e) {
        std::cerr << "Runtime Error: " << e.what() << "\n";
    }
}

mxvm::Program *signal_program = nullptr;

void signal_action(int signum) {
    if(signum == SIGINT) {
        std::cout << "MXVM: Signal SIGINT Recived Exiting...\n";
        if(mxvm::debug_mode) {
            std::cout << "MXVM: Debug Mode Dumping Memory.\n";
            if(signal_program != nullptr)
                signal_program->memoryDump(std::cout);
        }
        exit(EXIT_FAILURE);
    }
}

 int action_interpret(std::string_view input, std::string_view mod_path) {
    int exitCode = 0;
    std::unique_ptr<mxvm::Program> program(new mxvm::Program());
    std::fstream debug_output;

    if(mxvm::debug_mode) {
        debug_output.open("debug_info.txt", std::ios::out);
        if(!debug_output.is_open()) {
            std::cerr << "Error could not open debug_info.txt\n";
        }
    
    }

    signal_program =  program.get();
    struct sigaction sa;
    sa.sa_handler = signal_action;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);

    try {
        std::string input_file(input);
        std::fstream file;
        file.open(input_file, std::ios::in);
        if(!file.is_open()) {
            throw mx::Exception("Error could not open file: " + input_file);
        }
        std::ostringstream stream;
        stream << file.rdbuf();
        file.close();
        mxvm::Parser parser(stream.str());
        parser.scan();
        
        parser.module_path = std::string(mod_path);
        if(parser.generateProgramCode(mxvm::Mode::MODE_INTERPRET, program)) {
            if(mxvm::debug_mode && debug_output.is_open()) program->print(debug_output);
            exitCode = program->exec();
            if(mxvm::debug_mode && debug_output.is_open()) program->post(debug_output);
            if(mxvm::debug_mode) {
                if(debug_output.is_open())
                    program->memoryDump(debug_output);
            }
        } else {
            std::cerr << "Error: Failed to generate intermediate code.\n";
            exit(EXIT_FAILURE);
        }
    } catch(const mx::Exception &e) {
        std::cerr << "Exception: "<< e.what() << "\n";
        if(mxvm::debug_mode) {
            if(debug_output.is_open())
                program->memoryDump(debug_output);
        }
    } catch(const std::runtime_error &e) {
        std::cerr << "Runtime Error: " << e.what() << "\n";
        if(mxvm::debug_mode) {
            if(debug_output.is_open())
                program->memoryDump(debug_output);
        }
    } 

    if(mxvm::debug_mode) {
        if(debug_output.is_open()) {
            debug_output.close();
            std::cout << "Generated debug information: debug_info.txt\n";
        }
    }

    return exitCode;
}

