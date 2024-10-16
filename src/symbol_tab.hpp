#ifndef SYMBOL_TAB_H
#define SYMBOL_TAB_H

#include <map>
#include <string>
#include <vector>
#include <memory>

#include <koopa.h>

class BaseAST;

struct LValSymbol {
    enum SymbolType {
        Const,
        Var,
        Array,
        Pointer,
        Function
    } type;
    void *number;
    LValSymbol() {}
    LValSymbol(SymbolType _type, void * _number) : type(_type), number(_number) {}
};

class SymbolTab {

    std::vector<std::map<std::string, LValSymbol>> sym_stk;

public:
    void NewEnv() {
        sym_stk.push_back(std::map<std::string, LValSymbol>());
    }
    void AddSymbol(const std::string &name, LValSymbol koopa_item) {
        
        auto &list = sym_stk.back();
        assert(list.count(name) == 0);  // 'name' should not be in the 'list'

        list.emplace(name, koopa_item);
    }
    LValSymbol GetSymbol(const std::string &name) {
        LValSymbol res;
        std::vector<std::map<std::string, LValSymbol>>::reverse_iterator r_it;
        for (r_it = sym_stk.rbegin(); r_it != sym_stk.rend(); ++r_it)
            if ((*r_it).count(name) != 0) {
                res = (*r_it)[name];
                break;
            }
        return res;
    }
    void DeleteEnv() {
        sym_stk.pop_back();
    }
};
#endif