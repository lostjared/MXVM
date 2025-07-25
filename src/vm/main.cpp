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
#include<vector>

enum class vm_action { null_action = 0, translate , interpret };
enum class vm_target { x86_64_linux };

struct Args {
    std::string source_file;
    std::string output_file;
    std::string module_path;
    std::string object_path = ".";
    std::string include_path = "/usr/local/include/mxvm/modules";
    vm_action action = vm_action::null_action;
    vm_target target = vm_target::x86_64_linux;
    std::vector<std::string> argv;
    bool object = false;
};

void process_arguments(Args *args);
int action_translate(std::string_view include_path, std::string_view object_path, bool object, std::string_view input, std::string_view mod_path, std::string_view output, vm_target &target);
int action_interpret(std::string_view include_path, std::string_view object_path, const std::vector<std::string> &argv, std::string_view input, std::string_view mod_path);
int translate_x64_linux(std::string_view include_path, std::string_view object_path, bool object, std::string_view input, std::string_view mod_path, std::string_view output);

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
    .addOptionSingle('c', "object")
    .addOptionDouble(135, "object", "File contains object")
    .addOptionSingleValue('x', "object path")
    .addOptionDoubleValue(136, "object-path", "Object path")
    .addOptionSingleValue('p', "path")
    .addOptionDoubleValue(134, "path", "module path")
    .addOptionSingleValue('I', "include path")
    .addOptionDoubleValue(137, "include", "include path");
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
                case 'I':
                case 137:
                    args.include_path = arg.arg_value;
                break;
                case 'x':
                case 136:
                    args.object_path = arg.arg_value;
                break;
                case 'c':
                case 135:
                    args.object = true;
                    break;
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
        std::cerr << "MXVM: Sytnax Error: "<< exec.text() << "\n";
        exit(EXIT_FAILURE);
    }

    if(args.source_file.empty()) {
        std::cerr << "MXVM: Error source file required..\n";
        exit(EXIT_FAILURE);
    }
    if(args.module_path.empty()) {
        std::cerr << "MXVM: Errror: please set module path.\n";
        exit(EXIT_FAILURE);
    }


    bool add = false;
    for(int i = 0; i < argc; ++i) {
        if(std::string(argv[i]) == "--args") {
            add = true; 
            continue;
        }
        if(add == true) {
            args.argv.push_back(argv[i]);
        }
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
        std::cerr << "MXVM: Error: input file: " << args->source_file << " does not exist or is not regular file.\n";
        exit(EXIT_FAILURE);
    }
    if(args->action == vm_action::translate) {
        exitCode = action_translate(args->include_path, args->object_path, args->object, args->source_file, args->module_path, args->output_file, args->target);
    } else if(args->action == vm_action::interpret && !args->source_file.empty()) {
        exitCode = action_interpret(args->include_path, args->object_path, args->argv, args->source_file, args->module_path);
    } else if(args->action == vm_action::null_action && !args->source_file.empty()) {
        exitCode = action_interpret(args->include_path, args->object_path, args->argv, args->source_file, args->module_path);
    } else {
        std::cerr << "MXVM: Error invalid action/command\n";
        exit(EXIT_FAILURE);
    }
    exit(exitCode);
}

int action_translate(std::string_view include_path, std::string_view object_path, bool object, std::string_view input, std::string_view mod_path, std::string_view output, vm_target &target) {
    switch(target) {
        case vm_target::x86_64_linux:
            return translate_x64_linux(include_path, object_path, object, input, mod_path, output);
        break;
    }
    return 0;
}

int translate_x64_linux(std::string_view include_path, std::string_view object_path, bool object, std::string_view input, std::string_view mod_path, std::string_view output) {
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
        program->setObject(false);
        parser.module_path = std::string(mod_path);
        parser.object_path = std::string(object_path);
        parser.object_mode = object;
        parser.include_path =  include_path;
        if(parser.generateProgramCode(object, mxvm::Mode::MODE_COMPILE, program)) {
            std::string output_file(output);
            std::string program_name = output_file.empty() ? program->name + ".s" : output_file;
            std::fstream file;
            file.open(program_name, std::ios::out);
            if(file.is_open()) {
                program->generateCode(object, file);
                file.close();
                std::string mainFunc = " Object";
                if(object == false) {
                    mainFunc = " Program";
                }
                std::cout << "MXVM: Compiled: " << program_name << mainFunc << "\n";
            }
        } else {
            std::cerr << "MXVM: Error: Failed to generate intermediate code.\n";
            exit(EXIT_FAILURE);
        }
    } catch(const mx::Exception &e) {
        std::cerr << "MXVM: Exception: " << e.what() << "\n";
        return EXIT_FAILURE;
    } catch(const std::runtime_error &e) {
        std::cerr << "MXVM: Runtime Error: " << e.what() << "\n";
        return EXIT_FAILURE;
    } catch(const std::exception &e) {
        std::cerr << "MXVM: Exception: " << e.what() << "\n";
        return EXIT_FAILURE;
    }
    catch(...) {
        std::cerr << "MXVM: Unknown Exception.\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

mxvm::Program *signal_program = nullptr;

void signal_action(int signum) {
    if(signum == SIGINT) {
        std::cout << "MXVM: Signal SIGINT Received Exiting...\n";
        if(mxvm::debug_mode) {
            std::cout << "MXVM: Debug Mode Dumping Memory.\n";
            if(signal_program != nullptr)
                signal_program->memoryDump(std::cout);
        }
        exit(EXIT_FAILURE);
    }
}

 int action_interpret(std::string_view include_path, std::string_view object_path, const std::vector<std::string> &argv, std::string_view input, std::string_view mod_path) {
    int exitCode = 0;
    std::unique_ptr<mxvm::Program> program(new mxvm::Program());
    program->setArgs(argv);
    std::fstream debug_output;
    if(mxvm::debug_mode) {
        debug_output.open("debug_info.txt", std::ios::out);
        if(!debug_output.is_open()) {
            std::cerr << "MXVM: Error could not open debug_info.txt\n";
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
        parser.object_path = std::string(object_path);
        parser.include_path = std::string(include_path);
        if(parser.generateProgramCode(false, mxvm::Mode::MODE_INTERPRET, program)) {
            program->flatten(program.get());
            exitCode = program->exec();
            
            if(mxvm::debug_mode) {
                if(debug_output.is_open()) {
                    program->print(debug_output);
                    program->post(debug_output);
                    program->memoryDump(debug_output);
                }
            }
        } else {
            std::cerr << "MXVM: Error: Failed to generate intermediate code.\n";
            exit(EXIT_FAILURE);
        }
    } catch(const mx::Exception &e) {
        std::cerr << "MXVM: Exception: "<< e.what() << "\n";
        if(mxvm::debug_mode) {
            if(debug_output.is_open())
                program->memoryDump(debug_output);
        }
    } catch(const std::runtime_error &e) {
        std::cerr << "MXVM: Runtime Error: " << e.what() << "\n";
        if(mxvm::debug_mode) {
            if(debug_output.is_open())
                program->memoryDump(debug_output);
        }
    } catch(const std::exception &e) {
        std::cerr << "MXVM: Exception: " << e.what() << "\n";
        if(mxvm::debug_mode) {
            if(debug_output.is_open())
                program->memoryDump(debug_output);
        }
    }  
    catch(...) {
        std::cerr << "MXVM: Unknown Exception.\n";
        if(mxvm::debug_mode) {
            if(debug_output.is_open())
                program->memoryDump(debug_output);
        }
    }

    if(mxvm::debug_mode) {
        if(debug_output.is_open()) {
            debug_output.close();
            std::cout << "MXVM: Generated debug information: debug_info.txt\n";
        }
    }
    if(mxvm::debug_mode) {
        std::cout << "MXVM: Program exited with: " << exitCode << "\n";
    }
    return exitCode;
}

