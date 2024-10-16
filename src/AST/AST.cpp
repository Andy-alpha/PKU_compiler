#include "AST/AST.hpp"
Block BaseAST::big_block;
SymbolTab BaseAST::sym_tab;
LoopMaintainer BaseAST::loop_maintainer;

char *new_char_arr(std::string str) {
    size_t n = str.length();
    char *res = new char[n + 1];
    str.copy(res, n + 1);
    res[n] = 0;
    return res;
}

// 包装一层，不然BaseAST的派生类只能用反人类语句例如 compunit_ast.operator<<(sd::cout); 现可以使用std::cout << compunit_ast;
std::ostream& operator<< (std::ostream& os, const BaseAST& ast) {
    ast.Dump(os);
    return os;
}
//#include "AST/code_AST.hpp"
//#include "AST/array_AST.hpp"
// 以下函数定义于code_AST.hpp
void CompUnitAST::add_lib_funcs(std::vector<const void*> &funcs) const {
    koopa_raw_function_data_t *func;
    koopa_raw_type_kind_t *ty;
    std::vector<const void *> fparams;

    func = new koopa_raw_function_data_t();
    ty = new koopa_raw_type_kind_t();
    ty->tag = KOOPA_RTT_FUNCTION;
    ty->data.function.params = empty_koopa_raw_slice(KOOPA_RSIK_TYPE);
    ty->data.function.ret = simple_koopa_raw_type_kind(KOOPA_RTT_INT32);
    func->ty = ty;
    func->name = "@getint";
    func->params = empty_koopa_raw_slice(KOOPA_RSIK_VALUE);
    func->bbs = empty_koopa_raw_slice(KOOPA_RSIK_BASIC_BLOCK);
    sym_tab.AddSymbol("getint", LValSymbol(LValSymbol::Function, func));
    funcs.push_back(func);

    func = new koopa_raw_function_data_t();
    ty = new koopa_raw_type_kind_t();
    ty->tag = KOOPA_RTT_FUNCTION;
    ty->data.function.params = empty_koopa_raw_slice(KOOPA_RSIK_TYPE);
    ty->data.function.ret = simple_koopa_raw_type_kind(KOOPA_RTT_INT32);
    func->ty = ty;
    func->name = "@getch";
    func->params = empty_koopa_raw_slice(KOOPA_RSIK_VALUE);
    func->bbs = empty_koopa_raw_slice(KOOPA_RSIK_BASIC_BLOCK);
    sym_tab.AddSymbol("getch", LValSymbol(LValSymbol::Function, func));
    funcs.push_back(func);

    func = new koopa_raw_function_data_t();
    ty = new koopa_raw_type_kind_t();
    ty->tag = KOOPA_RTT_FUNCTION;
    fparams.clear();
    fparams.push_back(make_int_pointer_type());
    // make koopa raw slice from vector
    ty->data.function.params = make_koopa_raw_slice(fparams, KOOPA_RSIK_TYPE);
    ty->data.function.ret = simple_koopa_raw_type_kind(KOOPA_RTT_INT32);
    func->ty = ty;
    func->name = "@getarray";
    func->params = empty_koopa_raw_slice(KOOPA_RSIK_VALUE);
    func->bbs = empty_koopa_raw_slice(KOOPA_RSIK_BASIC_BLOCK);
    sym_tab.AddSymbol("getarray", LValSymbol(LValSymbol::Function, func));
    funcs.push_back(func);

    func = new koopa_raw_function_data_t();
    ty = new koopa_raw_type_kind_t();
    ty->tag = KOOPA_RTT_FUNCTION;
    fparams.clear();
    fparams.push_back(simple_koopa_raw_type_kind(KOOPA_RTT_INT32));
    ty->data.function.params = make_koopa_raw_slice(fparams, KOOPA_RSIK_TYPE);
    ty->data.function.ret = simple_koopa_raw_type_kind(KOOPA_RTT_UNIT);
    func->ty = ty;
    func->name = "@putint";
    func->params = empty_koopa_raw_slice(KOOPA_RSIK_VALUE);
    func->bbs = empty_koopa_raw_slice(KOOPA_RSIK_BASIC_BLOCK);
    sym_tab.AddSymbol("putint", LValSymbol(LValSymbol::Function, func));
    funcs.push_back(func);

    func = new koopa_raw_function_data_t();
    ty = new koopa_raw_type_kind_t();
    ty->tag = KOOPA_RTT_FUNCTION;
    fparams.clear();
    fparams.push_back(simple_koopa_raw_type_kind(KOOPA_RTT_INT32));
    ty->data.function.params = make_koopa_raw_slice(fparams, KOOPA_RSIK_TYPE);
    ty->data.function.ret = simple_koopa_raw_type_kind(KOOPA_RTT_UNIT);
    func->ty = ty;
    func->name = "@putch";
    func->params = empty_koopa_raw_slice(KOOPA_RSIK_VALUE);
    func->bbs = empty_koopa_raw_slice(KOOPA_RSIK_BASIC_BLOCK);
    sym_tab.AddSymbol("putch", LValSymbol(LValSymbol::Function, func));
    funcs.push_back(func);

    func = new koopa_raw_function_data_t();
    ty = new koopa_raw_type_kind_t();
    ty->tag = KOOPA_RTT_FUNCTION;
    fparams.clear();
    fparams.push_back(simple_koopa_raw_type_kind(KOOPA_RTT_INT32));
    fparams.push_back(make_int_pointer_type());
    // make koopa raw slice from vector
    ty->data.function.params = make_koopa_raw_slice(fparams, KOOPA_RSIK_TYPE);
    ty->data.function.ret = simple_koopa_raw_type_kind(KOOPA_RTT_UNIT);
    func->ty = ty;
    func->name = "@putarray";
    func->params = empty_koopa_raw_slice(KOOPA_RSIK_VALUE);
    func->bbs = empty_koopa_raw_slice(KOOPA_RSIK_BASIC_BLOCK);
    sym_tab.AddSymbol("putarray", LValSymbol(LValSymbol::Function, func));
    funcs.push_back(func);

    func = new koopa_raw_function_data_t();
    ty = new koopa_raw_type_kind_t();
    ty->tag = KOOPA_RTT_FUNCTION;
    ty->data.function.params = empty_koopa_raw_slice(KOOPA_RSIK_TYPE);
    ty->data.function.ret = simple_koopa_raw_type_kind(KOOPA_RTT_UNIT);
    func->ty = ty;
    func->name = "@starttime";
    func->params = empty_koopa_raw_slice(KOOPA_RSIK_VALUE);
    func->bbs = empty_koopa_raw_slice(KOOPA_RSIK_BASIC_BLOCK);
    sym_tab.AddSymbol("starttime", LValSymbol(LValSymbol::Function, func));
    funcs.push_back(func);

    func = new koopa_raw_function_data_t();
    ty = new koopa_raw_type_kind_t();
    ty->tag = KOOPA_RTT_FUNCTION;
    ty->data.function.params = empty_koopa_raw_slice(KOOPA_RSIK_TYPE);
    ty->data.function.ret = simple_koopa_raw_type_kind(KOOPA_RTT_UNIT);
    func->ty = ty;
    func->name = "@stoptime";
    func->params = empty_koopa_raw_slice(KOOPA_RSIK_VALUE);
    func->bbs = empty_koopa_raw_slice(KOOPA_RSIK_BASIC_BLOCK);
    sym_tab.AddSymbol("stoptime", LValSymbol(LValSymbol::Function, func));
    funcs.push_back(func);
}

CompUnitAST::CompUnitAST(std::vector<BaseAST*> &_func_list, InstSet &_value_list) {
    for(BaseAST* func : _func_list)
        func_list.emplace_back(func);
    for(auto &pair : _value_list)
    {
        if(pair.first == Decl)
            value_list.push_back(make_pair(pair.first, std::unique_ptr<BaseAST>(new GlobalVarDefAST(pair.second))));
        else if(pair.first == ArrayDecl)
            value_list.push_back(make_pair(pair.first, std::unique_ptr<BaseAST>(new GlobalArrayDefAST(pair.second))));
        else
            value_list.push_back(make_pair(pair.first, std::move(pair.second)));
    }
}

