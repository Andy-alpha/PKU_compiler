#ifndef EXP_AST_H
#define EXP_AST_H

#include "AST/base_AST.hpp"

class ExpAST : public BaseAST {
public:
    enum ExpType{
        Primary,
        Op,
        Function,
        Num,
        Array
    } type;
    std::string op;
    std::unique_ptr<BaseAST> unaryExp;
    std::unique_ptr<BaseAST> leftExp; // may be primary
    std::unique_ptr<BaseAST> rightExp;
    ExpAST() = default;
    ExpAST(std::unique_ptr<BaseAST> &_unaryExp) {
        unaryExp = std::move(_unaryExp);
    }

    /*virtual*/ void *build_koopa_values() const {
        return unaryExp->build_koopa_values();
    }

    /*virtual*/ int CalcValue() const {
        return unaryExp->CalcValue();
    }
    /*virtual*/ void *koopa_leftvalue() const {
        std::cerr << "Not Implement koopa_leftvalue" << std::endl;
        assert(false);
        return 0;
    }
    // 我自己加入的运算符重载
    // Dump()函数，用于输出内容至命令行，实际上可以重载运算符<<.
    //virtual void Dump123() const {printf("Nothing\n");}
protected:
    //virtual void Dump() const = 0;
    virtual void Dump(std::ostream& os) const {
        os << "This is Dump() of ExpAST";
    }
};
// Remember, the main idea is to deal with left values, which are not available when trying to copy directly. So use `std::move()` to implement it.
class LValAST : public ExpAST {
public:
    //ExpType type;
    std::string name;
    std::vector<std::unique_ptr<BaseAST>> idx;  // index list
    LValAST(const char *_name) {
        type = Num;
        name = _name;
    }
    LValAST(const char *_name, std::vector<BaseAST*> &_idx){
        type = Array;
        name = _name;
        for(auto &i : _idx)
            idx.emplace_back(i);
    } 

        // 将变量作为左值返回（返回该左值的变量本身）
    void *koopa_leftvalue() const override {
        if(type == Array)  {
            koopa_raw_value_data *get;
            koopa_raw_value_t src = (koopa_raw_value_t)sym_tab.GetSymbol(name).number;
            if(src->ty->data.pointer.base->tag == KOOPA_RTT_POINTER) { // 如果是指针，包括指向的内容一起迁移
                koopa_raw_value_t src = (koopa_raw_value_t)sym_tab.GetSymbol(name).number;
                // Create KOOPA raw value data, ``payload''.
                koopa_raw_value_data *payload = Init(src->ty->data.pointer.base,
                    nullptr, empty_koopa_raw_slice(KOOPA_RSIK_VALUE),
                    make_koopa_raw_value_kind(KOOPA_RVT_LOAD, 0, src));   

                big_block.Push_back(payload);

                bool first = true;
                src = payload;
                for(auto &i : idx) {
                    koopa_raw_type_kind *ty = new koopa_raw_type_kind();
                    if(first) { // 对第一个单独处理
                        get = Init(src->ty, nullptr, empty_koopa_raw_slice(KOOPA_RSIK_VALUE),
                            make_koopa_raw_value_kind(KOOPA_RVT_GET_PTR, 0, src, (koopa_raw_value_t)i->build_koopa_values()));
                        first = false;
                    }
                    else {
                        // Load the following part. Store pointer and array. 
                        ty->tag = KOOPA_RTT_POINTER;
                        ty->data.pointer.base = src->ty->data.pointer.base->data.array.base;

                        get = Init(ty, nullptr, empty_koopa_raw_slice(KOOPA_RSIK_VALUE),
                            make_koopa_raw_value_kind(KOOPA_RVT_GET_ELEM_PTR, 0, src, (koopa_raw_value_t)i->build_koopa_values()));
                    }
                    big_block.Push_back(get);
                    src = get;
                }
            }
            else { //其余情况，和对非第一个的处理是一样的。
                for(auto &i : idx) {
                    koopa_raw_type_kind *ty = new koopa_raw_type_kind();
                    // begin
                    ty->tag = KOOPA_RTT_POINTER;
                    ty->data.pointer.base = src->ty->data.pointer.base->data.array.base;
                    get = Init(ty, nullptr, empty_koopa_raw_slice(KOOPA_RSIK_VALUE),
                            make_koopa_raw_value_kind(KOOPA_RVT_GET_ELEM_PTR, 0, src, (koopa_raw_value_t)i->build_koopa_values()));
                    // end
                    big_block.Push_back(get);
                    src = get;
                }
            }
            return get;
        }
        else if (type == Num) {
            return (void *)sym_tab.GetSymbol(name).number;
        }
        return nullptr;
    }
    // 将变量作为右值返回（读取变量里存储的值）
    void *build_koopa_values() const override {
    // 主要目的是根据变量的类型（常量、变量、数组或指针）来构建一个`koopa_raw_value_data`对象，并返回这个对象的指针。

    //首先创建一个新的`koopa_raw_value_data`对象，并根据变量的类型进行不同的处理：

    // - 如果变量是常量，函数直接返回这个常量的值。
    // - 如果变量是一个普通变量，创建一个新的`koopa_raw_value_data`对象，设置其类型为`KOOPA_RTT_INT32`，并将其源设置为变量的值。
    // - 如果变量是一个数组，函数会根据数组的索引来获取元素的指针，并需要加载元素的值。
    // - 如果变量是一个指针，函数会首先加载指针指向的值，然后根据索引来获取元素的指针，并可能需要加载元素的值。

    // 在处理过程中，函数会将创建的`koopa_raw_value_data`对象添加到`big_block`中。最后，函数返回创建的`koopa_raw_value_data`对象的指针。
        koopa_raw_value_data *res = new koopa_raw_value_data();
        auto var = sym_tab.GetSymbol(name);
        if (var.type == LValSymbol::Const)
            return (void *)var.number;
        else if (var.type == LValSymbol::Var) {
            delete res;
            res = Init(simple_koopa_raw_type_kind(KOOPA_RTT_INT32),
                    nullptr,
                    empty_koopa_raw_slice(KOOPA_RSIK_VALUE),
                    make_koopa_raw_value_kind(KOOPA_RVT_LOAD, 0, (koopa_raw_value_t)var.number));

            big_block.Push_back(res);
        }
        else if (var.type == LValSymbol::Array) {

            bool need_load = false;
            koopa_raw_value_data *get;
            koopa_raw_value_data *src = (koopa_raw_value_data*)var.number;
            if (idx.empty()) {
                koopa_raw_type_kind *ty = new koopa_raw_type_kind();
                ty->tag = KOOPA_RTT_POINTER;
                ty->data.pointer.base = src->ty->data.pointer.base->data.array.base;

                get = Init(ty, nullptr, empty_koopa_raw_slice(KOOPA_RSIK_VALUE),
                        make_koopa_raw_value_kind(KOOPA_RVT_GET_ELEM_PTR, 0, src, make_koopa_interger(0)));

                big_block.Push_back(get);
            }
            else {
                for(auto &i : idx) {
                    koopa_raw_type_kind *ty = new koopa_raw_type_kind();
                    ty->tag = KOOPA_RTT_POINTER;
                    ty->data.pointer.base = src->ty->data.pointer.base->data.array.base;

                    get = Init(ty, nullptr, empty_koopa_raw_slice(KOOPA_RSIK_VALUE),
                        make_koopa_raw_value_kind(KOOPA_RVT_GET_ELEM_PTR, 0, src, (koopa_raw_value_t)i->build_koopa_values()));

                    big_block.Push_back(get);
                    src = get;
                    if(ty->data.pointer.base->tag == KOOPA_RTT_INT32)
                        need_load = true;
                }
            }
            if(need_load) {
                delete res;
                res = Init(simple_koopa_raw_type_kind(KOOPA_RTT_INT32),
                    nullptr,
                    empty_koopa_raw_slice(KOOPA_RSIK_VALUE),
                    make_koopa_raw_value_kind(KOOPA_RVT_LOAD, 0, get));

                big_block.Push_back(res);
            }
            else if(src->ty->data.pointer.base->tag == KOOPA_RTT_ARRAY) {
                koopa_raw_type_kind *ty = new koopa_raw_type_kind();
                ty->tag = KOOPA_RTT_POINTER;
                ty->data.pointer.base = src->ty->data.pointer.base->data.array.base;
                delete res;
                res = Init(ty, nullptr, empty_koopa_raw_slice(KOOPA_RSIK_VALUE),
                    make_koopa_raw_value_kind(KOOPA_RVT_GET_ELEM_PTR, 0, src, make_koopa_interger(0)));

                big_block.Push_back(res);
            }
            else
                res = src;
        }
        else if (var.type == LValSymbol::Pointer) {

            koopa_raw_value_data *src = (koopa_raw_value_data*)var.number;
            koopa_raw_value_data *load0 = Init(src->ty->data.pointer.base, nullptr, empty_koopa_raw_slice(KOOPA_RSIK_VALUE),
                    make_koopa_raw_value_kind(KOOPA_RVT_LOAD, 0, src));
            big_block.Push_back(load0);

            bool need_load = false;
            bool first = true;
            koopa_raw_value_data *get;
            src = load0;
            for(auto &i : idx) {
                //get = new koopa_raw_value_data();
                koopa_raw_type_kind *ty = new koopa_raw_type_kind();
                if(first) {
                    get = Init(src->ty, nullptr, empty_koopa_raw_slice(KOOPA_RSIK_VALUE),
                        make_koopa_raw_value_kind(KOOPA_RVT_GET_PTR, 0, src, (koopa_raw_value_t)i->build_koopa_values()));
                    first = false;
                }
                else {
                    ty->tag = KOOPA_RTT_POINTER;
                    ty->data.pointer.base = src->ty->data.pointer.base->data.array.base;
                    get = Init(ty, nullptr, empty_koopa_raw_slice(KOOPA_RSIK_VALUE),
                        make_koopa_raw_value_kind(KOOPA_RVT_GET_ELEM_PTR, 0, src, (koopa_raw_value_t)i->build_koopa_values()));
                }
                big_block.Push_back(get);
                src = get;
                if(get->ty->data.pointer.base->tag == KOOPA_RTT_INT32)
                    need_load = true;
            }
            if(need_load) {
                delete res;
                res = Init(simple_koopa_raw_type_kind(KOOPA_RTT_INT32),
                    nullptr, empty_koopa_raw_slice(KOOPA_RSIK_VALUE),
                    make_koopa_raw_value_kind(KOOPA_RVT_LOAD, 0, get));
                
                big_block.Push_back(res);
            }
            else if(src->ty->data.pointer.base->tag == KOOPA_RTT_ARRAY) {
                koopa_raw_type_kind *ty = new koopa_raw_type_kind();
                ty->tag = KOOPA_RTT_POINTER;
                ty->data.pointer.base = src->ty->data.pointer.base->data.array.base;
                delete res;
                res = Init(ty, nullptr, empty_koopa_raw_slice(KOOPA_RSIK_VALUE),
                    make_koopa_raw_value_kind(KOOPA_RVT_GET_ELEM_PTR, 0, src, make_koopa_interger(0)));
                big_block.Push_back(res);
            }
            else
                res = src;
        }
        return res;
    }
    int CalcValue() const override { 
        //只有常量能够计算值
        auto var = sym_tab.GetSymbol(name);
        assert(var.type == LValSymbol::Const);
        return ((koopa_raw_value_t)var.number)->kind.data.integer.value;
    }
};

// Primary expressions.
class PrimaryExpAST : public ExpAST {
public:
    std::unique_ptr<BaseAST> nextExp; // Exp or Number
    PrimaryExpAST(std::unique_ptr<BaseAST> &_nextExp) {
        nextExp = std::move(_nextExp);
    }
    void *build_koopa_values() const override {
        return nextExp->build_koopa_values();
    }
    int CalcValue() const override {
        return nextExp->CalcValue();
    }
};

// Unary expressions.
class UnaryExpAST : public ExpAST {
    // Unary expressions.
    // UnaryExp ::= PrimaryExp | UNARYOP UnaryExp | ADDOP UnaryExp | IDENT '(' FuncRParams ')' | IDENT '(' ')';
public:
    std::unique_ptr<BaseAST> nextExp; // PrimaryExp or UnaryExp
    std::vector<BaseAST*> funcRParams;
    // Constructors of the class,
    // when the type is PrimaryExp.
    UnaryExpAST(std::unique_ptr<BaseAST> &_primary_exp) {
        type = Primary;
        nextExp = std::move(_primary_exp);
    }
    // when the type is UnaryOP.
    UnaryExpAST(const char *_op, std::unique_ptr<BaseAST> &_unary_exp) {

        type = Op;
        op = std::string(_op);
        nextExp = std::move(_unary_exp);
    }
    // when the type is Function.
    UnaryExpAST(const char *_ident, std::vector<BaseAST*> &_rparams) {
        type = Function;
        op = _ident;
        funcRParams = _rparams;
    }

    void *build_koopa_values() const override {
        //根据传入的函数实参，构建一个`koopa_raw_value_data`对象，并返回这个对象的指针`res`。
        NumberAST zero(0);
        koopa_raw_value_data *res = nullptr;
        koopa_raw_function_data_t *func = nullptr;
        std::vector<const void *> rpa;  // real parameters
        switch (type) {
            default:
                assert(false);
            case Primary:
                res = (koopa_raw_value_data *)nextExp->build_koopa_values();
                break;
            case Function:
                func = (koopa_raw_function_data_t *)sym_tab.GetSymbol(op).number;
                for(auto rp : funcRParams)
                    rpa.push_back(rp->build_koopa_values());
                
                res = Init(func->ty->data.function.ret, nullptr, 
                    empty_koopa_raw_slice(KOOPA_RSIK_VALUE),
                    make_koopa_raw_value_kind(KOOPA_RVT_CALL));
                res->kind.data.call.callee = func;
                // make koopa raw slice from vector
                res->kind.data.call.args = make_koopa_raw_slice(rpa, KOOPA_RSIK_VALUE);
                big_block.Push_back(res);
                break;
            case Op:
                if (op == "+") {
                    res = (koopa_raw_value_data *)nextExp->build_koopa_values();
                    break;
                }
                int _operator = 1;
                if (op == "-")
                    _operator = KOOPA_RBO_SUB;
                else if (op == "!")
                    _operator = KOOPA_RBO_EQ;
                res = Init(simple_koopa_raw_type_kind(KOOPA_RTT_INT32),
                    nullptr,
                    empty_koopa_raw_slice(KOOPA_RSIK_VALUE),
                    make_koopa_raw_value_kind(KOOPA_RVT_BINARY,
                        _operator,
                        (koopa_raw_value_t)zero.build_koopa_values(),       // lhs
                        (koopa_raw_value_t)nextExp->build_koopa_values())   // rhs
                    );
                big_block.Push_back(res);
                // break;
        }
        return res;
    }
    int CalcValue() const override {

        if (type == Primary)
            return nextExp->CalcValue();
        int res = 0;
        if (op == "+")
            res = nextExp->CalcValue();
        else if (op == "-")
            res = -nextExp->CalcValue();
        else if (op == "!")
            res = !nextExp->CalcValue();
        return res;
    }
};

// Multiplicative expressions.
class MulExpAST : public ExpAST {
    // MulEXp ::= UnaryExp | MulExp MULOP UnaryExp;
    // MULOP ::= "*"" | "/" | "%";
public:

    MulExpAST(std::unique_ptr<BaseAST> &_primary_exp) {
        type = Primary;
        leftExp = std::move(_primary_exp);
    }
    MulExpAST(std::unique_ptr<BaseAST> &_left_exp, const char *_op, std::unique_ptr<BaseAST> &_right_exp) {
        type = Op;
        leftExp = std::move(_left_exp);
        op = std::string(_op);
        rightExp = std::move(_right_exp);
    }

    void *build_koopa_values() const override {
        koopa_raw_value_data *res = nullptr;
        switch (type) {
            default:
                assert(false);
            case Primary:
                res = (koopa_raw_value_data *)leftExp->build_koopa_values();
                break;
            case Op:
                //auto &binary = res->kind.data.binary;
                int _operator = 8;
                if (op == "*")
                    _operator = KOOPA_RBO_MUL;
                else if (op == "/")
                    _operator = KOOPA_RBO_DIV;
                else if (op == "%")
                    _operator = KOOPA_RBO_MOD;
                res = Init(simple_koopa_raw_type_kind(KOOPA_RTT_INT32),
                    nullptr,
                    empty_koopa_raw_slice(KOOPA_RSIK_VALUE),
                    make_koopa_raw_value_kind(KOOPA_RVT_BINARY,
                        _operator,
                        (koopa_raw_value_t)leftExp->build_koopa_values(),       // lhs
                        (koopa_raw_value_t)rightExp->build_koopa_values())   // rhs
                    );
                big_block.Push_back(res);
                // break;
        }
        return res;
    }
    int CalcValue() const override {
        if (type == Primary)
            return leftExp->CalcValue();
        int res = 0;
        if (op == "*")
            res = leftExp->CalcValue() * rightExp->CalcValue();
        else if (op == "/")
            res = leftExp->CalcValue() / rightExp->CalcValue();
        else if (op == "%")
            res = leftExp->CalcValue() % rightExp->CalcValue();
        return res;
    }
};

// Additive expressions.
class AddExpAST : public ExpAST {
    // AddExp ::= MulExp | AddExp ADDOP MulExp;
    // ADDOP ::= "+" | "-";
public:

    AddExpAST(std::unique_ptr<BaseAST> &_primary_exp) {
        type = Primary;
        leftExp = std::move(_primary_exp);
    }
    AddExpAST(std::unique_ptr<BaseAST> &_left_exp, const char *_op, std::unique_ptr<BaseAST> &_right_exp) {
        type = Op;
        leftExp = std::move(_left_exp);
        op = std::string(_op);
        rightExp = std::move(_right_exp);
    }

    void *build_koopa_values() const override {
        koopa_raw_value_data *res = nullptr;
        switch (type) {
            default:
                assert(false);
            case Primary:
                res = (koopa_raw_value_data *)leftExp->build_koopa_values();
                break;
            case Op:
                int _operator = 6;
                if (op == "+")
                    _operator = KOOPA_RBO_ADD;
                else if (op == "-")
                    _operator = KOOPA_RBO_SUB;
                res = Init(simple_koopa_raw_type_kind(KOOPA_RTT_INT32),
                    nullptr,
                    empty_koopa_raw_slice(KOOPA_RSIK_VALUE),
                    make_koopa_raw_value_kind(KOOPA_RVT_BINARY,
                        _operator,
                        (koopa_raw_value_t)leftExp->build_koopa_values(),       // lhs
                        (koopa_raw_value_t)rightExp->build_koopa_values())   // rhs
                    );
                big_block.Push_back(res);
                // break;
        }
        return res;
    }
    int CalcValue() const override
    {
        if (type == Primary)
            return leftExp->CalcValue();
        int res = 0;
        if (op == "+")
            res = leftExp->CalcValue() + rightExp->CalcValue();
        else if (op == "-")
            res = leftExp->CalcValue() - rightExp->CalcValue();
        return res;
    }
};

// Relational expressions.
class RelExpAST : public ExpAST {
    // RelExp ::= AddExp | RelExp RELOP AddExp;
    // RELOP ::= "<" | ">" | "<=" | ">=";
public:

    RelExpAST(std::unique_ptr<BaseAST> &_primary_exp) {
        type = Primary;
        leftExp = std::move(_primary_exp);
    }
    RelExpAST(std::unique_ptr<BaseAST> &_left_exp, const char *_op, std::unique_ptr<BaseAST> &_right_exp) {
        type = Op;
        leftExp = std::move(_left_exp);
        op = std::string(_op);
        rightExp = std::move(_right_exp);
    }

    void *build_koopa_values() const override {
        koopa_raw_value_data *res = nullptr;
        switch (type) {
            default:
                assert(false);
            case Primary:
                res = (koopa_raw_value_data *)leftExp->build_koopa_values();
                break;
            case Op:
                int _operator = 3;
                if (op == "<")
                    _operator = KOOPA_RBO_LT;
                else if (op == "<=")
                    _operator = KOOPA_RBO_LE;
                else if (op == ">")
                    _operator = KOOPA_RBO_GT;
                else if (op == ">=")
                    _operator = KOOPA_RBO_GE;
                res = Init(simple_koopa_raw_type_kind(KOOPA_RTT_INT32),
                    nullptr,
                    empty_koopa_raw_slice(KOOPA_RSIK_VALUE),
                    make_koopa_raw_value_kind(KOOPA_RVT_BINARY,
                        _operator,
                        (koopa_raw_value_t)leftExp->build_koopa_values(),       // lhs
                        (koopa_raw_value_t)rightExp->build_koopa_values())   // rhs
                    );
                big_block.Push_back(res);
                // break;
        }
        return res;
    }
    int CalcValue() const override {
        if (type == Primary)
            return leftExp->CalcValue();
        int res = 0;
        if (op == "<")
            res = leftExp->CalcValue() < rightExp->CalcValue();
        else if (op == "<=")
            res = leftExp->CalcValue() <= rightExp->CalcValue();
        else if (op == ">")
            res = leftExp->CalcValue() > rightExp->CalcValue();
        else if (op == ">=")
            res = leftExp->CalcValue() >= rightExp->CalcValue();
        return res;
    }
};

// Equality expressions.
class EqExpAST : public ExpAST {
    // EqExp ::= RelExp | EqExp EQOP RelExp;
    // EQOP ::= "==" | "!=";
public:

    EqExpAST(std::unique_ptr<BaseAST> &_primary_exp) {
        type = Primary;
        leftExp = std::move(_primary_exp);
    }
    EqExpAST(std::unique_ptr<BaseAST> &_left_exp, const char *_op, std::unique_ptr<BaseAST> &_right_exp) {
        type = Op;
        leftExp = std::move(_left_exp);
        op = std::string(_op);
        rightExp = std::move(_right_exp);
    }

    void *build_koopa_values() const override {
        koopa_raw_value_data *res = nullptr;
        switch (type) {
            default:
                assert(false);
            case Primary:
                res = (koopa_raw_value_data *)leftExp->build_koopa_values();
                break;
            case Op:
                int _operator = 0;
                if (op == "==")
                    _operator = KOOPA_RBO_EQ;
                else if (op == "!=")
                    _operator = KOOPA_RBO_NOT_EQ;
                res = Init(simple_koopa_raw_type_kind(KOOPA_RTT_INT32),
                    nullptr,
                    empty_koopa_raw_slice(KOOPA_RSIK_VALUE),
                    make_koopa_raw_value_kind(KOOPA_RVT_BINARY,
                        _operator,
                        (koopa_raw_value_t)leftExp->build_koopa_values(),       // lhs
                        (koopa_raw_value_t)rightExp->build_koopa_values())   // rhs
                    );
                big_block.Push_back(res);
                // break;
        }
        return res;
    }
    int CalcValue() const override {
        if (type == Primary)
            return leftExp->CalcValue();
        int res = 0;
        if (op == "==")
            res = leftExp->CalcValue() == rightExp->CalcValue();
        else if (op == "!=")
            res = leftExp->CalcValue() != rightExp->CalcValue();
        return res;
    }
};

// Logical AND expressions.
class LAndExpAST : public ExpAST {
private:
    koopa_raw_value_data *make_op_koopa(koopa_raw_value_t exp, koopa_raw_binary_op Op) const {
        NumberAST zero(0);
        koopa_raw_value_data * res = 
            Init(simple_koopa_raw_type_kind(KOOPA_RTT_INT32),
                nullptr,
                empty_koopa_raw_slice(KOOPA_RSIK_VALUE),
                make_koopa_raw_value_kind(KOOPA_RVT_BINARY,
                    Op,
                    exp,       // lhs
                    (koopa_raw_value_t)zero.build_koopa_values())   // rhs
            );
        big_block.Push_back(res);
        return res;
    }
// LAndExp ::= EqExp | LAndExp "&&" EqExp;
public:

    LAndExpAST(std::unique_ptr<BaseAST> &_primary_exp) {
        type = Primary;
        leftExp = std::move(_primary_exp);
    }
    LAndExpAST(std::unique_ptr<BaseAST> &_left_exp, const char *_op, std::unique_ptr<BaseAST> &_right_exp) {
        type = Op;
        leftExp = std::move(_left_exp);
        op = std::string(_op);
        rightExp = std::move(_right_exp);
    }

    void *build_koopa_values() const override {
        std::unique_ptr<NumberAST> zero(new NumberAST(0));
        koopa_raw_value_data *res = nullptr;
        switch (type) {
            default:
                assert(false);
            case Primary:
                res = (koopa_raw_value_data *)leftExp->build_koopa_values();
                break;
            case Op:
                // Allcate a temporary variable to store the result of the expression.
                koopa_raw_value_data *temp_var =
                    Init(make_int_pointer_type(),
                        new_char_arr("%temp"),
                        empty_koopa_raw_slice(KOOPA_RSIK_VALUE),
                        make_koopa_raw_value_kind(KOOPA_RVT_ALLOC, 0, nullptr));
                big_block.Push_back(temp_var);
                // Store the result of the left expression to the temporary variable.
                koopa_raw_value_data *temp_store =
                    Init(simple_koopa_raw_type_kind(KOOPA_RTT_UNIT),
                        nullptr,
                        empty_koopa_raw_slice(),
                        make_koopa_raw_value_kind(KOOPA_RVT_STORE, 0,
                            (koopa_raw_value_t)zero->build_koopa_values(), temp_var));
                big_block.Push_back(temp_store);
                // branching process
                koopa_raw_value_data *branching =
                    Init(simple_koopa_raw_type_kind(KOOPA_RTT_UNIT), nullptr,
                        empty_koopa_raw_slice(KOOPA_RSIK_VALUE),
                        make_koopa_raw_value_kind(KOOPA_RVT_BRANCH)
                        );
                koopa_raw_branch_t &branch = branching->kind.data.branch;
                branch.cond = make_op_koopa((koopa_raw_value_t)leftExp->build_koopa_values(), KOOPA_RBO_NOT_EQ);
                koopa_raw_basic_block_data_t *true_block = new koopa_raw_basic_block_data_t();
                koopa_raw_basic_block_data_t *end_block = new koopa_raw_basic_block_data_t();
                branch.true_bb = true_block;  // true basic block
                branch.false_bb = end_block;  // false basic block, which is also the end part
                branch.true_args = empty_koopa_raw_slice(KOOPA_RSIK_VALUE);
                branch.false_args = empty_koopa_raw_slice(KOOPA_RSIK_VALUE);
                big_block.Push_back(branching);
                // true part
                true_block->name = new_char_arr("%true");
                true_block->params = empty_koopa_raw_slice(KOOPA_RSIK_VALUE);
                true_block->used_by = empty_koopa_raw_slice(KOOPA_RSIK_VALUE);
                //AddNewBasicBlock(true_block);
                big_block.Push_back(true_block);
                // Store the result of the right expression to the temporary variable.
                koopa_raw_value_data *b_store =
                    Init(simple_koopa_raw_type_kind(KOOPA_RTT_UNIT), nullptr,
                        empty_koopa_raw_slice(),
                        make_koopa_raw_value_kind(KOOPA_RVT_STORE, 0,
                            make_op_koopa((koopa_raw_value_t)rightExp->build_koopa_values(), KOOPA_RBO_NOT_EQ),
                            temp_var)
                        );
                big_block.Push_back(b_store);
                big_block.Push_back(JumpInst(end_block));
                // end part
                end_block->name = new_char_arr("%end");
                end_block->params = empty_koopa_raw_slice(KOOPA_RSIK_VALUE);
                end_block->used_by = empty_koopa_raw_slice(KOOPA_RSIK_VALUE);
                //AddNewBasicBlock(end_block);
                big_block.Push_back(end_block);
                // Load the value of the temporary variable.
                res = Init(
                    simple_koopa_raw_type_kind(KOOPA_RTT_INT32),
                    nullptr,
                    empty_koopa_raw_slice(KOOPA_RSIK_VALUE),
                    make_koopa_raw_value_kind(KOOPA_RVT_LOAD, 0, temp_var));
                big_block.Push_back(res);
        }
        return res;
    }
    int CalcValue() const override {
        if (type == Primary)
            return leftExp->CalcValue();
        int res = 0;
        if (op == "&&")
            res = leftExp->CalcValue() && rightExp->CalcValue();
        return res;
    }
};

// Logical OR expressions.
class LOrExpAST : public ExpAST {
private:
    koopa_raw_value_data *make_op_koopa(koopa_raw_value_t exp, koopa_raw_binary_op Op) const {
        NumberAST zero(0);
        koopa_raw_value_data * res = 
            Init(simple_koopa_raw_type_kind(KOOPA_RTT_INT32),
                nullptr,
                empty_koopa_raw_slice(KOOPA_RSIK_VALUE),
                make_koopa_raw_value_kind(KOOPA_RVT_BINARY,
                    Op,
                    exp,       // lhs
                    (koopa_raw_value_t)zero.build_koopa_values())   // rhs
            );
        big_block.Push_back(res);
        return res;
    }

public:

    LOrExpAST(std::unique_ptr<BaseAST> &_primary_exp) {
        type = Primary;
        leftExp = std::move(_primary_exp);
    }
    LOrExpAST(std::unique_ptr<BaseAST> &_left_exp, const char *_op, std::unique_ptr<BaseAST> &_right_exp) {
        type = Op;
        leftExp = std::move(_left_exp);
        op = std::string(_op);
        rightExp = std::move(_right_exp);
    }

    void *build_koopa_values() const override {
        std::unique_ptr<NumberAST> zero(new NumberAST(0));
        std::unique_ptr<NumberAST> one(new NumberAST(1));
        koopa_raw_value_data *res = nullptr;
        switch (type) {
            default:
                assert(false);
            case Primary:
                res = (koopa_raw_value_data *)leftExp->build_koopa_values();
                break;
            case Op:
                koopa_raw_value_data *temp_var =
                    Init(make_int_pointer_type(),
                        new_char_arr("%temp"),
                        empty_koopa_raw_slice(KOOPA_RSIK_VALUE),
                        make_koopa_raw_value_kind(KOOPA_RVT_ALLOC, 0, nullptr));
                big_block.Push_back(temp_var);
                // Store the result of the left expression to the temporary variable.
                koopa_raw_value_data *temp_store =
                    Init(simple_koopa_raw_type_kind(KOOPA_RTT_UNIT),
                        nullptr,
                        empty_koopa_raw_slice(),
                        make_koopa_raw_value_kind(KOOPA_RVT_STORE, 0,
                            (koopa_raw_value_t)one->build_koopa_values(), temp_var));
                big_block.Push_back(temp_store);
                // branching process
                koopa_raw_value_data *branching =
                    Init(simple_koopa_raw_type_kind(KOOPA_RTT_UNIT), nullptr,
                        empty_koopa_raw_slice(KOOPA_RSIK_VALUE),
                        make_koopa_raw_value_kind(KOOPA_RVT_BRANCH)
                        );
                koopa_raw_branch_t &branch = branching->kind.data.branch;
                branch.cond = make_op_koopa((koopa_raw_value_t)leftExp->build_koopa_values(), KOOPA_RBO_EQ);
                koopa_raw_basic_block_data_t *true_block = new koopa_raw_basic_block_data_t();
                koopa_raw_basic_block_data_t *end_block = new koopa_raw_basic_block_data_t();
                branch.true_bb = true_block;
                branch.false_bb = end_block;
                branch.true_args = empty_koopa_raw_slice(KOOPA_RSIK_VALUE);
                branch.false_args = empty_koopa_raw_slice(KOOPA_RSIK_VALUE);
                big_block.Push_back(branching);
                // true part
                true_block->name = new_char_arr("%true");
                true_block->params = empty_koopa_raw_slice(KOOPA_RSIK_VALUE);
                true_block->used_by = empty_koopa_raw_slice(KOOPA_RSIK_VALUE);
                //AddNewBasicBlock(true_block);
                big_block.Push_back(true_block);
                // Store the result of the right expression to the temporary variable.
                koopa_raw_value_data *b_store =
                    Init(simple_koopa_raw_type_kind(KOOPA_RTT_UNIT), nullptr,
                        empty_koopa_raw_slice(),
                        make_koopa_raw_value_kind(KOOPA_RVT_STORE, 0,
                            make_op_koopa((koopa_raw_value_t)rightExp->build_koopa_values(), KOOPA_RBO_NOT_EQ),
                            temp_var)
                        );
                big_block.Push_back(b_store);
                big_block.Push_back(JumpInst(end_block));
                // end part
                end_block->name = new_char_arr("%end");
                end_block->params = empty_koopa_raw_slice(KOOPA_RSIK_VALUE);
                end_block->used_by = empty_koopa_raw_slice(KOOPA_RSIK_VALUE);
                //AddNewBasicBlock(end_block);
                big_block.Push_back(end_block);
                // Load the value of the temporary variable.
                res = Init(
                    simple_koopa_raw_type_kind(KOOPA_RTT_INT32),
                    nullptr,
                    empty_koopa_raw_slice(KOOPA_RSIK_VALUE),
                    make_koopa_raw_value_kind(KOOPA_RVT_LOAD, 0, temp_var));
                big_block.Push_back(res);
        }
        return res;
    }
    int CalcValue() const override {
        if (type == Primary)
            return leftExp->CalcValue();
        int res = 1;
        if (op == "||")
            res = leftExp->CalcValue() || rightExp->CalcValue();
        return res;
    }
};
#endif
