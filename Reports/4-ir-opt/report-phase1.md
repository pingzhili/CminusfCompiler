# Lab4 实验报告-阶段一

罗正 PB1903\*\*\*\*     李平治 PB1907\*\*\*\*

## 实验要求

阅读活跃变量和静态单赋值格式构造文档

阅读loopSearch和Mem2Reg的相关源代码

能描述优化的基本流程

理解CFG和强连通分量算法

理解phi函数和支配树在复写传播和SSA中的作用

完成思考题



## 思考题
### LoopSearch
1. `LoopSearch `**中直接用于描述一个循环的数据结构是什么？需要给出其具体类型。**

   loopsearch.hpp中使用了一个基本块指针`BasicBlock *	` 的集合：

   ```c++
   BBset_t = std::unordered_set<BasicBlock *>
   ```

   其中 `BasicBlock` 是IR中生成的基本块

   

2. **循环入口是重要的信息，请指出**`LoopSearch`**中如何获取一个循环的入口？需要指出具体代码，并解释思路。**

   获取循环入口通过函数 `find_loop_base` 实现。

   ```c++
   CFGNodePtr LoopSearch::find_loop_base(CFGNodePtrSet *set, CFGNodePtrSet &reserved)
   ```

   - 输入一个Tarjan Algorithm获得的强连通分量 `set` 和已经获取的最外层循环入口块集合 `reserved`
   - 输出该强连通分量中未获取到的最外层循环入口块。

   具体来说：

   ```c++
   CFGNodePtr base = nullptr;
   for (auto n : *set)
   {
       // Step1. 遍历强连通分量结点
       for (auto prev : n->prevs)
       {
           // Step2. 遍历该基本块的前驱
           if (set->find(prev) == set->end())
           {
                   // Step3. 若该基本块的前驱不在该强联通分量中，则该块是该强连通分量中最外层循环的入口块。此时用base记录该结点
                   base = n;
           }
       }
   }
   if (base != nullptr)
       // Step4.1. 若base块不为空，则说明已找到，返回
       return base;
   for (auto res : reserved)
   {
       // Step4.2. 若base块为空，则说明当前强连通分量所有块的前驱都在内部，即该循环是某一个更大循环的内循环。因为大循环的入口块的后继都在此前被删除，故此时找不到外部的前驱，但相关信息在删除时被保存在了reserved集合中，因此遍历reserved集合中的所有base
       for (auto succ : res->succs)
       {
           // Step5. 遍历base的后继结点
           if (set->find(succ) != set->end())
           {
               // Step6. 如果该后继结点在强连通分量集合中，则该后继就是当前小循环的入口
               base = succ;
           }
       }
   }
   return base;
   ```

   

3. **仅仅找出强连通分量并不能表达嵌套循环的结构。为了处理嵌套循环，**`LoopSearch`**在Tarjan algorithm的基础之上做了什么特殊处理？**

   Tarjan Algorithm帮助`LoopSearch`找到的强连通分量集合，实际上是所有最外层循环的集`scss`。搜索算法在此基础上做了以下工作：

   * 维护辅助集合`reserved`来保存当前已经找到的`base`循环入口
   * 遍历`scss`中的所有最外层循环，找到当前最外层循环的入口块`base`，保存相关信息后将其放入`reserved`中
   * 将`base`从其前驱结点的后继列表中删除，将`base`从其后继结点的前驱列表中删除，即抹除`base`在其他所有结点处存在的痕迹。这样将会破坏当前所在的最外层循环，从而使得循环数减小`1`，并且下次迭代时得以进入内部的循环
   * 遍历`scss`结束时，依据更新后的CFG重置`scss`和结点遍历信息，重新开始迭代，直至不存在循环/强连通分量。（实际上，每次重新开始迭代都是一次向内层循环的深入，这里可以理解为对循环的广度优先搜索）

   

4. **某个基本块可以属于多层循环中，**`LoopSearch`**找出其所属的最内层循环的思路是什么？这里需要用到什么数据？这些数据在何时被维护？需要指出数据的引用与维护的代码，并简要分析。**

   找出所属最内层循环的函数为：

   ```c++
   BBset_t *get_inner_loop(BasicBlock* bb) {
           if(bb2base.find(bb) == bb2base.end())
               return nullptr;
           return base2loop[bb2base[bb]];
       }
   ```

   这里用到的`bb2base`和`base2loop`字典映射，主要是依靠的是`bb2base`记录了基本块`bb`和其所在最内层循环入口`base`的对应关系，其维护是在问题3中所述的搜索中进行的，涉及的代码段如下：

   ```c++
   // step 4: store result
   auto bb_set = new BBset_t;
   std::string node_set_string = "";
   for (auto n : *scc)
   {
       bb_set->insert(n->bb);
       node_set_string = node_set_string + n->bb->get_name() + ',';
   }
   loop_set.insert(bb_set);
   func2loop[func].insert(bb_set);
   base2loop.insert({base->bb, bb_set});
   loop2base.insert({bb_set, base->bb});
   // step 5: map each node to loop base
   for (auto bb : *bb_set)
   {
       if (bb2base.find(bb) == bb2base.end())
           bb2base.insert({bb, base->bb});
       else
           bb2base[bb] = base->bb;
   }
   ```
   
   具体来说：
   
   * 每次搜索到一个`base`时，都将其对应的当前强连通图（当前的最外层循环）入口与其作映射并存入`bb2base`字典中；
   * 同一个`bb` 能够对应到多个循环的入口`base`。然而如问题3所述，该算法是对于循环的广度优先搜索，靠近内部的循环将晚于其外部大循环被算法搜索到，因此其`bb` 到当前`base`的映射将会覆盖此前外部循环的记录；
   * 当循环结束时，`bb2base `所记录的映射就是基本块到最内层循环入口了；
   * 最后通过`base2loop`找到最内层循环。
   
   

### Mem2reg
1. **请简述概念：支配性、严格支配性、直接支配性、支配边界。**

   * 支配性：

     如果从入口结点到结点 m 的每条路径都经过结点 n，则称结点 n 支配结点 m，记为$n\in Dom(m)$。平凡地，$n\in Dom(n)$。

   * 严格支配性：

     当且仅当 $a\in Dom(b)-\{b\}$时，a 严格支配 b

   * 直接支配性：

     严格支配 n 的结点集中与 n 最接近的结点称为 n 的直接支配结点。

     或者说，从入口结点到达 n 的所有路径上，结点 n 的最后一个支配结点称为直接支配结点。

   * 支配边界：

     将相对于 n 具有如下性质的结点 m 的集合称为 n 的支配边界，记为 `DF(n)`：

     （1）n 支配 m 的一个前驱；

     （2）n 并不严格支配 m。

     通俗来说，其表示「在离开n的每条CFG路径上，从结点n可达但不支配的第一个结点」的集合。

   

2. `phi`**节点是SSA的关键特征，请简述**`phi`**节点的概念，以及引入**`phi`**节点的理由。**

   phi节点是用于根据当前块的前导选择值的指令，为选择语句之后的语句选出来正确的分支中的值。

   其引入的目的是解决静态单赋值赋值一次的问题，为后续指令选择变量的正确版本，例如：

   ```c++
   if(flag) 
       x = 1; 
   else 
       x = 2;
   y = x;
   ```

   在 SSA 中给这两个分支中的 x 使用不同的名字，则此处对 y 的取值无法决定，因此引入 `phi` 函数用来确定在 x 应该使用哪个名字的变量值：

   ```c++
   if(flag) 
       x1 = -1; 
   else 
       x2 = 1;
   y = phi(x1,x2); // phi(x1,x2) 返回某个变元的值，取决于到达 phi 时所通过的路径
   ```

   

3. **下面给出的**`cminus`**代码显然不是SSA的，后面是使用lab3的功能将其生成的LLVM IR（未加任何Pass），说明对一个变量的多次赋值变成了什么形式？**

   `cminus`代码：

   ```c++
   int main(void){
       int a;
       a = 0;
       a = 1 + 2;
       a = a * 4;
       return 0;
   }
   ```

   LLVM IR代码：

   ```c++
   ; ModuleID = 'cminus'
   source_filename = "non_ssa.cminus"
   
   declare i32 @input()
   
   declare void @output(i32)
   
   declare void @outputFloat(float)
   
   declare void @neg_idx_except()
   
   define i32 @main() {
   label_entry:
     %op0 = alloca i32
     store i32 0, i32* %op0
     %op1 = add i32 1, 2
     store i32 %op1, i32* %op0
     %op2 = load i32, i32* %op0
     %op3 = mul i32 %op2, 4
     store i32 %op3, i32* %op0
     ret i32 0
   }
   ```

   解释一下，就是变成了反复对新变量赋值再赋给原变量的形式，也就是如下代码：

   ```c++
   int a0;
   a0 = 0;
   a1 = 1 + 2;
   a0 = a1;
   a2 = a0;
   a3 = a2 * 4;
   a0 = a3;
   ```

   

4. **对下面给出的**`cminus`**程序，使用lab3的功能，分别关闭/开启**`Mem2Reg`**生成LLVM IR。对比生成的两段LLVM IR，开启**`Mem2Reg`**后，每条**`load`**,** `store`**指令发生了变化吗？变化或者没变化的原因是什么？请分类解释。**

   `cminus`代码：

   ```c++
   int globVar;
   int func(int x){
       if(x > 0){
           x = 0;
       }
       return x;
   }
   int main(void){
       int arr[10];
       int b;
       globVar = 1;
       arr[5] = 999;
       b = 2333;
       func(b);
       func(globVar);
       return 0;
   }
   ```

   - 关于变化

   子函数开始处对参数的存储和再读取被删去，而是直接使用函数传入的参数寄存器:

   ```c++
   // %op1 = alloca i32
   // store i32 %arg0, i32* %op1
   // %op2 = load i32, i32* %op1
   %op3 = icmp sgt i32 %arg0, 0// %op3 = icmp sgt i32 %op2, 0
   ```

   在需要从分支语句中调用正确的值时，使用 `phi`语句来选择:

   ```c++
   label6:                                               
   // store i32 0, i32* %op1
      br label %label7
   label7:  
   // %op8 = load i32, i32* %op1
      %op9 = phi i32 [ %arg0, %label_entry ], [ 0, %label6 ]
   ```

   如果对某变量的使用只有一个定值到达，且该定值把一个常量赋给其，则可以用该常量替换它，并且不分配寄存器给它：

   ```c++
   // store i32 2333, i32* %op1
   // %op6 = load i32, i32* %op1
   // %op7 = call i32 @func(i32 %op6)
      %op7 = call i32 @func(i32 2333)
   ```

   - 关于没变化

   由于SSA本身的限制，对于gep，即针对数组或结构体的操作不做处理，如：

   `%op5 = getelementptr [10 x i32], [10 x i32]* %op0, i32 0, i32 5`

   而全局变量由于在每一个块中都需要有不同的实例，也不需要处理，如：

   ```c++
   @globVar = global i32 zeroinitializer
   %op8 = load i32, i32* @globVar
   ```

   

5. **指出放置phi节点的代码，并解释是如何使用支配树的信息的。需要给出代码中的成员变量或成员函数名称。**

   ```c++
   std::map<std::pair<BasicBlock *, Value *>, bool> bb_has_var_phi; // bb has phi for var
   for (auto var : global_live_var_name)
   {
       std::vector<BasicBlock *> work_list;
       work_list.assign(live_var_2blocks[var].begin(), live_var_2blocks[var].end());
       for (int i = 0; i < work_list.size(); i++)
       {
           auto bb = work_list[i];
           for (auto bb_dominance_frontier_bb : dominators_->get_dominance_frontier(bb))
           {
               if (bb_has_var_phi.find({bb_dominance_frontier_bb, var}) == bb_has_var_phi.end())
               {
                   // generate phi for bb_dominance_frontier_bb & add bb_dominance_frontier_bb to work list
                   auto phi = PhiInst::create_phi(var->get_type()->get_pointer_element_type(), bb_dominance_frontier_bb);
                   phi->set_lval(var);
                   bb_dominance_frontier_bb->add_instr_begin(phi);
                   work_list.push_back(bb_dominance_frontier_bb);
                   bb_has_var_phi[{bb_dominance_frontier_bb, var}] = true;
               }
           }
       }
   }
   ```

   * Step1. 遍历每个活跃全局变量 `var	`
   * Step2. 遍历`var`所出现的所有BasicBlock集合 `live_var_2blocks[var]`，每次开始迭代时将`work_list`重置为该集合
   * Step3. 通过支配树遍历`work_list` 中bb的支配边界结点集合`dominators_->get_dominance_frontier(bb)` 中的每个结点bb `bb_dominance_frontier_bb`
   * Step4. 如果当前支配边界结点bb中不存在当前`var`的`phi`函数，则生成`bb_dominance_frontier_bb` 的`phi`函数，并将`bb_dominance_frontier_bb` 加入到`work_list`
   * `work_list` 的更新实际上相当于沿着前驱结点的直接支配结点向上遍历，直至到达`var`的直接支配结点
   
   

### 代码阅读总结

**LoopSearch **

循环查找是属于一种分析类Pass，该类Pass是用来为之后的优化Pass获取`CFG`的必要信息，而不会对`CFG`本身造成改变；

循环查找的主要目的就是为了找到程序当中的所有循环，以及这些循环的入口块（即循环的条件块）等信息。该分析Pass也是后续的循环不变式外提优化的基础。

简要流程：

（1）利用 `Tarjan` 算法获得有向图中的强连通分量。

（2）根据cminusf 语法的 IR 生成规则，每个循环都只有一个入口，这个入口就是包含进入循环的判断语句的基本块。

（3）维护一个数组 `reserved` 来保存当前程序中已经找到 base 的循环的循环入口块，在`find_loop_base`函数中，可以便于寻找嵌套循环的入口。

（4）所有的循环都是从外层循环开始向内遍历寻找循环入口块。

**Mem2Reg **

LLVM IR在clang的codegen后并不是strict-SSA结构，这时局部变量表现为alloca指令，同时对局部变量通过load和store进行读写操作，这会导致局部变量可能会存在多个store指令，而SSA要求每个变量只能有一个。

这时LLVM会通过标准的SSA构造算法来将原始IR转换成minimal-SSA（最小静态单赋值形式）并最终转换成prune-SSA（剪枝静态单赋值形式），这都在mem2reg的pass中实现。

简要流程：

（1）插入块所需要的phi函数。利用CFG生成支配树，找到全局名字集合并重写代码。

（2）重命名。重命名实际上是一个删除多余的load和store指令的过程，由于SSA本身的限制，对于gep，即针对数组或结构体的操作不做处理；而全局变量由于在每一个块中都需要有不同的实例，也不需要处理。

通过对活跃变量文档、静态单赋值格式构造文档的阅读和对Loop_Search、Mem2Reg、Dominators源码的阅读和分析，我们对LLVM IR依据SSA的标准进行优化的原理与流程有了初步的认识，为后续的实验做好铺垫与准备。



### 实验反馈 （可选 不会评分）

静态单赋值格式构造的文档阅读起来比较困难，或许找一些容易理解的文档

代码可以适当增加一些注释



### 组间交流 （可选）

无
