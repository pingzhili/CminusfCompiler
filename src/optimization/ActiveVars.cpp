#include "ActiveVars.hpp"
#include <algorithm>
#include "logging.hpp"

#define CONST_INT(ptr) (dynamic_cast<ConstantInt *>(ptr) != nullptr)
#define CONST_FP(ptr) (dynamic_cast<ConstantFP *>(ptr) != nullptr)
#define CONTAIN(set, val) (set.find(val) != set.end())

void ActiveVars::run()
{
    std::ofstream output_active_vars;
    output_active_vars.open("active_vars.json", std::ios::out);
    output_active_vars << "[";
    for (auto &func : this->m_->get_functions())
    {
        if (func->get_basic_blocks().empty())
        {
            continue;
        }
        else
        {
            func_ = func;
            func_->set_instr_name();
            live_in.clear();
            live_out.clear();

            // 在此分析 func_ 的每个bb块的活跃变量，并存储在 live_in live_out 结构内

            for (auto &BB : func_->get_basic_blocks())
            {
                std::set<Value *> use_var = {};
                std::set<Value *> def_var = {};
                for (auto &instr : BB->get_instructions())
                {
                    if (instr->is_ret())
                    {
                        //返回指令，只需考虑有返回值
                        if (!dynamic_cast<ReturnInst *>(instr)->is_void_ret() && !CONTAIN(def_var, instr->get_operand(0)) && !CONST_INT(instr->get_operand(0)) && !CONST_FP(instr->get_operand(0)))
                        {
                            use_var.insert(instr->get_operand(0));
                        }
                    }
                    else if (instr->is_br())
                    {
                        //跳转指令，只需考虑条件跳转
                        if (dynamic_cast<BranchInst *>(instr)->is_cond_br() && !CONTAIN(def_var, instr->get_operand(0)) && !CONST_INT(instr->get_operand(0)) && !CONST_FP(instr->get_operand(0)))
                        {
                            use_var.insert(instr->get_operand(0));
                        }
                    }
                    else if (instr->isBinary() || instr->is_fcmp() || instr->is_cmp())
                    {
                        //运算和比较指令，两操作数，左值就是指令自身
                        if (!CONTAIN(def_var, instr->get_operand(0)) && !CONST_INT(instr->get_operand(0)) && !CONST_FP(instr->get_operand(0)))
                        {
                            use_var.insert(instr->get_operand(0));
                        }
                        if (!CONTAIN(def_var, instr->get_operand(1)) && !CONST_INT(instr->get_operand(1)) && !CONST_FP(instr->get_operand(1)))
                        {
                            use_var.insert(instr->get_operand(1));
                        }
                        if (!CONTAIN(use_var, instr))
                        {
                            def_var.insert(instr);
                        }
                    }
                    else if (instr->is_alloca())
                    {
                        //alloca指令，没有右值
                        if (!CONTAIN(use_var, instr))
                        {
                            def_var.insert(instr);
                        }
                    }
                    else if (instr->is_load())
                    {
                        //load指令，一操作数，左值就是指令自身
                        if (!CONTAIN(def_var, instr->get_operand(0)) && !CONST_INT(instr->get_operand(0)) && !CONST_FP(instr->get_operand(0)))
                        {
                            use_var.insert(instr->get_operand(0));
                        }
                        if (!CONTAIN(use_var, instr))
                        {
                            def_var.insert(instr);
                        }
                    }
                    else if (instr->is_store())
                    {
                        //store指令，两操作数，指令自身不会作为操作数被引用
                        if (!CONTAIN(def_var, instr->get_operand(0)) && !CONST_INT(instr->get_operand(0)) && !CONST_FP(instr->get_operand(0)))
                        {
                            use_var.insert(instr->get_operand(0));
                        }
                        if (!CONTAIN(def_var, instr->get_operand(1)) && !CONST_INT(instr->get_operand(1)) && !CONST_FP(instr->get_operand(1)))
                        {
                            use_var.insert(instr->get_operand(1));
                        }
                    }
                    else if (instr->is_zext() || instr->is_si2fp() || instr->is_fp2si())
                    {
                        //CastInst指令，一操作数
                        if (!CONTAIN(def_var, instr->get_operand(0)))
                        {
                            use_var.insert(instr->get_operand(0));
                        }
                        if (!CONTAIN(use_var, instr))
                        {
                            def_var.insert(instr);
                        }
                    }
                    else if (instr->is_phi())
                    {
                        //phi指令，pair(bb,var)中的var为右值，指令本身为左值
                        for (int i = 0; i < instr->get_num_operand() / 2; i++)
                        {
                            if (!CONTAIN(def_var, instr->get_operand(2 * i)) && !CONST_INT(instr->get_operand(2 * i)) && !CONST_FP(instr->get_operand(2 * i)))
                            {
                                use_var.insert(instr->get_operand(2 * i));
                                phiUse[BB].insert(instr->get_operand(2 * i));
                                phiOut[dynamic_cast<BasicBlock *>(instr->get_operand(2 * i + 1))].insert(instr->get_operand(2 * i));
                            }
                            if (!CONTAIN(use_var, instr))
                            {
                                def_var.insert(instr);
                            }
                        }
                    }
                    else if (instr->is_call())
                    {
                        //call指令，第一个参数label不计，指令本身为左值
                        for (int i = 1; i < instr->get_num_operand(); i++)
                        {
                            if (!CONTAIN(def_var, instr->get_operand(i)) && !CONST_INT(instr->get_operand(i)) && !CONST_FP(instr->get_operand(i)))
                            {
                                use_var.insert(instr->get_operand(i));
                            }
                        }
                        if (!CONTAIN(use_var, instr) && !instr->is_void())
                        {
                            def_var.insert(instr);
                        }
                    }
                    else if (instr->is_gep())
                    {
                        //gep指令，参数数量不定，指令本身为左值
                        for (int i = 0; i < instr->get_num_operand(); i++)
                        {
                            if (!CONTAIN(def_var, instr->get_operand(i)) && !CONST_INT(instr->get_operand(i)) && !CONST_FP(instr->get_operand(i)))
                            {
                                use_var.insert(instr->get_operand(i));
                            }
                        }
                        if (!CONTAIN(use_var, instr))
                        {
                            def_var.insert(instr);
                        }
                    }
                }
                useSet.insert(std::pair<BasicBlock *, std::set<Value *>>(BB, use_var));
                defSet.insert(std::pair<BasicBlock *, std::set<Value *>>(BB, def_var));
            }

            bool is_changed = true;
            while (is_changed)
            {
                is_changed = false;
                for (auto &BB : func_->get_basic_blocks())
                {
                    if (!(BB->get_succ_basic_blocks().size() || BB->get_pre_basic_blocks().size()))
                        continue;

                    //对live_out操作
                    std::set<Value *> OutSet = {};
                    for (auto &succBB : BB->get_succ_basic_blocks())
                    {
                        for (auto &item : live_in[succBB])
                        {
                            if (!CONTAIN(phiUse[succBB], item))
                                OutSet.insert(item);
                            else if (CONTAIN(phiUse[succBB], item) && CONTAIN(phiOut[BB], item))
                                OutSet.insert(item);
                        }
                    }
                    live_out[BB] = OutSet;

                    //对live_in操作
                    std::set<Value *> InSet = OutSet;
                    for (auto &item : defSet[BB])
                    {
                        if (CONTAIN(InSet, item))
                            InSet.erase(item);
                    }
                    for (auto &item : useSet[BB])
                    {
                        InSet.insert(item);
                    }
                    if (InSet != live_in[BB])
                        is_changed = true;
                    live_in[BB] = InSet;
                }
            }

            output_active_vars << print();
            output_active_vars << ",";
        }
    }
    output_active_vars << "]";
    output_active_vars.close();
    return;
}

std::string ActiveVars::print()
{
    std::string active_vars;
    active_vars += "{\n";
    active_vars += "\"function\": \"";
    active_vars += func_->get_name();
    active_vars += "\",\n";

    active_vars += "\"live_in\": {\n";
    for (auto &p : live_in)
    {
        if (p.second.size() == 0)
        {
            continue;
        }
        else
        {
            active_vars += "  \"";
            active_vars += p.first->get_name();
            active_vars += "\": [";
            for (auto &v : p.second)
            {
                active_vars += "\"%";
                active_vars += v->get_name();
                active_vars += "\",";
            }
            active_vars += "]";
            active_vars += ",\n";
        }
    }
    active_vars += "\n";
    active_vars += "    },\n";

    active_vars += "\"live_out\": {\n";
    for (auto &p : live_out)
    {
        if (p.second.size() == 0)
        {
            continue;
        }
        else
        {
            active_vars += "  \"";
            active_vars += p.first->get_name();
            active_vars += "\": [";
            for (auto &v : p.second)
            {
                active_vars += "\"%";
                active_vars += v->get_name();
                active_vars += "\",";
            }
            active_vars += "]";
            active_vars += ",\n";
        }
    }
    active_vars += "\n";
    active_vars += "    }\n";

    active_vars += "}\n";
    active_vars += "\n";
    return active_vars;
}
