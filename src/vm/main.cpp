#include<mxvm/mxvm.hpp>
#include"argz.hpp"
#include<iostream>
#include<string>
#include<string_view>

enum class vm_action { null_action = 0, translate , interpret };
enum class vm_target { x86_64_linux };

struct Args {
    std::string source_file;
    std::string output_file;
    vm_action action = vm_action::null_action;
    vm_target target = vm_target::x86_64_linux;
};

void process_arguments(Args *args);
void action_translate(std::string_view input, std::string_view output, vm_target &target);
void action_interpret(std::string_view input);
void translate_x64_linux(std::string_view input, std::string_view output);

Args proc_args(int argc, char **argv) {
    Args args;
    mx::Argz<std::string> argz(argc, argv);

    argz.addOptionSingleValue('o', "output file")
    .addOptionSingleValue('a', "action")
    .addOptionDoubleValue(128, "action", "action to take [translate,  interpret]")
    .addOptionSingleValue('t', "target")
    .addOptionDoubleValue(129, "target", "output target")
    ;
    mx::Argument<std::string> arg;
    int value = 0;
    try {
        while((value = argz.proc(arg)) != -1) {
            switch(value) {
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
    return args;
}


int main(int argc, char **argv) {
    Args args = proc_args(argc, argv);
    process_arguments(&args);
    return 0;
}

void process_arguments(Args *args) { 
    if(args->action == vm_action::translate && !args->output_file.empty()) {
        action_translate(args->source_file, args->output_file, args->target);
    } else if(args->action == vm_action::interpret && !args->source_file.empty()) {
        action_interpret(args->source_file);
    } else if(args->action == vm_action::null_action && !args->source_file.empty()) {
        action_interpret(args->source_file);
    } else {
        std::cerr << "Error invalid action/command\n";
        exit(EXIT_FAILURE);
    }
}

void action_translate(std::string_view input, std::string_view output, vm_target &target) {
    switch(target) {
        case vm_target::x86_64_linux:
            translate_x64_linux(input, output);
        break;
    }
}

void translate_x64_linux(std::string_view input, std::string_view output) {

}

void action_interpret(std::string_view input) {

}

