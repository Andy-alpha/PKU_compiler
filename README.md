# 编译原理课程实践报告：糟糕的编译器

信息科学技术学院 2100012933 袁云鹏

声明：本实践的代码文件组织架构和一些函数的写法参考了GitHub上的开源实现[StupidSysY2RV](https://github.com/CaptainHarryChen/StupidSysY2RV/) ，并经过了Github Copilot的帮助，例如AST文件的布局和sysy.y等。不过转换成Koopa IR的功能函数、和转换成RISCV部分都经过了我的重写（其中，对于一些惯用操作，比如，load/store指令，可以多次复用，以减轻重复工作量。当然，这样可就不好改成SSA了）。

同时，感谢[实践文档](https://pku-minic.github.io/online-doc/#/)的详尽。


## 一、编译器概述

### 1.1 基本功能

本编译器基本具备如下功能：
1. 可以将SysY（C语言的子集）编译为Koopa IR。
2. 可以将Koopa IR编译为RISCV代码。
3. 具有性能测试选项和能力。

### 1.2 主要特点

本编译器的主要特点是函数复用程度高、模块化管理强、易于维护。

函数复用程度高是因为无论在循环中还是生成AST的过程，都有大量相同的赋值语句。

模块化管理是因为本编译器把SysY->RISCV, 拆成了SysY->koopa, koopa->RISCV，理论上把koopa IR换成别的中间表示也说得通。

易于维护是因为本编译器使用面向对象方法，一个测试点往往只需要修改少量对应模块就可以通过。

## 二、编译器设计

### 2.1 主要模块组成

编译器由三个主要模块组成：

1. `sysy.l/sysy.y`部分负责词法/语法分析
2. `AST`、`symbol_tab.hpp`和`utils`（工具函数）部分负责解析语法树，对于指令、表达式和数组重点分别操作，并管理正确的符号表/作用域，形成koopa IR的内存形式。
3. `block`和`loop_maintainer`部分负责大型基本块以及循环的维护。

```
src/
|-main.cpp
|-symbol_tab.hpp
|-sysy.l
|-sysy.y
|-AST/
	|--AST.hpp
	|--AST.cpp
	|--base_AST.hpp
	|--code_AST.hpp
	|--exp_AST.hpp
	|--array_AST.hpp
|-utils/
	|--block.hpp
	|--loop_maintainer.hpp
	|--koopa_util.hpp
	|--koopa_util.cpp
	|--riscv_util.hpp
	|--riscv_util.cpp
```

### 2.2 主要数据结构

本编译器最核心的数据结构是BaseAST作为所有语法分析树的基类。静态的全局变量`symbol_tab`、`block`和`loop_maintainer`跨文件地共享符号表、当前的块信息。同时设计了虚函数用来给不同的衍生类作具体实现。

```c++
class BaseAST {
public:
    virtual ~BaseAST() = default;
    // 输出koopa对象，并在全局环境添加各种信息
    virtual void *build_koopa_values() const；
    // 用于表达式AST求值
    virtual int CalcValue()；
    // 返回该AST的左值（用于变量）
    virtual void *koopa_leftvalue() const；
    // 我自己加入调试用的运算符重载
    friend std::ostream& operator<< (std::ostream& os, const BaseAST& ast);
    static SymbolTab sym_tab;
    static Block big_block;
    static LoopMaintainer loop_maintainer;
protected:
    virtual void Dump(std::ostream& os) const {
        os << "This is Dump() of BaseAST";
    }
};
```

如果将一个SysY程序视作一棵树，那么一个`class CompUnit`的实例就是这棵树的根，根据这一情况设计了数据结构`CompUnit`，它继承自`BaseAST`

```C++
// CompUnit 是 BaseAST
class CompUnitAST : public BaseAST {
    // 把一些启动时就需要的库函数的符号等加入其中，包括@getint, @getch, @getarray, @putch, @putarray, @starttime, @stoptime
    void add_lib_funcs(std::vector<const void*> &funcs) const;
public:
    // 用智能指针管理对象。至于Dump()，之后再考虑添加。
    std::vector<std::unique_ptr<BaseAST>> func_list;
    InstSet value_list;

    CompUnitAST(std::vector<BaseAST*> &_func_list, InstSet &_value_list);
    // 定义自己的生成koopa raw program的函数：
    // 创建新的作用域
    // 从(InstSet)类型的value_list中取出pair对，判断类型是不是合法的。
    // 对于常量可以直接计算值，否则直接调用对应派生类的函数。
    koopa_raw_program_t to_koopa_raw_program() const;
```

在`if...else...`语句方面，由于涉及到二义性问题（`if (a) if (b) x; else y;`），所以本编译器设计

```yacc
IfExp : IF '(' Exp ')';

Stmt : IfExp Stmt ELSE Stmt | IfExp Stmt | … 
```

通过把`%token IF`和最近的表达式`%token Exp`合成一个整体， 防止了else空悬问题。

除此之外，为了代码编写的便利性，在编译器的前端和后端部分都各有一个`Push_back()`的函数。设计它的目的是能够在全局添加指令，并且有着森严的层级。

### 2.3 主要设计考虑及算法选择

#### 2.3.1 符号表的设计考虑

符号表类中定义`New_Env()`和`Delete_Env()`函数，用vector实现了栈式结构模拟符号表的嵌套。其中vector的每个元素都是一个map，每个map建立koopa到整形地址的映射。同时，map方便进行查询功能：

- **插入符号定义:** 向符号表中添加一个常量符号, 同时记录这个符号的常量值, 也就是一个 32 位整数.
- **确认符号定义是否存在:** 给定一个符号, 查询符号表中是否存在这个符号的定义.
- **查询符号定义:** 给定一个符号表中已经存在的符号, 返回这个符号对应的常量值.

实现上，`New_Env()`和`Delete_Env()`函数分别就是压栈和弹栈，push_back()就是嵌套深一层，pop_back()就是少嵌套一层，元素个数即代表层数，这样顺便完成了界定作用域的工作。查询的时候，则是由深至浅倒过来查询。

#### 2.3.2 寄存器分配策略

简单地全放在栈上。运算中途会用到t0,t1,t2寄存器（主要是load/store时），但是之后数据不保存。对于传入函数参数a0-a7，则还是按顺序存在a0-a7这8个寄存器里。但是多的依然放栈上。
在转换成RISCV时，会计算所有语句以及变量等占用的空间大小，并对齐16的整数倍。然后在栈上分配对应的空间。

#### 2.3.3 采用的优化策略
没有。

## 三、编译器实现

### 3.1 各阶段编码细节

#### Lv0

由于我是在M1芯片上运行的macOS系统，而实验镜像是基于x86/amd64架构的，所以我查找到有人用相同的环境从源码构建了可用的arm64镜像（hln7897/compiler-dev:M1-Silicon），所以我的路径就不是`maxxing:compiler-dev`了。但其余部分相似。

首先参看完整的EBNF范式，写了第一版的sysy.y

#### Lv1. main函数和Lv2. 初试目标代码生成

对于`main()`函数的实现还算简单，`main`替换为`@main`，类型暂时全部定为i32（后面在函数一节有void，需要在koopa中把fun @func : i32的冒号去掉）。

用`sysy.l`里定义的正则表达式识别单行注释并忽略之。

对于多行注释的正则表达式，经过查询，以下是可以完成的。`MultiLineComment "/*"([^*]|\*+[^*/])*\*+"/"`

#### Lv3. 表达式

不知道为什么，单个字符只能用单引号，不能用双引号，否则会导致syntax error。也许是flex或者bison的问题。为防止问题，索性把所有的"int" "const"等都换成了大写的TOKEN（INT, CONST），单引号只给小括号、中括号等使用。

#### Lv4. 常量和变量

在ExpAST中添加了CalcValue功能来求常量的值。

拜变量所赐，SymbolTab类现在支持多层了。设为static统管全局。

BaseAST添加了新的虚函数用于处理左值。不然的话仅用固定值的函数重载返回变量很麻烦。

#### Lv5. 语句块和作用域

作用域嵌套就用是Lv4的SymbolTab。

在语法分析阶段，建立了全局的语句暂留栈，用vector保存指令和相应AST的pair。CompUnit会用DFS的方式传递给AST的对应实现。

Koopa IR 函数内部不能定义相同名字的变量，我尝试加后缀"variable_x"，x是数字。这样可以区分（后来发现Koopa IR 中出现多个名称相同的符号时，它会自动给新的名称加个后缀 xx_0 、xx_1 等，所以无需考虑一个语句块内定义多个同名变量的问题）。

#### Lv6. if语句

在`if...else...`语句方面，由于涉及到二义性问题（`if (a) if (b) x; else y;`），所以本编译器设计

```ebnf
IfExp : IF '(' Exp ')';

Stmt : IfExp Stmt ELSE Stmt | IfExp Stmt | … 
```
不过这是合并法。拆分也是可以的。比如说，把Stmt拆成MatchedStmt和UnmatchedStmt，然后分别考虑。

```ebnf
Stmt  : MatchedStmt 
	    | UnmatchedStmt
MatchedStmt : If '(' Exp ')' MatchedStmt 
		  ELSE MatchedStmt
	    | other
unmatched_stmt : If '(' Exp ')' Stmt
	    | If '(' Exp ')' MatchedStmt
		  else UnmatchedStmt
```

对于短路求值，将&&，||设为跳转形式，具体而言，就是

```C++
// && pseudocode
result = 0;
if (lhs != 0)
    goto %true;
else goto %end;
%true:
	result = rhs != 0;
%end:
…
    
// || pseudocode
result = 1;
if (lhs == 0)
    goto %true;
else goto %end;
%true:
	result = rhs != 0;
%end:
…
```

#### Lv7. while语句

使用了`loop_maintainer`记录while语句的三个重要组成部分：%while_entry, %while_body, 和%end。

这样的好处是，由于其栈结构天然地可以表示嵌套层级关系，所以遇到break/continue时，只需要分别定位到本嵌套层的%end和%while_entry处，就能实现该功能。

#### Lv8. 函数和全局变量

##### 全局变量/作用域

由于有了`symbol tab`的栈式结构，局部变量会在其作用域内覆盖同名全局变量，因为vector是倒着查找的，先找到的一定是嵌套更深那层的变量。

还有另一个问题，就是忘记了未显式初始化的全局变量, 其 (元素) 值均应该被初始化为 0，后面的数组也是如此。为此，需要专门区分并处理放在全局的变量/数组。

##### 函数
`FuncFParam` 定义函数的一个形式参数。为实现方便，将其放入一个单独的作用域。

函数调用形式是 `UnaryExp ::= IDENT "(" FuncRParams ")"`, 其中的 `FuncRParams` 表示实际参数。实际参数的类型和个数必须与 `IDENT` 对应的函数定义的形参完全匹配。如果没有，则置为空。语法分析的sysy.y里定义`func_list, fparams,rparams`，用于存储函数列表、形式参数列表和实际参数列表。

对于调用函数的栈空间分配，参考实践文档：

> 统计需要为局部变量分配的栈空间 $S$, 需要为 `ra` 分配的栈空间 $R$, 以及需要为传参预留的栈空间 $A$, $R=has\_call?\;4:0$, $A=4·\max\{\max_{i=0}^{n-1}\text{len}_i-8,\;0\}$。计算三者相加的值，并且上取整为16的倍数 $S'$。
> prologue：更新sp。如果 $R$ 不为 0, 在 $\text{sp}+S'-4$​ 对应的栈内存中保存 `ra` 寄存器的值。
> epilogue: 从栈帧中恢复 `ra` 寄存器，再复原sp，最后ret。

#### Lv9. 数组

其中对于数组的处理存在不一致的行为。会在少数数组的测例上存在CLE。~~此处指11_arr_params.koopa和13_。目前还在寻找原因。~~经过排查，原来是sysy.y写得有问题，漏掉了一个分号，导致语法分析时会出错。

##### 一维数组

处理放在全局的数组。如果没有初始化，设为0。

##### 多维数组

对于多维数组对齐的问题，用函数递归解决。先从当前待处理的维度中的最后一维 (第 n 维) 开始填充数据（预处理成数组）

sub_preprocess则每次检测对齐了某维度边界位置与否。如果是的话，下一个聚合（aggregate）类型就放在下一个最大可能维度的数组的初始化列表。然后递归处理。其中对于在global范围的array，还要注意填.zero 0。

##### 数组参数

这里遇到的问题是返回变量左值时，`getelemptr`和`getptr`虽然在在汇编层面完全一致，但是前者要求指针必须是一个数组指针，并且返回的类型会解引用一级。所以对于多维数组参数，对于第一个参数特殊处理，用`getptr`先“平移”一个值，然后再依次使用`getelemptr`.


### 3.2 工具软件介绍
1. `例如flex/bison`：词法分析和语法分析。flex处理了一元、二元运算符、注释等。bison生成语法分析树。
2. `例如libkoopa`：作为一种中间表示，在内存中构建了koopa IR的AST，然后能自动生成代码。
3. `其它软件或库`：gdb/lldb调试，在docker中需要额外加参数才能启用。给某个函数打断点或者执行到出错位置，主要解决一些segmentation fault.

### 3.3 测试情况说明

1. Lv4的multi_return中，由于每个基本块末端都有一个控制流转移语句（br, jump, ret），如果有多个return，得为每个return生成不同的基本块。
2. Lv4的var_name3中，有变量也叫ret。但是多余的这些变量不在SymbolTab里，所以没有影响。
3. RISCV中，`bnez`的跳转范围只有`imm12`，而`j`型指令跳转范围就广了。所以修改为跳转到下一行的标签，再生成一个`j`命令跳转到需要的地方（-2048~+2047以外）。
4. 还有一个遇到的坑：对于int类型函数，有的时候`return 0;`要自己加上。这里采用的方式是在一个基本块结束、清除不可达代码时，顺便检测是否需要添加br、jump、ret等。

## 四、实习总结

- unique_ptr问题：一开始上手的时候不习惯`unique_ptr`，总是尝试直接用它赋值，但是它默认是不能被拷贝的，只有用`std::move()`移动右值。
- 不兼容的类型限定符问题：经常没有注意就把`const A&`赋给`A`，或者是一个const对象没有调用const函数，抑或是const函数无意中修改了类成员或者调用了非常量函数，编译报错。还好能找出并且修改。
- 类型不能显式转换问题：一开始为了图方便，经常在koopa的诸多类型中使用强制类型转换，但是由于struct之间定义不同，一般不能cast，即使少数可以，有时会出现莫名其妙的问题。这种情况只能写类型转换的功能函数，对于struct中的数据挨个赋值。
- 野指针或者空悬指针访存问题，非常之多。new的指针所指向的临时空间，我想delete掉，但常常因疏忽，会出现非法访存。所以很多本来应该delete的地方我干脆就没有管。当然这是不好的习惯。

### 4.1 收获和体会

1. 收获了一个可堪一用的编译器。
2. 深入学习了编译相关知识。
3. 加强对C++面向对象的理解。
4. 又复习了曾经用过的RISCV。

### 4.2 学习过程中的难点，以及对实习过程和内容的建议

难点：数组的实现还是比较繁琐的，很多只针对单变量的处理方式在数组面前都得特判/推倒重来。koopa的结构体过于多，每种类型都要处理。

建议：有些挺坑人的测试样例，希望能多给些提示。

### 4.3 对老师讲解内容与方式的建议

无
