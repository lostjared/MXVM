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

    std::unique_ptr<ProgramNode> PascalParser::parseProgram() {
        expectToken("program");
        int lineNum = token->getLine();
        next();
        expectToken(types::TokenType::TT_ID);
        std::string programName = token->getTokenValue();
        next();
        expectToken(";");
        next();
        auto block = parseBlock();
        expectToken(".");
        auto programNode = std::make_unique<ProgramNode>(programName, std::move(block));
        programNode->setLineNumber(lineNum);
        return programNode;
    }

    bool PascalParser::match(const std::string& s) {
        if (peekIs(s)) { next(); return true; }
        return false;
    }

    bool PascalParser::match(types::TokenType t) {
        if (peekIs(t)) { next(); return true; }
        return false;
    }

    std::unique_ptr<BlockNode> PascalParser::parseBlock() {
        int lineNum = token ? token->getLine() : 1;
        auto declarations = parseDeclarations();
        auto compoundStatement = parseCompoundStatement();
        auto blockNode = std::make_unique<BlockNode>(std::move(declarations), std::move(compoundStatement));
        blockNode->setLineNumber(lineNum);
        return blockNode;
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
               lower == "case" || lower == "of" || lower == "repeat" || lower == "until" || 
               lower == "array" || lower == "of" || lower == "type" || lower == "record";
    }

    std::unique_ptr<ASTNode> PascalParser::parseArrayDeclaration(const std::string& varName) {
        if (!peekIs("array")) error("Expected 'array' keyword");
        next();
        if (!peekIs("[")) error("Expected '[' after 'array'");
        next();
        auto lowerBound = parseExpression();
        if (!peekIs("..")) error("Expected '..' in array bounds");
        next();
        auto upperBound = parseExpression();
        if (!peekIs("]")) error("Expected ']' after array bounds");
        next();
        if (!peekIs("of")) error("Expected 'of' after array bounds");
        next();
        if (!peekIs(types::TokenType::TT_ID)) error("Expected type identifier after 'of'");
        std::string elementType = token->getTokenValue();
        next();
        auto arrayType = std::make_unique<ArrayTypeNode>(elementType, std::move(lowerBound), std::move(upperBound));
        std::vector<std::unique_ptr<ASTNode>> initializers;
        if (peekIs(":=")) {
            next();
            if (!peekIs("(")) error("Expected '(' after ':=' in array initialization");
            next();
            if (!peekIs(")")) {
                do {
                    initializers.push_back(parseExpression());
                    if (peekIs(",")) next(); else break;
                } while (true);
            }
            if (!peekIs(")")) error("Expected ')' after array initializers");
            next();
        }
        return std::make_unique<ArrayDeclarationNode>(varName, std::move(arrayType), std::move(initializers));
    }

    bool PascalParser::isArrayAccess() {
        return peekIs(types::TokenType::TT_ID);
    }

    std::unique_ptr<ASTNode> PascalParser::parseProcedureDeclaration() {
        expectToken("procedure");
        int lineNum = token->getLine();
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
        auto procDeclNode = std::make_unique<ProcDeclNode>(procName, std::move(parameters), std::move(block));
        procDeclNode->setLineNumber(lineNum);
        return procDeclNode;
    }

    std::unique_ptr<ASTNode> PascalParser::parseFunctionDeclaration() {
        expectToken("function");
        int lineNum = token->getLine();
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
        auto funcDeclNode = std::make_unique<FuncDeclNode>(funcName, std::move(parameters), returnType, std::move(block));
        funcDeclNode->setLineNumber(lineNum);
        return funcDeclNode;
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
        int lineNum = token ? token->getLine() : 1;
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
        auto parameterNode = std::make_unique<ParameterNode>(std::move(identifiers), type, isVar);
        parameterNode->setLineNumber(lineNum);
        return parameterNode;
    }

    std::unique_ptr<CompoundStmtNode> PascalParser::parseCompoundStatement() {
        int lineNum = token ? token->getLine() : 1;
        expectToken("begin");
        next();

        auto statements = parseStatementList();

        expectToken("end");
        next();
        
        auto compoundNode = std::make_unique<CompoundStmtNode>(std::move(statements));
        compoundNode->setLineNumber(lineNum);
        return compoundNode;
    }

    std::unique_ptr<ASTNode> PascalParser::parseStatement() {
        if (!token) {
            auto emptyNode = std::make_unique<EmptyStmtNode>();
            emptyNode->setLineNumber(1);
            return emptyNode;
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
            auto lhs = parseLValue();
            int lineNum = token ? token->getLine() : 1;

            if (peekIs(":=")) {
                next(); 
                auto rhs = parseExpression();
                auto assignmentNode = std::make_unique<AssignmentNode>(std::move(lhs), std::move(rhs));
                assignmentNode->setLineNumber(lineNum);
                return assignmentNode;
            }

            if (auto varNode = dynamic_cast<VariableNode*>(lhs.get())) {
                if (peekIs("(")) {
                    return parseProcedureCall(varNode->name);
                }
                return lhs;
            }

            error("Invalid statement: expected ':=' for assignment or '(' for procedure call");
            return nullptr;
        } else {
            auto emptyNode = std::make_unique<EmptyStmtNode>();
            emptyNode->setLineNumber(token ? token->getLine() : 1);
            return emptyNode;
        }
    }

    std::unique_ptr<ASTNode> PascalParser::parseAssignmentOrProcCall() {
        expectToken(types::TokenType::TT_ID);
        std::string name = token->getTokenValue();
        int lineNum = token->getLine();
        next();
        if (peekIs(":=")) {
            next();
            auto variable = std::make_unique<VariableNode>(name);
            variable->setLineNumber(lineNum);
            auto expression = parseExpression();
            auto assignmentNode = std::make_unique<AssignmentNode>(std::move(variable), std::move(expression));
            assignmentNode->setLineNumber(lineNum);
            return assignmentNode;
        } else {
            return parseProcedureCall(name);
        }
    }

    std::unique_ptr<ASTNode> PascalParser::parseIfStatement() {
        expectToken("if");
        int lineNum = token->getLine();
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
        auto ifStmtNode = std::make_unique<IfStmtNode>(std::move(condition), std::move(thenStatement), std::move(elseStatement));
        ifStmtNode->setLineNumber(lineNum);
        return ifStmtNode;
    }

    std::unique_ptr<ASTNode> PascalParser::parseWhileStatement() {
        expectToken("while");
        int lineNum = token->getLine();
        next();
        auto condition = parseExpression();
        expectToken("do");
        next();
        auto statement = parseStatement();
        auto whileStmtNode = std::make_unique<WhileStmtNode>(std::move(condition), std::move(statement));
        whileStmtNode->setLineNumber(lineNum);
        return whileStmtNode;
    }

    std::unique_ptr<ASTNode> PascalParser::parseForStatement() {
        expectToken("for");
        int lineNum = token->getLine();
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
        auto forStmtNode = std::make_unique<ForStmtNode>(variable, std::move(startValue), std::move(endValue), isDownto, std::move(statement));
        forStmtNode->setLineNumber(lineNum);
        return forStmtNode;
    }

    std::unique_ptr<ASTNode> PascalParser::parseRepeatStatement() {
        expectToken("repeat");
        int lineNum = token->getLine();
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
        auto repeatStmtNode = std::make_unique<RepeatStmtNode>(std::move(statements), std::move(condition));
        repeatStmtNode->setLineNumber(lineNum);
        return repeatStmtNode;
    }

    std::unique_ptr<ASTNode> PascalParser::parseExpression() {
        int lineNum = token ? token->getLine() : 1;
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
            left->setLineNumber(lineNum);
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
            left->setLineNumber(lineNum);
        }
        return left;
    }

    std::unique_ptr<ASTNode> PascalParser::parseSimpleExpression() {
        std::unique_ptr<ASTNode> result;
        int lineNum = token ? token->getLine() : 1;
        if (peekIs("+") || peekIs("-") || peekIs("not")) {
            UnaryOpNode::Operator op = UnaryOpNode::PLUS;
            if (peekIs("+")) op = UnaryOpNode::PLUS;
            else if (peekIs("-")) op = UnaryOpNode::MINUS;
            else if (peekIs("not")) op = UnaryOpNode::NOT;
            next();
            auto operand = parseTerm();
            result = std::make_unique<UnaryOpNode>(op, std::move(operand));
            result->setLineNumber(lineNum);
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
            result->setLineNumber(lineNum);
        }
        return result;
    }

    std::unique_ptr<ASTNode> PascalParser::parseTerm() {
        int lineNum = token ? token->getLine() : 1;
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
            result->setLineNumber(lineNum);
        }
        return result;
    }

    std::unique_ptr<ASTNode> PascalParser::parseFactor() {
        int lineNum = token ? token->getLine() : 1;
        
        if (peekIs(types::TokenType::TT_NUM)) {
            std::string value = token->getTokenValue();
            next();
            bool isReal = value.find('.') != std::string::npos || value.find('e') != std::string::npos || value.find('E') != std::string::npos;
            auto numberNode = std::make_unique<NumberNode>(value, !isReal, isReal);
            numberNode->setLineNumber(lineNum);
            return numberNode;
        } else if (peekIs(types::TokenType::TT_STR)) {
            std::string value = token->getTokenValue();
            next();
            auto stringNode = std::make_unique<StringNode>(value);
            stringNode->setLineNumber(lineNum);
            return stringNode;
        } else if (peekIs("true") || peekIs("false")) {
            bool value = (token->getTokenValue() == "true");
            next();
            auto booleanNode = std::make_unique<BooleanNode>(value);
            booleanNode->setLineNumber(lineNum);
            return booleanNode;
        } else if (peekIs("(")) {
            next();
            auto expr = parseExpression();
            expectToken(")");
            next();
            return expr;
        } else if (peekIs(types::TokenType::TT_ID)) {
            std::string name = token->getTokenValue();
            next();
            std::unique_ptr<ASTNode> left = std::make_unique<VariableNode>(name);
            left->setLineNumber(lineNum);
            while (true) {
                if (peekIs("[")) {
                    next();
                    auto index = parseExpression();
                    expectToken("]");
                    next();
                    if (auto varNode = dynamic_cast<VariableNode*>(left.get())) {
                        left = std::make_unique<ArrayAccessNode>(varNode->name, std::move(index));
                    } else {
                        error("Array base must be an identifier");
                    }
                    left->setLineNumber(lineNum);
                } else if (peekIs(".")) {
                    next();
                    expectToken(types::TokenType::TT_ID);
                    std::string fieldName = token->getTokenValue();
                    next();
                    left = std::make_unique<FieldAccessNode>(std::move(left), fieldName);
                    left->setLineNumber(lineNum);
                } else if (peekIs("(")) {
                    if (auto varNode = dynamic_cast<VariableNode*>(left.get())) {
                        return parseFunctionCall(varNode->name);
                    } else {
                        error("Function call must be on a simple identifier");
                    }
                }
                else {
                    break; 
                }
            }
            return left;

        } else {
            error("Expected factor");
            return nullptr;
        }
    }

    std::unique_ptr<ASTNode> PascalParser::parseProcedureCall(const std::string& name) {
        int lineNum = token ? token->getLine() : 1;
        std::vector<std::unique_ptr<ASTNode>> arguments;
        if (peekIs("(")) {
            next();
            arguments = parseArgumentList();
            expectToken(")");
            next();
        }
        auto procCallNode = std::make_unique<ProcCallNode>(name, std::move(arguments));
        procCallNode->setLineNumber(lineNum);
        return procCallNode;
    }

    std::unique_ptr<ASTNode> PascalParser::parseFunctionCall(const std::string& name) {
        int lineNum = token ? token->getLine() : 1;
        std::vector<std::unique_ptr<ASTNode>> arguments;
        expectToken("(");
        next();
        arguments = parseArgumentList();
        expectToken(")");
        next();
        auto funcCallNode = std::make_unique<FuncCallNode>(name, std::move(arguments));
        funcCallNode->setLineNumber(lineNum);
        return funcCallNode;
    }

    std::unique_ptr<ASTNode> PascalParser::parseCaseStatement() {
        expectToken("case");
        int lineNum = token->getLine();
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
            branches.push_back(std::make_unique<CaseStmtNode::CaseBranch>(std::move(values), std::move(statement)));
            if (peekIs(";")) next();
        }
        std::unique_ptr<ASTNode> elseStatement = nullptr;
        if (peekIs("else")) {
            next();
            elseStatement = parseStatement();
            if (peekIs(";")) next();
        }
        expectToken("end");
        next();
        auto caseStmtNode = std::make_unique<CaseStmtNode>(std::move(expression), std::move(branches), std::move(elseStatement));
        caseStmtNode->setLineNumber(lineNum);
        return caseStmtNode;
    }

    std::unique_ptr<ASTNode> PascalParser::parseConstDeclaration() {
        expectToken("const");
        int lineNum = token->getLine();
        next();
        std::vector<std::unique_ptr<ConstDeclNode::ConstAssignment>> assignments;
        do {
            expectToken(types::TokenType::TT_ID);
            std::string identifier = token->getTokenValue();
            next();
            expectToken("=");
            next();
            auto value = parseExpression();
            assignments.push_back(std::make_unique<ConstDeclNode::ConstAssignment>(identifier, std::move(value)));
            expectToken(";");
            next();
        } while (peekIs(types::TokenType::TT_ID) && !isKeyword(token->getTokenValue()));
        auto constDeclNode = std::make_unique<ConstDeclNode>(std::move(assignments));
        constDeclNode->setLineNumber(lineNum);
        return constDeclNode;
    }

    std::unique_ptr<ASTNode> PascalParser::parseVarDeclaration() {
        std::vector<std::string> identifiers;
        std::vector<std::unique_ptr<ASTNode>> initializers;

        do {
            if (!peekIs(types::TokenType::TT_ID)) {
                error("Expected identifier in variable declaration");
            }
            
            std::string identifier = token->getTokenValue();
            if (isKeyword(identifier) && (identifier == "procedure" || identifier == "function" || identifier == "begin")) {
                error("Unexpected keyword '" + identifier + "' in variable declaration");
            }
            
            identifiers.push_back(identifier);
            next();
            
            if (peekIs(",")) {
                next();
            } else {
                break;
            }
        } while (true);
        
        if (!peekIs(":")) {
            error("Expected ':' after variable identifier(s)");
        }
        next(); // skip ':'

        std::unique_ptr<ASTNode> declNode;

        if (peekIs("array")) {
            declNode = parseArrayDeclaration(identifiers[0]);
        } else if (peekIs("record")) {
            auto recordTypeNode = parseRecordType();
            declNode = std::make_unique<VarDeclNode>(std::move(identifiers), std::move(recordTypeNode), std::move(initializers));
        } else {
            if (!peekIs(types::TokenType::TT_ID)) {
                error("Expected type identifier");
            }
            std::string type = token->getTokenValue();
            next();

            if (peekIs(":=")) {
                next();
                initializers.push_back(parseExpression());
            }

            declNode = std::make_unique<VarDeclNode>(std::move(identifiers), type, std::move(initializers));
        }

        if (!peekIs(";")) {
            error("Expected ';' after variable declaration");
        }
        next(); 
        
        return declNode;
    }

    std::unique_ptr<ASTNode> PascalParser::parseTypeDeclaration() {
        expectToken("type");
        int lineNum = token->getLine();
        next();
        
        std::vector<std::unique_ptr<ASTNode>> typeDeclarations;
        
        do {
            expectToken(types::TokenType::TT_ID);
            std::string typeName = token->getTokenValue();
            next();
            expectToken("=");
            next();
            
            std::unique_ptr<ASTNode> typeDefinition;
            
            if (peekIs("record")) {
                auto recordTypeAst = parseRecordType(); 
                auto recordTypeNode = std::unique_ptr<RecordTypeNode>(static_cast<RecordTypeNode*>(recordTypeAst.release()));
                typeDefinition = std::make_unique<RecordDeclarationNode>(typeName, std::move(recordTypeNode));
            } else if (peekIs("array")) {
                error("Array type declarations not yet implemented");
            } else {
                expectToken(types::TokenType::TT_ID);
                std::string baseType = token->getTokenValue();
                next();
                typeDefinition = std::make_unique<TypeAliasNode>(typeName, baseType);
            }
            
            expectToken(";");
            next();
            
            typeDeclarations.push_back(std::move(typeDefinition));
            
        } while (peekIs(types::TokenType::TT_ID) && !isKeyword(token->getTokenValue()));
        
        auto typeDeclNode = std::make_unique<TypeDeclNode>(std::move(typeDeclarations));
        typeDeclNode->setLineNumber(lineNum);
        return typeDeclNode;
    }

    std::vector<std::unique_ptr<ASTNode>> PascalParser::parseDeclarations() {
        std::vector<std::unique_ptr<ASTNode>> declarations;
        
        while (peekIs("type") || peekIs("const") || peekIs("var") || peekIs("procedure") || peekIs("function")) {
            if (peekIs("type")) {
                declarations.push_back(parseTypeDeclaration());
            } else if (peekIs("const")) {
                declarations.push_back(parseConstDeclaration());
            } else if (peekIs("var")) {
                if (match("var")) {
                    while (peekIs(types::TokenType::TT_ID) && !isKeyword(token->getTokenValue())) {
                        auto decl = parseVarDeclaration();
                        declarations.push_back(std::move(decl));
                    }
                }
            } else if (peekIs("procedure")) {
                declarations.push_back(parseProcedureDeclaration());
            } else if (peekIs("function")) {
                declarations.push_back(parseFunctionDeclaration());
            }
        }
        return declarations;
    }

    std::vector<std::unique_ptr<ASTNode>> PascalParser::parseStatementList() {
        std::vector<std::unique_ptr<ASTNode>> statements;
        while (!peekIs("end") && !peekIs("else") && !peekIs("until")) {
            statements.push_back(parseStatement());
            if (peekIs(";")) {
                next(); 
                if (peekIs("end") || peekIs("else") || peekIs("until")) {
                    break;
                }
            } else {
                break;
            }
        }
        return statements;
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

    bool PascalParser::isMulOperator() {
        return peekIs("*") || peekIs("/") || peekIs("div") || peekIs("mod");
    }

    bool PascalParser::isRelationalOperator() {
        return peekIs("=") || peekIs("<>") || peekIs("<") || peekIs("<=") || peekIs(">") || peekIs(">=");
    }

    std::string PascalParser::getRelationalOp() {
        if (peekIs("=")) return "=";
        if (peekIs("<>")) return "<>";
        if (peekIs("<=")) return "<=";
        if (peekIs(">=")) return ">=";
        if (peekIs("<")) return "<";
        if (peekIs(">")) return ">";
        error("Invalid relational operator");
        return "";
    }

    std::string PascalParser::tokenTypeToString(types::TokenType type) {
        switch (type) {
            case types::TokenType::TT_ID: return "identifier";
            case types::TokenType::TT_NUM: return "number";
            case types::TokenType::TT_STR: return "string";
            case types::TokenType::TT_SYM: return "symbol";
            default: return "unknown token";
        }
    }


    std::unique_ptr<ASTNode> PascalParser::parseRecordType() {
        expectToken("record");
        next();

        std::vector<std::unique_ptr<ASTNode>> fields;

        while (!peekIs("end")) {
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
            std::string fieldType = token->getTokenValue();
            next();
            expectToken(";");
            next();
            fields.push_back(std::make_unique<VarDeclNode>(std::move(identifiers), fieldType));
        }

        expectToken("end");
        next();

        return std::make_unique<RecordTypeNode>(std::move(fields));
    }

     std::unique_ptr<ASTNode> PascalParser::parseLValue() {
        int lineNum = token ? token->getLine() : 1;
        expectToken(types::TokenType::TT_ID);
        std::string name = token->getTokenValue();
        next();

        std::unique_ptr<ASTNode> left = std::make_unique<VariableNode>(name);
        left->setLineNumber(lineNum);

        while (true) {
            if (peekIs("[")) {
                next(); // consume '['
                auto index = parseExpression();
                expectToken("]");
                next(); // consume ']'
                if (auto varNode = dynamic_cast<VariableNode*>(left.get())) {
                    left = std::make_unique<ArrayAccessNode>(varNode->name, std::move(index));
                } else {
                    error("Array base must be an identifier");
                    return nullptr;
                }
                left->setLineNumber(lineNum);
            } else if (peekIs(".")) {
                next(); // consume '.'
                expectToken(types::TokenType::TT_ID);
                std::string fieldName = token->getTokenValue();
                next();
                left = std::make_unique<FieldAccessNode>(std::move(left), fieldName);
                left->setLineNumber(lineNum);
            } else {
                break;
            }
        }
        return left;
    }
}
