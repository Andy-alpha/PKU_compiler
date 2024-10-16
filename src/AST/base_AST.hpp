//#pragma once
#ifndef BASE_AST_H
#define BASE_AST_H

#include <string>
#include <vector>
#include <iostream>

#include <cassert>

#include "utils/koopa_util.hpp"
#include "symbol_tab.hpp"
#include "utils/block.hpp"
#include "utils/loop_maintainer.hpp"

//char *new_char_arr(std::string str);

// 所有 AST 的基类
class BaseAST {
public:
    static SymbolTab sym_tab;
    static Block big_block;
    static LoopMaintainer loop_maintainer;

    virtual ~BaseAST() = default;
    // 输出koopa对象，并在全局环境添加各种信息
    virtual void *build_koopa_values() const {
        std::cerr << "Not Implement build_koopa_values" << std::endl;
        assert(false);
        return nullptr;
    }
    // 用于表达式AST求值
    virtual int CalcValue() const {
        std::cerr << "Not Implement CalcValue" << std::endl;
        assert(false);
        return 0;
    }
    // 返回该AST的左值（用于变量）
    virtual void *koopa_leftvalue() const {
        std::cerr << "Not Implement koopa_leftvalue" << std::endl;
        assert(false);
        return 0;
    }
    // 我自己加入的运算符重载
    friend std::ostream& operator<< (std::ostream& os, const BaseAST& ast);
    // Dump()函数，用于输出内容至命令行，实际上可以重载运算符<<.
    //virtual void Dump123() const {printf("Nothing\n");}
protected:
    //virtual void Dump() const = 0;
    virtual void Dump(std::ostream& os) const {
        os << "This is Dump() of BaseAST";
    }
    //virtual void Dump0(std::ostream& os) const = 0;
};



class NumberAST : public BaseAST {
public:
    int val;
    NumberAST(int _val) { val = _val; }

    void *build_koopa_values() const override {
        koopa_raw_value_data *res = Init(simple_koopa_raw_type_kind(KOOPA_RTT_INT32),
            nullptr,
            empty_koopa_raw_slice(KOOPA_RSIK_VALUE),
            make_koopa_raw_value_kind(KOOPA_RVT_INTEGER, val)
        );
        return res;
    }
    int CalcValue() const override {
        return val;
    }

protected:
    void Dump(std::ostream& os) const override {
        os << "NumberAST { int ";
        os << std::to_string(val);
        os << " }";
    }
};
#endif