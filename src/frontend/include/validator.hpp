#ifndef __VALIDATOR_H_
#define __VALIDATOR_H_

#include "scanner/scanner.hpp"
#include "scanner/exception.hpp"
#include "scanner/types.hpp"
#include <string>
#include <unordered_set>
#include <unordered_map>

namespace mxx {

    struct Scope {
        std::unordered_set<std::string> vars;
        std::unordered_set<std::string> consts;
        std::unordered_set<std::string> types;
        std::unordered_set<std::string> funcs;
        std::unordered_set<std::string> procs;
        std::unordered_set<std::string> params;
    };

    class TPValidator {
    public:
        explicit TPValidator(const std::string& source_);

        bool validate(const std::string& name);
    private:
        std::unordered_map<std::string, std::unordered_set<std::string>> recordFieldScopesByType;
        std::string currentRecordTypeName;
        std::vector<Scope> scopeStack;
        scan::Scanner scanner;
        const scan::TToken* token = nullptr;
        std::string source;
        std::string filename;
        size_t index = 0;

        bool isPascalKeyword(const std::string& s) const;

        std::unordered_set<std::string> declaredVars;
        std::unordered_set<std::string> declaredConsts;
        std::unordered_set<std::string> declaredFuncs;
        std::unordered_set<std::string> declaredProcs;
        void pushScope();
        void popScope();
        Scope* currentScope();
        const Scope* currentScope() const;
        bool isVariableInCurrentScope(const std::string& name) const;
        std::string currentScopeName() const;


        bool isVarDeclaredHere(const std::string& name) const;
        bool isTypeDeclaredHere(const std::string& name) const;
        bool isFuncDeclaredHere(const std::string& name) const;
        bool isProcDeclaredHere(const std::string& name) const;
        bool isParamDeclaredHere(const std::string& name) const;
        void declareVar(const std::string& name, const scan::TToken* at);
        void declareConst(const std::string& name, const scan::TToken* at);
        void declareFunc(const std::string& name, const scan::TToken* at);
        void declareProc(const std::string& name, const scan::TToken* at);
        void declareParam(const std::string& name, const scan::TToken* at);


        void checkVar(const std::string& name, const scan::TToken* at);
        void checkVarOrConst(const std::string& name, const scan::TToken* at);
        void checkConstOnly(const std::string& name, const scan::TToken* at);

        void parseConstExpr(const std::unordered_set<std::string>& stops,
                            bool constOnly);

                            std::unordered_set<std::string> declaredTypes;

        bool isBuiltinType() const;
        void declareType(const std::string& name, const scan::TToken* at);
        void checkType(const std::string& name, const scan::TToken* at);
        bool isBuiltinConst(const std::string& name) const;
        bool next();
        bool match(const std::string& s) const;
        bool match(types::TokenType t) const;
        bool peekIs(const std::string& s);

        void require(const std::string& s);
        void require(types::TokenType t);
        void requireKW(const std::string& k);

        void fail(const std::string& msg);
        void failHere(const std::string& msg);
        void failAt(const scan::TToken* at, const std::string& msg);

        static std::string tokenTypeToString(types::TokenType t);
        static std::string lower(std::string s);
        std::string found() const;
        bool isKW(const std::string& k) const;

        void parseProgram();
        void parseUses();
        void parseBlock();
        void parseLabelSection();
        void parseConstSection();
        void parseTypeSection();
        void parseVarSection();
        void parseSubprogram();
        void parseFormalParams();
        void parseIdentList();
        void parseType(const std::string &);
        void parseTypeName();
        void parseSubrange();
        void parseConstant();
        void parseConstSimple();
        void parseCompoundStatement();
        void parseStatement();
        void parseIf();
        void parseWhile();
        void parseRepeat();
        void parseFor();
        void parseCase();
        void parseCaseLabelList();
        void parseCaseLabel();
        void parseWith();
        void parseGoto();
        void parseSimpleOrCallOrAssign();
        void parseDesignator();
        void parseActualParams();
        void parseExprStop(const std::unordered_set<std::string>& stops);

        bool isRelOp() const;
        bool isAddOp() const;
        bool isMulOp() const;
        bool isSetOp() const;
    private:
        static std::string toLower(const std::string& s);
        void predeclareType(const std::string& name); 
        bool isBuiltinTypeName(const std::string& key) const;
        void pushRecordFieldScope(const std::string& recordTypeName);
        void popRecordFieldScope();
        bool inRecordFieldScope() const;

        void declareRecordField(const std::string& name, const scan::TToken* at);
        void parseFieldIdentList();
    };

} 
#endif