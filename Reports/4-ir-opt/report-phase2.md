# Lab4 实验报告

罗正 PB1903\*\*\*\*     李平治 PB1907\*\*\*\*

## 实验要求

实现基本优化 Pass

- 常量传播和死代码删除

如果一个变量的值可以在编译优化阶段直接计算出，那么就直接将该变量替换为常量（即计算出的结果值）。

a. 只需要考虑过程内的常量传播，可以不用考虑数组，**全局变量只需要考虑块内的常量传播**，同时删除无用的赋值和计算语句。

b. 整形浮点型都需要考虑

c. 对于`a=1/0`的情形，可以不考虑，即可以做处理也可以不处理。

- 循环不变式外提

要能够实现将与循环无关的表达式提取到循环的外面，无需考虑数组与全局变量。

- 活跃变量分析

能够实现分析bb块的入口和出口的活跃变量，参考资料见附件(紫书9.2.4节)。

材料中没有phi节点的设计，因此数据流方程：$OUT[B] =\cup_{s是B的后继}IN[S]$​ 的定义蕴含着S入口处活跃的变量在它所有前驱的出口处都是活跃的，由于`phi`指令的特殊性，例如`%0 = phi [%op1, %bb1], [%op2, %bb2]`如果使用如上数据流方程，则默认此`phi`指令同时产生了`op1`与`op2`的活跃性，而事实上只有控制流从`%bb1`传过来才有`%op1`的活跃性，从`%bb2`传过来才有`%op2`的活跃性。因此对此数据流方程需要做一些修改：

$`OUT[B] =\cup_{s是B的后继}IN[S]\cup_{s是B的后继} phi\_uses[S,B]`$。  

其中`IN[S]`是S中剔除`phi`指令后分析出的入口变量结果。`phi_uses[S,B]`表示S中的`phi`指令参数中`label`为B的对应变量。

## 实验难点

* 常量传播
  * 在常量传播后，无用跳转的删除
  * phi指令的处理
* 循环不变式外提
  * 找出循环不变式
  * 外提的处理

+ 活跃变量
  + phi指令的特殊处理：修改数据流方程
  + 伪代码的算法实现

## 实验设计

* 常量传播
  实现思路及相应代码：

  1. 遍历每个函数块

  2. 在每个函数块中，通过一个栈数据结构，从函数入口开始进行对 `BasicBlock` 的深度优先遍历

  3. 遍历每个 `BasicBlcok` 中的所有指令 `instr`，并根据`instr` 类型进行处理:

     1. `phi` 指令: 

        若当前指令来自的bb已被删除，则删去该指令

        ```c++
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
        ```

        

     2. 二元运算(四则运算)指令:

        若两个右操作数均为常量，则可进行常量替换和指令删除

        ```c++
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
        ```

        

     3. `zext` 字拓展指令:

        若操作数为常量，则进行常量替换和指令删除

        ```c++
        if (cast_constantint(instr->get_operand(0)) != nullptr)
                                {
                                    auto bool_val =
                                        cast_constantint(instr->get_operand(0))->get_value();
                                    auto const_val = ConstantInt::get(static_cast<int>(bool_val), m_);
                                    instr->replace_all_use_with(const_val);
                                    to_delete_instrs.insert(instr);
                                }
        ```

        

     4. `fp2si` 或 `si2fp` 类型转换指令:

        若操作数为常量，则进行常量替换和指令删除

        ```c++
        // Take `fp2si` as an example here.
        if (cast_constantfp(instr->get_operand(0)) != nullptr)
                                {
                                    auto int_val = static_cast<int>(
                                        cast_constantfp(instr->get_operand(0))->get_value());
                                    auto const_val = ConstantInt::get(int_val, m_);
                                    instr->replace_all_use_with(const_val);
                                    to_delete_instrs.insert(instr);
                                }
        ```

        

     5. `store` 和 `load` 指令:

        定义一个字典，来存储`store`指令所构造的指针到对其 `load` 构成的栈。当遍历到 `load` 指令时，若字典中存在对应指针的值，直接将 `load` 指令替换到该值即可；否则其将入栈，以便下次判断是否需要 `load`

        ```c++
        std::unordered_map<Value *, std::vector<Value *>> pointer_const;
        ```

        

        ```c++
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
        ```

        

     6. `cmp` 或 `fcmp` 比较指令:

        若两个右操作数均为常量，则可进行常量替换和指令删除

        ```c++
        // Take `cmp` as an example here
        if (cast_constantint(lhs) != nullptr &&
                                    cast_constantint(rhs) != nullptr)
                                {
                                    auto cmp_op = dynamic_cast<CmpInst *>(instr)->get_cmp_op();
                                    auto cmp_val = CF.compute_comp(cmp_op, cast_constantint(lhs),
                                                                   cast_constantint(rhs));
                                    instr->replace_all_use_with(cmp_val);
                                    to_delete_instrs.insert(instr);
                                }
        ```

        

     7. `br` 跳转指令:

        对条件判断是否是常量，若是则将语句替换为相应的直接跳转

        ```c++
        // Take 'int operator' and 'always true' as an example here
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
        /* ... ... */
        /* ... ... */
        ```

        


  优化前后的IR对比（举一个例子）并辅以简单说明：

  * CMinusf code:

    ```assembly
    int opa;
    int opb;
    int opc;
    int opd;
    
    int max(void)
    {
        opa = 0*1*2*3*4*5*6*7;
        opb = 1*2*3*4*5*6*7*8;
        opc = 2*3*4*5*6*7*8*9;
        opd = 3*4*5*6*7*8*9*10;
    
        if( opa < opb){
            if( opb < opc){
                if( opc < opd){
                    return opd;
                }
            }
        }
        return 0;
    }
    
    void main(void){
        int i;
        
        i = 0;
        
        while(i<200000000)
        {
            max();
            i=i+1;
        }
    
        output(opa);
        output(opb);
        output(opc);
        output(opd);
        
        return ;
    }
    ```


  * Before optimization:

    ```assembly
    ; ModuleID = 'cminus'
    source_filename = "testcase-3.cminus"
    
    @opa = global i32 zeroinitializer
    @opb = global i32 zeroinitializer
    @opc = global i32 zeroinitializer
    @opd = global i32 zeroinitializer
    declare i32 @input()
    
    declare void @output(i32)
    
    declare void @outputFloat(float)
    
    declare void @neg_idx_except()
    
    define i32 @max() {
    label_entry:
      %op0 = mul i32 0, 1
      %op1 = mul i32 %op0, 2
      %op2 = mul i32 %op1, 3
      %op3 = mul i32 %op2, 4
      %op4 = mul i32 %op3, 5
      %op5 = mul i32 %op4, 6
      %op6 = mul i32 %op5, 7
      store i32 %op6, i32* @opa
      %op7 = mul i32 1, 2
      %op8 = mul i32 %op7, 3
      %op9 = mul i32 %op8, 4
      %op10 = mul i32 %op9, 5
      %op11 = mul i32 %op10, 6
      %op12 = mul i32 %op11, 7
      %op13 = mul i32 %op12, 8
      store i32 %op13, i32* @opb
      %op14 = mul i32 2, 3
      %op15 = mul i32 %op14, 4
      %op16 = mul i32 %op15, 5
      %op17 = mul i32 %op16, 6
      %op18 = mul i32 %op17, 7
      %op19 = mul i32 %op18, 8
      %op20 = mul i32 %op19, 9
      store i32 %op20, i32* @opc
      %op21 = mul i32 3, 4
      %op22 = mul i32 %op21, 5
      %op23 = mul i32 %op22, 6
      %op24 = mul i32 %op23, 7
      %op25 = mul i32 %op24, 8
      %op26 = mul i32 %op25, 9
      %op27 = mul i32 %op26, 10
      store i32 %op27, i32* @opd
      %op28 = load i32, i32* @opa
      %op29 = load i32, i32* @opb
      %op30 = icmp slt i32 %op28, %op29
      %op31 = zext i1 %op30 to i32
      %op32 = icmp ne i32 %op31, 0
      br i1 %op32, label %label33, label %label39
    label33:                                                ; preds = %label_entry
      %op34 = load i32, i32* @opb
      %op35 = load i32, i32* @opc
      %op36 = icmp slt i32 %op34, %op35
      %op37 = zext i1 %op36 to i32
      %op38 = icmp ne i32 %op37, 0
      br i1 %op38, label %label40, label %label46
    label39:                                                ; preds = %label_entry, %label46
      ret i32 0
    label40:                                                ; preds = %label33
      %op41 = load i32, i32* @opc
      %op42 = load i32, i32* @opd
      %op43 = icmp slt i32 %op41, %op42
      %op44 = zext i1 %op43 to i32
      %op45 = icmp ne i32 %op44, 0
      br i1 %op45, label %label47, label %label49
    label46:                                                ; preds = %label33, %label49
      br label %label39
    label47:                                                ; preds = %label40
      %op48 = load i32, i32* @opd
      ret i32 %op48
    label49:                                                ; preds = %label40
      br label %label46
    }
    define void @main() {
    label_entry:
      br label %label1
    label1:                                                ; preds = %label_entry, %label6
      %op15 = phi i32 [ 0, %label_entry ], [ %op9, %label6 ]
      %op3 = icmp slt i32 %op15, 200000000
      %op4 = zext i1 %op3 to i32
      %op5 = icmp ne i32 %op4, 0
      br i1 %op5, label %label6, label %label10
    label6:                                                ; preds = %label1
      %op7 = call i32 @max()
      %op9 = add i32 %op15, 1
      br label %label1
    label10:                                                ; preds = %label1
      %op11 = load i32, i32* @opa
      call void @output(i32 %op11)
      %op12 = load i32, i32* @opb
      call void @output(i32 %op12)
      %op13 = load i32, i32* @opc
      call void @output(i32 %op13)
      %op14 = load i32, i32* @opd
      call void @output(i32 %op14)
      ret void
    }
    ```

    

  * After optimization:

    ```assembly
    @opa = global i32 zeroinitializer
    @opb = global i32 zeroinitializer
    @opc = global i32 zeroinitializer
    @opd = global i32 zeroinitializer
    declare i32 @input()
    
    declare void @output(i32)
    
    declare void @outputFloat(float)
    
    declare void @neg_idx_except()
    
    define i32 @max() {
    label_entry:
      store i32 0, i32* @opa
      store i32 40320, i32* @opb	
      store i32 362880, i32* @opc
      store i32 1814400, i32* @opd
      # 以上的常量计算与赋值过程被编译优化中计算出来并赋给寄存器
      # 常量比较与跳转指令在优化中被删除
      br label %label33
    label33:                                                ; preds = %label_entry%label_entry
      %op34 = load i32, i32* @opb
      %op35 = load i32, i32* @opc
      %op36 = icmp slt i32 %op34, %op35
      %op37 = zext i1 %op36 to i32
      %op38 = icmp ne i32 %op37, 0
      br i1 %op38, label %label40, label %label46
    label39:                                                ; preds = %label_entry, %label46
      ret i32 0
    label40:                                                ; preds = %label33
      %op41 = load i32, i32* @opc
      %op42 = load i32, i32* @opd
      %op43 = icmp slt i32 %op41, %op42
      %op44 = zext i1 %op43 to i32
      %op45 = icmp ne i32 %op44, 0
      br i1 %op45, label %label47, label %label49
    label46:                                                ; preds = %label33, %label49
      br label %label39
    label47:                                                ; preds = %label40
      %op48 = load i32, i32* @opd
      ret i32 %op48
    label49:                                                ; preds = %label40
      br label %label46
    }
    define void @main() {
    label_entry:
      br label %label1
    label1:                                                ; preds = %label_entry, %label6
      %op15 = phi i32 [ 0, %label_entry ], [ %op9, %label6 ]
      %op3 = icmp slt i32 %op15, 200000000
      %op4 = zext i1 %op3 to i32
      %op5 = icmp ne i32 %op4, 0
      br i1 %op5, label %label6, label %label10
    label6:                                                ; preds = %label1
      %op7 = call i32 @max()
      %op9 = add i32 %op15, 1
      br label %label1
    label10:                                                ; preds = %label1
      %op11 = load i32, i32* @opa
      call void @output(i32 %op11)
      %op12 = load i32, i32* @opb
      call void @output(i32 %op12)
      %op13 = load i32, i32* @opc
      call void @output(i32 %op13)
      %op14 = load i32, i32* @opd
      call void @output(i32 %op14)
      ret void
    }
    ```

    



* 循环不变式外提
  实现思路：

  1. 通过LoopSearch提供的循环搜索Pass获取到的所有循环。
  2. 对每个循环体内所有指令执行遍历，存储一个循环体内所有指令的左值构成集合。
  3. 再次遍历，跳过br、call、load、store、phi等可能会导致外部环境变化的代码，这些指令不外提。剩余指令中若右值全部不在2中存储的集合中，则说明该指令是循环不变的，于是将该指令外提到循环入口处。
  4. 若2和3中出现了指令外提，则转2，因为外提的不变式可能导致新的不变式产生；否则进入下一个循环。

  相应代码：

  ```c++
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
  ```

  优化前后的IR对比（举一个例子）并辅以简单说明：

  这里以testcase-1为例

  测试文件

  ```c++
  void main(void){
      int i;
      int j;
      int ret;
  
      i = 1;
      
      while(i<10000)
      {
          j = 0;
          while(j<10000)
          {
              ret = (i*i*i*i*i*i*i*i*i*i)/i/i/i/i/i/i/i/i/i/i;
              j=j+1;
          }
          i=i+1;
      }
  	output(ret);
      return ;
  }
  ```

  优化前

  ```assembly
  declare i32 @input()
  
  declare void @output(i32)
  
  declare void @outputFloat(float)
  
  declare void @neg_idx_except()
  
  define void @main() {
  label_entry:
    %op0 = alloca i32
    %op1 = alloca i32
    %op2 = alloca i32
    store i32 1, i32* %op0
    br label %label3
  label3:                                                ; preds = %label_entry, %label58
    %op4 = load i32, i32* %op0
    %op5 = icmp slt i32 %op4, 10000
    %op6 = zext i1 %op5 to i32
    %op7 = icmp ne i32 %op6, 0
    br i1 %op7, label %label8, label %label9
  label8:                                                ; preds = %label3
    store i32 0, i32* %op1
    br label %label11
  label9:                                                ; preds = %label3
    %op10 = load i32, i32* %op2
    call void @output(i32 %op10)
    ret void
  label11:                                                ; preds = %label8, %label16
    %op12 = load i32, i32* %op1
    %op13 = icmp slt i32 %op12, 10000
    %op14 = zext i1 %op13 to i32
    %op15 = icmp ne i32 %op14, 0
    br i1 %op15, label %label16, label %label58
  label16:                                                ; preds = %label11
    %op17 = load i32, i32* %op0
    %op18 = load i32, i32* %op0
    %op19 = mul i32 %op17, %op18
    %op20 = load i32, i32* %op0
    %op21 = mul i32 %op19, %op20
    %op22 = load i32, i32* %op0
    %op23 = mul i32 %op21, %op22
    %op24 = load i32, i32* %op0
    %op25 = mul i32 %op23, %op24
    %op26 = load i32, i32* %op0
    %op27 = mul i32 %op25, %op26
    %op28 = load i32, i32* %op0
    %op29 = mul i32 %op27, %op28
    %op30 = load i32, i32* %op0
    %op31 = mul i32 %op29, %op30
    %op32 = load i32, i32* %op0
    %op33 = mul i32 %op31, %op32
    %op34 = load i32, i32* %op0
    %op35 = mul i32 %op33, %op34
    %op36 = load i32, i32* %op0
    %op37 = sdiv i32 %op35, %op36
    %op38 = load i32, i32* %op0
    %op39 = sdiv i32 %op37, %op38
    %op40 = load i32, i32* %op0
    %op41 = sdiv i32 %op39, %op40
    %op42 = load i32, i32* %op0
    %op43 = sdiv i32 %op41, %op42
    %op44 = load i32, i32* %op0
    %op45 = sdiv i32 %op43, %op44
    %op46 = load i32, i32* %op0
    %op47 = sdiv i32 %op45, %op46
    %op48 = load i32, i32* %op0
    %op49 = sdiv i32 %op47, %op48
    %op50 = load i32, i32* %op0
    %op51 = sdiv i32 %op49, %op50
    %op52 = load i32, i32* %op0
    %op53 = sdiv i32 %op51, %op52
    %op54 = load i32, i32* %op0
    %op55 = sdiv i32 %op53, %op54
    store i32 %op55, i32* %op2
    %op56 = load i32, i32* %op1
    %op57 = add i32 %op56, 1
    store i32 %op57, i32* %op1
    br label %label11
  label58:                                                ; preds = %label11
    %op59 = load i32, i32* %op0
    %op60 = add i32 %op59, 1
    store i32 %op60, i32* %op0
    br label %label3
  }
  ```

  优化后

  ```assembly
  declare i32 @input()
  
  declare void @output(i32)
  
  declare void @outputFloat(float)
  
  declare void @neg_idx_except()
  
  define void @main() {
  label_entry:
    br label %label3
  label3:                                                ; preds = %label_entry, %label58
    %op61 = phi i32 [ %op64, %label58 ], [ undef, %label_entry ]
    %op62 = phi i32 [ 1, %label_entry ], [ %op60, %label58 ]
    %op63 = phi i32 [ %op65, %label58 ], [ undef, %label_entry ]
    %op5 = icmp slt i32 %op62, 10000
    %op6 = zext i1 %op5 to i32
    %op7 = icmp ne i32 %op6, 0
    br i1 %op7, label %label8, label %label9
  label8:                                                ; preds = %label3
    %op19 = mul i32 %op62, %op62
    %op21 = mul i32 %op19, %op62
    %op23 = mul i32 %op21, %op62
    %op25 = mul i32 %op23, %op62
    %op27 = mul i32 %op25, %op62
    %op29 = mul i32 %op27, %op62
    %op31 = mul i32 %op29, %op62
    %op33 = mul i32 %op31, %op62
    %op35 = mul i32 %op33, %op62
    %op37 = sdiv i32 %op35, %op62
    %op39 = sdiv i32 %op37, %op62
    %op41 = sdiv i32 %op39, %op62
    %op43 = sdiv i32 %op41, %op62
    %op45 = sdiv i32 %op43, %op62
    %op47 = sdiv i32 %op45, %op62
    %op49 = sdiv i32 %op47, %op62
    %op51 = sdiv i32 %op49, %op62
    %op53 = sdiv i32 %op51, %op62
    %op55 = sdiv i32 %op53, %op62
    br label %label11
  label9:                                                ; preds = %label3
    call void @output(i32 %op61)
    ret void
  label11:                                                ; preds = %label8, %label16
    %op64 = phi i32 [ %op61, %label8 ], [ %op55, %label16 ]
    %op65 = phi i32 [ 0, %label8 ], [ %op57, %label16 ]
    %op13 = icmp slt i32 %op65, 10000
    %op14 = zext i1 %op13 to i32
    %op15 = icmp ne i32 %op14, 0
    br i1 %op15, label %label16, label %label58
  label16:                                                ; preds = %label11
    %op57 = add i32 %op65, 1
    br label %label11
  label58:                                                ; preds = %label11
    %op60 = add i32 %op62, 1
    br label %label3
  }
  ```

  可以发现一长串的繁重计算已经被提到外部了。

  

* 活跃变量分析
  实现思路：

  1. 遍历流图，分析基本块中每个语句。
  2. 计算每个基本块中的use和def集合，取操作数分析右值，若不在def中，加入use；再分析左值，若不在use中，加入def。计算phiuse和phiout（即phi_uses[S, B]的分开表达）集合。注意常数不属于活跃变量。注意不同指令类型的处理。
  3. 逆序迭代求各块的out/in，将书中的伪代码实现出来。注意phi语句带来的变化，涉及集合运算需要留意。
  4. 若2和3中出现了指令外提，则转2，因为外提的不变式可能导致新的不变式产生；否则进入下一个循环。

  相应的代码：

  不同指令的实现有所区别，如ret和br指令仅有use集合，alloca仅有def集合，binary等是典型的两操作数处理，load和zext是一操作数......而phi指令处理则最为关键，需要额外记录phiuse和phiout集合，且需要注意成对出现的数据。

  ```c++
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
  ```

  后续的实现相对不算复杂，在迭代中对live_in和live_out分开处理，注意集合运算即可，难点还是在于集合的获取上。

  ```c++
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
  ```

  

### 实验总结

通过对编译优化的实际代码编写，更加深刻理解了常量传播、活跃变量与循环不变式的具体处理方法与相关细节；

对c++、stl工具与常见数据结构以及logging等debug工具的使用更加得心应手；

通过掌握编译优化知识，进一步加深了对编译型语言运行过程的理解；同时也使得以后在写解释型语言时，自然联想到相关结构并进行手动优化(doge)。

### 实验反馈 （可选 不会评分）

总体体验比lab3稍好一些

### 组间交流 （可选）

无
