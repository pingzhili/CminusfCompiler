#include "cminusf_builder.hpp"
#include "logging.hpp"

// use these macros to get constant value
#define CONST_FP(num) \
    ConstantFP::get((float)num, module.get())
#define CONST_INT(num) \
    ConstantInt::get(num, module.get())


// You can define global variables here
// to store state
Value *public_val = nullptr;
Argument *arg = nullptr;
bool isReturned = false;

/*
 * use CMinusfBuilder::Scope to construct scopes
 * scope.enter: enter a new scope
 * scope.exit: exit current scope
 * scope.push: add a new binding to current scope
 * scope.find: find and return the value bound to the name
 */

void CminusfBuilder::visit(ASTProgram &node) {  // TODO
    LOG(INFO) << "Visiting ASTProgram.\n";
    if (!isReturned){
        if (node.declarations.back()->id != "main"){
            LOG(ERROR) << "The last function should be main funtion.\n";
            exit(1);
        }
        for (auto decl: node.declarations){
            decl->accept(*this);
        }
    }
    
}

void CminusfBuilder::visit(ASTNum &node) { 
    LOG(INFO) << "Visiting ASTNum.\n";
    if (node.type == TYPE_INT){
        public_val = CONST_INT(node.i_val);
    } else if (node.type == TYPE_FLOAT){
        public_val = CONST_FP(node.f_val);
    } else {
        LOG(ERROR) << "ASTNum should not be of type void.\n";
        exit(1);
    }
}

void CminusfBuilder::visit(ASTVarDeclaration &node) { 
    LOG(INFO) << "Visiting ASTVarDeclaration.\n";
    Type *var_type;
    if (!isReturned) {
        if (node.type == TYPE_INT){
            var_type = Type::get_int32_type(module.get());
        }else {
            var_type = Type::get_float_type(module.get());
        }
    
        if (node.num == nullptr){ // type-specifier ID
            if (scope.in_global()){
                auto init_zero = ConstantZero::get(var_type, module.get());
                auto var = GlobalVariable::create(node.id, module.get(), var_type, false, init_zero);
                scope.push(node.id, var);
            }else{
                auto var = builder->create_alloca(var_type);
                scope.push(node.id, var);
            }
            LOG(DEBUG) << "[VarDeclaration] node.id is:" << node.id;
        }else { // type-specifier ID [INT]
            LOG(DEBUG) << "[VarDeclaration] node.id is:" << node.id;
            LOG(DEBUG) << "[VarDeclaration] node.num is:" << node.num;
            node.num->accept(*this);
            if (!public_val->get_type()->is_integer_type()){
                LOG(ERROR) << "Array length should be an integer.\n";
                exit(1);
            } else if (node.num->i_val < 0) {
                LOG(ERROR) << "Array length should be a positive integer.\n";
                exit(1);
            }
            auto *array_type = ArrayType::get(var_type, node.num->i_val);
            if (scope.in_global()){
                auto init_zero = ConstantZero::get(array_type, module.get());
                auto var = GlobalVariable::create(node.id, module.get(), array_type, false, init_zero);
                if (!scope.push(node.id, var)){
                LOG(ERROR) << "This variable has been declared before!\n";
                }
            }else{
                auto var = builder->create_alloca(array_type);
                if (!scope.push(node.id, var)){
                LOG(ERROR) << "This variable has been declared before!\n";
            }
            }
        }
    }
    
}

void CminusfBuilder::visit(ASTFunDeclaration &node) { 
    LOG(INFO) << "Visiting ASTFunDeclaration\n";
    Type *ret_type;
    if (!isReturned){
        scope.enter();
        auto param_types = std::vector<Type *>{};
        if (node.type == TYPE_INT) {
            ret_type = Type::get_int32_type(module.get());
        } else if (node.type == TYPE_FLOAT) {
            ret_type = Type::get_float_type(module.get());
        } else {
            ret_type = Type::get_void_type(module.get());
        }

        if (node.params.size() > 0){
            for (const auto &param : node.params) {
                if (param->type == TYPE_INT){
                    if (param->isarray){
                        param_types.push_back(Type::get_int32_ptr_type(module.get()));
                    } else {
                        param_types.push_back(Type::get_int32_type(module.get()));
                    }
                } else if (param->type == TYPE_FLOAT){
                    if (param->isarray){
                        param_types.push_back(Type::get_float_ptr_type(module.get()));
                    } else {
                        param_types.push_back(Type::get_float_type(module.get()));
                    }
                } else {
                    LOG(ERROR) << "Parameter variable must not be of type void.\n";
                    exit(1);
                }
            }
        }

        auto fun_type = FunctionType::get(ret_type, param_types);
        auto fun = Function::create(fun_type, node.id, module.get());
        scope.exit();
        if (!scope.push(node.id, fun)){
            LOG(ERROR) << "This function has been declared before.\n";
            exit(1);
        }
        scope.enter();

        auto parent_bb = builder->get_insert_block();
        auto bb = BasicBlock::create(module.get(), node.id, fun);
        builder->set_insert_point(bb);
        int i = 0;
        auto fun_args = fun->get_args(); // list of args
        for (auto arg_p = fun_args.begin(); arg_p != fun_args.end(); arg_p++){
            arg = *arg_p;
            node.params[i]->accept(*this);
            i++;
        }
        node.compound_stmt->accept(*this);
        builder->set_insert_point(parent_bb);

        scope.exit();
        isReturned = false;
    }
}

void CminusfBuilder::visit(ASTParam &node) { 
    LOG(INFO) << "Visiting ASTParam\n";
    Type *para_type;
    if (node.type == TYPE_INT) {
        para_type = Type::get_int32_type(module.get());
    } else if (node.type == TYPE_FLOAT) {
        para_type = Type::get_float_type(module.get());
    } else {
        LOG(ERROR) << "Parameter must not be of type void.\n";
        exit(1);
    }

    if (node.isarray) {
        para_type = Type::get_pointer_type(para_type);
    }
    
    auto param = builder->create_alloca(arg->get_type());
    builder->create_store(arg, param);
    if (!scope.push(node.id, param)){
        LOG(ERROR) << "This parameter has been declared before.\n";
        exit(1);
    }
}

void CminusfBuilder::visit(ASTCompoundStmt &node) { 
    LOG(INFO) << "Visiting ASTCompoundStmt\n";
    if (!isReturned){
        scope.enter();
        if (node.local_declarations.size() > 0){
            for (auto decl: node.local_declarations){
                decl->accept(*this);
            }
        } else {
            LOG(WARNING) << "There is an empty CompoundStmt declaration.\n";
        }
        if (node.statement_list.size() > 0){
            for (auto stmt: node.statement_list){
                stmt->accept(*this);
            }
        } else{
            LOG(WARNING) << "There is an empty CompoundStmt statement.\n";
        }
        scope.exit();
    }
}

void CminusfBuilder::visit(ASTExpressionStmt &node) { 
    LOG(INFO) << "Visiting ASTExpressionStmt\n";
    if (!isReturned){
        if (node.expression != nullptr){
            node.expression->accept(*this);
        } else {
            LOG(WARNING) << "There is an empty expression.\n";
        }
    }
}

void CminusfBuilder::visit(ASTSelectionStmt &node) { 
    LOG(INFO) << "Visiting ASTSelectionStmt\n";
    auto cond = public_val;
    if (!isReturned){
        node.expression->accept(*this);
        if (cond->get_type()->is_pointer_type()){
            cond = builder->create_load(cond);
        }
        if (cond->get_type()->is_integer_type()){
            cond = builder->create_icmp_ne(cond, CONST_INT(0));
        }else if (cond->get_type()->is_float_type()){
            cond = builder->create_fcmp_ne(cond, CONST_FP(0));
        }

        auto trueBB = BasicBlock::create(module.get(), "", builder->get_insert_block()->get_parent());
        auto falseBB = BasicBlock::create(module.get(), "", builder->get_insert_block()->get_parent());
        auto retBB = BasicBlock::create(module.get(), "", builder->get_insert_block()->get_parent());
        builder->create_cond_br(cond, trueBB, falseBB);
        bool trueRet;
        bool falseRet;
        /*Start of True Block*/
        builder->set_insert_point(trueBB);
        scope.enter();
        node.if_statement->accept(*this);
        scope.exit();
        if (isReturned){
            trueRet = true;
        } else {
            builder->create_br(retBB);
        }
        isReturned = false;
        /*End of True Block*/
        /*Start of False Block*/
        builder->set_insert_point(falseBB);
        if (node.else_statement != nullptr) {
            scope.enter();
            node.else_statement->accept(*this);
            scope.exit();
            if (isReturned){
                falseRet = true;
            } else {
                builder->create_br(retBB);
            }
            isReturned = false;
        } else{
            builder->create_br(retBB)
;        }
        /*End of False Block*/
        builder->set_insert_point(retBB);
    }
}

void CminusfBuilder::visit(ASTIterationStmt &node) { 
    LOG(INFO) << "Visiting ASTIterationStmt\n";
    if (!isReturned){
        auto trueBB = BasicBlock::create(module.get(), "", builder->get_insert_block()->get_parent());
        auto falseBB = BasicBlock::create(module.get(), "", builder->get_insert_block()->get_parent());
        auto condBB = BasicBlock::create(module.get(), "", builder->get_insert_block()->get_parent());
        auto cond = public_val;

        builder->create_br(condBB);
        builder->set_insert_point(condBB);
        node.expression->accept(*this);
        if (cond->get_type()->is_pointer_type()){
            cond = builder->create_load(cond);
        }
        if (cond->get_type()->is_integer_type()){
            cond = builder->create_icmp_ne(cond, CONST_INT(0));
        }else if (cond->get_type()->is_float_type()){
            cond = builder->create_fcmp_ne(cond, CONST_FP(0));
        }
        builder->create_cond_br(cond, trueBB, falseBB);
        builder->set_insert_point(trueBB);

        scope.enter();
        node.statement->accept(*this);
        scope.exit();

        if (!isReturned){
            builder->create_br(condBB);
        }
        isReturned = false;
        builder->set_insert_point(falseBB);

    }
}

void CminusfBuilder::visit(ASTReturnStmt &node) { 
    LOG(INFO) << "Visiting ASTReturnStmt\n";
    auto retType = builder->get_insert_block()->get_parent()->get_return_type();
    if (!isReturned){
        if (node.expression == nullptr){
            builder->create_void_ret();
        } else{
            node.expression->accept(*this);
            if (public_val->get_type() == retType){
                builder->create_ret(public_val);
            } else if (retType->is_integer_type()){
                auto ret = builder->create_fptosi(public_val, Type::get_int32_type(module.get()));
                builder->create_ret(ret);
            } else if (retType->is_float_type()){
                auto ret = builder->create_sitofp(public_val, Type::get_float_type(module.get()));
                builder->create_ret(ret);
            }
        }
        isReturned = true;
    } else{
        builder->create_void_ret();
    }
}

void CminusfBuilder::visit(ASTVar &node) { 
    LOG(INFO) << "Visiting ASTVar\n";
    auto val = scope.find(node.id);
    LOG(DEBUG) << "[Var] node.id is:" << node.id;
    if (node.expression == nullptr){
        public_val = val;
    } else {
        node.expression->accept(*this);
        auto index = public_val;
        if (index->get_type()->is_float_type()){
            index = builder->create_fptosi(index, Type::get_int32_type(module.get()));
        } else if (static_cast<IntegerType *>(index->get_type())->get_num_bits() == 1){
            index = builder->create_zext(index, Type::get_int32_type(module.get()));
        }
        
        auto cmp = builder->create_icmp_lt(index, CONST_INT(0));
        auto trueBB = BasicBlock::create(module.get(), "", builder->get_insert_block()->get_parent());
        auto falseBB = BasicBlock::create(module.get(), "", builder->get_insert_block()->get_parent());
        builder->create_cond_br(cmp, trueBB, falseBB);
        builder->set_insert_point(trueBB);
        builder->create_call(scope.find("neg_idx_except"), {});
        builder->create_br(falseBB);
        builder->set_insert_point(falseBB);
        val = builder->create_gep(val, {CONST_INT(0), index});
        public_val = builder->create_load(val);
    }
}

void CminusfBuilder::visit(ASTAssignExpression &node) { 
    LOG(INFO) << "Visiting ASTAssignExpression\n";
    if(!isReturned){
        node.var.get()->accept(*this);
        auto var = public_val;
        node.expression.get()->accept(*this);
        auto expr = public_val;
        if (expr->get_type()->is_pointer_type()){
            public_val = builder->create_load(public_val);
        }
        auto var_type = var->get_type()->get_pointer_element_type();
       if (var_type != expr->get_type()){
           if (expr->get_type()->is_integer_type()){
               expr = builder->create_sitofp(expr, Type::get_float_type(module.get()));
           } else if (expr->get_type()->is_float_type()){
               expr = builder->create_fptosi(expr, Type::get_int32_type(module.get()));
           } else {
               LOG(WARNING) << "In an AssignmentExpression, got unkown type matching.\n";
           }
       }
       builder->create_store(expr, var);
    }
}

void CminusfBuilder::visit(ASTSimpleExpression &node) { 
    LOG(INFO) << "Visiting ASTSimpleExpression\n";
    if (!isReturned){
        if (node.additive_expression_r == nullptr){
            node.additive_expression_l->accept(*this);
        } else {
            node.additive_expression_l->accept(*this);
            auto lval = public_val;
            auto ltype = lval->get_type();
            node.additive_expression_r->accept(*this);
            auto rval = public_val;
            auto rtype = rval->get_type();
            Value *cmp;
            bool isInt = true;
            //Transforming type
            if (ltype->is_integer_type()){
                if (rtype->is_float_type()){
                    lval = builder->create_sitofp(lval, Type::get_float_type(module.get()));
                    isInt = false;
                }
            } else if (ltype->is_float_type()){
                if (rtype->is_integer_type()){
                    rval = builder->create_sitofp(rval, Type::get_float_type(module.get()));
                }
                isInt = false;
            }

            switch (node.op){
                case OP_LT:
                    if (isInt){
                        cmp = builder->create_icmp_lt(lval, rval);
                    }else {
                        cmp = builder->create_fcmp_lt(lval, rval);
                    }
                    break;
                case OP_LE:
                    if (isInt){
                        cmp = builder->create_icmp_le(lval, rval);
                    }else {
                        cmp = builder->create_fcmp_le(lval, rval);
                    }
                    break;
                case OP_GT:
                    if (isInt){
                        cmp = builder->create_icmp_gt(lval, rval);
                    }else {
                        cmp = builder->create_fcmp_gt(lval, rval);
                    }
                    break;
                case OP_GE:
                    if (isInt){
                        cmp = builder->create_icmp_ge(lval, rval);
                    }else {
                        cmp = builder->create_fcmp_ge(lval, rval);
                    }
                    break;
                case OP_EQ:
                    if (isInt){
                        cmp = builder->create_icmp_eq(lval, rval);
                    }else {
                        cmp = builder->create_fcmp_eq(lval, rval);
                    }
                    break;
                case OP_NEQ:
                    if (isInt){
                        cmp = builder->create_icmp_ne(lval, rval);
                    }else {
                        cmp = builder->create_fcmp_ne(lval, rval);
                    }
                    break;
            } 
            public_val = builder->create_zext(cmp, Type::get_int32_type(module.get()));
        }
    }
}

void CminusfBuilder::visit(ASTAdditiveExpression &node) { 
    LOG(INFO) << "Visiting ASTAdditiveExpression\n";
    if (!isReturned){
        if (node.additive_expression == nullptr){
            if (node.term != nullptr){
                node.term->accept(*this);
            }else {
                LOG(ERROR) << "Wrong additive_expression.\n";
            }
        } else{
            node.additive_expression->accept(*this);
            auto lval = public_val;
            auto ltype = lval->get_type();
            node.term->accept(*this);
            auto rval = public_val;
            auto rtype = rval->get_type();
            
            // Get value of pointer 
            if (ltype->is_integer_type() && static_cast<IntegerType *>(ltype)->get_num_bits() == 1){
                lval = builder->create_zext(lval, Type::get_int32_type(module.get()));
                ltype = lval->get_type();
            } 
            if (rtype->is_integer_type() && static_cast<IntegerType *>(rtype)->get_num_bits() == 1){
                rval = builder->create_zext(rval, Type::get_int32_type(module.get()));
                rtype = rval->get_type();
            }

            // Type transforming
            if (ltype->is_integer_type()){
                if (rtype->is_float_type()){
                    lval = builder->create_sitofp(lval, Type::get_float_type(module.get()));
                    if (node.op == OP_PLUS){
                        public_val = builder->create_fadd(lval, rval);
                    } else if (node.op == OP_MINUS){
                        public_val = builder->create_fsub(lval, rval);
                    } else {
                        LOG(ERROR) << "Unkown ADDOP.\n";
                        exit(1);
                    }
                } else {
                    if (node.op == OP_PLUS){
                        public_val = builder->create_iadd(lval, rval);
                    } else if (node.op == OP_MINUS){
                        public_val = builder->create_isub(lval, rval);
                    } else {
                        LOG(ERROR) << "Unkown ADDOP.\n";
                        exit(1);
                    }
                }
            } else if (ltype->is_float_type()){
                if (rtype->is_integer_type()){
                    rval = builder->create_sitofp(rval, Type::get_float_type(module.get()));
                } 

                if (node.op == OP_PLUS){
                    public_val = builder->create_fadd(lval, rval);
                } else if (node.op == OP_MINUS){
                    public_val = builder->create_fsub(lval, rval);
                } else {
                    LOG(ERROR) << "Unkown ADDOP.\n";
                    exit(1);
                }
            } else {
                LOG(ERROR) << "Unkown left additive_expression type.\n";
                exit(1);
            }
            
        }
    }
}

void CminusfBuilder::visit(ASTTerm &node) { 
    LOG(INFO) << "Visiting ASTTerm\n";
    if (!isReturned){
        if (node.term == nullptr){
            if (node.factor != nullptr){
                node.factor->accept(*this);
            }else {
                LOG(ERROR) << "Wrong Term.\n";
            }
        } else{
            node.term->accept(*this);
            auto lval = public_val;
            auto ltype = lval->get_type();
            node.factor->accept(*this);
            auto rval = public_val;
            auto rtype = rval->get_type();
            
            // Get value of pointer 
            if (ltype->is_integer_type() && static_cast<IntegerType *>(ltype)->get_num_bits() == 1){
                lval = builder->create_zext(lval, Type::get_int32_type(module.get()));
                ltype = lval->get_type();
            } 
            if (rtype->is_integer_type() && static_cast<IntegerType *>(rtype)->get_num_bits() == 1){
                rval = builder->create_zext(rval, Type::get_int32_type(module.get()));
                rtype = rval->get_type();
            }

            // Type transforming
            if (ltype->is_integer_type()){
                if (rtype->is_float_type()){
                    lval = builder->create_sitofp(lval, Type::get_float_type(module.get()));
                    if (node.op == OP_MUL){
                        public_val = builder->create_fmul(lval, rval);
                    } else if (node.op == OP_DIV){
                        public_val = builder->create_fdiv(lval, rval);
                    } else {
                        LOG(ERROR) << "Unkown MULOP.\n";
                        exit(1);
                    }
                } else {
                    if (node.op == OP_MUL){
                        public_val = builder->create_imul(lval, rval);
                    } else if (node.op == OP_DIV){
                        public_val = builder->create_isdiv(lval, rval);
                    } else {
                        LOG(ERROR) << "Unkown MULOP.\n";
                        exit(1);
                    }
                }
            } else if (ltype->is_float_type()){
                if (rtype->is_integer_type()){
                    rval = builder->create_sitofp(rval, Type::get_float_type(module.get()));
                }
                
                if (node.op == OP_MUL){
                    public_val = builder->create_fmul(lval, rval);
                } else if (node.op == OP_DIV){
                    public_val = builder->create_fdiv(lval, rval);
                } else {
                    LOG(ERROR) << "Unkown MULOP.\n";
                    exit(1);
                }
            } else {
                LOG(ERROR) << "Unkown left additive_expression type.\n";
                exit(1);
            }
        }
    }
}

void CminusfBuilder::visit(ASTCall &node) { 
    LOG(INFO) << "Visiting ASTCall\n";
    auto fun = scope.find(node.id);
    std::vector<Value *> args;
    auto fun_type = static_cast<FunctionType *>(fun->get_type());
    fun = static_cast<Function *>(fun);
    int i=0;
    for (auto arg_type = fun_type->param_begin(); arg_type != fun_type->param_end(); arg_type++){
        node.args[i]->accept(*this);
        if (*arg_type != public_val->get_type()){
            if (public_val->get_type()->is_integer_type()){
                public_val = builder->create_sitofp(public_val, Type::get_float_type(module.get()));
            } else if (public_val->get_type()->is_float_type()){
                public_val = builder->create_fptosi(public_val, Type::get_int32_type(module.get()));
            } else {
                LOG(ERROR) << "Unkown argument type.\n";
                exit(1);
            }
        }
        args.push_back(public_val);
        i++;
    }
    public_val = builder->create_call(fun, args);
}
