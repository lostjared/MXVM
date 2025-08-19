#include "parser.hpp"
#include <algorithm>
#include <cctype>

namespace pascal {

    std::unique_ptr<ASTNode> PascalParser::parseProgram() {
        // Check if we have any tokens
        if (!token) {
            error("Empty input - no tokens found");
        }
        
        // program identifier ; block .
        expectToken("program");
        next();
        
        if (!peekIs(types::TokenType::TT_ID)) {
            error("Expected program name");
        }
        
        std::string programName = token->getTokenValue();
        next();
        
        expectToken(";");
        next();
        
        auto block = parseBlock();
        
        expectToken(".");
        
        return std::make_unique<ProgramNode>(programName, std::move(block));
    }

    std::unique_ptr<ASTNode> PascalParser::parseBlock() {
        // block = [declarations] compound-statement
        auto declarations = parseDeclarations();
        auto compoundStmt = parseCompoundStatement();
        
        return std::make_unique<BlockNode>(std::move(declarations), std::move(compoundStmt));
    }

    std::vector<std::unique_ptr<ASTNode>> PascalParser::parseDeclarations() {
        std::vector<std::unique_ptr<ASTNode>> declarations;
        
        // Parse variable declarations
        while (peekIs("var")) {
            declarations.push_back(parseVarDeclaration());
        }
        
        // Parse procedure/function declarations
        while (peekIs("procedure") || peekIs("function")) {
            if (peekIs("procedure")) {
                declarations.push_back(parseProcedureDeclaration());
            } else {
                declarations.push_back(parseFunctionDeclaration());
            }
        }
        
        return declarations;
    }

    std::unique_ptr<ASTNode> PascalParser::parseVarDeclaration() {
        // var identifier-list : type ;
        expectToken("var");
        next();
        
        std::vector<std::string> identifiers;
        
        // Parse identifier list
        if (!peekIs(types::TokenType::TT_ID)) {
            error("Expected variable name");
        }
        
        identifiers.push_back(token->getTokenValue());
        next();
        
        while (peekIs(",")) {
            next(); // consume comma
            if (!peekIs(types::TokenType::TT_ID)) {
                error("Expected variable name after comma");
            }
            identifiers.push_back(token->getTokenValue());
            next();
        }
        
        expectToken(":");
        next();
        
        if (!peekIs(types::TokenType::TT_ID) || !isType(token->getTokenValue())) {
            error("Expected type name");
        }
        
        std::string type = token->getTokenValue();
        next();
        
        expectToken(";");
        next();
        
        return std::make_unique<VarDeclNode>(std::move(identifiers), type);
    }

    std::unique_ptr<ASTNode> PascalParser::parseProcedureDeclaration() {
        // procedure identifier [( parameter-list )] ; block ;
        expectToken("procedure");
        next();
        
        if (!peekIs(types::TokenType::TT_ID)) {
            error("Expected procedure name");
        }
        
        std::string procName = token->getTokenValue();
        next();
        
        std::vector<std::unique_ptr<ASTNode>> parameters;
        
        if (peekIs("(")) {
            next(); // consume (
            parameters = parseParameterList();
            expectToken(")");
            next();
        }
        
        expectToken(";");
        next();
        
        auto block = parseBlock();
        
        expectToken(";");
        next();
        
        return std::make_unique<ProcDeclNode>(procName, std::move(parameters), std::move(block));
    }

    std::unique_ptr<ASTNode> PascalParser::parseFunctionDeclaration() {
        // function identifier [( parameter-list )] : type ; block ;
        expectToken("function");
        next();
        
        if (!peekIs(types::TokenType::TT_ID)) {
            error("Expected function name");
        }
        
        std::string funcName = token->getTokenValue();
        next();
        
        std::vector<std::unique_ptr<ASTNode>> parameters;
        
        if (peekIs("(")) {
            next(); // consume (
            parameters = parseParameterList();
            expectToken(")");
            next();
        }
        
        expectToken(":");
        next();
        
        if (!peekIs(types::TokenType::TT_ID) || !isType(token->getTokenValue())) {
            error("Expected return type");
        }
        
        std::string returnType = token->getTokenValue();
        next();
        
        expectToken(";");
        next();
        
        auto block = parseBlock();
        
        expectToken(";");
        next();
        
        return std::make_unique<FuncDeclNode>(funcName, std::move(parameters), returnType, std::move(block));
    }

    std::vector<std::unique_ptr<ASTNode>> PascalParser::parseParameterList() {
        std::vector<std::unique_ptr<ASTNode>> parameters;
        
        if (!peekIs(")")) {
            parameters.push_back(parseParameter());
            
            while (peekIs(";")) {
                next(); // consume ;
                parameters.push_back(parseParameter());
            }
        }
        
        return parameters;
    }

    std::unique_ptr<ASTNode> PascalParser::parseParameter() {
        // [var] identifier-list : type
        bool isVar = false;
        
        if (peekIs("var")) {
            isVar = true;
            next();
        }
        
        std::vector<std::string> identifiers;
        
        if (!peekIs(types::TokenType::TT_ID)) {
            error("Expected parameter name");
        }
        
        identifiers.push_back(token->getTokenValue());
        next();
        
        while (peekIs(",")) {
            next(); // consume comma
            if (!peekIs(types::TokenType::TT_ID)) {
                error("Expected parameter name after comma");
            }
            identifiers.push_back(token->getTokenValue());
            next();
        }
        
        expectToken(":");
        next();
        
        if (!peekIs(types::TokenType::TT_ID) || !isType(token->getTokenValue())) {
            error("Expected parameter type");
        }
        
        std::string type = token->getTokenValue();
        next();
        
        return std::make_unique<ParameterNode>(std::move(identifiers), type, isVar);
    }

    std::unique_ptr<ASTNode> PascalParser::parseCompoundStatement() {
        // begin statement-list end
        expectToken("begin");
        next();
        
        std::vector<std::unique_ptr<ASTNode>> statements;
        
        if (!peekIs("end")) {
            statements.push_back(parseStatement());
            
            while (peekIs(";")) {
                next(); // consume ;
                if (!peekIs("end")) {
                    statements.push_back(parseStatement());
                }
            }
        }
        
        expectToken("end");
        next();
        
        return std::make_unique<CompoundStmtNode>(std::move(statements));
    }

    std::unique_ptr<ASTNode> PascalParser::parseStatement() {
        if (!token) {
            return std::make_unique<EmptyStmtNode>();
        }
        
        if (peekIs("begin")) {
            return parseCompoundStatement();
        } else if (peekIs("if")) {
            return parseIfStatement();
        } else if (peekIs("while")) {
            return parseWhileStatement();
        } else if (peekIs("for")) {
            return parseForStatement();
        } else if (peekIs(types::TokenType::TT_ID)) {
            return parseAssignmentOrProcCall();
        } else {
            // Empty statement
            return std::make_unique<EmptyStmtNode>();
        }
    }

    std::unique_ptr<ASTNode> PascalParser::parseAssignmentOrProcCall() {
        if (!peekIs(types::TokenType::TT_ID)) {
            error("Expected identifier");
        }
        
        std::string name = token->getTokenValue();
        next();
        
        if (peekIs(":=")) {
            // Assignment
            next(); // consume :=
            auto variable = std::make_unique<VariableNode>(name);
            auto expression = parseExpression();
            return std::make_unique<AssignmentNode>(std::move(variable), std::move(expression));
        } else if (peekIs("(") || isBuiltinProcedure(name)) {
            // Procedure call
            return parseProcedureCall(name);
        } else {
            // Simple procedure call without parameters
            return std::make_unique<ProcCallNode>(name, std::vector<std::unique_ptr<ASTNode>>());
        }
    }

    std::unique_ptr<ASTNode> PascalParser::parseIfStatement() {
        // if expression then statement [else statement]
        expectToken("if");
        next();
        
        auto condition = parseExpression();
        
        expectToken("then");
        next();
        
        auto thenStmt = parseStatement();
        
        std::unique_ptr<ASTNode> elseStmt = nullptr;
        if (peekIs("else")) {
            next(); // consume else
            elseStmt = parseStatement();
        }
        
        return std::make_unique<IfStmtNode>(std::move(condition), std::move(thenStmt), std::move(elseStmt));
    }

    std::unique_ptr<ASTNode> PascalParser::parseWhileStatement() {
        // while expression do statement
        expectToken("while");
        next();
        
        auto condition = parseExpression();
        
        expectToken("do");
        next();
        
        auto statement = parseStatement();
        
        return std::make_unique<WhileStmtNode>(std::move(condition), std::move(statement));
    }

    std::unique_ptr<ASTNode> PascalParser::parseForStatement() {
        // for identifier := expression to|downto expression do statement
        expectToken("for");
        next();
        
        if (!peekIs(types::TokenType::TT_ID)) {
            error("Expected variable name in for loop");
        }
        
        std::string variable = token->getTokenValue();
        next();
        
        expectToken(":=");
        next();
        
        auto startValue = parseExpression();
        
        bool isDownto = false;
        if (peekIs("to")) {
            next();
        } else if (peekIs("downto")) {
            isDownto = true;
            next();
        } else {
            error("Expected 'to' or 'downto' in for loop");
        }
        
        auto endValue = parseExpression();
        
        expectToken("do");
        next();
        
        auto statement = parseStatement();
        
        return std::make_unique<ForStmtNode>(variable, std::move(startValue), 
                                            std::move(endValue), isDownto, std::move(statement));
    }

    std::unique_ptr<ASTNode> PascalParser::parseExpression() {
        // expression = simple-expression [relational-operator simple-expression]
        auto left = parseSimpleExpression();
        
        if (isRelationalOperator()) {
            auto op = getRelationalOp();
            next();
            auto right = parseSimpleExpression();
            return std::make_unique<BinaryOpNode>(std::move(left), op, std::move(right));
        }
        
        return left;
    }

    std::unique_ptr<ASTNode> PascalParser::parseSimpleExpression() {
        // simple-expression = [sign] term {add-operator term}
        std::unique_ptr<ASTNode> result;
        
        // Handle leading sign
        if (peekIs("+") || peekIs("-")) {
            auto op = getUnaryOp();
            next();
            auto operand = parseTerm();
            result = std::make_unique<UnaryOpNode>(op, std::move(operand));
        } else {
            result = parseTerm();
        }
        
        // Handle add operators
        while (isAddOperator()) {
            auto op = getAddOp();
            next();
            auto right = parseTerm();
            result = std::make_unique<BinaryOpNode>(std::move(result), op, std::move(right));
        }
        
        return result;
    }

    std::unique_ptr<ASTNode> PascalParser::parseTerm() {
        // term = factor {mul-operator factor}
        auto result = parseFactor();
        
        while (isMulOperator()) {
            auto op = getMulOp();
            next();
            auto right = parseFactor();
            result = std::make_unique<BinaryOpNode>(std::move(result), op, std::move(right));
        }
        
        return result;
    }

    std::unique_ptr<ASTNode> PascalParser::parseFactor() {
        // factor = variable | number | string | boolean | function-call | (expression) | not factor
        if (!token) {
            error("Unexpected end of input");
        }
        
        if (peekIs(types::TokenType::TT_NUM)) {
            std::string value = token->getTokenValue();
            next();
            return std::make_unique<NumberNode>(value, true);
        } else if (peekIs(types::TokenType::TT_STR)) {
            std::string value = token->getTokenValue();
            next();
            return std::make_unique<StringNode>(value);
        } else if (peekIs("true") || peekIs("false")) {
            bool value = (token->getTokenValue() == "true");
            next();
            return std::make_unique<BooleanNode>(value);
        } else if (peekIs("(")) {
            next(); // consume (
            auto expr = parseExpression();
            expectToken(")");
            next();
            return expr;
        } else if (peekIs("not")) {
            next(); // consume not
            auto operand = parseFactor();
            return std::make_unique<UnaryOpNode>(UnaryOpNode::NOT, std::move(operand));
        } else if (peekIs(types::TokenType::TT_ID)) {
            std::string name = token->getTokenValue();
            next();
            
            if (peekIs("(")) {
                // Function call
                return parseFunctionCall(name);
            } else {
                // Variable
                return std::make_unique<VariableNode>(name);
            }
        } else {
            error("Expected factor");
            return nullptr;
        }
    }

    std::unique_ptr<ASTNode> PascalParser::parseProcedureCall(const std::string& name) {
        std::vector<std::unique_ptr<ASTNode>> arguments;
        
        if (peekIs("(")) {
            next(); // consume (
            arguments = parseArgumentList();
            expectToken(")");
            next();
        }
        
        return std::make_unique<ProcCallNode>(name, std::move(arguments));
    }

    std::unique_ptr<ASTNode> PascalParser::parseFunctionCall(const std::string& name) {
        std::vector<std::unique_ptr<ASTNode>> arguments;
        
        expectToken("(");
        next();
        arguments = parseArgumentList();
        expectToken(")");
        next();
        
        return std::make_unique<FuncCallNode>(name, std::move(arguments));
    }

    std::vector<std::unique_ptr<ASTNode>> PascalParser::parseArgumentList() {
        std::vector<std::unique_ptr<ASTNode>> arguments;
        
        if (!peekIs(")")) {
            arguments.push_back(parseExpression());
            
            while (peekIs(",")) {
                next(); // consume comma
                arguments.push_back(parseExpression());
            }
        }
        
        return arguments;
    }

    // Utility Methods
    bool PascalParser::isRelationalOperator() {
        return peekIs("=") || peekIs("<>") || peekIs("<") || 
               peekIs("<=") || peekIs(">") || peekIs(">=");
    }

    bool PascalParser::isAddOperator() {
        return peekIs("+") || peekIs("-") || peekIs("or");
    }

    bool PascalParser::isMulOperator() {
        return peekIs("*") || peekIs("/") || peekIs("div") || 
               peekIs("mod") || peekIs("and");
    }

    BinaryOpNode::OpType PascalParser::getRelationalOp() {
        if (peekIs("=")) return BinaryOpNode::EQUAL;
        if (peekIs("<>")) return BinaryOpNode::NOT_EQUAL;
        if (peekIs("<")) return BinaryOpNode::LESS;
        if (peekIs("<=")) return BinaryOpNode::LESS_EQUAL;
        if (peekIs(">")) return BinaryOpNode::GREATER;
        if (peekIs(">=")) return BinaryOpNode::GREATER_EQUAL;
        error("Invalid relational operator");
        return BinaryOpNode::EQUAL;
    }

    BinaryOpNode::OpType PascalParser::getAddOp() {
        if (peekIs("+")) return BinaryOpNode::PLUS;
        if (peekIs("-")) return BinaryOpNode::MINUS;
        if (peekIs("or")) return BinaryOpNode::OR;
        error("Invalid add operator");
        return BinaryOpNode::PLUS;
    }

    BinaryOpNode::OpType PascalParser::getMulOp() {
        if (peekIs("*")) return BinaryOpNode::MULTIPLY;
        if (peekIs("/")) return BinaryOpNode::DIVIDE;
        if (peekIs("div")) return BinaryOpNode::DIV;
        if (peekIs("mod")) return BinaryOpNode::MOD;
        if (peekIs("and")) return BinaryOpNode::AND;
        error("Invalid multiply operator");
        return BinaryOpNode::MULTIPLY;
    }

    UnaryOpNode::OpType PascalParser::getUnaryOp() {
        if (peekIs("+")) return UnaryOpNode::PLUS;
        if (peekIs("-")) return UnaryOpNode::MINUS;
        if (peekIs("not")) return UnaryOpNode::NOT;
        error("Invalid unary operator");
        return UnaryOpNode::PLUS;
    }

    bool PascalParser::isType(const std::string& token) {
        std::string lower = token;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        return lower == "integer" || lower == "real" || lower == "boolean" || 
               lower == "char" || lower == "string";
    }

    bool PascalParser::isBuiltinProcedure(const std::string& name) {
        std::string lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        return lower == "write" || lower == "writeln" || lower == "read" || 
               lower == "readln";
    }

    bool PascalParser::isBuiltinFunction(const std::string& name) {
        std::string lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        return lower == "abs" || lower == "sqr" || lower == "sqrt" || 
               lower == "sin" || lower == "cos" || lower == "ln" || 
               lower == "exp" || lower == "trunc" || lower == "round";
    }

    // Error handling
    void PascalParser::error(const std::string& message) {
        throw ParseException("Parse error: " + message + 
                           (token ? " at '" + token->getTokenValue() + "'" : " at end of input"));
    }

    void PascalParser::expectToken(const std::string& expected) {
        if (!peekIs(expected)) {
            error("Expected '" + expected + "'");
        }
    }

    void PascalParser::expectToken(types::TokenType expected) {
        if (!peekIs(expected)) {
            error("Expected " + tokenTypeToString(expected));
        }
    }

} // namespace pascal