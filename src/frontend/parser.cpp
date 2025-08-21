#include "parser.hpp"
#include <algorithm>
#include <cctype>
#include <set>

namespace pascal {
    
   
    void PascalParser::error(const std::string& message) {
        throw ParseException("Parse error: " + message + (token ? " at '" + token->getTokenValue() + "'" : " at end of input"));
    }

    void PascalParser::expectToken(const std::string& expected) {
        if (!peekIs(expected)) error("Expected '" + expected + "'" + " on Line: " + std::to_string(token->getLine()));
    }

    void PascalParser::expectToken(types::TokenType expected) {
        if (!peekIs(expected)) error("Expected " + tokenTypeToString(expected));
    }

    std::unique_ptr<ASTNode> PascalParser::parseProgram() {
        expectToken("program");
        next();

        expectToken(types::TokenType::TT_ID);
        std::string programName = token->getTokenValue();
        next();

        expectToken(";");
        next();

        auto block = parseBlock();

        expectToken(".");
        return std::make_unique<ProgramNode>(programName, std::move(block));
    }

    bool PascalParser::match(const std::string& s) {
        if (peekIs(s)) { next(); return true; }
        return false;
    }

    bool PascalParser::match(types::TokenType t) {
        if (peekIs(t)) { next(); return true; }
        return false;
    }

    std::unique_ptr<ASTNode> PascalParser::parseBlock() {
        auto declarations = parseDeclarations();
        auto compoundStmt = parseCompoundStatement();
        return std::make_unique<BlockNode>(std::move(declarations), std::move(compoundStmt));
    }


    std::vector<std::unique_ptr<ASTNode>> PascalParser::parseDeclarations() {
        std::vector<std::unique_ptr<ASTNode>> declarations;
        
        if (match("var")) {  
            while (peekIs(types::TokenType::TT_ID)) {
                if (isKeyword(token->getTokenValue())) {
                    break; 
                }
                
                std::vector<std::string> identifiers;
                
                identifiers.push_back(token->getTokenValue());
                next();  
                
                while (match(",")) {  
                    if (!peekIs(types::TokenType::TT_ID))
                        error("Expected identifier after comma");
                        
                    identifiers.push_back(token->getTokenValue());
                    next();  
                }
                
                if (!match(":"))  
                    error("Expected ':' after identifiers found: " + token->getTokenValue() + " on Line: " + std::to_string(token->getLine()));
                    
                if (!peekIs(types::TokenType::TT_ID))
                    error("Expected type name");
                    
                std::string type = token->getTokenValue();
                next();  
                
                std::unique_ptr<ASTNode> initializer = nullptr;
                if (match(":=")) {  
                    initializer = parseExpression();
                }
                
                if (!match(";"))
                    error("Expected ';' after variable declaration");

                if (initializer) {
                    std::vector<std::unique_ptr<ASTNode>> initializers;
                    initializers.push_back(std::move(initializer));
                    declarations.push_back(std::make_unique<VarDeclNode>(
                        std::move(identifiers), type, std::move(initializers)));
                } else {
                    declarations.push_back(std::make_unique<VarDeclNode>(
                        std::move(identifiers), type));
                }
            }
        }

        while (peekIs("procedure") || peekIs("function")) {
            if (peekIs("procedure")) {
                declarations.push_back(parseProcedureDeclaration());
            } else {
                declarations.push_back(parseFunctionDeclaration());
            }
        }

        return declarations;
    }
    bool PascalParser::isKeyword(const std::string& token) {
        std::string lower = token;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        return lower == "program" || lower == "var" || lower == "begin" || lower == "end" ||
            lower == "if" || lower == "then" || lower == "else" || lower == "while" ||
            lower == "do" || lower == "for" || lower == "to" || lower == "downto" ||
            lower == "procedure" || lower == "function" || lower == "integer" ||
            lower == "real" || lower == "boolean" || lower == "string" || lower == "char" ||
            lower == "true" || lower == "false" || lower == "div" || lower == "mod" ||
            lower == "and" || lower == "or" || lower == "not" || 
            lower == "case" || lower == "of" || lower == "repeat" || lower == "until";  
    }
    std::unique_ptr<ASTNode> PascalParser::parseVarDeclaration() {
        expectToken("var");
        next();

        std::vector<std::string> identifiers;
        expectToken(types::TokenType::TT_ID);
        identifiers.push_back(token->getTokenValue());
        next();

        while (peekIs(",")) {
            next(); 
            expectToken(types::TokenType::TT_ID);
            identifiers.push_back(token->getTokenValue());
            next();
        }

        expectToken(":");
        next();

        expectToken(types::TokenType::TT_ID);
        std::string type = token->getTokenValue();
        next();

        expectToken(";");
        next();

        return std::make_unique<VarDeclNode>(std::move(identifiers), type);
    }

    std::unique_ptr<ASTNode> PascalParser::parseProcedureDeclaration() {
        expectToken("procedure");
        next();

        expectToken(types::TokenType::TT_ID);
        std::string procName = token->getTokenValue();
        next();

        std::vector<std::unique_ptr<ASTNode>> parameters;
        if (peekIs("(")) {
            next(); 
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
        expectToken("function");
        next();

        expectToken(types::TokenType::TT_ID);
        std::string funcName = token->getTokenValue();
        next();

        std::vector<std::unique_ptr<ASTNode>> parameters;
        if (peekIs("(")) {
            next(); 
            parameters = parseParameterList();
            expectToken(")");
            next();
        }

        expectToken(":");
        next();

        expectToken(types::TokenType::TT_ID);
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
                next(); 
                parameters.push_back(parseParameter());
            }
        }

        return parameters;
    }

    std::unique_ptr<ASTNode> PascalParser::parseParameter() {
        bool isVar = false;
        if (peekIs("var")) {
            isVar = true;
            next();
        }

        std::vector<std::string> identifiers;
        expectToken(types::TokenType::TT_ID);
        identifiers.push_back(token->getTokenValue());
        next();

        while (peekIs(",")) {
            next(); 
            expectToken(types::TokenType::TT_ID);
            identifiers.push_back(token->getTokenValue());
            next();
        }

        expectToken(":");
        next();

        expectToken(types::TokenType::TT_ID);
        std::string type = token->getTokenValue();
        next();

        return std::make_unique<ParameterNode>(std::move(identifiers), type, isVar);
    }

    std::unique_ptr<ASTNode> PascalParser::parseCompoundStatement() {
        expectToken("begin");
        next();

        std::vector<std::unique_ptr<ASTNode>> statements;

        if (!peekIs("end")) {
            statements.push_back(parseStatement());

            while (peekIs(";")) {
                next(); 
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
        } else if (peekIs("repeat")) {  
            return parseRepeatStatement();
        } else if (peekIs("case")) {  
            return parseCaseStatement();
        } else if (peekIs(types::TokenType::TT_ID)) {
            return parseAssignmentOrProcCall();
        } else {
            return std::make_unique<EmptyStmtNode>();
        }
    }

    std::unique_ptr<ASTNode> PascalParser::parseAssignmentOrProcCall() {
        expectToken(types::TokenType::TT_ID);
        std::string name = token->getTokenValue();
        next();

        if (peekIs(":=")) {
            next(); 
            
            auto variable = std::make_unique<VariableNode>(name);
            auto expression = parseExpression();
            return std::make_unique<AssignmentNode>(std::move(variable), std::move(expression));
        } else {
            return parseProcedureCall(name);
        }
    }

    std::unique_ptr<ASTNode> PascalParser::parseIfStatement() {
        expectToken("if");
        next();
        
        auto condition = parseExpression();
        
        expectToken("then");
        next();
        
        auto thenStatement = parseStatement();
        
        std::unique_ptr<ASTNode> elseStatement = nullptr;
        if (peekIs("else")) {
            next();
            elseStatement = parseStatement();  
        }
        
        return std::make_unique<IfStmtNode>(std::move(condition), 
                                             std::move(thenStatement),
                                             std::move(elseStatement));
    }

    std::unique_ptr<ASTNode> PascalParser::parseWhileStatement() {
        expectToken("while");
        next();

        auto condition = parseExpression();

        expectToken("do");
        next();

        auto statement = parseStatement();

        return std::make_unique<WhileStmtNode>(std::move(condition), std::move(statement));
    }

    std::unique_ptr<ASTNode> PascalParser::parseForStatement() {
        expectToken("for");
        next();

        expectToken(types::TokenType::TT_ID);
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

    std::unique_ptr<ASTNode> PascalParser::parseRepeatStatement() {
        expectToken("repeat");
        next();

        std::vector<std::unique_ptr<ASTNode>> statements;
        
        
        while (!peekIs("until")) {
            statements.push_back(parseStatement());
            
            if (peekIs(";")) {
                next();
            } else if (!peekIs("until")) {
                error("Expected ';' or 'until'");
            }
        }
        
        expectToken("until");
        next();
        
        auto condition = parseExpression();
        
        return std::make_unique<RepeatStmtNode>(std::move(statements), std::move(condition));
    }

    std::unique_ptr<ASTNode> PascalParser::parseExpression() {
        auto left = parseSimpleExpression();

        
        if (isRelationalOperator()) {
            std::string op = getRelationalOp();
            next();
            auto right = parseSimpleExpression();
            
            BinaryOpNode::OpType enumOp = BinaryOpNode::EQUAL; 
            if (op == "=") enumOp = BinaryOpNode::EQUAL;
            else if (op == "<>") enumOp = BinaryOpNode::NOT_EQUAL;
            else if (op == "<") enumOp = BinaryOpNode::LESS;
            else if (op == "<=") enumOp = BinaryOpNode::LESS_EQUAL;
            else if (op == ">") enumOp = BinaryOpNode::GREATER;
            else if (op == ">=") enumOp = BinaryOpNode::GREATER_EQUAL;
            
            left = std::make_unique<BinaryOpNode>(std::move(left), enumOp, std::move(right));
        }

        
        while (peekIs("and") || peekIs("or")) {
            BinaryOpNode::OpType op;
            if (peekIs("and")) {
                op = BinaryOpNode::AND;
                next();
            } else {
                op = BinaryOpNode::OR;
                next();
            }
            auto right = parseSimpleExpression();
            left = std::make_unique<BinaryOpNode>(std::move(left), op, std::move(right));
        }

        return left;
    }

    std::unique_ptr<ASTNode> PascalParser::parseSimpleExpression() {
        std::unique_ptr<ASTNode> result;

        
        if (peekIs("+") || peekIs("-") || peekIs("not")) {
            UnaryOpNode::Operator op = UnaryOpNode::PLUS;  
            if (peekIs("+")) op = UnaryOpNode::PLUS;
            else if (peekIs("-")) op = UnaryOpNode::MINUS;
            else if (peekIs("not")) op = UnaryOpNode::NOT;
            next();
            
            auto operand = parseTerm();
            result = std::make_unique<UnaryOpNode>(op, std::move(operand));
        } else {
            result = parseTerm();
        }

        
        while (peekIs("+") || peekIs("-")) {
            BinaryOpNode::OpType op;
            if (peekIs("+")) op = BinaryOpNode::PLUS;
            else op = BinaryOpNode::MINUS;
            next();
            
            auto right = parseTerm();
            result = std::make_unique<BinaryOpNode>(std::move(result), op, std::move(right));
        }

        return result;
    }

    std::unique_ptr<ASTNode> PascalParser::parseTerm() {
        auto result = parseFactor();

        while (isMulOperator()) {
             
            BinaryOpNode::OpType op = BinaryOpNode::MULTIPLY;  
            if (peekIs("*")) op = BinaryOpNode::MULTIPLY;
            else if (peekIs("/")) op = BinaryOpNode::DIVIDE;
            else if (peekIs("div")) op = BinaryOpNode::DIV;
            else if (peekIs("mod")) op = BinaryOpNode::MOD;
            next();
            
            auto right = parseFactor();
            result = std::make_unique<BinaryOpNode>(std::move(result), op, std::move(right));
        }

        return result;
    }

    std::unique_ptr<ASTNode> PascalParser::parseFactor() {
        if (peekIs(types::TokenType::TT_NUM)) {
            std::string value = token->getTokenValue();
            next();
            
            bool isReal = value.find('.') != std::string::npos ||
                          value.find('e') != std::string::npos ||
                          value.find('E') != std::string::npos;
            
            return std::make_unique<NumberNode>(value, !isReal, isReal);
        } else if (peekIs(types::TokenType::TT_STR)) {
            std::string value = token->getTokenValue();
            next();
            return std::make_unique<StringNode>(value);
        } else if (peekIs("true") || peekIs("false")) {
            bool value = (token->getTokenValue() == "true");
            next();
            return std::make_unique<BooleanNode>(value);
        } else if (peekIs("(")) {
            next(); 
            auto expr = parseExpression();
            expectToken(")");
            next();
            return expr;
        } else if (peekIs(types::TokenType::TT_ID)) {
            std::string name = token->getTokenValue();
            next();

            if (peekIs("(")) {
                return parseFunctionCall(name);
            } else {
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
            next(); 
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
                next(); 
                arguments.push_back(parseExpression());
            }
        }

        return arguments;
    }

    bool PascalParser::isRelationalOperator() {
        return peekIs("=") || peekIs("<>") || peekIs("<") || 
               peekIs("<=") || peekIs(">") || peekIs(">=");
    }

    bool PascalParser::isAddOperator() {
        return peekIs("+") || peekIs("-");  
    }

    bool PascalParser::isMulOperator() {
        return peekIs("*") || peekIs("/") || peekIs("div") || peekIs("mod");  
    }

    std::string PascalParser::getRelationalOp() {
        if (peekIs("=")) return "=";
        if (peekIs("<>")) return "<>";
        if (peekIs("<")) return "<";
        if (peekIs("<=")) return "<=";
        if (peekIs(">")) return ">";
        if (peekIs(">=")) return ">=";
        error("Invalid relational operator");
        return "=";
    }

    std::string PascalParser::getAddOp() {
        if (peekIs("+")) return "+";
        if (peekIs("-")) return "-";
        error("Invalid add operator");
        return "+";
    }

    std::string PascalParser::getMulOp() {
        if (peekIs("*")) return "*";
        if (peekIs("/")) return "/";
        if (peekIs("div")) return "div";
        if (peekIs("mod")) return "mod";
        error("Invalid multiply operator");
        return "*";
    }

    bool PascalParser::isType(const std::string& token) {
        std::string lower = token;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        return lower == "integer" || lower == "real" || lower == "boolean" || 
               lower == "char" || lower == "string" || lower == "record"; 
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

    BinaryOpNode::OpType PascalParser::getComparisonOperator(const std::string& op) {
        if (op == "=") return BinaryOpNode::EQUAL;
        if (op == "<>") return BinaryOpNode::NOT_EQUAL;
        if (op == "<") return BinaryOpNode::LESS;
        if (op == "<=") return BinaryOpNode::LESS_EQUAL;
        if (op == ">") return BinaryOpNode::GREATER;
        if (op == ">=") return BinaryOpNode::GREATER_EQUAL;
        throw std::runtime_error("Unknown comparison operator: " + op);
    }

    BinaryOpNode::OpType PascalParser::getLogicalOperator(const std::string& op) {
        if (op == "and") return BinaryOpNode::AND;
        if (op == "or") return BinaryOpNode::OR;
        throw std::runtime_error("Unknown logical operator: " + op);
    }

    BinaryOpNode::OpType PascalParser::getArithmeticOperator(const std::string& op) {
        if (op == "+") return BinaryOpNode::PLUS;
        if (op == "-") return BinaryOpNode::MINUS;
        if (op == "*") return BinaryOpNode::MULTIPLY;
        if (op == "/") return BinaryOpNode::DIVIDE;
        if (op == "mod") return BinaryOpNode::MOD;
        throw std::runtime_error("Unknown arithmetic operator: " + op);
    }

    std::unique_ptr<ASTNode> PascalParser::parseCaseStatement() {
        expectToken("case");
        next();
        
        auto expression = parseExpression();
        
        expectToken("of");
        next();
        
        std::vector<std::unique_ptr<CaseStmtNode::CaseBranch>> branches;

        while (!peekIs("end") && !peekIs("else")) {
            std::vector<std::unique_ptr<ASTNode>> values;
            
            values.push_back(parseExpression());
            
            while (peekIs(",")) {
                next(); 
                values.push_back(parseExpression());
            }
            
            expectToken(":");
            next();
            
            auto statement = parseStatement();
            
            branches.push_back(std::make_unique<CaseStmtNode::CaseBranch>(
                std::move(values), std::move(statement)));
            
            if (peekIs(";")) {
                next();
            }
        }
        
        std::unique_ptr<ASTNode> elseStatement = nullptr;
        if (peekIs("else")) {
            next();
            elseStatement = parseStatement();
            if (peekIs(";")) {
                next();
            }
        }
        
        expectToken("end");
        next();
        
        return std::make_unique<CaseStmtNode>(std::move(expression), 
                                              std::move(branches), 
                                              std::move(elseStatement));
    }

}
