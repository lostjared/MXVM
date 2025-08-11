#ifndef _VALID_H_X
#define _VALID_H_X

#include<iostream>
#include<string>
#include<vector>
#include<unordered_map>
#include<unordered_set>
#include"scanner/scanner.hpp"
#include"mxvm/instruct.hpp"

namespace mxvm {


    enum class OpKind { Num, Hex, Str, Id, Member, Label, Any };

    struct ParsedOp {
        OpKind kind;
        std::string text;
        const scan::TToken* at;
    };

    struct UseVar   { std::string name; const scan::TToken* at; };
    struct UseLabel { std::string name; const scan::TToken* at; };

    enum class VArity { None, AnyTail, ArgsTail };

    struct OpSpec {
        std::string name;
        std::vector<OpKind> fixed;
        VArity varPolicy = VArity::None;
        int minArgs = -1;
        int maxArgs = -1;
    };

    class Validator {
    private:
        std::string filename;
        scan::Scanner scanner;
        std::string source;
        size_t index = 0;
        const scan::TToken* token = nullptr;
        std::string current_function;
        std::unordered_map<std::string, std::unordered_set<std::string>> function_vars;
    public:
        Validator(const std::string &source);
        struct VarNamePair {
            std::string first;           
            const scan::TToken* second;  
        };
        std::vector<VarNamePair> var_names;
        void validateAgainstSpec(
            const std::string& op,
            const std::vector<ParsedOp>& ops,
            const std::unordered_map<std::string, Variable>& vars,
            const std::unordered_map<std::string, std::string>& labels,
            std::vector<UseVar>& usedVarsRef,
            std::vector<UseLabel>& usedLabelsRef);

        bool validate(const std::string &name);
        bool match(const std::string &m);
        void require(const std::string &r);
        bool match(const types::TokenType &t);
        void require(const types::TokenType &t);
        bool next();
        bool peekIs(const std::string &s);
        bool peekIs(const types::TokenType &t);
        std::string tokenTypeToString(types::TokenType t);
        void collect_labels(std::unordered_map<std::string, std::string> &labels);
        ParsedOp parseOperand();
        std::vector<ParsedOp> parseOperandList();
    };
}

#endif