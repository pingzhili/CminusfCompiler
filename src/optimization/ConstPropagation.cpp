#include "ConstPropagation.hpp"
#include "logging.hpp"
#include <cstdlib>

using namespace std;
// 给出了返回整形值的常数折叠实现，大家可以参考，在此基础上拓展
// 当然如果同学们有更好的方式，不强求使用下面这种方式

ConstantInt *ConstFolder::compute(Instruction::OpID op, ConstantInt *value1,
                                  ConstantInt *value2)
{
    switch (op)
    {
    case Instruction::add:
        return ConstantInt::get(value1->get_value() + value2->get_value(), module_);
        break;
    case Instruction::sub:
        return ConstantInt::get(value1->get_value() - value2->get_value(), module_);
        break;
    case Instruction::mul:
        return ConstantInt::get(value1->get_value() * value2->get_value(), module_);
        break;
    case Instruction::sdiv:
        return ConstantInt::get((int)(value1->get_value() / value2->get_value()), module_);
        break;
    default:
        return nullptr;
        break;
    }
}

ConstantFP *ConstFolder::computefp(Instruction::OpID op, ConstantFP *value1,
                                   ConstantFP *value2)
{
    switch (op)
    {
    case Instruction::fadd:
        return ConstantFP::get(value1->get_value() + value2->get_value(), module_);
        break;
    case Instruction::fsub:
        return ConstantFP::get(value1->get_value() - value2->get_value(), module_);
        break;
    case Instruction::fmul:
        return ConstantFP::get(value1->get_value() * value2->get_value(), module_);
        break;
    case Instruction::fdiv:
        return ConstantFP::get((float)(value1->get_value() / value2->get_value()), module_);
        break;
    default:
        return nullptr;
        break;
    }
}

ConstantInt *ConstFolder::compute_comp(CmpInst::CmpOp op, ConstantInt *value1,
                                       ConstantInt *value2)
{
    switch (op)
    {
    case CmpInst::EQ:
        return ConstantInt::get(value1->get_value() == value2->get_value(), module_);
    case CmpInst::NE:
        return ConstantInt::get(value1->get_value() != value2->get_value(), module_);
        break;
    case CmpInst::GT:
        return ConstantInt::get(value1->get_value() > value2->get_value(), module_);
        break;
    case CmpInst::GE:
        return ConstantInt::get(value1->get_value() >= value2->get_value(), module_);
        break;
    case CmpInst::LT:
        return ConstantInt::get(value1->get_value() < value2->get_value(), module_);
        break;
    case CmpInst::LE:
        return ConstantInt::get(value1->get_value() <= value2->get_value(), module_);
        break;
    default:
        return nullptr;
        break;
    }
}

ConstantInt *ConstFolder::compute_fcomp(FCmpInst::CmpOp op, ConstantFP *value1,
                                        ConstantFP *value2)
{
    switch (op)
    {
    case FCmpInst::EQ:
        return ConstantInt::get(value1->get_value() == value2->get_value(), module_);
        break;
    case FCmpInst::NE:
        return ConstantInt::get(value1->get_value() != value2->get_value(), module_);
        break;
    case FCmpInst::GT:
        return ConstantInt::get(value1->get_value() > value2->get_value(), module_);
        break;
    case FCmpInst::GE:
        return ConstantInt::get(value1->get_value() >= value2->get_value(), module_);
        break;
    case FCmpInst::LT:
        return ConstantInt::get(value1->get_value() < value2->get_value(), module_);
        break;
    case FCmpInst::LE:
        return ConstantInt::get(value1->get_value() <= value2->get_value(), module_);
        break;
    default:
        return nullptr;
        break;
    }
}

// 用来判断value是否为ConstantFP，如果不是则会返回nullptr
ConstantFP *cast_constantfp(Value *value)
{
    auto constant_fp_ptr = dynamic_cast<ConstantFP *>(value);
    if (constant_fp_ptr)
    {
        return constant_fp_ptr;
    }
    else
    {
        return nullptr;
    }
}
ConstantInt *cast_constantint(Value *value)
{
    auto constant_int_ptr = dynamic_cast<ConstantInt *>(value);
    if (constant_int_ptr)
    {
        return constant_int_ptr;
    }
    else
    {
        return nullptr;
    }
}

void ConstPropagation::run()
{
    // 从这里开始吧！
    auto CF = ConstFolder(m_);
    for (auto func : m_->get_functions())
    {
        if (func->get_basic_blocks().size())
        {
            std::queue<BasicBlock *> BBqueue;
            std::set<BasicBlock *> visitedBB;
            BBqueue.push(func->get_entry_block());

            /*Const propagation*/
            while (!BBqueue.empty())
            { // DFS
                auto bb = BBqueue.front();
                BBqueue.pop();
                visitedBB.insert(bb);
                std::set<Instruction *> to_delete_instrs;
                std::unordered_map<Value *, std::vector<Value *>> pointer_const;
                for (auto instr : bb->get_instructions())
                {
                    if (instr->get_num_operand() < 1)
                    {
                        continue;
                    }
                    if (instr->is_phi())
                    {
                        for (int i = 0; i < instr->get_num_operand() / 2; i++)
                        {
                            auto br =
                                static_cast<BasicBlock *>(instr->get_operand(2 * i + 1));
                            bool in_pre_bb = false;
                            for (auto pre_bb : bb->get_pre_basic_blocks())
                            {
                                if (pre_bb == br)
                                {
                                    in_pre_bb = true;
                                }
                            }
                            if (in_pre_bb == false)
                            {
                                instr->remove_operands(2 * i, 2 * i + 1);
                            }
                        }
                    }
                    else if (instr->isBinary())
                    {
                        auto bin_instr = dynamic_cast<BinaryInst *>(instr);
                        auto lval = instr;
                        auto lhs = bin_instr->get_operand(0);
                        auto rhs = bin_instr->get_operand(1);
                        auto op = instr->get_instr_type();
                        if ((instr->is_add()) || (instr->is_sub()) || (instr->is_mul()) ||
                            (instr->is_div()))
                        {
                            ConstantInt *const_val;
                            if (cast_constantint(lhs) != nullptr &&
                                cast_constantint(rhs) != nullptr)
                            {
                                const_val = CF.compute(op, cast_constantint(lhs),
                                                       cast_constantint(rhs));
                                instr->replace_all_use_with(const_val);
                                to_delete_instrs.insert(instr);
                            }
                        }
                        else
                        {
                            ConstantFP *const_val;
                            if (cast_constantfp(lhs) != nullptr &&
                                cast_constantfp(rhs) != nullptr)
                            {
                                const_val = CF.computefp(op, cast_constantfp(lhs),
                                                         cast_constantfp(rhs));
                                instr->replace_all_use_with(const_val);
                                to_delete_instrs.insert(instr);
                            }
                        }
                    }
                    else if (instr->is_zext())
                    {
                        if (cast_constantint(instr->get_operand(0)) != nullptr)
                        {
                            auto bool_val =
                                cast_constantint(instr->get_operand(0))->get_value();
                            auto const_val = ConstantInt::get(static_cast<int>(bool_val), m_);
                            instr->replace_all_use_with(const_val);
                            to_delete_instrs.insert(instr);
                        }
                    }
                    else if (instr->is_si2fp())
                    {
                        if (cast_constantint(instr->get_operand(0)) != nullptr)
                        {
                            auto fp_val = static_cast<float>(
                                cast_constantint(instr->get_operand(0))->get_value());
                            auto const_val = ConstantFP::get(fp_val, m_);
                            instr->replace_all_use_with(const_val);
                            to_delete_instrs.insert(instr);
                        }
                    }
                    else if (instr->is_fp2si())
                    {
                        if (cast_constantfp(instr->get_operand(0)) != nullptr)
                        {
                            auto int_val = static_cast<int>(
                                cast_constantfp(instr->get_operand(0))->get_value());
                            auto const_val = ConstantInt::get(int_val, m_);
                            instr->replace_all_use_with(const_val);
                            to_delete_instrs.insert(instr);
                        }
                    }
                    else if (instr->is_store())
                    {
                        pointer_const[static_cast<StoreInst *>(instr)->get_lval()]
                            .push_back(static_cast<StoreInst *>(instr)->get_rval());
                    }
                    else if (instr->is_load())
                    {
                        if (pointer_const.find(instr->get_operand(0)) !=
                            pointer_const.end())
                        {
                            instr->replace_all_use_with(
                                pointer_const[instr->get_operand(0)].back());
                            to_delete_instrs.insert(instr);
                        }
                        else
                        {
                            pointer_const[instr->get_operand(0)].push_back(instr);
                        }
                    }
                    else if (instr->is_cmp())
                    {
                        auto lhs = instr->get_operand(0);
                        auto rhs = instr->get_operand(1);
                        if (cast_constantint(lhs) != nullptr &&
                            cast_constantint(rhs) != nullptr)
                        {
                            auto cmp_op = dynamic_cast<CmpInst *>(instr)->get_cmp_op();
                            auto cmp_val = CF.compute_comp(cmp_op, cast_constantint(lhs),
                                                           cast_constantint(rhs));
                            instr->replace_all_use_with(cmp_val);
                            to_delete_instrs.insert(instr);
                        }
                    }
                    else if (instr->is_fcmp())
                    {
                        auto lhs = instr->get_operand(0);
                        auto rhs = instr->get_operand(1);
                        if (cast_constantfp(lhs) != nullptr &&
                            cast_constantfp(rhs) != nullptr)
                        {
                            auto cmp_op = dynamic_cast<FCmpInst *>(instr)->get_cmp_op();
                            auto cmp_val = CF.compute_fcomp(cmp_op, cast_constantfp(lhs),
                                                            cast_constantfp(rhs));
                            instr->replace_all_use_with(cmp_val);
                            to_delete_instrs.insert(instr);
                        }
                    }
                    else if (instr->is_br())
                    {
                        auto br_instr = dynamic_cast<BranchInst *>(instr);
                        if (br_instr->is_cond_br())
                        {
                            auto br_cond = br_instr->get_operand(0);
                            auto trueBB =
                                dynamic_cast<BasicBlock *>(br_instr->get_operand(1));
                            auto falseBB =
                                dynamic_cast<BasicBlock *>(br_instr->get_operand(2));

                            if (dynamic_cast<ConstantInt *>(br_cond))
                            {
                                if (dynamic_cast<ConstantInt *>(br_cond)
                                        ->get_value())
                                { // Always true
                                    br_instr->replace_all_use_with(
                                        BranchInst::create_br(trueBB, bb));
                                    if (visitedBB.find(trueBB) == visitedBB.end())
                                    {
                                        visitedBB.insert(trueBB);
                                        BBqueue.push(trueBB);
                                    }
                                }
                                else
                                { // Always false
                                    br_instr->replace_all_use_with(
                                        BranchInst::create_br(falseBB, bb));
                                    if (visitedBB.find(falseBB) == visitedBB.end())
                                    {
                                        visitedBB.insert(falseBB);
                                        BBqueue.push(falseBB);
                                    }
                                }
                                to_delete_instrs.insert(instr);
                            }
                            else if (dynamic_cast<ConstantFP *>(br_cond))
                            {
                                if (dynamic_cast<ConstantFP *>(br_cond)
                                        ->get_value())
                                { // Always true
                                    br_instr->replace_all_use_with(
                                        BranchInst::create_br(trueBB, bb));
                                    if (visitedBB.find(trueBB) == visitedBB.end())
                                    {
                                        visitedBB.insert(trueBB);
                                        BBqueue.push(trueBB);
                                    }
                                }
                                else
                                { // Always false
                                    br_instr->replace_all_use_with(
                                        BranchInst::create_br(falseBB, bb));
                                    if (visitedBB.find(falseBB) == visitedBB.end())
                                    {
                                        visitedBB.insert(falseBB);
                                        BBqueue.push(falseBB);
                                    }
                                }
                                to_delete_instrs.insert(instr);
                            }
                            else
                            {
                                if (visitedBB.find(trueBB) == visitedBB.end())
                                {
                                    visitedBB.insert(trueBB);
                                    BBqueue.push(trueBB);
                                }
                                if (visitedBB.find(falseBB) == visitedBB.end())
                                {
                                    visitedBB.insert(falseBB);
                                    BBqueue.push(falseBB);
                                }
                            }
                        }
                        else
                        {
                            auto trueBB =
                                dynamic_cast<BasicBlock *>(br_instr->get_operand(0));
                            if (visitedBB.find(trueBB) == visitedBB.end())
                            {
                                visitedBB.insert(trueBB);
                                BBqueue.push(trueBB);
                            }
                        }
                    }
                }
                for (auto instr : to_delete_instrs)
                {
                    bb->delete_instr(instr);
                }
            }
        }
    }
}

