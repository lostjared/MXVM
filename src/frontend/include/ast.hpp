#ifndef _PASCAL_AST_H_
#define _PASCAL_AST_H_

#include <memory>
#include <vector>
#include <string>
#include <iostream>
#include <variant>

namespace pascal {

    class ASTVisitor;

    class ASTNode {
        int line_number = 0;
    public:
        virtual ~ASTNode() = default;
        virtual void accept(ASTVisitor& visitor) = 0;
        virtual std::string toString() const = 0;
        virtual void print(std::ostream& out, int indent = 0) const;
        void setLineNumber(int line) { line_number = line; }
        int getLineNumber() const { return line_number; }
    };

    
    class ExitNode :  public ASTNode {
    public:
        virtual void accept(ASTVisitor &visitor) override;
        virtual std::string toString() const override;
        std::unique_ptr<ASTNode> expr;
        ExitNode() = default;
        explicit ExitNode(std::unique_ptr<ASTNode> e) : expr(std::move(e)) {}
    };

    class ContinueNode :  public ASTNode {
    public:
        virtual void accept(ASTVisitor &visitor) override;
        virtual std::string toString() const override;
        std::unique_ptr<ASTNode> expr;
        ContinueNode() = default;
        explicit ContinueNode(std::unique_ptr<ASTNode> e) : expr(std::move(e)) {}
    };

    class BreakNode :  public ASTNode {
    public:
        virtual void accept(ASTVisitor &visitor) override;
        virtual std::string toString() const override;
        std::unique_ptr<ASTNode> expr;
        BreakNode() = default;
        explicit BreakNode(std::unique_ptr<ASTNode> e) : expr(std::move(e)) {}
    };

    class CompoundStmtNode; 

    class BlockNode : public ASTNode {
    public:
        std::vector<std::unique_ptr<ASTNode>> declarations;
        std::unique_ptr<CompoundStmtNode> compoundStatement;

        BlockNode(std::vector<std::unique_ptr<ASTNode>> decls, std::unique_ptr<CompoundStmtNode> stmt);

        void accept(ASTVisitor& visitor) override;
        std::string toString() const override;
    };

    class ProgramNode : public ASTNode {
    public:
        std::string name;
        std::unique_ptr<BlockNode> block;

        ProgramNode(const std::string& name, std::unique_ptr<BlockNode> blk);

        void accept(ASTVisitor& visitor) override;
        std::string toString() const override;
    };

    class VarDeclNode : public ASTNode {
    public:
        std::vector<std::string> identifiers;
        std::variant<std::string, std::unique_ptr<ASTNode>> type;
        std::vector<std::unique_ptr<ASTNode>> initializers;

        VarDeclNode(std::vector<std::string> identifiers,
                    std::variant<std::string, std::unique_ptr<ASTNode>> type,
                    std::vector<std::unique_ptr<ASTNode>> initializers)
            : identifiers(std::move(identifiers)), type(std::move(type)),
              initializers(std::move(initializers)) {}

        void accept(ASTVisitor& visitor) override;
        std::string toString() const override;
    };


    class TypeDeclNode : public ASTNode {
    public:
        std::vector<std::unique_ptr<ASTNode>> typeDeclarations;
        
        TypeDeclNode(std::vector<std::unique_ptr<ASTNode>> declarations) 
            : typeDeclarations(std::move(declarations)) {}
        
       void accept(ASTVisitor& visitor) override;
       std::string toString() const override;
    };

    class TypeAliasNode : public ASTNode {
    public:
        std::string typeName;
        std::string baseType;
        
        TypeAliasNode(const std::string& name, const std::string& base)
            : typeName(name), baseType(base) {}
       
       void accept(ASTVisitor& visitor) override;
        std::string toString() const override;
    };

    class ConstDeclNode : public ASTNode {
    public:
        struct ConstAssignment {
            std::string identifier;
            std::unique_ptr<ASTNode> value;
            
            ConstAssignment(const std::string& id, std::unique_ptr<ASTNode> val)
                : identifier(id), value(std::move(val)) {}
        };
        
        std::vector<std::unique_ptr<ConstAssignment>> assignments;  
        ConstDeclNode(std::vector<std::unique_ptr<ConstAssignment>> assigns)
            : assignments(std::move(assigns)) {}
        
        void accept(ASTVisitor& visitor) override;
        std::string toString() const override;
    };

    class FuncDeclNode : public ASTNode {
    public:
        std::string name;
        std::vector<std::unique_ptr<ASTNode>> parameters;
        std::string returnType;
        std::shared_ptr<ASTNode> block; 

        FuncDeclNode(const std::string& n, std::vector<std::unique_ptr<ASTNode>> p, const std::string& rt, std::unique_ptr<ASTNode> b)
            : name(n), parameters(std::move(p)), returnType(rt), block(std::move(b)) {}
        
        void accept(ASTVisitor& visitor) override;
        std::string toString() const override;
    };

    class ProcDeclNode : public ASTNode {
    public:
        std::string name;
        std::vector<std::unique_ptr<ASTNode>> parameters;
        std::shared_ptr<ASTNode> block; 

        ProcDeclNode(const std::string& n, std::vector<std::unique_ptr<ASTNode>> p, std::unique_ptr<ASTNode> b)
            : name(n), parameters(std::move(p)), block(std::move(b)) {}
        
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

    enum class VarType;

    class SimpleTypeNode : public ASTNode {
    public:
        std::string typeName;
        
        SimpleTypeNode(const std::string& typeName) : typeName(typeName) {}
        
        void accept(ASTVisitor& visitor) override;
        std::string toString() const override { return "SimpleType: " + typeName; }
    };

    class ArrayTypeNode : public ASTNode {
    public:
        std::unique_ptr<ASTNode> elementType;
        std::unique_ptr<ASTNode> lowerBound;
        std::unique_ptr<ASTNode> upperBound;

        ArrayTypeNode(std::unique_ptr<ASTNode> elementType,  
                    std::unique_ptr<ASTNode> lowerBound,
                    std::unique_ptr<ASTNode> upperBound)
            : elementType(std::move(elementType)), 
            lowerBound(std::move(lowerBound)), 
            upperBound(std::move(upperBound)) {}

         std::string toString() const override;
         void accept(ASTVisitor &visitor) override;
    };
    class ArrayDeclarationNode : public ASTNode {
    public:
        std::string name;
        std::unique_ptr<ArrayTypeNode> arrayType;
        std::vector<std::unique_ptr<ASTNode>> initializers;
        
        ArrayDeclarationNode(std::string name, std::unique_ptr<ArrayTypeNode> arrayType,
                            std::vector<std::unique_ptr<ASTNode>> initializers = {})
            : name(std::move(name)), arrayType(std::move(arrayType)), 
              initializers(std::move(initializers)) {}
        
        void accept(ASTVisitor& visitor) override;
        std::string toString() const override;
    };

    class ArrayTypeDeclarationNode : public ASTNode {
    public:
        std::string name;
        std::unique_ptr<ArrayTypeNode> arrayType;
        
        ArrayTypeDeclarationNode(const std::string& name, std::unique_ptr<ArrayTypeNode> arrayType)
            : name(name), arrayType(std::move(arrayType)) {}
        
        void accept(ASTVisitor& visitor) override;
        std::string toString() const override;
    };

    class ArrayAccessNode : public ASTNode {
    public:
        std::unique_ptr<ASTNode> base; 
        std::unique_ptr<ASTNode> index;
        
        ArrayAccessNode(std::unique_ptr<ASTNode> base, std::unique_ptr<ASTNode> index)
            : base(std::move(base)), index(std::move(index)) {}
     
        void accept(ASTVisitor& visitor) override;
        std::string toString() const override;
    };

    class ArrayAssignmentNode : public ASTNode {
    public:
        std::string arrayName;
        std::unique_ptr<ASTNode> index;
        std::unique_ptr<ASTNode> value;
        
        ArrayAssignmentNode(std::string arrayName, std::unique_ptr<ASTNode> index,
                           std::unique_ptr<ASTNode> value)
            : arrayName(std::move(arrayName)), index(std::move(index)), 
              value(std::move(value)) {}
        
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

    class CaseStmtNode : public ASTNode {
    public:
        struct CaseBranch {
            std::vector<std::unique_ptr<ASTNode>> values; 
            std::unique_ptr<ASTNode> statement;
            
            CaseBranch(std::vector<std::unique_ptr<ASTNode>> vals, std::unique_ptr<ASTNode> stmt)
                : values(std::move(vals)), statement(std::move(stmt)) {}
        };
        
        std::unique_ptr<ASTNode> expression;
        std::vector<std::unique_ptr<CaseBranch>> branches;
        std::unique_ptr<ASTNode> elseStatement;  
        
        CaseStmtNode(std::unique_ptr<ASTNode> expr, 
                    std::vector<std::unique_ptr<CaseBranch>> branches,
                    std::unique_ptr<ASTNode> elseStmt = nullptr)
            : expression(std::move(expr)), branches(std::move(branches)), elseStatement(std::move(elseStmt)) {}
        
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
        bool isReal;
        
        NumberNode(const std::string& val, bool integer = true, bool real = false) 
            : value(val), isInteger(integer), isReal(real) {}
        
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

    class RepeatStmtNode : public ASTNode {
    public:
        std::vector<std::unique_ptr<ASTNode>> statements;
        std::unique_ptr<ASTNode> condition;
        
        RepeatStmtNode(std::vector<std::unique_ptr<ASTNode>> stmts, std::unique_ptr<ASTNode> cond)
            : statements(std::move(stmts)), condition(std::move(cond)) {}
        
        void accept(ASTVisitor& visitor) override;
        
        std::string toString() const override {
            return "RepeatStmt";
        }
    };

    class RecordTypeNode : public ASTNode {
    public:
        std::vector<std::unique_ptr<ASTNode>> fields; 
        RecordTypeNode(std::vector<std::unique_ptr<ASTNode>> fields);
        void accept(ASTVisitor& visitor) override;
        std::string toString() const override;
    };

    class RecordDeclarationNode : public ASTNode {
    public:
        std::string name;
        std::unique_ptr<RecordTypeNode> recordType;
        RecordDeclarationNode(const std::string& name, std::unique_ptr<RecordTypeNode> recordType);
        void accept(ASTVisitor& visitor) override;
        std::string toString() const override;
    };

    class FieldAccessNode : public ASTNode {
    public:
        std::unique_ptr<ASTNode> recordExpr;
        std::string fieldName;
        FieldAccessNode(std::unique_ptr<ASTNode> recordExpr, const std::string& fieldName);
        void accept(ASTVisitor& visitor) override;
        std::string toString() const override;
    };

    class ASTVisitor {
    public:
        virtual ~ASTVisitor() = default;
        virtual void visit(ProgramNode& node) = 0;
        virtual void visit(BlockNode& node) = 0;
        virtual void visit(VarDeclNode& node) = 0;
        virtual void visit(ConstDeclNode& node) = 0;
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
        virtual void visit(RepeatStmtNode& node) = 0;
        virtual void visit(CaseStmtNode& node) = 0;
        virtual void visit(ArrayTypeNode& node) = 0;
        virtual void visit(ArrayDeclarationNode& node) = 0;
        virtual void visit(ArrayAccessNode& node) = 0;
        virtual void visit(ArrayAssignmentNode& node) = 0;
        virtual void visit(RecordTypeNode& node) = 0;
        virtual void visit(RecordDeclarationNode& node) = 0;
        virtual void visit(FieldAccessNode& node) = 0;
        virtual void visit(TypeDeclNode& node) = 0;
        virtual void visit(TypeAliasNode& node) = 0;
        virtual void visit(ExitNode &node) = 0;
        virtual void visit(SimpleTypeNode& node) = 0;
        virtual void visit(ArrayTypeDeclarationNode& node) = 0;
        virtual void visit(ContinueNode& node) = 0;
        virtual void visit(BreakNode&  node) = 0;
    };
} 

#endif