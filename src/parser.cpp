#include"mxvm/parser.hpp"
#include"scanner/exception.hpp"
#include"scanner/scanner.hpp"

namespace mxvm {
    Parser::Parser(const std::string &source) : source_file(source), scanner(source)  {

    }
    uint64_t  Parser::scan() {
        try {
            scanner.scan();
        } catch(scan::ScanExcept &e){
            std::cerr << "Scanner error: " << e.why() << "\n";
        }
        return scanner.size();
    }

    auto Parser::operator[](size_t pos) {
        if(pos < scanner.size()) {
            return scanner[pos];
        }
        throw mx::Exception("Error: scanner index out of bounds.\n");
        return scanner[0];
    }

    void Parser::parse() {
        for(uint64_t i = 0; i < scanner.size(); ++i) {
            auto t = this->operator[](i);
            if(t.getTokenValue() != "\n")
            t.print(std::cout);
        }
    }
}