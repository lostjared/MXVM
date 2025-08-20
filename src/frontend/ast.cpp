#include "ast.hpp"
#include <sstream>

namespace pascal {

    void ASTNode::print(std::ostream& out, int indent) const {
        for (int i = 0; i < indent; ++i) out << "  ";
        out << toString() << std::endl;
    }

    void ProgramNode::accept(ASTVisitor& visitor) {
        visitor.visit(*this);
    }

    std::string ProgramNode::toString() const {
        return "Program: " + name;
    }

    void BlockNode::accept(ASTVisitor& visitor) {
        visitor.visit(*this);
    }

    std::string BlockNode::toString() const {
        return "Block with " + std::to_string(declarations.size()) + " declarations";
    }

    void VarDeclNode::accept(ASTVisitor& visitor) {
        visitor.visit(*this);
    }

    std::string VarDeclNode::toString() const {
        std::ostringstream oss;
        oss << "VarDecl: ";
        for (size_t i = 0; i < identifiers.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << identifiers[i];
        }
        oss << " : " << type;
        return oss.str();
    }

    void ProcDeclNode::accept(ASTVisitor& visitor) {
        visitor.visit(*this);
    }

    std::string ProcDeclNode::toString() const {
        return "ProcDecl: " + name + " (" + std::to_string(parameters.size()) + " params)";
    }

    void FuncDeclNode::accept(ASTVisitor& visitor) {
        visitor.visit(*this);
    }

    std::string FuncDeclNode::toString() const {
        return "FuncDecl: " + name + " -> " + returnType;
    }

    void ParameterNode::accept(ASTVisitor& visitor) {
        visitor.visit(*this);
    }

    std::string ParameterNode::toString() const {
        std::ostringstream oss;
        oss << "Param: ";
        if (isVar) oss << "var ";
        for (size_t i = 0; i < identifiers.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << identifiers[i];
        }
        oss << " : " << type;
        return oss.str();
    }

    void CompoundStmtNode::accept(ASTVisitor& visitor) {
        visitor.visit(*this);
    }

    std::string CompoundStmtNode::toString() const {
        return "CompoundStmt: " + std::to_string(statements.size()) + " statements";
    }

    void AssignmentNode::accept(ASTVisitor& visitor) {
        visitor.visit(*this);
    }

    std::string AssignmentNode::toString() const {
        return "Assignment";
    }

    void IfStmtNode::accept(ASTVisitor& visitor) {
        visitor.visit(*this);
    }

    std::string IfStmtNode::toString() const {
        std::string s;
        if(elseStatement != nullptr) {
            s = "with else";
        }
        return "IfStmt " + s;
    }

    void WhileStmtNode::accept(ASTVisitor& visitor) {
        visitor.visit(*this);
    }

    std::string WhileStmtNode::toString() const {
        return "WhileStmt";
    }

    void ForStmtNode::accept(ASTVisitor& visitor) {
        visitor.visit(*this);
    }

    std::string ForStmtNode::toString() const {
        return "ForStmt: " + variable + (isDownto ? " downto" : " to");
    }

    void ProcCallNode::accept(ASTVisitor& visitor) {
        visitor.visit(*this);
    }

    std::string ProcCallNode::toString() const {
        return "ProcCall: " + name + " (" + std::to_string(arguments.size()) + " args)";
    }

    void BinaryOpNode::accept(ASTVisitor& visitor) {
        visitor.visit(*this);
    }

    std::string BinaryOpNode::toString() const {
        return "BinaryOp: " + opToString(operator_);
    }

    std::string BinaryOpNode::opToString(OpType op) {
        switch (op) {
            case PLUS: return "+";
            case MINUS: return "-";
            case MULTIPLY: return "*";
            case DIVIDE: return "/";
            case DIV: return "div";
            case MOD: return "mod";
            case EQUAL: return "=";
            case NOT_EQUAL: return "<>";
            case LESS: return "<";
            case LESS_EQUAL: return "<=";
            case GREATER: return ">";
            case GREATER_EQUAL: return ">=";
            case AND: return "and";
            case OR: return "or";
            default: return "?";
        }
    }

    void UnaryOpNode::accept(ASTVisitor& visitor) {
        visitor.visit(*this);
    }

    std::string UnaryOpNode::toString() const {
        std::string op;
        switch (operator_) {
            case PLUS: op = "+"; break;
            case MINUS: op = "-"; break;
            case NOT: op = "not"; break;
        }
        return "UnaryOp: " + op;
    }

    void FuncCallNode::accept(ASTVisitor& visitor) {
        visitor.visit(*this);
    }

    std::string FuncCallNode::toString() const {
        return "FuncCall: " + name + " (" + std::to_string(arguments.size()) + " args)";
    }

    void VariableNode::accept(ASTVisitor& visitor) {
        visitor.visit(*this);
    }

    std::string VariableNode::toString() const {
        return "Variable: " + name;
    }

    void NumberNode::accept(ASTVisitor& visitor) {
        visitor.visit(*this);
    }

    std::string NumberNode::toString() const {
        return "Number: " + value + (isInteger ? " (int)" : " (real)");
    }

    void StringNode::accept(ASTVisitor& visitor) {
        visitor.visit(*this);
    }

    std::string StringNode::toString() const {
        return "String: \"" + value + "\"";
    }

    void BooleanNode::accept(ASTVisitor& visitor) {
        visitor.visit(*this);
    }

    std::string BooleanNode::toString() const {
        std::string s;
        if(value) {
            s = "true";
        } else {
            s = "false";
        }
        return "Boolean: " + s;
    }

    void EmptyStmtNode::accept(ASTVisitor& visitor) {
        visitor.visit(*this);
    }

    std::string EmptyStmtNode::toString() const {
        return "EmptyStmt";
    }

    void PrettyPrintVisitor::printIndent() {
        for (int i = 0; i < indentLevel; ++i) {
            out << "  ";
        }
    }

    void PrettyPrintVisitor::visit(ProgramNode& node) {
        printIndent();
        out << "program " << node.name << ";" << std::endl;
        indentLevel++;
        node.block->accept(*this);
        indentLevel--;
        printIndent();
        out << "." << std::endl;
    }

    void PrettyPrintVisitor::visit(BlockNode& node) {
        for (auto& decl : node.declarations) {
            decl->accept(*this);
        }
        node.compoundStatement->accept(*this);
    }

    void PrettyPrintVisitor::visit(VarDeclNode& node) {
        printIndent();
        out << "var ";
        for (size_t i = 0; i < node.identifiers.size(); ++i) {
            if (i > 0) out << ", ";
            out << node.identifiers[i];
        }
        out << " : " << node.type << ";" << std::endl;
    }

    void PrettyPrintVisitor::visit(ProcDeclNode& node) {
        printIndent();
        out << "procedure " << node.name;
        if (!node.parameters.empty()) {
            out << "(";
            for (size_t i = 0; i < node.parameters.size(); ++i) {
                if (i > 0) out << "; ";
                node.parameters[i]->accept(*this);
            }
            out << ")";
        }
        out << ";" << std::endl;
        indentLevel++;
        node.block->accept(*this);
        indentLevel--;
        printIndent();
        out << ";" << std::endl;
    }

    void PrettyPrintVisitor::visit(FuncDeclNode& node) {
        printIndent();
        out << "function " << node.name;
        if (!node.parameters.empty()) {
            out << "(";
            for (size_t i = 0; i < node.parameters.size(); ++i) {
                if (i > 0) out << "; ";
                node.parameters[i]->accept(*this);
            }
            out << ")";
        }
        out << " : " << node.returnType << ";" << std::endl;
        indentLevel++;
        node.block->accept(*this);
        indentLevel--;
        printIndent();
        out << ";" << std::endl;
    }

    void PrettyPrintVisitor::visit(ParameterNode& node) {
        if (node.isVar) out << "var ";
        for (size_t i = 0; i < node.identifiers.size(); ++i) {
            if (i > 0) out << ", ";
            out << node.identifiers[i];
        }
        out << " : " << node.type;
    }

    void PrettyPrintVisitor::visit(CompoundStmtNode& node) {
        printIndent();
        out << "begin" << std::endl;
        indentLevel++;
        for (size_t i = 0; i < node.statements.size(); ++i) {
            node.statements[i]->accept(*this);
            if (i < node.statements.size() - 1) {
                out << ";";
            }
            out << std::endl;
        }
        indentLevel--;
        printIndent();
        out << "end";
    }

    void PrettyPrintVisitor::visit(AssignmentNode& node) {
        printIndent();
        node.variable->accept(*this);
        out << " := ";
        node.expression->accept(*this);
    }

    void PrettyPrintVisitor::visit(IfStmtNode& node) {
        printIndent();
        out << "if ";
        node.condition->accept(*this);
        out << " then" << std::endl;
        indentLevel++;
        node.thenStatement->accept(*this);
        indentLevel--;
        if (node.elseStatement) {
            out << std::endl;
            printIndent();
            out << "else" << std::endl;
            indentLevel++;
            node.elseStatement->accept(*this);
            indentLevel--;
        }
    }

    void PrettyPrintVisitor::visit(WhileStmtNode& node) {
        printIndent();
        out << "while ";
        node.condition->accept(*this);
        out << " do" << std::endl;
        indentLevel++;
        node.statement->accept(*this);
        indentLevel--;
    }

    void PrettyPrintVisitor::visit(ForStmtNode& node) {
        printIndent();
        out << "for " << node.variable << " := ";
        node.startValue->accept(*this);
        out << (node.isDownto ? " downto " : " to ");
        node.endValue->accept(*this);
        out << " do" << std::endl;
        indentLevel++;
        node.statement->accept(*this);
        indentLevel--;
    }

    void PrettyPrintVisitor::visit(ProcCallNode& node) {
        printIndent();
        out << node.name;
        if (!node.arguments.empty()) {
            out << "(";
            for (size_t i = 0; i < node.arguments.size(); ++i) {
                if (i > 0) out << ", ";
                node.arguments[i]->accept(*this);
            }
            out << ")";
        }
    }

    void PrettyPrintVisitor::visit(BinaryOpNode& node) {
        out << "(";
        node.left->accept(*this);
        out << " " << BinaryOpNode::opToString(node.operator_) << " ";
        node.right->accept(*this);
        out << ")";
    }

    void PrettyPrintVisitor::visit(UnaryOpNode& node) {
        switch (node.operator_) {
            case UnaryOpNode::PLUS: out << "+"; break;
            case UnaryOpNode::MINUS: out << "-"; break;
            case UnaryOpNode::NOT: out << "not "; break;
        }
        node.operand->accept(*this);
    }

    void PrettyPrintVisitor::visit(FuncCallNode& node) {
        out << node.name << "(";
        for (size_t i = 0; i < node.arguments.size(); ++i) {
            if (i > 0) out << ", ";
            node.arguments[i]->accept(*this);
        }
        out << ")";
    }

    void PrettyPrintVisitor::visit(VariableNode& node) {
        out << node.name;
    }

    void PrettyPrintVisitor::visit(NumberNode& node) {
        out << node.value;
    }

    void PrettyPrintVisitor::visit(StringNode& node) {
        out << "'" << node.value << "'";
    }

    void PrettyPrintVisitor::visit(BooleanNode& node) {
        out << (node.value ? "true" : "false");
    }

    void PrettyPrintVisitor::visit(EmptyStmtNode& node) {
    }

} 
