#include <algorithm>
#include "logging.hpp"
#include "LoopSearch.hpp"
#include "LoopInvHoist.hpp"

void LoopInvHoist::run()
{
    // 先通过LoopSearch获取循环的相关信息
    LoopSearch loop_searcher(m_, false);
    loop_searcher.run();

    // 接下来由你来补充啦！
    // 处理所有循环
    for (auto func : m_->get_functions())
    {
        for (auto bb_set : loop_searcher.get_loops_in_func(func))
        {
            auto base = loop_searcher.get_loop_base(bb_set);

            // 如果有变化需要迭代
            int flag = 1;
            while (flag)
            {
                flag = 0;

                // 保存左值
                std::unordered_set<Value *> exists;
                for (auto bb : *bb_set)
                {
                    for (auto ins : bb->get_instructions())
                    {
                        exists.insert(ins);
                    }
                }

                std::set<Instruction *> wait_list;
                std::unordered_map<Instruction *, BasicBlock *> mp;

                // 分析指令参数，找出循环不变式
                for (auto bb : *bb_set)
                {
                    for (auto ins : bb->get_instructions())
                    {
                        if (ins->is_phi() || ins->is_call() || ins->is_load() || ins->is_store() || ins->is_ret() || ins->is_br())
                        {
                            continue;
                        }

                        int in_loop = 0;
                        for (auto val : ins->get_operands())
                        {
                            if (exists.find(val) != exists.end())
                            {
                                in_loop = 1;
                                break;
                            }
                        }

                        if (!in_loop)
                        {
                            wait_list.insert(ins);
                            mp[ins] = bb;
                            exists.erase(ins);
                            flag = 1;
                        }
                    }
                }

                // 循环不变式外提
                for (auto list : wait_list)
                {                 
                    BasicBlock *bb;
                    Instruction *ins;
                    ins = list;
                    bb = mp[list];
                    // 此处取第1个前驱块即可
                    auto prev = *loop_searcher.get_loop_base(bb_set)->get_pre_basic_blocks().begin();
                    if (prev == nullptr)
                        continue;
                    auto term = prev->get_terminator();
                    prev->delete_instr(term);
                    bb->delete_instr(ins);

                    ins->set_parent(prev);

                    prev->add_instruction(ins);
                    prev->add_instruction(term);                
                }
            }
        }
    }
}
