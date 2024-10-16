%code requires {
  #include <memory>
  #include <string>
  #include "AST/AST.hpp"
}

%{

#include <iostream>
#include <memory>
#include <string>
#include <cstring>
#include <vector>
#include <map>
#include "AST/AST.hpp"

static std::vector<InstSet> global_stk;
static std::vector<BaseAST*> value_list, func_list, fparams, arr_size;
static std::vector<std::vector<BaseAST*>> rparams, idx_stk, arr_list;

// 加入一个有用的函数，remind InstSet 的类型是 std::vector<std::pair<InstType, std::unique_ptr<BaseAST>>>
// 是装载instruction_type和BaseAST指针的pair对
// 其中 InstType是一个枚举enum类型，合法的有ConstDecl, Decl, ArrayDecl, Stmt, Branch, While, Break, Continue
void push_Back(InstType instType, BaseAST *ast) { 
    // 不能直接先定义_pair，否则会调用析构函数.
   auto& inst_pair = global_stk.back();
   inst_pair.push_back(make_pair(instType, std::unique_ptr<BaseAST>(ast)));
}


// 声明 lexer 函数和错误处理函数
int yylex();
void yyerror(std::unique_ptr<BaseAST> &ast, const char *s);

%}

// 定义 parser 函数和错误处理函数的附加参数
%parse-param { std::unique_ptr<BaseAST> &ast }

// yylval 的定义, 我们把它定义成了一个联合体 (union)
// 因为 token 的值有的是字符串指针, 有的是整数
// 之前我们在 lexer 中用到的 str_val 和 int_val 就是在这里被定义的
// 至于为什么要用字符串指针而不直接用 string 或者 unique_ptr<string>?
// 回答：union既不能作为基类，也不能被继承自其他类，所以不能含有虚函数。事实上，构造函数/析构函数/拷贝构造函数/赋值运算符/虚函数的类成员变量都不行。
%union {
    std::string *str_val;
    int int_val;
    BaseAST *ast_val;
}

// lexer 返回的所有 token 种类的声明
%token <str_val> INT VOID RETURN CONST IF ELSE WHILE BREAK CONTINUE
%token <str_val> IDENT UNARYOP MULOP ADDOP RELOP EQOP LANDOP LOROP
%token <int_val> INT_CONST

// 非终结符的类型定义
%type <ast_val> CompUnit Unit
%type <ast_val> Decl ConstDecl ConstDef VarDecl VarDef ArraySize ArraySizeList 
%type <ast_val> FuncDef BType Block FuncFParam
%type <ast_val> LVal Number InitVal
%type <ast_val> Exp PrimaryExp IfExp UnaryExp MulExp AddExp RelExp EqExp LAndExp LOrExp

%%

// 开始符, CompUnit ::= FuncDef, 大括号后声明了解析完成后 parser 要做的事情
// $1 指代规则里第一个符号的返回值, 也就是 FuncDef 的返回值.
// CompUnit ::= [CompUnit] (Decl | FuncDef);
// 修正：CompUnit ::= GlobalList
// GlobalList ::= (FuncDef | Decl) | GlobalList (FuncDef | Decl)
// 开始从CompUnit推导时，先在环境栈里加入本函数所有的Instruction Set, 再在内部完成后弹栈。
CompUnit 
    : {
        global_stk.push_back(InstSet());
        }
        Unit {
        ast = std::unique_ptr<BaseAST>(new CompUnitAST(func_list, global_stk.back()));
        global_stk.pop_back();
    };

// CompUnit的变量/常量/函数声明的作用域从该声明处开始, 直到文件结尾.
// It acts like a GlobalList
Unit
    : Decl | Unit Decl
    | FuncDef {
        func_list.push_back($1);
    } | Unit FuncDef {
        func_list.push_back($2);
    };


Decl : ConstDecl | VarDecl;

// ConstDecl ::= "const" BType ConstDef {"," ConstDef} ";";
ConstDecl : CONST BType ConstDef_List ';';
ConstDef_List : ConstDef | ConstDef_List ',' ConstDef

// ConstDef      ::= IDENT {"[" ConstExp "]"} "=" ConstInitVal;
// ConstInitVal  ::= ConstExp | "{" [ConstInitVal {"," ConstInitVal}] "}";
// 其中中括号只是为了数组。我们单独分开数据和普通数据来考虑。
ConstDef
    : IDENT '=' Exp {
        auto constInitVal = std::unique_ptr<BaseAST>($3);
        push_Back(InstType::ConstDecl, new ConstDefAST($1->c_str(), constInitVal));
    }
    | IDENT ArraySizeList '=' InitVal {
        auto initVal = std::unique_ptr<BaseAST>($4);
        push_Back(InstType::ArrayDecl, new ArrayDefAST($1->c_str(), arr_size, initVal));
        arr_size.clear();
    };
    | IDENT ArraySizeList {
        push_Back(InstType::ArrayDecl, new ArrayDefAST($1->c_str(), arr_size));
        arr_size.clear();
    };

VarDecl : BType VarDefList ';';
VarDefList : VarDef | VarDefList ',' VarDef
VarDef
    : IDENT {
        push_Back(InstType::Decl, new VarDefAST($1->c_str()));
    }
    | IDENT '=' Exp {
        auto initVal = std::unique_ptr<BaseAST>($3);
        push_Back(InstType::Decl, new VarDefAST($1->c_str(), initVal));
    }
    | IDENT ArraySizeList '=' InitVal {
        auto initVal = std::unique_ptr<BaseAST>($4);
        push_Back(InstType::ArrayDecl, new ArrayDefAST($1->c_str(), arr_size, initVal));
        arr_size.clear();
    };
    | IDENT ArraySizeList {
        push_Back(InstType::ArrayDecl, new ArrayDefAST($1->c_str(), arr_size));
        arr_size.clear();
    }

// FuncDef ::= FuncType IDENT '(' ')' Block;
// 我们这里可以直接写 '(' 和 ')', 因为之前在 lexer 里已经处理了单个字符的情况
// 解析完成后, 把这些符号的结果收集起来, 然后拼成一个新的字符串, 作为结果返回
// $$ 表示非终结符的返回值, 我们可以通过给这个符号赋值的方法来返回结果
// 你可能会问, FuncType, IDENT 之类的结果已经是字符串指针了
// 为什么还要用 unique_ptr 接住它们, 然后再解引用, 把它们拼成另一个字符串指针呢
// 因为所有的字符串指针都是我们 new 出来的, new 出来的内存一定要 delete
// 否则会发生内存泄漏, 而 unique_ptr 这种智能指针可以自动帮我们 delete
// 虽然此处你看不出用 unique_ptr 和手动 delete 的区别, 但当我们定义了 AST 之后
// 这种写法会省下很多内存管理的负担
// 修改 FuncDef ::= BType IDENT '(' FuncFParams ')'
FuncDef
    : BType IDENT '('  {
            fparams.clear();
        } FuncFParams ')' Block {
            auto rettype = std::unique_ptr<BaseAST>($1);
            auto ident = std::unique_ptr<std::string>($2);
            auto block = std::unique_ptr<BaseAST>($7);
            $$ = new FuncDefAST(rettype, ident->c_str(), fparams, block);
    } | BType IDENT '(' ')' Block {
        fparams.clear();
        auto rettype = std::unique_ptr<BaseAST>($1);
        auto ident = std::unique_ptr<std::string>($2);
        auto block = std::unique_ptr<BaseAST>($5);
        $$ = new FuncDefAST(rettype, ident->c_str(), fparams, block);
    };

// 除了"int"，新考虑"void"的情况。 
BType
    : INT {
        $$ = new BTypeAST("int");
    } | VOID {
        $$ = new BTypeAST("void");
    };

// 形参一个或者多个，
FuncFParams : FuncFParam | FuncFParams ',' FuncFParam;

FuncFParam
    : INT IDENT {
        fparams.push_back(new FuncFParamAST(FuncFParamAST::Int, $2->c_str(), fparams.size()));
    }
    | INT IDENT '[' ']' {
        fparams.push_back(new FuncFParamAST(FuncFParamAST::Array, $2->c_str(), fparams.size(), arr_size));
    }
    | INT IDENT '[' ']' ArraySizeList {
        fparams.push_back(new FuncFParamAST(FuncFParamAST::Array, $2->c_str(), fparams.size(), arr_size));
        arr_size.clear();
    };

Block :
    '{' {
        global_stk.push_back(InstSet());
    }
    BlockItem_List '}' {
        $$ = new BlockAST(global_stk.back());
        global_stk.pop_back();
    } | '{' '}' {
        $$ = new BlockAST();
    };

BlockItem_List : BlockItem | BlockItem BlockItem_List ;

BlockItem : Decl | Stmt ;
//Stmt          ::= LVal "=" Exp ";"
//                | [Exp] ";"
//                | Block
//                | "if" "(" Exp ")" Stmt ["else" Stmt]
//                | "while" "(" Exp ")" Stmt
//                | "break" ";"
//                | "continue" ";"
//                | "return" [Exp] ";";
Stmt
    :  LVal '=' Exp ';' {
        auto lval = std::unique_ptr<BaseAST>($1);
        auto exp = std::unique_ptr<BaseAST>($3);
        push_Back(InstType::Stmt, new AssignmentAST(lval, exp));
    } | IfExp Stmt ELSE {
            global_stk.push_back(InstSet());
        } Stmt {
            auto exp = std::unique_ptr<BaseAST>($1);
            InstSet true_instset, false_instset;
            
            for(auto &inst : global_stk.back())
                false_instset.push_back(std::make_pair(inst.first, std::move(inst.second)));
            global_stk.pop_back();

            for(auto &inst : global_stk.back())
                true_instset.push_back(std::make_pair(inst.first, std::move(inst.second)));
            global_stk.pop_back();
            push_Back(InstType::Branch, new BranchAST(exp, true_instset, false_instset));
        } 
    | IfExp Stmt {
            auto exp = std::unique_ptr<BaseAST>($1);
            InstSet true_instset;
            for(auto &inst : global_stk.back())
                true_instset.push_back(std::make_pair(inst.first, std::move(inst.second)));
            global_stk.pop_back();
            push_Back(InstType::Branch, new BranchAST(exp, true_instset));
    } | WHILE '(' Exp ')' {
            global_stk.push_back(InstSet());
        } Stmt {
            auto exp = std::unique_ptr<BaseAST>($3);
            InstSet while_body;
            for(auto &inst : global_stk.back())
                while_body.push_back(std::make_pair(inst.first, std::move(inst.second)));
            global_stk.pop_back();
            push_Back(InstType::While, new WhileAST(exp, while_body));
    } | BREAK ';' {
        push_Back(InstType::Break, new BreakAST());
    } | CONTINUE ';' {
        push_Back(InstType::Continue, new ContinueAST());
    } | RETURN ';' {
        push_Back(InstType::Stmt, new ReturnAST());
    } | RETURN Exp ';' {
        auto number = std::unique_ptr<BaseAST>($2);
        push_Back(InstType::Stmt, new ReturnAST(number));
    } | ';' | Exp ';' {
        push_Back(InstType::Stmt, $1);
    } | Block {
        push_Back(InstType::Stmt, $1);
    };

IfExp
    : IF '(' Exp ')' {
        global_stk.push_back(InstSet());
        $$ = $3;
    }

ArraySizeList : ArraySize | ArraySizeList ArraySize;

ArraySize 
    : '[' Exp ']' {
        arr_size.push_back($2);
    };

InitVal : Exp {
        auto exp = std::unique_ptr<BaseAST>($1);
        $$ = new InitValAST(exp);
    }
    | '{' {
            auto vec = std::vector<BaseAST*>();
            arr_list.push_back(vec);
        } ArrInitList '}' {
        $$ = new InitValAST(arr_list.back());
        arr_list.pop_back();
    }
    | '{' '}' {
        auto vec = std::vector<BaseAST*>();
        arr_list.push_back(vec);
        $$ = new InitValAST(arr_list.back());
        arr_list.pop_back();
    };

ArrInitList : InitVal {
        arr_list.back().push_back($1);
    } 
    | ArrInitList ',' InitVal {
        arr_list.back().push_back($3);
    };

LVal
    : IDENT {
        $$ = new LValAST($1->c_str());
    }
    | IDENT {
            auto vec = std::vector<BaseAST*>();
            idx_stk.push_back(vec);
        } IndexList {
            $$ = new LValAST($1->c_str(), idx_stk.back());
            idx_stk.pop_back();
    };

IndexList : Index | IndexList Index

Index : '[' Exp ']' {
        idx_stk.back().push_back($2);
    };

Exp 
    : LOrExp {
        auto add_exp = std::unique_ptr<BaseAST>($1);
        $$ = new ExpAST(add_exp);
    };

PrimaryExp  
    : '(' Exp ')' {
        auto exp = std::unique_ptr<BaseAST>($2);
        $$ = new PrimaryExpAST(exp);
    }
    | Number {
        auto number = std::unique_ptr<BaseAST>($1);
        $$ = new PrimaryExpAST(number);
    }
    | LVal {
        auto lval = std::unique_ptr<BaseAST>($1);
        $$ = new PrimaryExpAST(lval);
    };

UnaryExp
    : PrimaryExp {
        auto primary_exp = std::unique_ptr<BaseAST>($1);
        $$ = new UnaryExpAST(primary_exp);
    }
    | UNARYOP UnaryExp {
        auto op = std::unique_ptr<std::string>($1);
        auto unary_exp = std::unique_ptr<BaseAST>($2);
        $$ = new UnaryExpAST(op->c_str(), unary_exp);
    }
    | ADDOP UnaryExp {
        auto op = std::unique_ptr<std::string>($1);
        auto unary_exp = std::unique_ptr<BaseAST>($2);
        $$ = new UnaryExpAST(op->c_str(), unary_exp);
    }
    | IDENT '(' {
            rparams.push_back(std::vector<BaseAST*>());
        } FuncRParams ')' {
            $$ = new UnaryExpAST($1->c_str(), rparams.back());
            rparams.pop_back();
    }
    | IDENT '(' ')' {
        rparams.push_back(std::vector<BaseAST*>());
        $$ = new UnaryExpAST($1->c_str(), rparams.back());
        rparams.pop_back();
    }
    ;

FuncRParams : FuncRParam | FuncRParams ',' FuncRParam;

FuncRParam 
    : Exp {
        rparams.back().push_back($1);
    }

MulExp
    : UnaryExp {
        auto unary_exp = std::unique_ptr<BaseAST>($1);
        $$ = new MulExpAST(unary_exp);
    }
    | MulExp MULOP UnaryExp {
        auto left_exp = std::unique_ptr<BaseAST>($1);
        auto op = std::unique_ptr<std::string>($2);
        auto right_exp = std::unique_ptr<BaseAST>($3);
        $$ = new MulExpAST(left_exp, op->c_str(), right_exp);
    };

AddExp
    : MulExp {
        auto mul_exp = std::unique_ptr<BaseAST>($1);
        $$ = new MulExpAST(mul_exp);
    }
    | AddExp ADDOP MulExp {
        auto left_exp = std::unique_ptr<BaseAST>($1);
        auto op = std::unique_ptr<std::string>($2);
        auto right_exp = std::unique_ptr<BaseAST>($3);
        $$ = new AddExpAST(left_exp, op->c_str(), right_exp);
    };

RelExp
    : AddExp {
        auto add_exp = std::unique_ptr<BaseAST>($1);
        $$ = new RelExpAST(add_exp);
    }
    | RelExp RELOP AddExp {
        auto left_exp = std::unique_ptr<BaseAST>($1);
        auto op = std::unique_ptr<std::string>($2);
        auto right_exp = std::unique_ptr<BaseAST>($3);
        $$ = new RelExpAST(left_exp, op->c_str(), right_exp);
    };

EqExp
    : RelExp {
        auto rel_exp = std::unique_ptr<BaseAST>($1);
        $$ = new EqExpAST(rel_exp);
    }
    | EqExp EQOP RelExp {
        auto left_exp = std::unique_ptr<BaseAST>($1);
        auto op = std::unique_ptr<std::string>($2);
        auto right_exp = std::unique_ptr<BaseAST>($3);
        $$ = new EqExpAST(left_exp, op->c_str(), right_exp);
    };

LAndExp
    : EqExp {
        auto eq_exp = std::unique_ptr<BaseAST>($1);
        $$ = new LAndExpAST(eq_exp);
    }
    | LAndExp LANDOP EqExp {
        auto left_exp = std::unique_ptr<BaseAST>($1);
        auto op = std::unique_ptr<std::string>($2);
        auto right_exp = std::unique_ptr<BaseAST>($3);
        $$ = new LAndExpAST(left_exp, op->c_str(), right_exp);
    };

LOrExp
    : LAndExp {
        auto land_exp = std::unique_ptr<BaseAST>($1);
        $$ = new LOrExpAST(land_exp);
    }
    | LOrExp LOROP LAndExp {
        auto left_exp = std::unique_ptr<BaseAST>($1);
        auto op = std::unique_ptr<std::string>($2);
        auto right_exp = std::unique_ptr<BaseAST>($3);
        $$ = new LOrExpAST(left_exp, op->c_str(), right_exp);
    };

Number
    : INT_CONST {
        $$ = new NumberAST($1);
    }
    ;

%%

// 定义错误处理函数, 其中第二个参数是错误信息
// parser 如果发生错误 (例如输入的程序出现了语法错误), 就会调用这个函数
void yyerror(std::unique_ptr<BaseAST> &ast, const char *s) {
    extern int yylineno;    // defined and maintained in lex
    extern char *yytext;    // defined and maintained in lex
    int len = strlen(yytext);

    char buf[512]={0};
    for (int i = 0; i < len; ++i) {
        sprintf(buf, "%s%d ", buf, yytext[i]);
    }
    std::cerr << "error: " << s << " at symbol '" << buf << "' on line " << yylineno << std::endl;
}