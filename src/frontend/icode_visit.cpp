#include"icode.hpp"

namespace pascal {

    void CodeGenVisitor::visit(ProgramNode& node) {
        name = node.name;
        if (node.block) {
            node.block->accept(*this);
        }
    }

    void CodeGenVisitor::visit(BlockNode& node) {
        for (auto& decl : node.declarations) {
            if (!decl) continue;
            if (dynamic_cast<TypeDeclNode*>(decl.get())) {
                decl->accept(*this);
            }
        }

        for (auto& decl : node.declarations) {
            if (!decl) continue;
            if (dynamic_cast<TypeDeclNode*>(decl.get())) continue;

            if (generatingDeferredCode) {
                if (dynamic_cast<ProcDeclNode*>(decl.get()) ||
                    dynamic_cast<FuncDeclNode*>(decl.get())) {
                    continue;
                }
            }
            decl->accept(*this);
        }

        if (node.compoundStatement) {
            node.compoundStatement->accept(*this);
        }
    }

    void CodeGenVisitor::visit(VarDeclNode& node) {
        for (const auto& varName : node.identifiers) {
            std::string mangledName = mangleVariableName(varName);
            std::string typeName;

            if (std::holds_alternative<std::string>(node.type)) {
                typeName = std::get<std::string>(node.type);
            } else if (std::holds_alternative<std::unique_ptr<ASTNode>>(node.type)) {
                auto& typeNode = std::get<std::unique_ptr<ASTNode>>(node.type);

                if (auto* arrayTypeNode = dynamic_cast<ArrayTypeNode*>(typeNode.get())) {
                    ArrayInfo info = buildArrayInfoFromNode(arrayTypeNode);
                    arrayInfo[mangledName] = std::move(info);

                    setVarType(varName, VarType::PTR);
                    int slot = newSlotFor(mangledName);
                    setSlotType(slot, VarType::PTR);
                    varSlot[varName]     = slot;
                    varSlot[mangledName] = slot;

                    updateDataSectionInitialValue(slotVar(slot), "ptr", "null");
                    emit3("alloc",
                        slotVar(slot),
                        std::to_string(arrayInfo[mangledName].elementSize),
                        std::to_string(arrayInfo[mangledName].size));

                    std::string currentScope = getCurrentScopeName();
                    if (currentScope.empty())  globalArrays.push_back(slotVar(slot));
                    else                       functionScopedArrays[currentScope].push_back(slotVar(slot));
                    continue;
                }

                if (auto* simpleTypeNode = dynamic_cast<SimpleTypeNode*>(typeNode.get())) {
                    typeName = simpleTypeNode->typeName;
                } else if (dynamic_cast<RecordTypeNode*>(typeNode.get())) {
                    typeName = "record";
                }
            } else {
                typeName = "unknown";
            }

            int slot = newSlotFor(mangledName);
            varSlot[varName]     = slot;
            varSlot[mangledName] = slot;

            std::string normalizedTypeName = resolveTypeName(lc(typeName));

            auto recordTypeIt = recordTypes.find(normalizedTypeName);
            if (recordTypeIt != recordTypes.end()) {
                varRecordType[mangledName] = normalizedTypeName;
                varRecordType[varName]     = normalizedTypeName;

                int recordSize = recordTypeIt->second.size;

                setVarType(varName, VarType::RECORD);
                setSlotType(slot, VarType::PTR);

                updateDataSectionInitialValue(slotVar(slot), "ptr", "null");
                emit3("alloc", slotVar(slot), std::to_string(recordSize), "1");

                allocateRecordFieldArrays(mangledName, normalizedTypeName);

                std::string currentScope = getCurrentScopeName();
                recordsToFreeInScope[currentScope].push_back(mangledName);
                continue;
            }

            auto aliasIt = arrayInfo.find(normalizedTypeName);
            if (aliasIt != arrayInfo.end()) {
                arrayInfo[mangledName] = aliasIt->second;

                setVarType(varName, VarType::PTR);
                setSlotType(slot, VarType::PTR);

                updateDataSectionInitialValue(slotVar(slot), "ptr", "null");
                emit3("alloc",
                    slotVar(slot),
                    std::to_string(arrayInfo[mangledName].elementSize),
                    std::to_string(arrayInfo[mangledName].size));

                std::string currentScope = getCurrentScopeName();
                if (currentScope.empty())  globalArrays.push_back(slotVar(slot));
                else                       functionScopedArrays[currentScope].push_back(slotVar(slot));
                continue;
            }

            VarType vType = getTypeFromString(typeName);
            setVarType(varName, vType);
            setSlotType(slot, vType);

            if (vType == VarType::PTR || vType == VarType::RECORD) {
                updateDataSectionInitialValue(slotVar(slot), "ptr", "null");
            } else if (vType == VarType::DOUBLE) {
                updateDataSectionInitialValue(slotVar(slot), "float", "0.0");
            } else {
                updateDataSectionInitialValue(slotVar(slot), "int", "0");
            }
        }
    }

    void CodeGenVisitor::visit(ProcCallNode& node) {
        auto handler = builtinRegistry.findHandler(node.name);
        if (handler) {
            handler->generate(*this, node.name, node.arguments);
            return;
        }

        std::vector<std::string> evaluated_args;
        size_t intRegIdx = 1;
        size_t ptrRegIdx = 0;
        size_t floatRegIdx = 0;

        for (auto& arg : node.arguments) {
            std::string argVal = eval(arg.get());
            evaluated_args.push_back(argVal);
            VarType argType = getExpressionType(arg.get());

            std::string targetReg;
            if (argType == VarType::STRING || argType == VarType::PTR || argType == VarType::RECORD) {
                if (ptrRegIdx < ptrRegisters.size()) targetReg = ptrRegisters[ptrRegIdx++];
            } else if (argType == VarType::DOUBLE) {
                if (floatRegIdx < floatRegisters.size()) targetReg = floatRegisters[floatRegIdx++];
            } else {
                if (intRegIdx < registers.size()) targetReg = registers[intRegIdx++];
            }
            if (!targetReg.empty()) emit2("mov", targetReg, argVal);
        }

        std::string mangledName = findMangledFuncName(node.name, true);
        emit1("call", "PROC_" + mangledName);

        for (const auto& arg : evaluated_args)
            if (isReg(arg) && !isParmReg(arg)) freeReg(arg);
    }

    void CodeGenVisitor::visit(FuncCallNode& node) {
        auto handler = builtinRegistry.findHandler(node.name);
        if (handler) {
            if (handler->generateWithResult(*this, node.name, node.arguments)) return;
        }

        std::vector<std::string> evaluated_args;
        size_t intRegIdx = 1;
        size_t ptrRegIdx = 0;
        size_t floatRegIdx = 0;

        for (auto& arg : node.arguments) {
            std::string argVal = eval(arg.get());
            evaluated_args.push_back(argVal);
            VarType argType = getExpressionType(arg.get());
            std::string targetReg;
            if (argType == VarType::STRING || argType == VarType::PTR || argType == VarType::RECORD) {
                if (ptrRegIdx < ptrRegisters.size()) targetReg = ptrRegisters[ptrRegIdx++];
            } else if (argType == VarType::DOUBLE) {
                if (floatRegIdx < floatRegisters.size()) targetReg = floatRegisters[floatRegIdx++];
            } else {
                if (intRegIdx < registers.size()) targetReg = registers[intRegIdx++];
            }
            if (!targetReg.empty()) emit2("mov", targetReg, argVal);
        }

        std::string mangledName = findMangledFuncName(node.name, false);
        emit1("call", "FUNC_" + mangledName);

        auto it = funcSignatures.find(node.name);
        VarType returnType = (it != funcSignatures.end()) ? it->second.returnType : VarType::INT;

        std::string resultLocation;
        if (returnType == VarType::DOUBLE) {
            resultLocation = allocFloatReg();
            emit2("mov", resultLocation, "xmm0");
        } else if (returnType == VarType::STRING || returnType == VarType::PTR || returnType == VarType::RECORD) {
            resultLocation = allocTempPtr();
            emit2("mov", resultLocation, "arg0");
        } else {
            resultLocation = allocReg();
            emit2("mov", resultLocation, "rax");
        }
        pushValue(resultLocation);

        for (const auto& arg : evaluated_args)
            if (isReg(arg) && !isParmReg(arg)) freeReg(arg);
    }

    void CodeGenVisitor::visit(FuncDeclNode& node) {
        std::string mangledName = mangleVariableName(node.name);
        if (declaredFuncs.count(mangledName)) {
            return;
        }
        declaredFuncs[mangledName] = true;

        FuncInfo funcInfo;
        funcInfo.returnType = getTypeFromString(node.returnType);
        for (auto& p : node.parameters) {
            if (auto pn = dynamic_cast<ParameterNode*>(p.get())) {
                for (size_t i = 0; i < pn->identifiers.size(); ++i) {
                    funcInfo.paramTypes.push_back(getTypeFromString(pn->type));
                }
            }
        }
        funcSignatures[node.name] = funcInfo;
        setVarType(node.name, funcInfo.returnType);

        auto path = scopeHierarchy;
        path.push_back(node.name);
        deferredFuncs.push_back({&node, path});

        scopeHierarchy.push_back(node.name);
        if (node.block) {
            if (auto blockNode = dynamic_cast<BlockNode*>(node.block.get())) {
                for (auto& decl : blockNode->declarations) {
                    if (dynamic_cast<ProcDeclNode*>(decl.get()) || dynamic_cast<FuncDeclNode*>(decl.get())) {
                        decl->accept(*this);
                    }
                }
            }
        }
        scopeHierarchy.pop_back();
    }

    void CodeGenVisitor::visit(ProcDeclNode& node) {
        std::string mangledName = mangleVariableName(node.name);
        if (declaredProcs.count(mangledName)) {
            return;
        }
        declaredProcs[mangledName] = true;

        auto path = scopeHierarchy;
        path.push_back(node.name);
        deferredProcs.push_back({&node, path});

        scopeHierarchy.push_back(node.name);
        if (node.block) {
            if (auto blockNode = dynamic_cast<BlockNode*>(node.block.get())) {
                for (auto& decl : blockNode->declarations) {
                    if (dynamic_cast<ProcDeclNode*>(decl.get()) || dynamic_cast<FuncDeclNode*>(decl.get())) {
                        decl->accept(*this);
                    }
                }
            }
        }
        scopeHierarchy.pop_back();
    }

    void CodeGenVisitor::visit(TypeAliasNode& node) {
        auto lc = [](std::string s){ std::transform(s.begin(), s.end(), s.begin(),
                                    [](unsigned char c){ return std::tolower(c); }); return s; };
        typeAliases[lc(node.typeName)] = lc(node.baseType);
    }

    void CodeGenVisitor::visit(ParameterNode& node) {
        auto lc = [](std::string s){ std::transform(s.begin(), s.end(), s.begin(),
                                    [](unsigned char c){ return std::tolower(c); }); return s;
        };
        for (auto &idRaw : node.identifiers) {
            std::string id = idRaw;
            int slot = newSlotFor(id);
            std::string t = lc(node.type);
            if (t == "string") {
                setVarType(id, VarType::STRING);
                setSlotType(slot, VarType::STRING);
            } else if (t == "integer" || t == "boolean") {
                setVarType(id, VarType::INT);
                setSlotType(slot, VarType::INT);
            } else if (t == "real") {
                setVarType(id, VarType::DOUBLE);
                setSlotType(slot, VarType::DOUBLE);
            } else {
                std::string rt = resolveTypeName(t);
                if (recordTypes.count(rt)) {
                    setVarType(id, VarType::RECORD);
                    setSlotType(slot, VarType::PTR);
                    varRecordType[id] = rt;
                    currentParamTypes[id] = rt;
                }
            }
        }
    }

    void CodeGenVisitor::visit(CompoundStmtNode& node) {
        for (auto &stmt : node.statements) {
            if (!stmt) continue;
            if (auto v = dynamic_cast<VariableNode*>(stmt.get())) {
                std::string mangled = findMangledFuncName(v->name, true);
                if (declaredProcs.count(mangled)) {
                    emit1("call", "PROC_" + mangled);
                    continue;
                }
            }
            stmt->accept(*this);
        }
    }

    void CodeGenVisitor::visit(IfStmtNode& node) {
        std::string elseL = newLabel("ELSE");
        std::string endL  = newLabel("ENDIF");

        std::string condResult = eval(node.condition.get());

        emit2("cmp", condResult, "0");
        emit1("je", elseL);

        if (isReg(condResult) && !isParmReg(condResult)) {
            freeReg(condResult);
        }
        if (node.thenStatement) {
            node.thenStatement->accept(*this);
        }
        emit1("jmp", endL);
        emitLabel(elseL);
        if (node.elseStatement) {
            node.elseStatement->accept(*this);
        }
        emitLabel(endL);
    }

    void CodeGenVisitor::visit(WhileStmtNode& node) {
        std::string start = newLabel("WHILE");
        std::string end   = newLabel("ENDWHILE");
        loopContinueLabels.push_back(start);
        loopEndLabels.push_back(end);
        emitLabel(start);
        std::string c = eval(node.condition.get());
        emit2("cmp", c, "0");
        emit1("je", end);
        if (isReg(c) && !isParmReg(c)) freeReg(c);
        if (node.statement) node.statement->accept(*this);
        emit1("jmp", start);
        emitLabel(end);
        loopContinueLabels.pop_back();
        loopEndLabels.pop_back();
    }

    void CodeGenVisitor::visit(ForStmtNode& node) {
        std::string startVal = eval(node.startValue.get());
        std::string endVal   = eval(node.endValue.get());

        startVal = coerceToIntImmediate(startVal);
        endVal   = coerceToIntImmediate(endVal);

        std::string mangledLoopVar = mangleVariableName(node.variable);
        int slot = newSlotFor(mangledLoopVar);
        emit2("mov", slotVar(slot), startVal);
        if (isReg(startVal) && !isParmReg(startVal)) freeReg(startVal);

        std::string loopStartLabel = newLabel("FOR");
        std::string loopEndLabel   = newLabel("ENDFOR");
        std::string continueLabel  = newLabel("FOR_CONTINUE");

        loopContinueLabels.push_back(continueLabel);
        loopEndLabels.push_back(loopEndLabel);

        emitLabel(loopStartLabel);

        std::string endReg = allocReg();
        emit2("mov", endReg, endVal);
        emit2("cmp", slotVar(slot), endReg);
        if (node.isDownto) emit1("jl", loopEndLabel); else emit1("jg", loopEndLabel);

        if (node.statement) node.statement->accept(*this);
        emitLabel(continueLabel);
        if (node.isDownto) emit2("sub", slotVar(slot), "1"); else emit2("add", slotVar(slot), "1");
        emit1("jmp", loopStartLabel);
        emitLabel(loopEndLabel);

        loopContinueLabels.pop_back();
        loopEndLabels.pop_back();

        if (isReg(endReg) && !isParmReg(endReg)) freeReg(endReg);
        if (isReg(endVal) && !isParmReg(endVal)) freeReg(endVal);
    }

    void CodeGenVisitor::visit(BinaryOpNode& node) {
        auto isStrLike = [&](VarType v){ return v == VarType::STRING || v == VarType::PTR; };
        VarType lt = getExpressionType(node.left.get());
        VarType rt = getExpressionType(node.right.get());

        if (node.operator_ == BinaryOpNode::PLUS && (isStrLike(lt) || isStrLike(rt))) {
            usedModules.insert("string");
            std::string left  = eval(node.left.get());
            std::string right = eval(node.right.get());
            std::string len1 = allocReg(), len2 = allocReg(), totalLen = allocReg();
            emit_invoke("strlen", {left});
            emit("return " + len1);
            emit_invoke("strlen", {right});
            emit("return " + len2);
            emit2("mov", totalLen, len1);
            emit2("add", totalLen, len2);
            emit2("add", totalLen, "1");

            std::string result_str = allocTempPtr();
            emit3("alloc", result_str, "1", totalLen);
            emit_invoke("strncpy", {result_str, left, len1});
            emit_invoke("strncat", {result_str, right, len2});

            pushValue(result_str);
            freeReg(len1); freeReg(len2); freeReg(totalLen);
            if (isReg(left) && !isParmReg(left)) freeReg(left);
            if (isReg(right) && !isParmReg(right)) freeReg(right);
            return;
        }
        try {
            std::string folded = foldNumeric(&node);
            if (!folded.empty()) {
                if (node.operator_ == BinaryOpNode::DIVIDE && isIntegerLiteral(folded)) folded += ".0";
                if (isFloatLiteral(folded)) pushValue(ensureFloatConstSymbol(folded));
                else pushValue(folded);
                return;
            }
        } catch (...) { }

        auto evalOperand = [&](ASTNode* n)->std::string {
            if (auto var = dynamic_cast<VariableNode*>(n)) {
                std::string v;
                if (tryGetConstNumeric(var->name, v)) return v;
            }
            return eval(n);
        };

        std::string left = evalOperand(node.left.get());
        std::string right = evalOperand(node.right.get());

        bool needsFloatOp = (lt == VarType::DOUBLE || rt == VarType::DOUBLE ||
                            isFloatLiteral(left) || isFloatLiteral(right) ||
                            node.operator_ == BinaryOpNode::DIVIDE);

        if (node.operator_ == BinaryOpNode::DIVIDE) {
            needsFloatOp = true;
            if (isIntegerLiteral(left) && !isFloatLiteral(left)) left += ".0";
            if (isIntegerLiteral(right) && !isFloatLiteral(right)) right += ".0";
        }

        if (node.operator_ == BinaryOpNode::DIV && (isFloatLiteral(left) || isFloatLiteral(right))) {
            if (isFloatLiteral(left)) left = std::to_string((long long)std::stod(left));
            if (isFloatLiteral(right)) right = std::to_string((long long)std::stod(right));
            needsFloatOp = false;
        }

        if (needsFloatOp && (node.operator_ == BinaryOpNode::PLUS ||
                            node.operator_ == BinaryOpNode::MINUS ||
                            node.operator_ == BinaryOpNode::MULTIPLY ||
                            node.operator_ == BinaryOpNode::DIVIDE)) {
            std::string dst;
            if (isFloatReg(left) && !isParmReg(left)) {
                dst = left;
            } else {
                dst = allocFloatReg();
                emit2("mov", dst, left);
            }

            emit2(node.operator_ == BinaryOpNode::DIVIDE ? "div" :
                node.operator_ == BinaryOpNode::MULTIPLY ? "mul" :
                node.operator_ == BinaryOpNode::MINUS ? "sub" : "add", dst, right);

            if (isReg(right) && !isParmReg(right)) freeReg(right);
            pushValue(dst);
            return;
        }

        auto emitBinary = [&](const char* op) {
            std::string dst;
            bool leftIsUsableReg = isReg(left) && !isParmReg(left);
            if (leftIsUsableReg) dst = left;
            else { dst = allocReg(); emit2("mov", dst, left); }
            emit2(op, dst, right);
            if (isReg(right) && !isParmReg(right)) freeReg(right);
            pushValue(dst);
        };

        switch (node.operator_) {
            case BinaryOpNode::PLUS:      emitBinary("add"); break;
            case BinaryOpNode::MINUS:     emitBinary("sub"); break;
            case BinaryOpNode::MULTIPLY:  emitBinary("mul"); break;
            case BinaryOpNode::DIVIDE:    emitBinary("div"); break;
            case BinaryOpNode::DIV:       emitBinary("div"); break;
            case BinaryOpNode::MOD:       emitBinary("mod"); break;
            case BinaryOpNode::AND:       pushLogicalAnd(left, right); break;
            case BinaryOpNode::OR:        pushLogicalOr(left, right); break;
            case BinaryOpNode::EQUAL:         pushCmpResult(left, right, "je");  break;
            case BinaryOpNode::NOT_EQUAL:     pushCmpResult(left, right, "jne"); break;
            case BinaryOpNode::LESS:          pushCmpResult(left, right, "jl");  break;
            case BinaryOpNode::LESS_EQUAL:    pushCmpResult(left, right, "jle"); break;
            case BinaryOpNode::GREATER:       pushCmpResult(left, right, "jg");  break;
            case BinaryOpNode::GREATER_EQUAL: pushCmpResult(left, right, "jge"); break;
            default: throw std::runtime_error("Unsupported binary operator");
        }
    }

    void CodeGenVisitor::visit(UnaryOpNode& node) {
        std::string v = eval(node.operand.get());
        switch (node.operator_) {
            case UnaryOpNode::MINUS: { std::string t = allocReg(); emit2("mov", t, "0"); emit2("sub", t, v); pushValue(t); break; }
            case UnaryOpNode::PLUS: { pushValue(v); break; }
            case UnaryOpNode::NOT: {
                emit("not " + v);
                pushValue(v);
                break;
            }
        }
    }

    void CodeGenVisitor::visit(VariableNode& node) {
        if (auto pit = currentParamLocations.find(node.name); pit != currentParamLocations.end()) {
            pushValue(pit->second);
            return;
        }

        std::string mangled = findMangledName(node.name);

        if (auto ct = compileTimeConstants.find(mangled); ct != compileTimeConstants.end()) {
            pushValue(ct->second);
            return;
        }
        if (auto ct2 = compileTimeConstants.find(node.name); ct2 != compileTimeConstants.end()) {
            pushValue(ct2->second);
            return;
        }

        if (auto sit = varSlot.find(mangled); sit != varSlot.end()) {
            pushValue(slotVar(sit->second));
            return;
        }

        pushValue(mangled);
    }

    void CodeGenVisitor::visit(NumberNode& node) {
        if (node.isReal || isRealNumber(node.value)) {
            std::string reg = allocFloatReg();
            emit2("mov", reg, ensureFloatConstSymbol(node.value));
            pushValue(reg);
        } else {
            pushValue(node.value);
        }
    }

    void CodeGenVisitor::visit(StringNode& node) {
        std::string sym = internString(node.value);
        pushValue(sym);
    }

    void CodeGenVisitor::visit(BooleanNode& node) {
        std::string reg = allocReg();
        emit2("mov", reg, node.value ? "1" : "0");
        pushValue(reg);
    }

    void CodeGenVisitor::visit(EmptyStmtNode& node) {
        // No operation
    }

    void CodeGenVisitor::visit(ConstDeclNode& node) {
        for (const auto& assignment : node.assignments) {
            if (auto strNode = dynamic_cast<StringNode*>(assignment->value.get())) {
                std::string sym = internString(strNode->value);

                std::string mangledName = mangleVariableName(assignment->identifier);
                compileTimeConstants[mangledName] = mangledName;
                compileTimeConstants[assignment->identifier] = mangledName;

                int slot = newSlotFor(mangledName);
                setVarType(assignment->identifier, VarType::PTR);
                setSlotType(slot, VarType::PTR);
                updateDataSectionInitialValue(slotVar(slot), "ptr", "null");
                prolog.push_back("mov " + slotVar(slot) + ", " + sym);
                continue;
            }

            try {
                std::string literalValue = evaluateConstantExpression(assignment->value.get());
                bool isFloat = isRealNumber(literalValue);
                if (isFloat) literalValue = ensureFloatConstSymbol(literalValue);

                std::string varType = isFloat ? "float" : "int";
                std::string mangledName = mangleVariableName(assignment->identifier);

                compileTimeConstants[mangledName] = (isFloat ? realConstants[literalValue] : literalValue);
                compileTimeConstants[assignment->identifier] = (isFloat ? realConstants[literalValue] : literalValue);

                int slot = newSlotFor(mangledName);
                VarType vType = isFloat ? VarType::DOUBLE : VarType::INT;
                setVarType(assignment->identifier, vType);
                setSlotType(slot, vType);

                updateDataSectionInitialValue(slotVar(slot), varType,
                    isFloat ? realConstants[literalValue] : literalValue);
            } catch (const std::runtime_error&) {
                std::string valueReg = eval(assignment->value.get());
                int slot = newSlotFor(assignment->identifier);
                VarType exprType = getExpressionType(assignment->value.get());
                setVarType(assignment->identifier, exprType);
                setSlotType(slot, exprType);
                std::string varLocation = slotVar(slot);
                emit2("mov", varLocation, valueReg);
                if (isReg(valueReg) && !isParmReg(valueReg)) freeReg(valueReg);
            }
        }
    }

    void CodeGenVisitor::visit(RepeatStmtNode& node) {
        std::string startLabel = newLabel("REPEAT");
        std::string endLabel = newLabel("UNTIL");
        std::string continueLabel = newLabel("REPEAT_CONTINUE");

        loopContinueLabels.push_back(continueLabel);
        loopEndLabels.push_back(endLabel);

        emitLabel(startLabel);
        for (auto& stmt : node.statements) if (stmt) stmt->accept(*this);
        emitLabel(continueLabel);
        std::string condResult = eval(node.condition.get());
        emit2("cmp", condResult, "0");
        emit1("je", startLabel);
        if (isReg(condResult) && !isParmReg(condResult)) freeReg(condResult);
        emitLabel(endLabel);
        loopContinueLabels.pop_back();
        loopEndLabels.pop_back();
    }

    void CodeGenVisitor::visit(CaseStmtNode& node) {
        std::string switchExpr = eval(node.expression.get());
        std::string endLabel = newLabel("CASE_END");
        std::vector<std::string> branchLabels;
        for (size_t i = 0; i < node.branches.size(); i++) branchLabels.push_back(newLabel("CASE_" + std::to_string(i)));
        std::string elseLabel = newLabel("CASE_ELSE");
        for (size_t i = 0; i < node.branches.size(); i++) {
            auto& branch = node.branches[i];
            for (auto& value : branch->values) {
                std::string caseValue = eval(value.get());
                emit2("cmp", switchExpr, caseValue);
                emit1("je", branchLabels[i]);
                if (isReg(caseValue) && !isParmReg(caseValue)) freeReg(caseValue);
            }
        }
        if (node.elseStatement) emit1("jmp", elseLabel); else emit1("jmp", endLabel);
        for (size_t i = 0; i < node.branches.size(); i++) {
            emitLabel(branchLabels[i]);
            if (node.branches[i]->statement) node.branches[i]->statement->accept(*this);
            emit1("jmp", endLabel);
        }
        if (node.elseStatement) { emitLabel(elseLabel); node.elseStatement->accept(*this); }
        emitLabel(endLabel);
        if (isReg(switchExpr) && !isParmReg(switchExpr)) freeReg(switchExpr);
    }

    void CodeGenVisitor::visit(ArrayTypeNode& node) {
        // Typically handled within other nodes like VarDeclNode or TypeDeclNode
    }

    void CodeGenVisitor::visit(ArrayDeclarationNode& node) {
        std::string mangledName = findMangledName(node.name);

        std::string elementType;
        if (auto simpleType = dynamic_cast<SimpleTypeNode*>(node.arrayType->elementType.get())) {
            elementType = simpleType->typeName;
        } else if (dynamic_cast<ArrayTypeNode*>(node.arrayType->elementType.get())) {
            elementType = "array";
        } else if (dynamic_cast<RecordTypeNode*>(node.arrayType->elementType.get())) {
            elementType = "record";
        } else {
            elementType = "integer";
        }

        int lowerBound = std::stoi(evaluateConstantExpression(node.arrayType->lowerBound.get()));
        int upperBound = std::stoi(evaluateConstantExpression(node.arrayType->upperBound.get()));
        int size = upperBound - lowerBound + 1;

        int elementSize = 8;

        if (isRecordTypeName(elementType)) {
            auto it = recordTypes.find(elementType);
            if (it != recordTypes.end()) {
                elementSize = it->second.size;
            }
        } else {
            elementSize = getArrayElementSize(elementType);
        }

        ArrayInfo info;
        info.elementType = elementType;
        info.lowerBound = lowerBound;
        info.upperBound = upperBound;
        info.size = size;
        info.elementSize = elementSize;
        mangledName = findMangledName(node.name);
        arrayInfo[mangledName] = std::move(info);
        int slot = newSlotFor(mangledName);
        varSlot[node.name] = slot;
        varSlot[mangledName] = slot;
        setVarType(node.name, VarType::PTR);
        setSlotType(slot, VarType::PTR);
        updateDataSectionInitialValue(slotVar(slot), "ptr", "null");
        emit3("alloc",
            slotVar(slot),
            std::to_string(elementSize),
            std::to_string(size));
        std::string currentScope = getCurrentScopeName();
        if (currentScope.empty())  globalArrays.push_back(slotVar(slot));
        else                       functionScopedArrays[currentScope].push_back(slotVar(slot));
    }

    void CodeGenVisitor::visit(FieldAccessNode& node) {
        std::string baseName;
        std::string recType;
        bool baseIsDirectPointer = false;

        if (auto* v = dynamic_cast<VariableNode*>(node.recordExpr.get())) {
            baseName = findMangledName(v->name);
            recType  = getVarRecordTypeName(v->name);
        } else if (dynamic_cast<ArrayAccessNode*>(node.recordExpr.get())) {
            node.recordExpr->accept(*this);
            baseName = popValue();
            baseIsDirectPointer = true;
            recType  = getVarRecordTypeNameFromExpr(node.recordExpr.get());
        } else {
            node.recordExpr->accept(*this);
            baseName = popValue();
            baseIsDirectPointer = true;
            recType  = getVarRecordTypeNameFromExpr(node.recordExpr.get());
        }

        recType = resolveTypeName(lc(recType));
        if (recType.empty()) {
            throw std::runtime_error("Unknown record type: (empty) for field " + node.fieldName);
        }

        auto recTypeIt = recordTypes.find(recType);
        if (recTypeIt == recordTypes.end()) {
            throw std::runtime_error("Unknown record type: " + recType + " for field " + node.fieldName);
        }

        const std::string fieldName = lc(node.fieldName);
        auto& recInfo = recTypeIt->second;
        auto  it      = recInfo.nameToIndex.find(fieldName);
        if (it == recInfo.nameToIndex.end()) {
            throw std::runtime_error("Field not found in record: " + fieldName + " in type " + recType);
        }

        const auto& fieldInfo = recInfo.fields[it->second];
        const int   byteOffset = fieldInfo.offset;

        std::string basePtr = baseIsDirectPointer ? baseName : ensurePtrBase(baseName);

        if (fieldInfo.isArray || isRecordTypeName(fieldInfo.typeName)) {
            std::string p = allocTempPtr();
            emit2("mov", p, basePtr);
            emit2("add", p, std::to_string(byteOffset));
            pushValue(p);
            return;
        }

        VarType fieldType = getTypeFromString(fieldInfo.typeName);
        std::string dst;
        if (fieldType == VarType::DOUBLE) {
            dst = allocFloatReg();
        } else if (fieldType == VarType::PTR || fieldType == VarType::STRING) {
            dst = allocTempPtr();
        } else {
            dst = allocReg();
        }

        emit4("load", dst, basePtr, std::to_string(byteOffset), "1");
        pushValue(dst);
    }

    void CodeGenVisitor::visit(AssignmentNode& node) {
        if (auto arr = dynamic_cast<ArrayAccessNode*>(node.variable.get())) {
            ArrayInfo* info = getArrayInfoForArrayAccess(arr);
            if (!info) throw std::runtime_error("Unknown array: " + getArrayNameFromBase(arr->base.get()));

            std::string rhs = eval(node.expression.get());
            std::string idx = eval(arr->index.get());

            if (getExpressionType(arr->index.get()) == VarType::DOUBLE) {
                std::string intIdx = allocReg();
                emit2("mov", intIdx, idx);
                if (isReg(idx) && !isParmReg(idx)) freeReg(idx);
                idx = intIdx;
            }

        #ifdef MXVM_BOUNDS_CHECK
            {
                std::string idxCopy = allocReg();
                emit2("mov", idxCopy, idx);
                emitArrayBoundsCheck(idxCopy, info->lowerBound, info->upperBound);
            }
        #endif

            std::string elemIndex = allocReg();
            emit2("mov", elemIndex, idx);
            if (info->lowerBound != 0) emit2("sub", elemIndex, std::to_string(info->lowerBound));

            std::string base;
            if (auto var = dynamic_cast<VariableNode*>(arr->base.get())) {
                std::string mangled = findMangledArrayName(var->name);
                base = ensurePtrBase(storageSymbolFor(mangled));
            } else if (auto field = dynamic_cast<FieldAccessNode*>(arr->base.get())) {
                field->recordExpr->accept(*this);
                std::string recPtr = popValue();
                std::string recType = getVarRecordTypeNameFromExpr(field->recordExpr.get());
                auto ofs_sz = getRecordFieldOffsetAndSize(recType, field->fieldName);
                int fieldOffset = ofs_sz.first;

                std::string arrayPtr = allocTempPtr();
                emit2("mov", arrayPtr, recPtr);
                emit2("add", arrayPtr, std::to_string(fieldOffset));
                base = arrayPtr;
            } else if (dynamic_cast<ArrayAccessNode*>(arr->base.get())) {
                arr->base->accept(*this);
                base = popValue();
                base = ensurePtrBase(base);
            } else {
                throw std::runtime_error("Unsupported array base in assignment");
            }

            emit4("store", rhs, base, elemIndex, std::to_string(info->elementSize));

            if (isReg(rhs) && !isParmReg(rhs)) freeReg(rhs);
            if (isReg(idx) && !isParmReg(idx)) freeReg(idx);
            freeReg(elemIndex);
            return;
        }

        if (auto field = dynamic_cast<FieldAccessNode*>(node.variable.get())) {
            std::string rhs = eval(node.expression.get());

            std::string baseName;
            std::string recType;
            bool baseIsDirectPointer = false;

            if (auto* v = dynamic_cast<VariableNode*>(field->recordExpr.get())) {
                baseName = findMangledName(v->name);
                recType  = getVarRecordTypeName(v->name);
            } else {
                field->recordExpr->accept(*this);
                baseName = popValue();
                baseIsDirectPointer = true;
                recType = getVarRecordTypeNameFromExpr(field->recordExpr.get());
            }

            recType = resolveTypeName(lc(recType));
            if (recType.empty()) {
                throw std::runtime_error("Unknown record type for field assignment: " + field->fieldName);
            }

            auto recTypeIt = recordTypes.find(recType);
            if (recTypeIt == recordTypes.end()) {
                throw std::runtime_error("Unknown record type: " + recType + " for field " + field->fieldName);
            }

            const std::string fieldName = lc(field->fieldName);
            auto& recInfo = recTypeIt->second;
            auto it = recInfo.nameToIndex.find(fieldName);
            if (it == recInfo.nameToIndex.end()) {
                throw std::runtime_error("Field not found in record: " + fieldName + " in type " + recType);
            }

            const auto& fieldInfo = recInfo.fields[it->second];
            const int byteOffset = fieldInfo.offset;

            std::string basePtr = baseIsDirectPointer ? baseName : ensurePtrBase(baseName);

            emit4("store", rhs, basePtr, std::to_string(byteOffset), "1");

            if (isReg(rhs) && !isParmReg(rhs)) freeReg(rhs);
            if (baseIsDirectPointer && isReg(basePtr) && !isParmReg(basePtr)) freeReg(basePtr);

            return;
        }

        auto varPtr = dynamic_cast<VariableNode*>(node.variable.get());
        if (!varPtr) return;

        std::string varName = varPtr->name;
        std::string rhs = eval(node.expression.get());

        auto it = currentParamLocations.find(varName);
        if (it != currentParamLocations.end()) {
            emit2("mov", it->second, rhs);
            if (isReg(rhs) && !isParmReg(rhs)) freeReg(rhs);
            return;
        }

        if (!currentFunctionName.empty() && varName == currentFunctionName) {
            VarType rt = getVarType(currentFunctionName);
            if (rt == VarType::STRING || rt == VarType::PTR || rt == VarType::RECORD)
                emit2("mov", "arg0", rhs);
            else if (rt == VarType::DOUBLE)
                emit2("mov", "xmm0", rhs);
            else
                emit2("mov", "rax", rhs);
            functionSetReturn = true;
        } else {
            std::string mangled = findMangledName(varName);
            auto it = varSlot.find(mangled);
            if (it != varSlot.end()) {
                emit2("mov", slotVar(it->second), rhs);
                recordLocation(varName, {ValueLocation::MEMORY, slotVar(it->second)});
            } else {
                emit2("mov", mangled, rhs);
                recordLocation(varName, {ValueLocation::MEMORY, mangled});
            }
        }

        if (isReg(rhs) && !isParmReg(rhs)) freeReg(rhs);
    }

    void CodeGenVisitor::visit(ArrayAssignmentNode& node) {
        auto it = arrayInfo.find(node.arrayName);
        if (it == arrayInfo.end()) throw std::runtime_error("Unknown array: " + node.arrayName);
        ArrayInfo &info = it->second;

        std::string value = eval(node.value.get());
        std::string index = eval(node.index.get());

        if (getExpressionType(node.index.get()) == VarType::DOUBLE) {
            std::string intIndex = allocReg();
            emit2("mov", intIndex, index);
            if (isReg(index) && !isParmReg(index)) freeReg(index);
            index = intIndex;
        }

    #ifdef MXVM_BOUNDS_CHECK
        {
            std::string idxCopy = allocReg();
            emit2("mov", idxCopy, index);
            emitArrayBoundsCheck(idxCopy, info.lowerBound, info.upperBound);
            if (isReg(idxCopy)) freeReg(idxCopy);
        }
    #endif

        std::string elementIndex = allocReg();
        emit2("mov", elementIndex, index);
        if (info.lowerBound != 0) emit2("sub", elementIndex, std::to_string(info.lowerBound));

        std::string base = storageSymbolFor(node.arrayName);
        base = ensurePtrBase(base);

        VarType elemType = VarType::INT;
        if (info.elementType == "real") elemType = VarType::DOUBLE;
        else if (info.elementType == "string" || info.elementType == "ptr") elemType = VarType::PTR;
        else if (isRecordTypeName(info.elementType)) elemType = VarType::RECORD;

        VarType rhsType = getExpressionType(node.value.get());
        if (elemType == VarType::DOUBLE && rhsType != VarType::DOUBLE && !isFloatReg(value)) {
            std::string f = allocFloatReg();
            emit2("mov", f, value);
            if (isReg(value) && !isParmReg(value)) freeReg(value);
            value = f;
        } else if (elemType != VarType::DOUBLE && rhsType == VarType::DOUBLE && isFloatReg(value)) {
            std::string i = allocReg();
            emit2("mov", i, value);
            if (isReg(value) && !isParmReg(value)) freeReg(value);
            value = i;
        }

        emit4("store", value, base, elementIndex, std::to_string(info.elementSize));

        if (isReg(value) && !isParmReg(value)) freeReg(value);
        if (isReg(index) && !isParmReg(index)) freeReg(index);
        freeReg(elementIndex);
    }

    void CodeGenVisitor::visit(ArrayAccessNode& node) {
        ArrayInfo* info = getArrayInfoForArrayAccess(&node);
        if (!info) {
            std::string arrayName = getArrayNameFromBase(node.base.get());
            throw std::runtime_error("Unknown array: " + arrayName);
        }

        std::string idx = eval(node.index.get());

        if (getExpressionType(node.index.get()) == VarType::DOUBLE) {
            std::string intIdx = allocReg();
            emit2("mov", intIdx, idx);
            if (isReg(idx) && !isParmReg(idx)) freeReg(idx);
            idx = intIdx;
        }

        std::string elemIndex = allocReg();
        emit2("mov", elemIndex, idx);
        if (info->lowerBound != 0) emit2("sub", elemIndex, std::to_string(info->lowerBound));

        std::string base;
        if (auto var = dynamic_cast<VariableNode*>(node.base.get())) {
            std::string mangled = findMangledArrayName(var->name);
            base = ensurePtrBase(storageSymbolFor(mangled));
        } else if (auto field = dynamic_cast<FieldAccessNode*>(node.base.get())) {
            field->recordExpr->accept(*this);
            std::string recPtr = popValue();
            std::string recType = getVarRecordTypeNameFromExpr(field->recordExpr.get());
            auto ofs_sz = getRecordFieldOffsetAndSize(recType, field->fieldName);
            int fieldOffset = ofs_sz.first;

            std::string arrayPtr = allocTempPtr();
            emit2("mov", arrayPtr, recPtr);
            emit2("add", arrayPtr, std::to_string(fieldOffset));
            base = arrayPtr;
        } else if (dynamic_cast<ArrayAccessNode*>(node.base.get())) {
            node.base->accept(*this);
            base = popValue();
        } else {
            throw std::runtime_error("Unsupported array base in access");
        }

        if (info->elementIsArray || isRecordTypeName(info->elementType)) {
            std::string offsetBytes = allocReg();
            emit2("mov", offsetBytes, elemIndex);
            emit2("mul", offsetBytes, std::to_string(info->elementSize));

            std::string elemPtr = allocTempPtr();
            emit2("mov", elemPtr, base);
            emit2("add", elemPtr, offsetBytes);
            pushValue(elemPtr);

            if (isReg(idx) && !isParmReg(idx)) freeReg(idx);
            freeReg(elemIndex);
            freeReg(offsetBytes);
            return;
        }

        VarType elemType = getExpressionType(&node);
        std::string dst;
        if (elemType == VarType::DOUBLE)      dst = allocFloatReg();
        else if (elemType == VarType::PTR ||
                elemType == VarType::STRING) dst = allocTempPtr();
        else                                  dst = allocReg();

        emit4("load", dst, base, elemIndex, std::to_string(info->elementSize));
        pushValue(dst);

        if (isReg(idx) && !isParmReg(idx)) freeReg(idx);
        freeReg(elemIndex);
    }

    void CodeGenVisitor::visit(RecordTypeNode& node) {
        // Typically handled within other nodes
    }

    void CodeGenVisitor::visit(RecordDeclarationNode& node) {
        RecordTypeInfo info;
        int offset = 0;

        auto lc = [](std::string s){
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
            return s;
        };

        auto resolveTypeName = [&](std::string t){
            t = lc(t);
            std::unordered_set<std::string> seen;
            while (typeAliases.count(t) && !seen.count(t)) {
                seen.insert(t);
                t = lc(typeAliases.at(t));
            }
            return t;
        };

        for (auto& f : node.recordType->fields) {
            auto& fieldDecl = static_cast<VarDeclNode&>(*f);
            bool fieldIsArray = false;
            ArrayInfo arrInfo{};
            std::string fieldTypeName = "unknown";

            if (std::holds_alternative<std::unique_ptr<ASTNode>>(fieldDecl.type)) {
                auto& typeNode = std::get<std::unique_ptr<ASTNode>>(fieldDecl.type);
                if (auto* at = dynamic_cast<ArrayTypeNode*>(typeNode.get())) {
                    fieldIsArray = true;
                    arrInfo = buildArrayInfoFromNode(at);
                    fieldTypeName = "array";
                } else if (dynamic_cast<RecordTypeNode*>(typeNode.get())) {
                    fieldTypeName = "record";
                } else if (auto* st = dynamic_cast<SimpleTypeNode*>(typeNode.get())) {
                    fieldTypeName = resolveTypeName(st->typeName);
                }
            } else {
                fieldTypeName = resolveTypeName(std::get<std::string>(fieldDecl.type));
            }

            for (const auto& rawName : fieldDecl.identifiers) {
                RecordField rf;
                rf.name = lc(rawName);
                rf.offset = offset;
                rf.isArray = fieldIsArray;

                if (fieldIsArray) {
                    rf.arrayInfo = arrInfo;
                    rf.typeName = "array";
                    rf.size = rf.arrayInfo.size * rf.arrayInfo.elementSize;
                } else if (isRecordTypeName(fieldTypeName)) {
                    rf.typeName = fieldTypeName;
                    auto it = recordTypes.find(fieldTypeName);
                    rf.size = (it != recordTypes.end()) ? it->second.size : 8;
                } else {
                    rf.typeName = fieldTypeName;
                    rf.size = getTypeSizeByName(rf.typeName);
                }

                info.nameToIndex[rf.name] = (int)info.fields.size();
                info.fields.push_back(rf);
                offset += rf.size;
            }
        }

        info.size = offset;
        std::string recordName = lc(node.name);
        recordTypes[recordName] = info;
    }

    void CodeGenVisitor::visit(TypeDeclNode& node) {
        for (auto& typeDecl : node.typeDeclarations) {
            typeDecl->accept(*this);
        }
    }

    void CodeGenVisitor::visit(SimpleTypeNode& node) {
        // Typically handled within other nodes
    }

    void CodeGenVisitor::visit(ArrayTypeDeclarationNode& node) {
        std::string typeKey = resolveTypeName(lc(node.name));
        ArrayInfo info = buildArrayInfoFromNode(node.arrayType.get());
        arrayInfo[typeKey] = std::move(info);
    }

    void CodeGenVisitor::visit(ExitNode& node) {
        if (node.expr) {
            std::string retVal = eval(node.expr.get());
            VarType rt = VarType::UNKNOWN;
            if (!currentFunctionName.empty()) {
                rt = getVarType(currentFunctionName);
            }

            if (rt == VarType::STRING || rt == VarType::PTR || rt == VarType::RECORD) {
                emit2("mov", "arg0", retVal);
            } else if (rt == VarType::DOUBLE) {
                emit2("mov", "xmm0", retVal);
            } else {
                emit2("mov", "rax", retVal);
            }

            functionSetReturn = true;
            if (isReg(retVal) && !isParmReg(retVal)) freeReg(retVal);
        }
        std::string endLabel = getCurrentEndLabel();
        if (!endLabel.empty()) {
            emit1("jmp", endLabel);
        } else {
            emit("ret");
        }
    }

    void CodeGenVisitor::visit(BreakNode& node) {
        if (!loopEndLabels.empty()) {
            emit1("jmp", loopEndLabels.back());
        }
    }

    void CodeGenVisitor::visit(ContinueNode& node) {
        if (!loopContinueLabels.empty()) {
            emit1("jmp", loopContinueLabels.back());
        }
    }
}