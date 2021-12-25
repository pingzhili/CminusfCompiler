# Lab3 实验报告

罗正 PB1903\*\*\*\*     李平治 PB1907\*\*\*\*

## 实验难点

1、在代码的方面，选取几个关键的函数说明难点：

#### VarDeclaration

var-declaration -> type-specifier ID; | type-specifier ID [NUM]

- 需要区分变量是在全局还是局部；
- 如果声明的是数组，要检查数组下标值是否大于零；
- 根据语义要求，全局变量需要初始化为 0。

#### FunDeclaration

fun-declaration -> type-specifier ID (prams) compound-stmt

- 维护函数的参数及其返回值类型；
- 维护函数输入的参数值和函数体内部语句；
- 下一个`ASTParam`用来分析函数的参数类型；
- 如果参数是数组，则为数组引用传递（指针），此处不需要考虑数组下标是否合法。
- 若未给出 `return` ，则需由编译器补充

#### Selection statement

selection-stmt -> if ( expression ) statement | if ( expression ) statement else statement

- 向下遍历if的expression条件，对于expression不同类型，最终转化为int32或flaot；
- 检查selection-stmt展开之后的类型，是否有else statement，分支创建上有所区别；
- 值得注意的是，在每个statement内部，需要考虑是否在statement内return。
- 需要考虑导致 `label` 为空的情形，并进行处理

#### Return statement

var-declaration -> type-specifier ID; | type-specifier ID [NUM]

- 根据expression类型，如果是指针，需要把指针load为对应的值；
- 若return void，则直接创建空返回；
- 需要用`builder->get_insert_block()->get_parent()->get_return_type()`来检测函数返回值类型与我们实际要返回的类型是否相同，若不同，则进行强制类型转换。

#### Var

var -> ID | ID [ expression ]

- 同样是根据是否是数组进行讨论，对于expression，需要转换为int32类型；
- 注意数组下标非负的限制，需要创建一个判断分支去判断是否大于0;
- 最后将任意id转化为int\*,float\*类型，然后使用create_gep取值并返回；
- 如果数组下标小于0，则调用neg_idx_except报错。

#### AssignExpression

expression -> var = expression | simple-expression

- var可以是一个整型变量、浮点变量，或者一个取了下标的数组变量。

- 获取的expression返回值有不同类型，需要讨论；

- 在expression和var的类型不同时，需要以var为准，对expression做强制类型转换。

#### SimpleExpression

simple-expression -> additive-expression relop additive-expression | additive-expression​

- 注意浮点值和整型值一起运算时，整型值需要转成浮点类型，运算结果也是浮点类型。

#### ASTAdditiveExpression

additive_expression -> additive_expression addop term | term

- 如果是term，那么直接调用它的accept函数；

- 如果是`additive_expression addop term`，那么要对加减法符号两端对操作数进行类型转换，根据操作数类型调用相应的加减法函数。

#### ASTCall

- 处理函数名，需要进入对应作用域，并取出函数声明时的形参列表；

- 处理传入参数，需取值存入vector中，并与声明时的参数列表比较。
- 注意函数调用的参数可以为空，并注意参数个数类型不一致时的强制类型转换。



2、在DEBUG方面：

- DEBUG较为困难。首先需要学会使用LOG，在编写代码的过程中就需要加入大量的LOG语句，测试时根据LOG报错ERROR等查看出错位置，插入LOG的位置也有不少讲究；
- 有时LOG也不能解决问题，需要单个测试得到.ll文件查看代码，推测出错原因；
- 试图扩展`eval.py`的功能进行一些输出操作等但好像没用，可能是对py不了解的原因，在后面的实验反馈也写到了这一点。



3、在协作方面：

- 分工很难明确，或者说比较难进行分工，有较多内容需要讨论；
- 组员课程安排不同，导致异步编写实验；
- 缺少协作经验，有1+1<2的感觉。



## 实验设计

（请写明为了顺利完成本次实验，加入了哪些亮点设计，并对这些设计进行解释。）

1. 如何设计全局变量

   ```cpp
   Value *public_val = nullptr;
   /*用于传递继承属性的通用全局变量，默认使用它*/
   
   Argument *arg = nullptr;
   /*用于保存参数信息，同样是传递继承属性，仅在访问ASTParam时使用*/
   /*在访问Param前对其进行赋值*/
   
   Function *cur_fun = nullptr;
   /*用于保存当前所在函数信息，在补充return时用于获取函数返回值类型*/
   /*每次函数开始时将它赋值为当前函数*/
   
   bool isReturned = false;
   /*用于判断当前函数是否已return，若已return则后续代码无需生成*/
   /*1.当函数开始时，重置其为false*/
   /*2.当产生return时，将其置为true*/
   /*3.若其仅为if-else中某一分支置为true，则在if-else结束之后重新置为false*/
   
   bool require_lvalue = false;
   /*用于判断是否需要表达式左边变量值，在访问ASTVar时使用*/
   /*1.在赋值语句中，对所赋值变量访问前将它赋值为true*/
   /*2.在访问Var结点后，将其重新赋值为false*/
   ```

2. 遇到的难点以及解决方案

   * 除去在实验难点中提到的，此处值得说明的是在最后DEBUG时，complex有两个testcase无法通过，发现在 if-else 和 while 语句相关部分，可能会产生IR空标签。为此我们在 `ASTFunction` 和 `ASTSelection` 部分对当前 BasicBlock 进行判断，若 `get_terminator()` 为 `nullptr`，即确实为空标签，则移除该标签。

3. 如何降低生成 IR 中的冗余

   * 当 if-else 语句无论如何都发生 return 时，则其后的语句不再可能运行，因此在这种情形下可以不生成对应中间代码。具体来说，通过两次获取 `isReturned` 的值，来判断是否属于这种情形。注意，这种情况下可能会产生空标签，处理思路见上。

   * 当某一函数在非 if-else 中发生了一次 `return`，则后续代码均为无效代码，此时可直接不生成对应中间代码。我们同样是通过 `isReturned` 变量来判断是否属于该情形。

     

## 实验总结

我们在写 C 程序的时候不关心自己写的 C 语句是如何执行的，曾经学了一点编程的皮毛，以为计算机是很简单的东西，实际上只不过是借着高级程序语言的便利性所以自以为是罢了。学习编译原理并做了几次实验之后，终于对高级程序语言代码的翻译有所了解。

尤其是这一次的实验，几乎从头开始完成如此大量的代码，是所有课程中的第一次。实验过程比较痛苦，但也是很好的锻炼，最终比较深入地了解了中间代码的生成过程。

早期计算机程序语言发展较为缓慢，程序员编写计算机程序时必须十分了解计算机的底层指令代码通过将这些微程序指令组合排列从而完成一个特定功能的程序，这对程序员的要求非常高。人们一直在研究如何高效地开发计算机程序，使编程的门槛降低。实验的过程仿佛就在见证计算机编程领域的发展，计算机行业不断地在让程序员工作更加便利、让自身的发展之路更加便利、让人们的生活更加便利。

现代的编译器将我们的代码翻译成机器语言的同时，也在不断精简、优化，提高机器执行效率。以前没有感受，只有当自己亲自去实现的时候，哪怕只是cminus的小小一部分，都感觉十分复杂。这次的实验让我们体会到了计算机领域的博大精深，李诚老师说的不错，这样有成就感的一件事，可以写入自己的履历中。



## 实验反馈 （可选 不会评分）

感觉难度比较大，懵的时间很久；

遇上连续的期中考试，实际可用于完成实验的时间还是比较紧张；

eval.py或许可以增加一些可扩展性？



## 组间交流 （可选）

询问了debug相关和可能产生错误的原因等，未与其它小组交流代码实现的相关内容。