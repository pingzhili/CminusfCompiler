#include "BasicBlock.h"
#include "Constant.h"
#include "Function.h"
#include "IRBuilder.h"
#include "Module.h"
#include "Type.h"

#include <iostream>
#include <memory>

#ifdef DEBUG                                             // 用于调试信息,大家可以在编译过程中通过" -DDEBUG"来开启这一选项
#define DEBUG_OUTPUT std::cout << __LINE__ << std::endl; // 输出行号的简单示例
#else
#define DEBUG_OUTPUT
#endif

#define CONST_INT(num) \
    ConstantInt::get(num, module)

#define CONST_FP(num) \
    ConstantFP::get(num, module) // 得到常数值的表示,方便后面多次用到

int main()
{
    auto module = new Module("Cminus code"); // module name是什么无关紧要
    auto builder = new IRBuilder(nullptr, module);
    Type *Int32Type = Type::get_int32_type(module);
    Type *FloatType = Type::get_float_type(module);

    // main定义
    auto mainFunTy = FunctionType::get(Int32Type, {});
    auto mainFun = Function::create(mainFunTy, "main", module);
    auto bb = BasicBlock::create(module, "entry", mainFun);

    builder->set_insert_point(bb); 

    auto retAlloca = builder->create_alloca(Int32Type); // 在内存中分配返回值的位置
    builder->create_store(CONST_INT(0), retAlloca);     // 默认 ret 0

    // main内容
    auto aAlloca = builder->create_alloca(FloatType); // 在内存中分配参a的位置
    builder->create_store(CONST_FP(5.555), aAlloca);
    auto aLoad = builder->create_load(aAlloca);              // 将参数a load上来
    auto fcmp = builder->create_fcmp_gt(aLoad, CONST_FP(1)); // a和1的比较

    auto trueBB = BasicBlock::create(module, "trueBB", mainFun); // true分支
    auto falseBB = BasicBlock::create(module, "falseBB", mainFun); // false分支                                            // 我调试的时候故意留下来的,以醒目地提醒你这个调试用的宏定义方法
    builder->create_cond_br(fcmp, trueBB, falseBB);

    builder->set_insert_point(trueBB);  // if true
    builder->create_ret(CONST_INT(233));

    builder->set_insert_point(falseBB); // if false
    builder->create_ret(CONST_INT(0));

    std::cout << module->print();
    delete module;
    return 0;
}
