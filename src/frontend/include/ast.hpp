#ifndef _PASCAL_AST_H_
#define _PASCAL_AST_H_

#include <memory>
#include <vector>
#include <string>
#include <iostream>

namespace pascal {

    class ASTVisitor;

    class ASTNode {
    public:
        virtual ~ASTNode() = default;
        virtual void accept(ASTVisitor& visitor) = 0;
        virtual std::string toString() const = 0;
        virtual void print(std::ostream& out, int indent = 0) const;
    };

    class ProgramNode : public ASTNode {
    public:
        std::string name;
        std::unique_ptr<ASTNode> block;
        
        ProgramNode(const std::string& programName, std::unique_ptr<ASTNode> programBlock)
            : name(programName), block(std::move(programBlock)) {}
        
        void accept(ASTVisitor& visitor) override;
        std::string toString() const override;
    };

    class BlockNode : public ASTNode {
    public:
        std::vector<std::unique_ptr<ASTNode>> declarations;
        std::unique_ptr<ASTNode> compoundStatement;
        
        BlockNode(std::vector<std::unique_ptr<ASTNode>> decls, std::unique_ptr<ASTNode> compound)
            : declarations(std::move(decls)), compoundStatement(std::move(compound)) {}
        
        void accept(ASTVisitor& visitor) override;
        std::string toString() const override;
    };

    class VarDeclNode : public ASTNode {
    public:
        std::vector<std::string> identifiers;
        std::string type;
        std::vector<std::unique_ptr<ASTNode>> initializers; 
        
        VarDeclNode(std::vector<std::string> ids, std::string t)
            : identifiers(std::move(ids)), type(std::move(t)) {}
        
        VarDeclNode(std::vector<std::string> ids, std::string t, 
                    std::vector<std::unique_ptr<ASTNode>> inits)
            : identifiers(std::move(ids)), type(std::move(t)), 
              initializers(std::move(inits)) {}
        
        void accept(ASTVisitor& visitor) override;
        std::string toString() const override;
    };

    class ProcDeclNode : public ASTNode {
    public:
        std::string name;
        std::vector<std::unique_ptr<ASTNode>> parameters;
        std::unique_ptr<ASTNode> block;
        
        ProcDeclNode(const std::string& procName, 
                     std::vector<std::unique_ptr<ASTNode>> params,
                     std::unique_ptr<ASTNode> procBlock)
            : name(procName), parameters(std::move(params)), block(std::move(procBlock)) {}
        
        void accept(ASTVisitor& visitor) override;
        std::string toString() const override;
    };

    class FuncDeclNode : public ASTNode {
    public:
        std::string name;
        std::vector<std::unique_ptr<ASTNode>> parameters;
        std::string returnType;
        std::unique_ptr<ASTNode> block;
        
        FuncDeclNode(const std::string& funcName,
                     std::vector<std::unique_ptr<ASTNode>> params,
                     const std::string& retType,
                     std::unique_ptr<ASTNode> funcBlock)
            : name(funcName), parameters(std::move(params)), returnType(retType), block(std::move(funcBlock)) {}
        
        void accept(ASTVisitor& visitor) override;
        std::string toString() const override;
    };

    class ParameterNode : public ASTNode {
    public:
        std::vector<std::string> identifiers;
        std::string type;
        bool isVar;
        
        ParameterNode(std::vector<std::string> ids, const std::string& paramType, bool var = false)
            : identifiers(std::move(ids)), type(paramType), isVar(var) {}
        
        void accept(ASTVisitor& visitor) override;
        std::string toString() const override;
    };

    class CompoundStmtNode : public ASTNode {
    public:
        std::vector<std::unique_ptr<ASTNode>> statements;
        
        CompoundStmtNode(std::vector<std::unique_ptr<ASTNode>> stmts)
            : statements(std::move(stmts)) {}
        
        void accept(ASTVisitor& visitor) override;
        std::string toString() const override;
    };

    class AssignmentNode : public ASTNode {
    public:
        std::unique_ptr<ASTNode> variable;
        std::unique_ptr<ASTNode> expression;
        
        AssignmentNode(std::unique_ptr<ASTNode> var, std::unique_ptr<ASTNode> expr)
            : variable(std::move(var)), expression(std::move(expr)) {}
        
        void accept(ASTVisitor& visitor) override;
        std::string toString() const override;
    };

    class IfStmtNode : public ASTNode {
    public:
        std::unique_ptr<ASTNode> condition;
        std::unique_ptr<ASTNode> thenStatement;
        std::unique_ptr<ASTNode> elseStatement; 
        
        IfStmtNode(std::unique_ptr<ASTNode> cond, 
                   std::unique_ptr<ASTNode> thenStmt,
                   std::unique_ptr<ASTNode> elseStmt = nullptr)
            : condition(std::move(cond)), thenStatement(std::move(thenStmt)), elseStatement(std::move(elseStmt)) {}
        
        void accept(ASTVisitor& visitor) override;
        std::string toString() const override;
    };

    class WhileStmtNode : public ASTNode {
    public:
        std::unique_ptr<ASTNode> condition;
        std::unique_ptr<ASTNode> statement;
        
        WhileStmtNode(std::unique_ptr<ASTNode> cond, std::unique_ptr<ASTNode> stmt)
            : condition(std::move(cond)), statement(std::move(stmt)) {}
        
        void accept(ASTVisitor& visitor) override;
        std::string toString() const override;
    };

    class ForStmtNode : public ASTNode {
    public:
        std::string variable;
        std::unique_ptr<ASTNode> startValue;
        std::unique_ptr<ASTNode> endValue;
        bool isDownto;
        std::unique_ptr<ASTNode> statement;
        
        ForStmtNode(const std::string& var,
                    std::unique_ptr<ASTNode> start,
                    std::unique_ptr<ASTNode> end,
                    bool downto,
                    std::unique_ptr<ASTNode> stmt)
            : variable(var), startValue(std::move(start)), endValue(std::move(end)), 
              isDownto(downto), statement(std::move(stmt)) {}
        
        void accept(ASTVisitor& visitor) override;
        std::string toString() const override;
    };

    class ProcCallNode : public ASTNode {
    public:
        std::string name;
        std::vector<std::unique_ptr<ASTNode>> arguments;
        
        ProcCallNode(const std::string& procName, std::vector<std::unique_ptr<ASTNode>> args)
            : name(procName), arguments(std::move(args)) {}
        
        void accept(ASTVisitor& visitor) override;
        std::string toString() const override;
    };

    class BinaryOpNode : public ASTNode {
    public:
        enum OpType { PLUS, MINUS, MULTIPLY, DIVIDE, DIV, MOD, EQUAL, NOT_EQUAL, LESS, LESS_EQUAL, GREATER, GREATER_EQUAL, AND, OR };
        
        
        std::unique_ptr<ASTNode> left;   
        OpType operator_;                
        std::unique_ptr<ASTNode> right;  
        
        BinaryOpNode(std::unique_ptr<ASTNode> l, OpType op, std::unique_ptr<ASTNode> r)
            : left(std::move(l)), operator_(op), right(std::move(r)) {}
        
        BinaryOpNode(OpType op, std::unique_ptr<ASTNode> l, std::unique_ptr<ASTNode> r)
            : left(std::move(l)), operator_(op), right(std::move(r)) {}
        
        void accept(ASTVisitor& visitor) override;
        std::string toString() const override;
        
        static std::string opToString(OpType op);
    };

    class UnaryOpNode : public ASTNode {
    public:
        enum Operator { PLUS, MINUS, NOT };
        
        Operator operator_;
        std::unique_ptr<ASTNode> operand;
        
        UnaryOpNode(Operator op, std::unique_ptr<ASTNode> operandNode)
            : operator_(op), operand(std::move(operandNode)) {}
        
        void accept(ASTVisitor& visitor) override;
        std::string toString() const override;
        
        static std::string opToString(Operator op);
    };

    class FuncCallNode : public ASTNode {
    public:
        std::string name;
        std::vector<std::unique_ptr<ASTNode>> arguments;
        
        FuncCallNode(const std::string& funcName, std::vector<std::unique_ptr<ASTNode>> args)
            : name(funcName), arguments(std::move(args)) {}
        
        void accept(ASTVisitor& visitor) override;
        std::string toString() const override;
    };

    class VariableNode : public ASTNode {
    public:
        std::string name;
        
        VariableNode(const std::string& varName) : name(varName) {}
        
        void accept(ASTVisitor& visitor) override;
        std::string toString() const override;
    };

    class NumberNode : public ASTNode {
    public:
        std::string value;
        bool isInteger;
        
        NumberNode(const std::string& val, bool integer = true) 
            : value(val), isInteger(integer) {}
        
        void accept(ASTVisitor& visitor) override;
        std::string toString() const override;
    };

    class StringNode : public ASTNode {
    public:
        std::string value;
        
        StringNode(const std::string& val) : value(val) {}
        
        void accept(ASTVisitor& visitor) override;
        std::string toString() const override;
    };

    class BooleanNode : public ASTNode {
    public:
        bool value;
        
        BooleanNode(bool val) : value(val) {}
        
        void accept(ASTVisitor& visitor) override;
        std::string toString() const override;
    };

    class EmptyStmtNode : public ASTNode {
    public:
        EmptyStmtNode() {}
        
        void accept(ASTVisitor& visitor) override;
        std::string toString() const override;
    };

    class ASTVisitor {
    public:
        virtual ~ASTVisitor() = default;
        virtual void visit(ProgramNode& node) = 0;
        virtual void visit(BlockNode& node) = 0;
        virtual void visit(VarDeclNode& node) = 0;
        virtual void visit(ProcDeclNode& node) = 0;
        virtual void visit(FuncDeclNode& node) = 0;
        virtual void visit(ParameterNode& node) = 0;
        virtual void visit(CompoundStmtNode& node) = 0;
        virtual void visit(AssignmentNode& node) = 0;
        virtual void visit(IfStmtNode& node) = 0;
        virtual void visit(WhileStmtNode& node) = 0;
        virtual void visit(ForStmtNode& node) = 0;
        virtual void visit(ProcCallNode& node) = 0;
        virtual void visit(BinaryOpNode& node) = 0;
        virtual void visit(UnaryOpNode& node) = 0;
        virtual void visit(FuncCallNode& node) = 0;
        virtual void visit(VariableNode& node) = 0;
        virtual void visit(NumberNode& node) = 0;
        virtual void visit(StringNode& node) = 0;
        virtual void visit(BooleanNode& node) = 0;
        virtual void visit(EmptyStmtNode& node) = 0;
    };

    class PrettyPrintVisitor : public ASTVisitor {
    private:
        std::ostream& out;
        int indentLevel;
        
        void printIndent();
        
    public:
        PrettyPrintVisitor(std::ostream& output) : out(output), indentLevel(0) {}
        
        void visit(ProgramNode& node) override;
        void visit(BlockNode& node) override;
        void visit(VarDeclNode& node) override;
        void visit(ProcDeclNode& node) override;
        void visit(FuncDeclNode& node) override;
        void visit(ParameterNode& node) override;
        void visit(CompoundStmtNode& node) override;
        void visit(AssignmentNode& node) override;
        void visit(IfStmtNode& node) override;
        void visit(WhileStmtNode& node) override;
        void visit(ForStmtNode& node) override;
        void visit(ProcCallNode& node) override;
        void visit(BinaryOpNode& node) override;
        void visit(UnaryOpNode& node) override;
        void visit(FuncCallNode& node) override;
        void visit(VariableNode& node) override;
        void visit(NumberNode& node) override;
        void visit(StringNode& node) override;
        void visit(BooleanNode& node) override;
        void visit(EmptyStmtNode& node) override;
    };

} 

#endif