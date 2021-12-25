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

    // main函数
    auto mainFunTy = FunctionType::get(Int32Type, {});
    auto mainFun = Function::create(mainFunTy, "main", module);
    auto bb = BasicBlock::create(module, "entry", mainFun);

    builder->set_insert_point(bb); // 一个BB的开始,将当前插入指令点的位置设在bb

    auto retAlloca = builder->create_alloca(Int32Type); // 在内存中分配返回值的位置
    builder->create_store(CONST_INT(0), retAlloca); // 默认 ret 0

    auto *arrayType = ArrayType::get(Int32Type,10);   // int a[10]
    auto aAlloca = builder->create_alloca(arrayType); // 在内存中分配a[10]的位置

    auto a0GEP = builder->create_gep(aAlloca, {CONST_INT(0), CONST_INT(0)}); // 获取a[0]地址
    builder->create_store(CONST_INT(10), a0GEP); //a[0] = 10

    auto a1GEP = builder->create_gep(aAlloca, {CONST_INT(0), CONST_INT(1)}); // 获取a[1]地址
    auto a0Load = builder->create_load(a0GEP); // 从a[0]地址读取a[0]
    auto mul = builder->create_imul(a0Load, CONST_INT(2)); // a[0] * 2
    builder->create_store(mul, a1GEP); // 将mul写入a[1]

    auto a1Load = builder->create_load(a1GEP); // 从a[1]地址读取a[1]
    builder->create_ret(a1Load); 

    std::cout << module->print();
    delete module;
    return 0;
}