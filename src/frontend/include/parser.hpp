#ifndef _PARSER_H__H
#define _PARSER_H__H
#include "scanner/scanner.hpp"
#include "scanner/exception.hpp"
#include "ast.hpp"
#include <stdexcept>
#include <string>
#include <vector>
#include <memory>
#include "validator.hpp"

namespace pascal {
    class ParseException : public std::runtime_error {
    public:
        explicit ParseException(const std::string& msg) : std::runtime_error(msg) {}
    };
}

namespace mxx {
    class XParser {
    public:
        XParser() = delete;
        explicit XParser(const std::string &source)
            : scanner(source), token(nullptr), index(0) {
            scanner.scan();
            scanner.removeEOL();  
            if (scanner.size() > 0) {
                token = &scanner[0];
                index = 0;
            }
        }

        bool next() {
            if (index + 1 < scanner.size()) {
                index++;
                token = &scanner[index];
                return true;
            }
            token = nullptr;
            return false;
        }

        bool peekIs(const std::string &s) {
            return token && token->getTokenValue() == s;
        }
        bool peekIs(const types::TokenType &t) {
            return token && token->getTokenType() == t;
        }

        static std::string tokenTypeToString(types::TokenType t) {
            switch (t) {
                case types::TokenType::TT_ID:  return "IDENTIFIER";
                case types::TokenType::TT_NUM: return "NUMBER";
                case types::TokenType::TT_STR: return "STRING";
                case types::TokenType::TT_SYM: return "SYMBOL";
                default: return "UNKNOWN";
            }
        }

        std::string filename;

    protected:
        scan::Scanner scanner;
        scan::TToken *token = nullptr;
        size_t index = 0;
    };
}

namespace pascal {
    class PascalParser : public mxx::XParser {
    public:
        PascalParser() = delete;
        explicit PascalParser(const std::string& source) : mxx::XParser(source), validator(source) {}

        std::unique_ptr<ProgramNode> parseProgram();
        mxx::TPValidator validator;
    private:
        
        void error(const std::string& message);
        void expectToken(const std::string& expected);
        void expectToken(types::TokenType expected);
        bool match(const std::string& s);
        bool match(types::TokenType t);
         std::unique_ptr<ASTNode> parseLValue();
        std::unique_ptr<BlockNode> parseBlock();
        std::unique_ptr<CompoundStmtNode> parseCompoundStatement();
        std::vector<std::unique_ptr<ASTNode>> parseDeclarations();
        std::unique_ptr<ASTNode> parseVarDeclaration();
        std::unique_ptr<ASTNode> parseConstDeclaration();
        std::unique_ptr<ASTNode> parseProcedureDeclaration();
        std::unique_ptr<ASTNode> parseFunctionDeclaration();
        std::vector<std::unique_ptr<ASTNode>> parseParameterList();
        std::vector<std::unique_ptr<ASTNode>> parseStatementList();
        std::unique_ptr<ASTNode> parseParameter();
        std::unique_ptr<ASTNode> parseRecordType();
        std::unique_ptr<ASTNode> parseTypeDeclaration();
        std::unique_ptr<ASTNode> parseStatement();
        std::unique_ptr<ASTNode> parseAssignmentOrProcCall();
        std::unique_ptr<ASTNode> parseIfStatement();
        std::unique_ptr<ASTNode> parseWhileStatement();
        std::unique_ptr<ASTNode> parseForStatement();
        std::unique_ptr<ASTNode> parseRepeatStatement();
        std::unique_ptr<ASTNode> parseCaseStatement();
        std::unique_ptr<ASTNode> parseExpression();
        std::unique_ptr<ASTNode> parseSimpleExpression();
        std::unique_ptr<ASTNode> parseTerm();
        std::unique_ptr<ASTNode> parseFactor();
        std::unique_ptr<ASTNode> parseProcedureCall(const std::string& name);
        std::unique_ptr<ASTNode> parseFunctionCall(const std::string& name);
        std::vector<std::unique_ptr<ASTNode>> parseArgumentList();

        bool isRelationalOperator();
        bool isAddOperator();
        bool isMulOperator();
        
        std::unique_ptr<ASTNode> parseTypeSpec() ;
        
        std::string getRelationalOp();
        std::string getAddOp();
        std::string getMulOp();
        std::string getUnaryOp();  
        BinaryOpNode::OpType getComparisonOperator(const std::string& op);
        BinaryOpNode::OpType getLogicalOperator(const std::string& op);
        BinaryOpNode::OpType getArithmeticOperator(const std::string& op);
        std::string tokenTypeToString(types::TokenType type);
        bool isType(const std::string& token);
        bool isBuiltinProcedure(const std::string& name);
        bool isBuiltinFunction(const std::string& name);
        bool isKeyword(const std::string& s);
        std::unique_ptr<ASTNode> parseArrayType();
        std::unique_ptr<ASTNode> parseArrayDeclaration(const std::string& varName);
        bool isArrayAccess(); 
    };
}

#endif