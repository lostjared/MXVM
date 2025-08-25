#include "validator.hpp"
#include <cctype>
#include <unordered_set>

namespace mxx {

    TPValidator::TPValidator(const std::string& source_) : scanner(source_), source(source_) {}

    bool TPValidator::validate(const std::string& name) {
        filename = name;
        scanner.scan();
        index = 0;
        token = nullptr;
        declaredVars.clear();
        declaredConsts.clear();
        next();
        parseProgram();
        if (token) failHere("Unexpected tokens after end of program");
        return true;
    }

    bool TPValidator::match(const std::string& s) const {
        return token && token->getTokenValue() == s;
    }

    bool TPValidator::match(types::TokenType t) const {
        return token && token->getTokenType() == t;
    }

    bool TPValidator::next() {
        while (index < scanner.size() &&
            scanner[index].getTokenType() == types::TokenType::TT_SYM &&
            scanner[index].getTokenValue() == "\n") {
            ++index;
        }
        if (index < scanner.size()) {
            token = &scanner[index++];
            return true;
        }
        token = nullptr;
        return false;
    }

    bool TPValidator::peekIs(const std::string& s) {
        return index < scanner.size() && scanner[index].getTokenValue() == s;
    }

    void TPValidator::require(const std::string& s) {
        if (!match(s)) {
            if (token) failAt(token, "Required: " + s + " Found: " + found());
            fail("Required: " + s + " Found: EOF");
        }
    }

    void TPValidator::require(types::TokenType t) {
        if (!match(t)) {
            if (token) failAt(token, "Required: " + tokenTypeToString(t) + " Found: " + found());
            fail("Required: " + tokenTypeToString(t) + " Found: EOF");
        }
    }

    void TPValidator::requireKW(const std::string& k) {
        if (!isKW(k)) {
            if (token) failAt(token, "Required: " + k + " Found: " + found());
            fail("Required: " + k + " Found: EOF");
        }
    }

    void TPValidator::fail(const std::string& msg) {
        throw mx::Exception("Syntax Error in '" + filename + "': " + msg);
    }

    void TPValidator::failHere(const std::string& msg) {
        if (token) throw mx::Exception("Syntax Error in '" + filename + "': " + msg + " at line " + std::to_string(token->getLine()));
        throw mx::Exception("Syntax Error in '" + filename + "': " + msg);
    }

    void TPValidator::failAt(const scan::TToken* at, const std::string& msg) {
        if (at) throw mx::Exception("Syntax Error in '" + filename + "': " + msg + " at line " + std::to_string(at->getLine()));
        throw mx::Exception("Syntax Error in '" + filename + "': " + msg);
    }

    std::string TPValidator::tokenTypeToString(types::TokenType t) {
        switch (t) {
            case types::TokenType::TT_ID: return "Identifier";
            case types::TokenType::TT_NUM: return "Number";
            case types::TokenType::TT_HEX: return "Hex";
            case types::TokenType::TT_STR: return "String";
            case types::TokenType::TT_SYM: return "Symbol";
            default: return "Unknown";
        }
    }

    std::string TPValidator::lower(std::string s) {
        for (auto& c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    }

    std::string TPValidator::found() const {
        if (!token) return "EOF";
        return token->getTokenValue() + ":" + tokenTypeToString(token->getTokenType());
    }

    bool TPValidator::isKW(const std::string& k) const {
        if (!token || !isPascalKeyword(k) || token->getTokenType()!=types::TokenType::TT_ID) return false;
        return lower(token->getTokenValue()) == k;
    }

    void TPValidator::declareVar(const std::string& name, const scan::TToken* at) {
        auto key = lower(name);
        if (declaredVars.count(key)) failAt(at, "Redeclaration of variable '" + name + "'");
        declaredVars.insert(key);
    }

    void TPValidator::declareConst(const std::string& name, const scan::TToken* at) {
        auto key = lower(name);
        if (declaredConsts.count(key)) failAt(at, "Redeclaration of constant '" + name + "'");
        declaredConsts.insert(key);
    }
    
    bool TPValidator::isBuiltinConst(const std::string& name) const {
        std::string key = lower(name);
        return key == "true" || key == "false" || key == "nil";
    }

    void TPValidator::checkVar(const std::string& name, const scan::TToken* at) {
        auto key = lower(name);
        if (!declaredVars.count(key) &&
            !declaredFuncs.count(key) &&
            !declaredProcs.count(key) &&
            !isBuiltinConst(name) && !isPascalKeyword(key)) {
            failAt(at, "Use of undeclared identifier '" + name + "'");
        }
    }

    void TPValidator::checkVarOrConst(const std::string& name, const scan::TToken* at) {
        auto key = lower(name);
        if (!declaredVars.count(key) && !declaredConsts.count(key)
            && !declaredFuncs.count(key) && !declaredProcs.count(key)
            && !isBuiltinConst(name) && !isPascalKeyword(key)) {
            failAt(at, "Use of undeclared identifier '" + name + "'");
        }
    }

    void TPValidator::checkConstOnly(const std::string& name, const scan::TToken* at) {
        if (!declaredConsts.count(lower(name)) && !isBuiltinConst(name))
            failAt(at, "Constant expression requires constant '" + name + "'");
    }

    void TPValidator::parseProgram() {
        requireKW("program");
        next();
        require(types::TokenType::TT_ID);
        next();
        require(";");
        next();
        if (isKW("uses")) parseUses();
        parseBlock();
        require(".");
        next();
    }

    void TPValidator::parseUses() {
        requireKW("uses");
        do {
            next();
            require(types::TokenType::TT_ID);
            next();
            if (match(",")) next();
            else break;
        } while (true);
        require(";");
        next();
    }

    void TPValidator::parseBlock() {
        if (isKW("label")) parseLabelSection();
        if (isKW("const")) parseConstSection();
        if (isKW("type")) parseTypeSection();
        if (isKW("var")) parseVarSection();
        while (isKW("procedure") || isKW("function")) parseSubprogram();
        parseCompoundStatement();
    }

    void TPValidator::parseLabelSection() {
        requireKW("label");
        do {
            next();
            require(types::TokenType::TT_NUM);
            next();
            if (match(",")) next();
            else break;
        } while (true);
        require(";");
        next();
    }

    void TPValidator::parseConstSection() {
        requireKW("const");
        next();
        while (match(types::TokenType::TT_ID)) {
            if (isKW("type") || isKW("var") || isKW("label") ||
                isKW("begin") || isKW("procedure") || isKW("function")) {
                break;
            }
            const auto* nameTok = token;
            std::string name = token->getTokenValue();
            next();
            require("=");
            next();
            parseConstExpr({";"}, true);
            declareConst(name, nameTok);
            require(";");
            next();
        }
    }

    void TPValidator::parseTypeSection() {
        requireKW("type");
        next();
        while (match(types::TokenType::TT_ID)) {
            if (isKW("var") || isKW("const") || isKW("label") ||
                isKW("begin") || isKW("procedure") || isKW("function")) {
                break;
            }
            const auto* nameTok = token;
            std::string name = token->getTokenValue();
            next();
            require("=");
            next();
            parseType();
            declareType(name, nameTok);
            require(";");
            next();
        }
    }

    void TPValidator::parseVarSection() {
        requireKW("var");
        next();
        while (match(types::TokenType::TT_ID)) {
            if (isKW("procedure") || isKW("function") || isKW("begin")) {
                break;
            }
            declareVar(token->getTokenValue(), token);
            next();
            while (match(",")) {
                next();
                require(types::TokenType::TT_ID);
                declareVar(token->getTokenValue(), token);
                next();
            }
            require(":");
            next();
            parseType();
            if (match("=")) {
                next();
                parseConstExpr({";"}, false);
            }
            require(";");
            next();
        }
    }
    void TPValidator::parseSubprogram() {
        if (isKW("procedure")) {
            requireKW("procedure");
            next();
            require(types::TokenType::TT_ID);
            declaredProcs.insert(lower(token->getTokenValue()));
            next();
            if (match("(")) parseFormalParams();
            require(";");
            next();
            if (isKW("forward")) {
                next();
                require(";");
                next();
                return;
            }
            parseBlock();
            require(";");
            next();
        } else {
            requireKW("function");
            next();
            require(types::TokenType::TT_ID);
            declaredFuncs.insert(lower(token->getTokenValue()));
            next();
            if (match("(")) parseFormalParams();
            require(":");
            next();
            parseTypeName();
            require(";");
            next();
            if (isKW("forward")) {
                next();
                require(";");
                next();
                return;
            }
            parseBlock();
            require(";");
            next();
        }
    }

    void TPValidator::parseFormalParams() {
        require("(");
        next();
        if (match(")")) { next(); return; }
        while (true) {
            bool byRef = isKW("var");
            if (byRef) next();
            parseIdentList();
            require(":");
            next();
            parseType();
            if (match(";")) { next(); continue; }
            require(")");
            next();
            break;
        }
    }

    void TPValidator::parseIdentList() {
        require(types::TokenType::TT_ID);
        declareVar(token->getTokenValue(), token);
        next();
        while (match(",")) {
            next();
            require(types::TokenType::TT_ID);
            declareVar(token->getTokenValue(), token);
            next();
        }
    }

    void TPValidator::parseType() {
        if (isKW("array")) {
            next();
            require("["); next();
            parseSubrange();
            while (match(",")) { next(); parseSubrange(); }
            require("]"); next();
            requireKW("of"); next();
            parseType();
            return;
        }
        if (isKW("record")) {
            next();
            while (!isKW("end")) {
                parseIdentList();
                require(":"); next();
                parseType();
                require(";"); next();
            }
            requireKW("end"); next();
            return;
        }
        if (isKW("set")) {
            next();
            requireKW("of"); next();
            parseTypeName();
            return;
        }
        if (match("^")) {
            next();
            parseTypeName();
            return;
        }
        if (isKW("string")) {
            next();
            if (match("[")) {
                next();
                parseConstExpr({"]"}, true);
                require("]"); next();
            }
            return;
        }
        if (isBuiltinType()) { next(); return; }
        parseTypeName();
    }

    void TPValidator::parseTypeName() {
        require(types::TokenType::TT_ID);
        std::string name = token->getTokenValue();
        if (!isBuiltinType()) checkType(name, token);
        next();
    }

    void TPValidator::parseSubrange() {
        parseConstExpr({"..", "]", ","}, true);
        if (match("..")) {
            next();
            parseConstExpr({"]", ","}, true);
        }
    }

    void TPValidator::parseConstant() {
        if (match("-")) { next(); parseConstSimple(); return; }
        parseConstSimple();
    }

    void TPValidator::parseConstSimple() {
        if (match(types::TokenType::TT_NUM) || match(types::TokenType::TT_HEX) || match(types::TokenType::TT_STR)) { next(); return; }
        if (isKW("true") || isKW("false") || isKW("nil")) { next(); return; }
        require(types::TokenType::TT_ID);
        checkConstOnly(token->getTokenValue(), token);
        next();
    }

    void TPValidator::parseCompoundStatement() {
        requireKW("begin");
        next();
        if (!isKW("end")) {
            parseStatement();
            while (match(";")) {
                next();
                if (isKW("end")) break;
                parseStatement();
            }
        }
        requireKW("end");
        next();
    }

    void TPValidator::parseStatement() {
        if (!token) failHere("Unexpected EOF in statement");
        if (isKW("begin")) { parseCompoundStatement(); return; }
        if (isKW("if")) { parseIf(); return; }
        if (isKW("while")) { parseWhile(); return; }
        if (isKW("repeat")) { parseRepeat(); return; }
        if (isKW("for")) { parseFor(); return; }
        if (isKW("case")) { parseCase(); return; }
        if (isKW("with")) { parseWith(); return; }
        if (isKW("goto")) { parseGoto(); return; }
        if (match(types::TokenType::TT_NUM)) failHere("Statement cannot start with number");
        if (match(types::TokenType::TT_ID)) { parseSimpleOrCallOrAssign(); return; }
        if (match(";") || match(")")) return;
        if (isKW("end")) return;
        failHere("Invalid statement start");
    }

    void TPValidator::parseIf() {
        requireKW("if");
        next();
        parseExprStop({"then"});
        requireKW("then");
        next();
        parseStatement();
        if (isKW("else")) {
            next();
            parseStatement();
        }
    }

    void TPValidator::parseWhile() {
        requireKW("while");
        next();
        parseExprStop({"do"});
        requireKW("do");
        next();
        parseStatement();
    }

    void TPValidator::parseRepeat() {
        requireKW("repeat");
        next();
        while (true) {
            if (isKW("until")) {
                break; 
            }
            parseStatement();
            if (match(";")) {
                next();
                if (isKW("until")) {
                    break;
                }
            } else if (isKW("until")) {
                break; 
            } else {
                failHere("Expected ';' or 'until'");
            }
        }
        requireKW("until");
        next();
        parseExprStop({";", "end"});
    }

    void TPValidator::parseFor() {
        requireKW("for");
        next();
        require(types::TokenType::TT_ID);
        checkVar(token->getTokenValue(), token);
        next();
        require(":=");
        next();
        parseExprStop({"to","downto"});
        if (isKW("to")) next();
        else { requireKW("downto"); next(); }
        parseExprStop({"do"});
        requireKW("do");
        next();
        parseStatement();
    }

    void TPValidator::parseCase() {
        requireKW("case");
        next();
        parseExprStop({"of"});
        requireKW("of");
        next();
        while (!isKW("end")) {
            parseCaseLabelList();
            require(":");
            next();
            parseStatement();
            if (match(";")) next();
            if (isKW("else")) {
                next();
                parseStatement();
                if (match(";")) next();
                break;
            }
            if (isKW("end")) break;
        }
        requireKW("end");
        next();
    }

    void TPValidator::parseCaseLabelList() {
        parseCaseLabel();
        while (match(",")) { next(); parseCaseLabel(); }
    }

    void TPValidator::parseCaseLabel() {
        if (match("-")) next();
        if (match(types::TokenType::TT_NUM) || match(types::TokenType::TT_STR) || match(types::TokenType::TT_ID)) {
            const auto* at = token;
            next();
            if (match("..")) {
                next();
                if (match("-")) next();
                if (match(types::TokenType::TT_NUM) || match(types::TokenType::TT_STR) || match(types::TokenType::TT_ID)) {
                    next();
                    return;
                }
                failAt(at, "Invalid case label range");
            }
            return;
        }
        failHere("Invalid case label");
    }

    void TPValidator::parseWith() {
        requireKW("with");
        next();
        parseDesignator();
        while (match(",")) { next(); parseDesignator(); }
        requireKW("do");
        next();
        parseStatement();
    }

    void TPValidator::parseGoto() {
        requireKW("goto");
        next();
        require(types::TokenType::TT_NUM);
        next();
    }

    void TPValidator::parseSimpleOrCallOrAssign() {
        if (match(types::TokenType::TT_ID) && peekIs("(")) {
            next();
            parseActualParams();
            return;
        }
        parseDesignator();
        if (match(":=")) {
            next();
            parseExprStop({";","end","else",")","]","until","of","do","then"});
            return;
        }
        if (match("(")) {
            parseActualParams();
            return;
        }
    }

    void TPValidator::parseDesignator() {
        require(types::TokenType::TT_ID);
        checkVar(token->getTokenValue(), token);
        next();
        while (true) {
            if (match(".")) {
                next();
                require(types::TokenType::TT_ID);
                next();
                continue;
            }
            if (match("[")) {
                next();
                parseExprStop({"]"});
                while (match(",")) { next(); parseExprStop({"]"}); }
                require("]");
                next();
                continue;
            }
            if (match("^")) {
                next();
                continue;
            }
            break;
        }
    }

    void TPValidator::parseActualParams() {
        require("(");
        next();
        if (match(")")) { next(); return; }
        while (true) {
            if (isKW("var")) next();
            parseExprStop({")",";",","});
            if (match(",")) { next(); continue; }
            require(")");
            next();
            break;
        }
    }

    void TPValidator::parseExprStop(const std::unordered_set<std::string>& stops) {
        int paren = 0, bracket = 0;
        bool expectOperand = true;
        while (token) {
            if (paren==0 && bracket==0 && token->getTokenType()==types::TokenType::TT_ID) {
                if (stops.count(lower(token->getTokenValue()))) return;
            }
            if (paren==0 && bracket==0 && token->getTokenType()==types::TokenType::TT_SYM) {
                if (stops.count(token->getTokenValue())) return;
            }
            if (match("(")) { ++paren; next(); expectOperand = true; continue; }
            if (match(")")) { if (paren<=0) break; --paren; next(); expectOperand = false; continue; }
            if (match("[")) { ++bracket; next(); expectOperand = true; continue; }
            if (match("]")) { if (bracket<=0) break; --bracket; next(); expectOperand = false; continue; }
            if (expectOperand) {
                if (match("+")||match("-")||isKW("not")||match("@")) { next(); continue; }
                if (match(types::TokenType::TT_NUM) || match(types::TokenType::TT_HEX) || match(types::TokenType::TT_STR)) { next(); expectOperand=false; continue; }
                if (match(types::TokenType::TT_ID)) {
                    const auto* at = token;
                    std::string name = token->getTokenValue();
                    next();
                    if (match("(")) {
                        parseActualParams();
                        expectOperand = false;
                        continue;
                    }
                    checkVarOrConst(name, at);
                    expectOperand = false;
                    continue;
                }
                failHere("Invalid expression");
            } else {
                if (isRelOp() || isAddOp() || isMulOp() || isSetOp()) { next(); expectOperand=true; continue; }
                if (match("^") || match(".")) { next(); expectOperand=true; continue; }
                if (match("(")) { ++paren; next(); expectOperand=true; continue; }
                if (match("[")) { ++bracket; next(); expectOperand=true; continue; }
                return;
            }
        }
    }

    void TPValidator::parseConstExpr(const std::unordered_set<std::string>& stops, bool constOnly) {
        int paren = 0;
        bool expectOperand = true;
        while (token) {
            if (paren==0) {
                if (token->getTokenType()==types::TokenType::TT_ID && stops.count(lower(token->getTokenValue()))) return;
                if (token->getTokenType()==types::TokenType::TT_SYM && stops.count(token->getTokenValue())) return;
            }
            if (match("(")) { ++paren; next(); expectOperand = true; continue; }
            if (match(")")) { if (paren<=0) break; --paren; next(); expectOperand = false; continue; }
            if (expectOperand) {
                if (match("+")||match("-")||isKW("not")) { next(); continue; }
                if (match(types::TokenType::TT_NUM) || match(types::TokenType::TT_HEX) || match(types::TokenType::TT_STR)) { next(); expectOperand=false; continue; }
                if (match(types::TokenType::TT_ID)) {
                    const auto* at = token;
                    if (constOnly) checkConstOnly(token->getTokenValue(), token);
                    next();
                    if (match("(")) failAt(at, "Function call not allowed in constant expression");
                    expectOperand = false;
                    continue;
                }
                failHere("Invalid constant expression");
            } else {
                if (match("+")||match("-")||match("*")||match("/") ||
                    isKW("div")||isKW("mod")||isKW("and")||isKW("or")||isKW("xor") ||
                    match("<")||match("<=")||match(">")||match(">=")||match("=")||match("<>")) { next(); expectOperand=true; continue; }
                return;
            }
        }
    }

    bool TPValidator::isRelOp() const { return isKW("in") || match("=")||match("<>")||match("<")||match("<=")||match(">")||match(">="); }
    bool TPValidator::isAddOp() const { return match("+")||match("-")||isKW("or")||isKW("xor"); }
    bool TPValidator::isMulOp() const { return match("*")||match("/")||isKW("div")||isKW("mod")||isKW("and")||isKW("shl")||isKW("shr"); }
    bool TPValidator::isSetOp() const { return isKW("union")||isKW("exclude")||isKW("symdiff"); }

    bool TPValidator::isBuiltinType() const {
        return isKW("integer") || isKW("real") || isKW("boolean") || isKW("char") ||
            isKW("byte") || isKW("word") || isKW("longint") || isKW("shortint") ||
            isKW("smallint") || isKW("cardinal") || isKW("string") || isKW("text");
    }

    void TPValidator::declareType(const std::string& name, const scan::TToken* at) {
        auto key = lower(name);
        if (declaredTypes.count(key)) failAt(at, "Redeclaration of type '" + name + "'");
        declaredTypes.insert(key);
    }

    void TPValidator::checkType(const std::string& name, const scan::TToken* at) {
        if (!declaredTypes.count(lower(name))) failAt(at, "Unknown type identifier '" + name + "'");
    }
    
    static const std::unordered_set<std::string> pascal_keywords = {
        "program", "var", "const", "type", "procedure", "function", "begin", "end",
        "if", "then", "else", "while", "do", "for", "to", "downto", "repeat", "until",
        "case", "of", "with", "goto",

        "div", "mod", "and", "or", "not", "in",
        "integer", "real", "boolean", "char", "byte", "word", "longint", "shortint",
        "smallint", "cardinal", "string", "text", "double", "single", "extended",
        "comp", "currency", "ptr", "array", "record", "set"
    };

    bool TPValidator::isPascalKeyword(const std::string& s) const {
        return pascal_keywords.count(lower(s));
    }

}
