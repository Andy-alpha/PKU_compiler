#ifndef CODE_AST_H
#define CODE_AST_H

#include <map>

#include "AST/base_AST.hpp"
#include "utils/koopa_util.hpp"

enum InstType {
    ConstDecl,
    Decl,
    ArrayDecl,
    Stmt,
    Branch,
    While,
    Break,
    Continue
};
// 是装载instruction_type和BaseAST指针的pair对
typedef std::vector<std::pair<InstType, std::unique_ptr<BaseAST>>> InstSet;

// CompUnit 是 BaseAST
class CompUnitAST : public BaseAST {
    // 把一些启动时就需要的库函数的符号等加入其中，包括@getint, @getch, @getarray, @putch, @putarray, @starttime, @stoptime
    void add_lib_funcs(std::vector<const void*> &funcs) const;
public:
    // 用智能指针管理对象
    //std::unique_ptr<BaseAST> func_def;
    //以上是我新加的
    //经过验证，不能加入第一行。不然有问题。至于Dump()，之后再考虑添加。
    std::vector<std::unique_ptr<BaseAST>> func_list;
    InstSet value_list;

    CompUnitAST(std::vector<BaseAST*> &_func_list, InstSet &_value_list);
    // 定义自己的生成koopa raw program的函数：
    koopa_raw_program_t to_koopa_raw_program() const {
        // 创建新的作用域
        sym_tab.NewEnv();
        std::vector<const void *> values;
        std::vector<const void *> funcs;
        add_lib_funcs(funcs);
        for(auto &pair : value_list) {
            // 从(InstSet)类型的value_list中取出pair对，判断类型是不是合法的。
            assert(pair.first == ConstDecl || pair.first == Decl || pair.first == ArrayDecl);
            if(pair.first == ConstDecl)
                pair.second->build_koopa_values(); // 这样可以直接计算值
            else
                values.push_back(pair.second->build_koopa_values());//否则直接调用对应派生类的函数。
        }
        for(auto &func_ast : func_list)
            funcs.push_back(func_ast->build_koopa_values());
        sym_tab.DeleteEnv();
        // 结束作用域
        koopa_raw_program_t res;
        // Make koopa raw slice from vector.
        res.values = make_koopa_raw_slice(values, KOOPA_RSIK_VALUE);
        res.funcs = make_koopa_raw_slice(funcs, KOOPA_RSIK_FUNCTION);

        return res;
    }


protected:
    void Dump(std::ostream& os) const override {
        os << "CompUnitAST { ";
        os << *func_list[0];// Buggy, supposed to show all the possible values.
        os << " }";
    }
};
// BType 是类型
class BTypeAST : public BaseAST {
public:
    std::string Btype; // Btype取值 int 或者 void，其余情况没有实现。
    BTypeAST(const char *_Btype) : Btype(_Btype) {}

    void *build_koopa_values() const override {
        // simple_koopa_raw_type_kind() 定义在了 koopa_util.hpp中，只接受KOOPA_RTT_INT32和KOOPA_RTT_UNIT的参数，否则会报错。
        if (Btype == "int")
            return simple_koopa_raw_type_kind(KOOPA_RTT_INT32);
        else if(Btype == "void")
            return simple_koopa_raw_type_kind(KOOPA_RTT_UNIT);
        return nullptr; // not implemented
    }
};
// FuncFParam 展开函数形参表，至少有一个参数。没参数的情况下是没有形参表。
class FuncFParamAST : public BaseAST {
public:
    enum ParamType { // 定义变量的类型（支持整形和数组）
        Int,
        Array
    } type;
    std::string name;
    int index;
    std::vector<std::unique_ptr<BaseAST>> sz_exp;

    FuncFParamAST(ParamType _type, const char *_name, int _index) : type(_type), name(_name), index(_index) {}
    FuncFParamAST(ParamType _type, const char *_name, int _index, std::vector<BaseAST*> &_sz_Exp) : type(_type), name(_name), index(_index) {
        for(auto exp : _sz_Exp)
            sz_exp.emplace_back(exp);
    }

    void *get_koopa_type() const {
        if (type == Int)
            return simple_koopa_raw_type_kind(KOOPA_RTT_INT32);
        else if(type == Array) {
            if(!sz_exp.empty()) {
                std::vector<int> sz;
                for(auto &exp : sz_exp)
                    sz.push_back(exp->CalcValue());
                koopa_raw_type_kind *ty = make_array_type(sz);
                koopa_raw_type_kind *koopa_type = new koopa_raw_type_kind();
                koopa_type->tag = KOOPA_RTT_POINTER;
                koopa_type->data.pointer.base = ty;
                return koopa_type;
            }
            else
                return make_int_pointer_type();
        }
        return nullptr;
    }

    void *build_koopa_values() const override {
        koopa_raw_value_data *res = Init((koopa_raw_type_kind*)get_koopa_type(),
            new_char_arr("@" + name),
            empty_koopa_raw_slice(KOOPA_RSIK_VALUE),
            make_koopa_raw_value_kind(KOOPA_RVT_FUNC_ARG_REF, index)
        );
        return res;
    }
};

// AST code block, enclosed in '{}'. Different from koopa basic block
class BlockAST : public BaseAST {
public:
    InstSet insts;

    BlockAST() {}
    BlockAST(InstSet &_insts) {
        for (auto &inst : _insts)
            insts.push_back(std::make_pair(inst.first, std::move(inst.second)));
        // 这里需要用std::move()移动右值，因为是unique_ptr，没法直接复制(duplicate)，但是拷贝(copy)内容是可以的
    }

    static void add_InstSet(const InstSet &insts) {
        sym_tab.NewEnv();
        for (const auto &inst : insts)
            inst.second->build_koopa_values();
        sym_tab.DeleteEnv();
    }

    inline void build_koopa_values_no_env() const {
        for (const auto &inst : insts)
            inst.second->build_koopa_values();
    }

    void *build_koopa_values() const override {
        add_InstSet(insts);
        return nullptr;
    }
    
protected:
    void Dump(std::ostream& os) const override {
        os << "(Block Dump not implemented)";
  }
};

// FuncDef 也是 BaseAST，用于函数定义。
class FuncDefAST : public BaseAST {
public:
    std::unique_ptr<BaseAST> func_type;
    std::string ident;
    std::vector<std::unique_ptr<FuncFParamAST>> fparams;
    std::unique_ptr<BlockAST> block;
    // FuncDef : FuncType '(' {FuncFParam}? ')' Block

    FuncDefAST(std::unique_ptr<BaseAST> &_func_type, const char *_ident, std::vector<BaseAST*> &_fparams, std::unique_ptr<BaseAST> &_block) : ident(_ident) {
        func_type = std::move(_func_type);
        for(BaseAST* fp : _fparams)
            fparams.emplace_back(dynamic_cast<FuncFParamAST*>(fp));
        block = std::unique_ptr<BlockAST>(dynamic_cast<BlockAST*>(_block.release()));
    }
    ~FuncDefAST() = default;

    void *build_koopa_values() const override {
        koopa_raw_function_data_t *res = new koopa_raw_function_data_t();
        sym_tab.AddSymbol(ident, LValSymbol(LValSymbol::SymbolType::Function, res));

        koopa_raw_type_kind_t *ty = new koopa_raw_type_kind_t();
        ty->tag = KOOPA_RTT_FUNCTION;
        std::vector<const void*> pair;
        for(auto &fp : fparams)
            pair.push_back(fp->get_koopa_type());
        // make raw slice from vector
        ty->data.function.params = make_koopa_raw_slice(pair, KOOPA_RSIK_TYPE);
        //ty->data.function.ret = (const struct koopa_raw_type_kind *)func_type->build_koopa_values();
        ty->data.function.ret = (koopa_raw_type_t)func_type->build_koopa_values();
        res->ty = ty;                           // Function类型
        res->name = new_char_arr("@" + ident);  // Function名称
        pair.clear();
        for(auto &fp : fparams)
            pair.push_back(fp->build_koopa_values());   // 形式参数
        // make raw slice from vector
        res->params = make_koopa_raw_slice(pair, KOOPA_RSIK_VALUE);
        // Create new basic block to be added later
        std::vector<const void *> blocks;
        big_block.SetBasicBlockBuf(&blocks);
        // Create new entry block
        koopa_raw_basic_block_data_t *entry_block = new koopa_raw_basic_block_data_t();
        entry_block->name = new_char_arr("%entry_" + ident);
        entry_block->params = empty_koopa_raw_slice(KOOPA_RSIK_VALUE);
        entry_block->used_by = empty_koopa_raw_slice(KOOPA_RSIK_VALUE);
        // Begin a new environment
        sym_tab.NewEnv();
        big_block.Push_back(entry_block);
        big_block.SetCurrentFunction(res);
        for(size_t i = 0; i < fparams.size(); ++i) { // Allocate memory for all parameters
            auto &fp = fparams[i];
            auto value = (koopa_raw_value_t)pair[i];
            koopa_raw_value_data *allo = AllocType("@" + fp->name, value->ty);
            enum LValSymbol::SymbolType type;
            if (allo->ty->data.pointer.base->tag == KOOPA_RTT_POINTER)
                type = LValSymbol::SymbolType::Pointer;
            else
                type = LValSymbol::SymbolType::Var;
            sym_tab.AddSymbol(fp->name, LValSymbol(type, allo));
            big_block.Push_back(allo);
            koopa_raw_value_data *sto = Init(simple_koopa_raw_type_kind(KOOPA_RTT_UNIT),
                nullptr,
                empty_koopa_raw_slice(KOOPA_RSIK_VALUE),
                make_koopa_raw_value_kind(KOOPA_RVT_STORE, 0, value, allo)
            );
            big_block.Push_back(sto);
        }
        block->build_koopa_values_no_env();
        sym_tab.DeleteEnv();    // End environment
        big_block.FinishCurrentBlock();
        // make koopa raw slice from vector
        res->bbs = make_koopa_raw_slice(blocks, KOOPA_RSIK_BASIC_BLOCK);

        return res;
    }
// 以下是我加的调试函数
protected:
    void Dump(std::ostream& os) const override {
        os << "FuncDefAST { ";
        os << *func_type;
//        func_type->Dump(); 不知道什么问题
        os << ", " << ident << ", ";
//        block->Dump(); 不知道什么问题
        os << *block;
        os << " }";
  }
};
// 处理return
class ReturnAST : public BaseAST {
public:
    std::unique_ptr<BaseAST> ret_num;
    ReturnAST() : ret_num(nullptr) {}
    ReturnAST(std::unique_ptr<BaseAST> &_ret_num) {
        ret_num = std::move(_ret_num);
    }
    void *build_koopa_values() const override {
        koopa_raw_value_data *res = 
            Init(simple_koopa_raw_type_kind(KOOPA_RTT_UNIT),
                nullptr,
                empty_koopa_raw_slice(KOOPA_RSIK_VALUE),
                make_koopa_raw_value_kind(KOOPA_RVT_RETURN));
        auto &value = res->kind.data.ret.value;
        value = ret_num ? (koopa_raw_value_t)ret_num->build_koopa_values() : nullptr;
        
        big_block.Push_back(res);
        return res;
    }
};
// 处理左值赋值
class AssignmentAST : public BaseAST {
public:
    std::unique_ptr<BaseAST> lval;
    std::unique_ptr<BaseAST> exp;
    AssignmentAST(std::unique_ptr<BaseAST> &_lval, std::unique_ptr<BaseAST> &_exp) {
        lval = std::move(_lval);
        exp = std::move(_exp);
    }
    void *build_koopa_values() const override {
        koopa_raw_value_data *res = 
            Init(simple_koopa_raw_type_kind(KOOPA_RTT_UNIT),
                nullptr,
                empty_koopa_raw_slice(KOOPA_RSIK_VALUE),
                make_koopa_raw_value_kind(KOOPA_RVT_STORE, 0,
                    (koopa_raw_value_t)exp->build_koopa_values(),
                    (koopa_raw_value_t)lval->koopa_leftvalue())
                );
        big_block.Push_back(res);
        return nullptr;
    }
};
// 处理分支
class BranchAST : public BaseAST {
private:
    // koopa_raw_slice_t _instructions = {nullptr, 0, KOOPA_RSIK_BASIC_BLOCK};
    koopa_raw_basic_block_data_t *bb_init(const char *_name, koopa_raw_slice_t _params, koopa_raw_slice_t _used_by, koopa_raw_slice_t _insts = {nullptr, 0, KOOPA_RSIK_BASIC_BLOCK}) const {
        koopa_raw_basic_block_data_t *res = new koopa_raw_basic_block_data_t();
        res->name = _name;
        res->params = _params;
        res->used_by = _used_by;
        return res;
    }
public:
    // 分为真值分支和假值分支，然后分别处理Instruction Set.
    std::unique_ptr<BaseAST> exp;
    InstSet true_instset;
    InstSet false_instset;
    // 这里考虑只有if语句和有if-else语句
    BranchAST(std::unique_ptr<BaseAST> &_exp, InstSet &_true_insts) {
        for (auto &inst : _true_insts)
            true_instset.push_back(std::make_pair(inst.first, std::move(inst.second)));
        exp = std::move(_exp);
    }
    BranchAST(std::unique_ptr<BaseAST> &_exp, InstSet &_true_insts, InstSet &_false_insts) {
        for (auto &inst : _true_insts)
            true_instset.push_back(std::make_pair(inst.first, std::move(inst.second)));
        for (auto &inst : _false_insts)
            false_instset.push_back(std::make_pair(inst.first, std::move(inst.second)));
        exp = std::move(_exp);
    }
    void *build_koopa_values() const override {
        koopa_raw_value_data *res = 
            Init(simple_koopa_raw_type_kind(KOOPA_RTT_UNIT),
                nullptr,
                empty_koopa_raw_slice(KOOPA_RSIK_VALUE),
                make_koopa_raw_value_kind(KOOPA_RVT_BRANCH)
                );
        koopa_raw_branch_t &branch = res->kind.data.branch;
        branch.cond = (koopa_raw_value_t)exp->build_koopa_values();
       
        koopa_raw_basic_block_data_t *true_block =
            bb_init(
                new_char_arr("%true"),
                empty_koopa_raw_slice(KOOPA_RSIK_VALUE),
                empty_koopa_raw_slice(KOOPA_RSIK_VALUE)
            );
        koopa_raw_basic_block_data_t *false_block =
            bb_init(
                new_char_arr("%false"),
                empty_koopa_raw_slice(KOOPA_RSIK_VALUE),
                empty_koopa_raw_slice(KOOPA_RSIK_VALUE)
            );
        koopa_raw_basic_block_data_t *end_block =
            bb_init(
                new_char_arr("%end"),
                empty_koopa_raw_slice(KOOPA_RSIK_VALUE),
                empty_koopa_raw_slice(KOOPA_RSIK_VALUE)
            );
        
        
        branch.true_bb = true_block;
        branch.true_args = empty_koopa_raw_slice(KOOPA_RSIK_VALUE);
        branch.false_bb = false_block;
        branch.false_args = empty_koopa_raw_slice(KOOPA_RSIK_VALUE);
        big_block.Push_back(res);

        // true
        big_block.Push_back(true_block);
        BlockAST::add_InstSet(this->true_instset);
        big_block.Push_back(JumpInst(end_block)); // 这里用了神奇的跳转

        // false
        big_block.Push_back(false_block);
        BlockAST::add_InstSet(this->false_instset);
        big_block.Push_back(JumpInst(end_block));

        // end
        big_block.Push_back(end_block);
        
        return nullptr;
    }
};

class WhileAST : public BaseAST {
private:
    koopa_raw_basic_block_data_t *bb_init(const char *_name, koopa_raw_slice_t _params, koopa_raw_slice_t _used_by, koopa_raw_slice_t _insts = {nullptr, 0, KOOPA_RSIK_BASIC_BLOCK}) const {
        koopa_raw_basic_block_data_t *res = new koopa_raw_basic_block_data_t();
        res->name = _name;
        res->params = _params;
        res->used_by = _used_by;
        return res;
    }
public:
    std::unique_ptr<BaseAST> exp;
    InstSet body_insts;
    WhileAST(std::unique_ptr<BaseAST> &_exp, InstSet &_body_insts) {
        for (auto &inst : _body_insts)
            body_insts.push_back(std::make_pair(inst.first, std::move(inst.second)));
        exp = std::move(_exp);
    }
    void *build_koopa_values() const override {
        koopa_raw_basic_block_data_t *while_entry =
            bb_init(
                new_char_arr("%while_entry"),
                empty_koopa_raw_slice(KOOPA_RSIK_VALUE),
                empty_koopa_raw_slice(KOOPA_RSIK_VALUE)
            );
        koopa_raw_basic_block_data_t *while_block =
            bb_init(
                new_char_arr("%while_block"),
                empty_koopa_raw_slice(KOOPA_RSIK_VALUE),
                empty_koopa_raw_slice(KOOPA_RSIK_VALUE)
            );
        koopa_raw_basic_block_data_t *end_block =
            bb_init(
                new_char_arr("%end_block"),
                empty_koopa_raw_slice(KOOPA_RSIK_VALUE),
                empty_koopa_raw_slice(KOOPA_RSIK_VALUE)
            );
        loop_maintainer.AddLoop(while_entry, while_block, end_block);

        big_block.Push_back(JumpInst(while_entry));
        big_block.Push_back(while_entry);

        koopa_raw_value_data *br = Init(simple_koopa_raw_type_kind(KOOPA_RTT_UNIT),
            nullptr,
            empty_koopa_raw_slice(KOOPA_RSIK_VALUE),
            make_koopa_raw_value_kind(KOOPA_RVT_BRANCH)
        );
        koopa_raw_branch_t &branch = br->kind.data.branch;
        branch.cond = (koopa_raw_value_t)exp->build_koopa_values();
        branch.true_bb = while_block;
        branch.false_bb = end_block;
        branch.true_args = empty_koopa_raw_slice(KOOPA_RSIK_VALUE);
        branch.false_args = empty_koopa_raw_slice(KOOPA_RSIK_VALUE);
        big_block.Push_back(br);

        big_block.Push_back(while_block);
        BlockAST::add_InstSet(this->body_insts);
        big_block.Push_back(JumpInst(while_entry));

        big_block.Push_back(end_block);
        
        loop_maintainer.PopLoop();
        return nullptr;
    }
};

class BreakAST : public BaseAST {
public:
    void *build_koopa_values() const override {
        big_block.Push_back(JumpInst(loop_maintainer.GetLoop().end_block));
        return nullptr;
    }
};

class ContinueAST : public BaseAST {
public:
    void *build_koopa_values() const override {
        big_block.Push_back(JumpInst(loop_maintainer.GetLoop().while_entry));
        return nullptr;
    }
};

class ConstDefAST : public BaseAST {
public:
    std::string name;
    std::unique_ptr<BaseAST> exp;

    ConstDefAST(const char *_name, std::unique_ptr<BaseAST> &_exp) : name(_name) {
        exp = std::move(_exp);
    }

    void *build_koopa_values() const override {
        koopa_raw_value_data *res = Init(
            simple_koopa_raw_type_kind(KOOPA_RTT_INT32),    // most variables are 'int'.
            nullptr,
            empty_koopa_raw_slice(KOOPA_RSIK_VALUE),
            make_koopa_raw_value_kind(KOOPA_RVT_INTEGER, exp->CalcValue())
        );
        sym_tab.AddSymbol(name, LValSymbol(LValSymbol::SymbolType::Const, res));
        return res;
    }
};

class VarDefAST : public BaseAST {
public:
    std::string name;
    std::unique_ptr<BaseAST> exp;

    VarDefAST(const char *_name) : name(_name), exp(nullptr){}

    VarDefAST(const char *_name, std::unique_ptr<BaseAST> &_exp) : name(_name) {
        exp = std::move(_exp);
    }

    void *build_koopa_values() const override {
        koopa_raw_value_data *res = Init(
            make_int_pointer_type(),    // most variables are 'int'.
            new_char_arr("@" + name),
            empty_koopa_raw_slice(KOOPA_RSIK_VALUE),
            make_koopa_raw_value_kind(KOOPA_RVT_ALLOC)
        );
        big_block.Push_back(res);
        sym_tab.AddSymbol(name, LValSymbol(LValSymbol::SymbolType::Var, res));

        if (exp) {
            koopa_raw_value_data *store = Init(
                simple_koopa_raw_type_kind(KOOPA_RTT_UNIT),
                nullptr,
                empty_koopa_raw_slice(),
                make_koopa_raw_value_kind(KOOPA_RVT_STORE, 0, (koopa_raw_value_t)exp->build_koopa_values(), res)
            );

            big_block.Push_back(store);
        }

        return res;
    }
};

class GlobalVarDefAST : public BaseAST {
public:
    std::string name;
    std::unique_ptr<BaseAST> exp;

    GlobalVarDefAST(std::unique_ptr<BaseAST> &vardef_ast) {
        VarDefAST *var = dynamic_cast<VarDefAST*>(vardef_ast.release());
        name = var->name;
        exp = std::move(var->exp);
        delete var;
    }

    void *build_koopa_values() const override {
        koopa_raw_value_data *res = Init(make_int_pointer_type(),
            new_char_arr("@" + name),
            empty_koopa_raw_slice(KOOPA_RSIK_VALUE),
            make_koopa_raw_value_kind(KOOPA_RVT_GLOBAL_ALLOC));
        auto &init = res->kind.data.global_alloc.init;
        if(exp)
            init = make_koopa_interger(exp->CalcValue());//(koopa_raw_value_data*)exp->build_koopa_values();
        else
            init = ZeroInit();
        big_block.Push_back(res);
        sym_tab.AddSymbol(name, LValSymbol(LValSymbol::SymbolType::Var, res));
        return res;
    }
};
#endif