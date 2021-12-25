# lab1 实验报告
PB1907\*\*\*\* 李平治

## 实验要求

* 理解`Cminus-f`词法和语法，了解`bison`和`flex`基础知识
* 根据`Cminus-f`的词法补全`src/parser/lexical_analyzer.l`文件，完成词法分析器。在`lexical_analyzer.l`文件中，只需补全模式和动作即可，能够输出识别出的`token`，`text` ,`line(刚出现的行数)`，`pos_start(该行开始位置)`，`pos_end(结束的位置,不包含)`。



## 实验难点

* 对`yytext`, `yylex()`, `yyerror()` 的理解与使用，需自行查询资料
* 词法正则表达式的正确书写
* 对`Cminus`注释的处理
* 对`node`及`pass_node()`的理解，以及`$`在正则表达式中的使用



## 实验设计

本实验为代码填空.

在`lexical_analyzer.l`中，需按照词法完成正则表达式匹配及对应的操作与返回值，并维护`lines`, `pos_start`, `pos_end`等.

在`syntax_analyzer.y`中，需按照语法构造终结符声明，并传递给`node()`.



## 实验结果验证

测试样例:

```c
int main(void) {
    /***
    ****/
    int testName;
    return 0;
}
```

词法分析结果:

```
Token	      Text	Line	Column (Start,End)
280  	       int	0	(0,3)
284  	      main	0	(4,8)
272  	         (	0	(8,9)
282  	      void	0	(9,13)
273  	         )	0	(13,14)
274  	         {	0	(15,16)
280  	       int	4	(5,8)
284  	  testName	4	(9,17)
270  	         ;	4	(17,18)
281  	    return	6	(5,11)
285  	         0	6	(12,13)
270  	         ;	6	(13,14)
275  	         }	7	(1,2)
```

语法分析结果:

```
>--+ program
|  >--+ declaration-list
|  |  >--+ declaration
|  |  |  >--+ fun-declaration
|  |  |  |  >--+ type-specifier
|  |  |  |  |  >--* int
|  |  |  |  >--* main
|  |  |  |  >--* (
|  |  |  |  >--+ params
|  |  |  |  |  >--* void
|  |  |  |  >--* )
|  |  |  |  >--+ compound-stmt
|  |  |  |  |  >--* {
|  |  |  |  |  >--+ local-declarations
|  |  |  |  |  |  >--+ local-declarations
|  |  |  |  |  |  |  >--* epsilon
|  |  |  |  |  |  >--+ var-declaration
|  |  |  |  |  |  |  >--+ type-specifier
|  |  |  |  |  |  |  |  >--* int
|  |  |  |  |  |  |  >--* testName
|  |  |  |  |  |  |  >--* ;
|  |  |  |  |  >--+ statement-list
|  |  |  |  |  |  >--+ statement-list
|  |  |  |  |  |  |  >--* epsilon
|  |  |  |  |  |  >--+ statement
|  |  |  |  |  |  |  >--+ return-stmt
|  |  |  |  |  |  |  |  >--* return
|  |  |  |  |  |  |  |  >--+ expression
|  |  |  |  |  |  |  |  |  >--+ simple-expression
|  |  |  |  |  |  |  |  |  |  >--+ additive-expression
|  |  |  |  |  |  |  |  |  |  |  >--+ term
|  |  |  |  |  |  |  |  |  |  |  |  >--+ factor
|  |  |  |  |  |  |  |  |  |  |  |  |  >--+ integer
|  |  |  |  |  |  |  |  |  |  |  |  |  |  >--* 0
|  |  |  |  |  |  |  |  >--* ;
|  |  |  |  |  >--* }

```

满足验证要求.



## 实验反馈

文档写的非常详细且友好，issue也给了很多帮助，首个实验体验极佳.

