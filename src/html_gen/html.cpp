#include<iostream>
#include<fstream>
#include<sstream>
#include"mxvm/html_gen.hpp"

int main(int argc, char **argv) {
    if(argc != 3) {
        std::cerr << "mxvm-html: requires 2 arguments.\n";
        exit(EXIT_FAILURE);
    }
    std::ostringstream stream;
    std::fstream file;
    file.open(argv[1], std::ios::in);
    if(!file.is_open()) {
        std::cerr << "mxvm-html: could not open file: " << argv[1] << "\n";
        exit(EXIT_FAILURE);
    }
    stream << file.rdbuf();
    mxvm::HTMLGen gen(stream.str());
    std::fstream ofile;
    ofile.open(argv[2], std::ios::out);
    if(!ofile.is_open()) {
        std::cerr << "mxvm-html: could not open output file: "<< argv[2] << "\n";
    }
    std::ostringstream output;
    gen.output(output, argv[1]);
    std::cout << output.str();
    ofile << output.str();
    ofile.close();
    file.close();
    return EXIT_SUCCESS;
}