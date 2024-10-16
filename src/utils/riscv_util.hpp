#ifndef RISCV_UTIL_H
#define RISCV_UTIL_H

#include <iostream>
#include <string>
#include <map>

#include <koopa.h>

using std::ostream, std::endl, std::map, std::string;

class koopa2RISCV {
    class Env {
        // total size and address
        size_t _size;
        map<koopa_raw_value_t, int> _addr;
    
    public:
        size_t current;
        bool has_call;
        Env() = default;
        ~Env() = default;
        Env(size_t size, bool _has_call) {
            this->_size = this->current = size;
            this->has_call = _has_call;
            this->_addr.clear();
        }
        // Get the current total size 
        size_t size() {
            return _size;
        }
        // Get the address (unsigned int) of certain koopa raw value `kval`.
        int addr(koopa_raw_value_t kval) {
            // Judge if `kval` is in the map, and return the address (unsigned int) if true.
            //if (addr.count(kval))
            //    return addr[kval];
            auto address_it = _addr.find(kval);
            if (address_it != _addr.end())
                return address_it->second;
            // else calculate the address and return.
            int t = calc_inst_size(kval);
            if (t == 0)
                return -1;	//Some error here.
            current -= t;
            //addr[kval] = current;
            _addr.emplace(kval, current);
            return current;
        }
    };
    int jump_index = 0;
    Env env;
    const char *current_func_name;
    ostream &output;
    // Some useful RISC-V code related functions.
    static size_t calc_func_size(koopa_raw_function_t kfunc, bool &has_call);
    static size_t calc_blk_size(koopa_raw_basic_block_t kblk, bool &has_call);
    static size_t calc_inst_size(koopa_raw_value_t kval);
    static size_t calc_type_size(koopa_raw_type_t ty);
    // Some useful RISC-V value related functions.
    void traversal_raw_slice(const koopa_raw_slice_t *rs);
    void gen_riscv_func(koopa_raw_function_t kfunc);
    void gen_riscv_block(koopa_raw_basic_block_t kblk);
    void gen_riscv_value(koopa_raw_value_t kval);

    void Load(koopa_raw_value_t kval, const string& reg);
    void Load(int addr, const string& reg);
    void Store(int addr, const string& reg);
    void Visit_aggregate(koopa_raw_value_t kval);
    void Visit_global_alloc(koopa_raw_value_t kalloc);
    void Visit_load(const koopa_raw_load_t *kload, int addr);
    void Visit_store(const koopa_raw_store_t *kstore);
    void Visit_get_ptr(const koopa_raw_get_ptr_t *kget, int addr);
    void Visit_get_elem_ptr(const koopa_raw_get_elem_ptr_t *kget, int addr);
    void Visit_binary(const koopa_raw_binary_t *kbinary, int addr);
    void Visit_branch(const koopa_raw_branch_t *kbranch);
    void Visit_jump(const koopa_raw_jump_t *kjump);
    void Visit_call(const koopa_raw_call_t *kcall, int addr);
    void Visit_return(const koopa_raw_return_t *kret);

public:
    // 构造函数接受一个输出流参数，用于输出生成的RISC-V汇编代码。
    koopa2RISCV(ostream &_out) : output(_out) {}
    // build(raw)接受要转换的Koopa IR程序。
    void build(const koopa_raw_program_t *raw);
};




#endif