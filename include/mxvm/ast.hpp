#ifndef __AST_HPP_X_
#define __AST_HPP_X_

#include <memory>
#include <vector>
#include <string>
#include "mxvm/instruct.hpp"

namespace mxvm {
    
    class ASTVisitor;

    
    class ASTNode {
    public:
        virtual ~ASTNode() = default;
        virtual void accept(ASTVisitor& visitor) = 0;
        virtual std::string toString() const = 0;
    };

    
    class ProgramNode : public ASTNode {
    public:
        std::string name;
        std::vector<std::unique_ptr<ASTNode>> sections;
        
        void addSection(std::unique_ptr<ASTNode> section) {
            sections.push_back(std::move(section));
        }
        
        void accept(ASTVisitor& visitor) override;
        std::string toString() const override;
    };

    
    class SectionNode : public ASTNode {
    public:
        enum SectionType { DATA, CODE, MODULE };
        SectionType type;
        std::vector<std::unique_ptr<ASTNode>> statements;
        
        SectionNode(SectionType sectionType) : type(sectionType) {}
        
        void addStatement(std::unique_ptr<ASTNode> stmt) {
            statements.push_back(std::move(stmt));
        }
        
        void accept(ASTVisitor& visitor) override;
        std::string toString() const override;
    };

    
    class VariableNode : public ASTNode {
    public:
        std::string name;
        VarType type;
        std::string initialValue;
        bool hasInitializer;
        size_t buffer_size = 0;

        VariableNode(VarType varType, const std::string& varName) 
            : name(varName), type(varType), hasInitializer(false), buffer_size(0) {}
        
        VariableNode(VarType varType, const std::string& varName, size_t buf_size) 
            : name(varName), type(varType), hasInitializer(false), buffer_size(buf_size) {}
        
        VariableNode(VarType varType, const std::string& varName, const std::string& value)
            : name(varName), type(varType), initialValue(value), hasInitializer(true), buffer_size(0) {}
        
        void accept(ASTVisitor& visitor) override;
        std::string toString() const override;
    };

    // docbir.com

    class ModuleNode : public ASTNode {
    public:
        std::string name;
        ModuleNode(const std::string &n) : name(n) {}
        void accept(ASTVisitor& visitor) override;
        std::string toString() const override;
    };
    
    class InstructionNode : public ASTNode {
    public:
        Inc instruction;
        std::vector<Operand> operands;
        
        InstructionNode(Inc inst) : instruction(inst) {}
        
        InstructionNode(Inc inst, const std::vector<Operand>& ops) 
            : instruction(inst), operands(ops) {}
        
        void addOperand(const Operand& operand) {
            operands.push_back(operand);
        }
        
        void accept(ASTVisitor& visitor) override;
        std::string toString() const override;
    };

    
    class LabelNode : public ASTNode {
    public:
        std::string name;
        bool function = false;
        LabelNode(const std::string& labelName) : name(labelName) {}
        
        void accept(ASTVisitor& visitor) override;
        std::string toString() const override;
    };

    
    class ExpressionNode : public ASTNode {
    public:
        std::string value;
        bool isNumeric;
        
        ExpressionNode(const std::string& val, bool numeric = false) 
            : value(val), isNumeric(numeric) {}
        
        void accept(ASTVisitor& visitor) override;
        std::string toString() const override;
    };

    
    class CommentNode : public ASTNode {
    public:
        std::string text;  
        CommentNode(const std::string& commentText) : text(commentText) {}
        
        void accept(ASTVisitor& visitor) override;
        std::string toString() const override;
    };

    
    class ASTVisitor {
    public:
        virtual ~ASTVisitor() = default;
        virtual void visit(ProgramNode& node) = 0;
        virtual void visit(SectionNode& node) = 0;
        virtual void visit(InstructionNode& node) = 0;
        virtual void visit(LabelNode& node) = 0;
        virtual void visit(VariableNode& node) = 0;
        virtual void visit(ExpressionNode& node) = 0;
        virtual void visit(CommentNode& node) = 0;
        virtual void visit(ModuleNode &node) = 0;
    };
}

#endif