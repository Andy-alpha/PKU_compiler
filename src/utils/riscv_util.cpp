#include "utils/riscv_util.hpp"

//
// Load certain koopa raw value `kval` to register `reg`
// 
// If -2048 ≤ addr ≤ 2047, then we can directly use `lw reg, addr(sp)`. 
//
// Or we must calculate the target pointer address first.
void koopa2RISCV::Load(koopa_raw_value_t kval, const string &reg) {
    if (kval->kind.tag == KOOPA_RVT_INTEGER)
        output << "    li " << reg << ", " << kval->kind.data.integer.value << endl;
    else if(kval->kind.tag == KOOPA_RVT_GLOBAL_ALLOC) {
        output << "    la t0, " << kval->name + 1 << endl;
        output << "    lw " << reg << ", 0(t0)" << endl;
    }
    else {
        int addr = env.addr(kval);
        Load(addr, reg);
    }
}

void koopa2RISCV::Load(int addr, const string &reg) {
    if(addr < -2048 || addr > 2047) {
        output << "    li t2, " << addr << endl;
        output << "    add t2, t2, sp" << endl;
        output << "    lw " << reg << ", " << "0(t2)" << endl;
    }
    else
        output << "    lw " << reg << ", " << addr << "(sp)" << endl;
}
//
// Store register `reg` to stack pointer (sp) + certain address.
// 
// If -2048 ≤ addr ≤ 2047, then we can directly use `sw reg, addr(sp)`. 
//
// Or we must calculate the target pointer address first.
void koopa2RISCV::Store(int addr, const string &reg) {
    if(addr < -2048 || addr > 2047) {
        output << "    li t2, " << addr << endl;
        output << "    add t2, t2, sp" << endl;
        output << "    sw " << reg << ", " << "0(t2)" << endl;
    }
    else
        output << "    sw " << reg << ", " << addr << "(sp)" << endl;
}
//
// Generate RISC-V value globally and totally.
//
// Add the `.word` part at the beginning of the code.
//
void koopa2RISCV::Visit_aggregate(koopa_raw_value_t kval) {
    if(kval->ty->tag == KOOPA_RTT_ARRAY) {
        for(int i = 0; i < kval->kind.data.aggregate.elems.len; ++i)
            Visit_aggregate((koopa_raw_value_t)kval->kind.data.aggregate.elems.buffer[i]);
    }
    else
        output << "    .word " << kval->kind.data.integer.value << endl;
}
//
// Generate RISC-V value allocation.
//
//  Include '.global', '.zero', and '.word'
//
void koopa2RISCV::Visit_global_alloc(koopa_raw_value_t kalloc) {
    // Remind: delete '@' in the beginning.
    output << ".global " << kalloc->name + 1 
    << endl
    << kalloc->name + 1 << ":" << endl;
    if (kalloc->kind.data.global_alloc.init->kind.tag == KOOPA_RVT_ZERO_INIT) {

        output << "    .zero " << calc_type_size(kalloc->ty->data.pointer.base) << endl;
    }
    else if(kalloc->kind.data.global_alloc.init->kind.tag == KOOPA_RVT_AGGREGATE)
        Visit_aggregate(kalloc->kind.data.global_alloc.init);
    else
        output << "    .word " << kalloc->kind.data.global_alloc.init->kind.data.integer.value << endl;
}

void koopa2RISCV::Visit_load(const koopa_raw_load_t *kload, int addr) {
    output << endl;

    if(kload->src->kind.tag == KOOPA_RVT_GET_ELEM_PTR || kload->src->kind.tag == KOOPA_RVT_GET_PTR) {
        Load(kload->src, "t0");
        output << "    lw t0, 0(t0)" << endl;
        Store(addr, "t0");
    }
    else {
        Load(kload->src, "t0");
        Store(addr, "t0");
    }
}

void koopa2RISCV::Visit_store(const koopa_raw_store_t *kstore) {
    output << endl;
    
    koopa_raw_value_t sto_value = kstore->value;
    koopa_raw_value_t sto_dest = kstore->dest;
    std::string dest;
    if(sto_dest->kind.tag == KOOPA_RVT_GLOBAL_ALLOC) {
        output << "    la t1, " << sto_dest->name + 1 << endl;
        dest = "(t1)";
    }
    else if(sto_dest->kind.tag == KOOPA_RVT_GET_ELEM_PTR || sto_dest->kind.tag == KOOPA_RVT_GET_PTR) {
        Load(sto_dest, "t1");
        dest = "(t1)";
    }
    else {
        int addr = env.addr(sto_dest);
        if(addr < -2048 || addr > 2047) {
            output << "    li t1, " << addr << endl;
            output << "    add t1, t1, sp" << endl;
            dest = "(t1)";
        }
        else
            dest = std::to_string(addr) + "(sp)";
    }
    if(sto_value->kind.tag == KOOPA_RVT_FUNC_ARG_REF) {
        koopa_raw_func_arg_ref_t arg = sto_value->kind.data.func_arg_ref;
        if(arg.index < 8)
            output << "    sw a" << arg.index << ", " << dest << endl;
        else {
            int offset = (arg.index - 8) * 4;
            /*
            if(offset < -2048 || offset > 2047) {
                output << "    li t2, " << offset << endl;
                output << "    addi t2, t2, sp" << endl;
                output << "    lw t0, (t2)" << endl;
            }
            else
                output << "    lw t0, " << offset << "(sp)" << endl;
            */
            Load(offset, "t0");
            output << "    sw t0, " << dest << endl;
        }
    }
    else {
        Load(sto_value, "t0");
        output << "    sw t0, " << dest << endl;
    }
}

void koopa2RISCV::Visit_get_ptr(const koopa_raw_get_ptr_t *kget, int addr) {
    output << endl;

    int src_addr = env.addr(kget->src);
    /*
    if(src_addr > 2047 || src_addr < -2048)
    {
        output << "    li t0, " << src_addr << endl;
        output << "    add t0, sp, t0" << endl;
    }
    else
        output << "    addi t0, sp, " << src_addr << endl;
    output << "    lw t0, 0(t0)" << endl;
    */

    Load(src_addr, "t0");
    Load(kget->index, "t1");
    int n = calc_type_size(kget->src->ty->data.pointer.base);
    output << "    li t2, " << n << endl;
    output << "    mul t1, t1, t2" << endl;
    output << "    add t0, t0, t1" << endl;
    Store(addr, "t0");
}

void koopa2RISCV::Visit_get_elem_ptr(const koopa_raw_get_elem_ptr_t *kget, int addr) {
    output << endl;

    if(kget->src->kind.tag == KOOPA_RVT_GLOBAL_ALLOC)
        output << "    la t0, " << kget->src->name + 1 << endl;
    else {
        int src_addr = env.addr(kget->src);
        if(src_addr > 2047 || src_addr < -2048)
        {
            output << "    li t0, " << src_addr << endl;
            output << "    add t0, sp, t0" << endl;
        }
        else
            output << "    addi t0, sp, " << src_addr << endl;
        if(kget->src->kind.tag == KOOPA_RVT_GET_ELEM_PTR || kget->src->kind.tag == KOOPA_RVT_GET_PTR)
            output << "    lw t0, 0(t0)" << endl;
    }
    Load(kget->index, "t1");
    int n = calc_type_size(kget->src->ty->data.pointer.base->data.array.base);
    output << "    li t2, " << n << endl;
    output << "    mul t1, t1, t2" << endl;
    output << "    add t0, t0, t1" << endl;
    Store(addr, "t0");
}

void koopa2RISCV::Visit_binary(const koopa_raw_binary_t *kbinary, int addr) {
    output << endl;

    Load(kbinary->lhs, "t0");
    Load(kbinary->rhs, "t1");
    switch (kbinary->op) {
    case KOOPA_RBO_NOT_EQ:
        output << "    xor t0, t0, t1" << endl;
        output << "    snez t0, t0" << endl;
        break;
    case KOOPA_RBO_EQ:
        output << "    xor t0, t0, t1" << endl;
        output << "    seqz t0, t0" << endl;
        break;
    case KOOPA_RBO_GT:
        output << "    sgt t0, t0, t1" << endl;
        break;
    case KOOPA_RBO_LT:
        output << "    slt t0, t0, t1" << endl;
        break;
    case KOOPA_RBO_GE:
        output << "    slt t0, t0, t1" << endl;
        output << "    xori t0, t0, 1" << endl;
        break;
    case KOOPA_RBO_LE:
        output << "    sgt t0, t0, t1" << endl;
        output << "    xori t0, t0, 1" << endl;
        break;
    case KOOPA_RBO_ADD:
        output << "    add t0, t0, t1" << endl;
        break;
    case KOOPA_RBO_SUB:
        output << "    sub t0, t0, t1" << endl;
        break;
    case KOOPA_RBO_MUL:
        output << "    mul t0, t0, t1" << endl;
        break;
    case KOOPA_RBO_DIV:
        output << "    div t0, t0, t1" << endl;
        break;
    case KOOPA_RBO_MOD:
        output << "    rem t0, t0, t1" << endl;
        break;
    case KOOPA_RBO_AND:
        output << "    and t0, t0, t1" << endl;
        break;
    case KOOPA_RBO_OR:
        output << "    or t0, t0, t1" << endl;
        break;
    case KOOPA_RBO_XOR:
        output << "    xor t0, t0, t1" << endl;
        break;
    case KOOPA_RBO_SHL:
        output << "    sll t0, t0, t1" << endl;
        break;
    case KOOPA_RBO_SHR:
        output << "    srl t0, t0, t1" << endl;
        break;
    case KOOPA_RBO_SAR:
        output << "    sra t0, t0, t1" << endl;
        break;
    }
    Store(addr, "t0");
}

void koopa2RISCV::Visit_branch(const koopa_raw_branch_t *kbranch) {
    output << endl;
    Load(kbranch->cond, "t0");
    // output << "    bnez t0, " << current_func_name << "_" << kbranch->true_bb->name + 1 << endl;
    // 解决跳转超限问题
    output << "    beqz t0, " << current_func_name << "_skip_" << jump_index << endl;
    output << "    j " << current_func_name << "_" << kbranch->true_bb->name + 1 << endl;
    output << current_func_name << "_skip_" << jump_index++ << ":" << endl;

    output << "    j " << current_func_name << "_" << kbranch->false_bb->name + 1 << endl;
}

void koopa2RISCV::Visit_jump(const koopa_raw_jump_t *kjump) {
    output << endl << "    j " << current_func_name << "_" << kjump->target->name + 1 << endl;
}

void koopa2RISCV::Visit_call(const koopa_raw_call_t *kcall, int addr) {
    output << endl;
    char reg[3] = "a0";
    
    for (int i = 0; i < kcall->args.len; ++i) {
        if (i < 8) {
            Load((koopa_raw_value_t)kcall->args.buffer[i], reg);
            reg[1] += 1;   // reg : "ai"
        }
        else {
            Load((koopa_raw_value_t)kcall->args.buffer[i], "t0");
            bool hascall;
            size_t func_size = calc_func_size(kcall->callee, hascall=false);
            func_size = func_size > 0 ? ((func_size - 1) / 16 + 1) * 16 : func_size;
            int offset = (i - 8) * 4 - func_size;
            Store(offset, "t0");
        }
    }
    output << "    call " << kcall->callee->name + 1 << endl;
    if (addr != -1)
        Store(addr, "a0");
}

void koopa2RISCV::Visit_return(const koopa_raw_return_t *kret) {
    output << endl;

    if (kret->value)
        Load(kret->value, "a0");
    int sz = env.size();
    if (env.has_call) {
        Load(sz - 4, "ra");
    }
    if (sz != 0) {
        if(sz < -2048 || sz > 2047) {
            output << "    li t0, " << sz << endl;
            output << "    add sp, sp, t0" << endl;
        }
        else
            output << "    addi sp, sp, " << sz << endl;
    }
    output << "    ret" << endl;
}


void koopa2RISCV::gen_riscv_func(koopa_raw_function_t kfunc) {
    if(kfunc->bbs.len == 0)
        return;
    const char *name = kfunc->name + 1;
    output << ".globl " << name 
    << endl
    << name << ":" << endl;

    bool has_call = false;
    int func_size = calc_func_size(kfunc, has_call);
    if(func_size != 0) {
        func_size = ((func_size - 1) / 16 + 1) * 16;
        if(-func_size < -2048 || -func_size > 2047) {
            output << "    li t0, " << -func_size << endl;
            output << "    add sp, sp, t0" << endl;
        }
        else
            output << "    addi sp, sp, " << -func_size << endl;
    }
    if(has_call) {
        int offset = func_size - 4;
        if(offset < -2048 || offset > 2047) {
            output << "    li t0, " << offset << endl;
            output << "    add t0, sp, t0" << endl;
            output << "    sw ra, 0(t0)" << endl;
        }
        else
            output << "    sw ra, " << offset << "(sp)" << endl;
    }
    env = Env((size_t)func_size, has_call);
    env.current -= has_call ? 4 : 0;
    // blocks
    current_func_name = kfunc->name + 1;
    traversal_raw_slice(&kfunc->bbs);
}

void koopa2RISCV::gen_riscv_block(koopa_raw_basic_block_t kblk)
{
    //TODO: params
    //TODO: used_by
    output << endl;
    output << current_func_name << "_" << kblk->name + 1 << ":" << endl;
    traversal_raw_slice(&kblk->insts);
}

void koopa2RISCV::gen_riscv_value(koopa_raw_value_t kval) {
    int addr = env.addr(kval);
    switch(kval->kind.tag) {
    case KOOPA_RVT_INTEGER:
        output << kval->kind.data.integer.value;
    case KOOPA_RVT_ZERO_INIT:
    case KOOPA_RVT_UNDEF:
        break;
    case KOOPA_RVT_AGGREGATE:
        Visit_aggregate(kval);
        break;
    case KOOPA_RVT_FUNC_ARG_REF:
    case KOOPA_RVT_BLOCK_ARG_REF:
    case KOOPA_RVT_ALLOC:
        break;
    case KOOPA_RVT_GLOBAL_ALLOC:
        Visit_global_alloc(kval);
        break;
    case KOOPA_RVT_LOAD:
        Visit_load(&kval->kind.data.load, addr);
        break;
    case KOOPA_RVT_STORE:
        Visit_store(&kval->kind.data.store);
        break;
    case KOOPA_RVT_GET_PTR:
        Visit_get_ptr(&kval->kind.data.get_ptr, addr);
        break;
    case KOOPA_RVT_GET_ELEM_PTR:
        Visit_get_elem_ptr(&kval->kind.data.get_elem_ptr, addr);
        break;
    case KOOPA_RVT_BINARY:
        Visit_binary(&kval->kind.data.binary, addr);
        break;
    case KOOPA_RVT_BRANCH:
        Visit_branch(&kval->kind.data.branch);
        break;
    case KOOPA_RVT_JUMP:
        Visit_jump(&kval->kind.data.jump);
        break;
    case KOOPA_RVT_CALL:
        Visit_call(&kval->kind.data.call, kval->ty->tag == KOOPA_RTT_UNIT ? -1 : addr);
        break;
    case KOOPA_RVT_RETURN:
        Visit_return(&kval->kind.data.ret);
        break;
    }
}

void koopa2RISCV::traversal_raw_slice(const koopa_raw_slice_t *rs) {
    for (uint32_t i = 0; i < rs->len; ++i) {
        const void *data = rs->buffer[i];
        switch (rs->kind) {
            default: break;
            case KOOPA_RSIK_FUNCTION:
                gen_riscv_func((koopa_raw_function_t)data);
                break;
            case KOOPA_RSIK_BASIC_BLOCK:
                gen_riscv_block((koopa_raw_basic_block_t)data);
                break;
            case KOOPA_RSIK_VALUE:
                gen_riscv_value((koopa_raw_value_t)data);
                // break;
        }
    }
}

void koopa2RISCV::build(const koopa_raw_program_t *raw) {
    output << ".data" << endl;
    traversal_raw_slice(&raw->values);
    output << ".text" << endl;
    traversal_raw_slice(&raw->funcs);
}


// static size_t函数，通过basic blocks计算函数的大小，has_call表示函数是否有函数调用，这样会在原本基础上加4保存返回地址
size_t koopa2RISCV::calc_func_size(koopa_raw_function_t kfunc, bool &has_call) {
    size_t size{0};
    uint32_t len = kfunc->bbs.len;
    for (uint32_t i = 0; i < len; ++i) {
        koopa_raw_basic_block_t data = (koopa_raw_basic_block_t)kfunc->bbs.buffer[i];
        size += calc_blk_size(data, has_call);
    }
    size += has_call ? 4 : 0;
    return size;
}
// static size_t函数，通过instructions计算basic block的大小，传入的has_call参数可能会在过程中被设为true.
size_t koopa2RISCV::calc_blk_size(koopa_raw_basic_block_t kblk, bool &has_call) {
    // TODO: did not count params 
    // 函数参数必须对齐到栈顶，其余栈帧内数据的排列方式, 比如顺序, 或者对齐到栈顶还是栈底，RISC-V没有明确规定
    size_t size{0};
    uint32_t len = kblk->insts.len;
    for (uint32_t i = 0; i < len; ++i) {
        koopa_raw_value_t data = (koopa_raw_value_t)kblk->insts.buffer[i];
        if(data->kind.tag == KOOPA_RVT_CALL)
            has_call = true;
        size += calc_inst_size(data);
    }
    return size;
}
// static size_t函数，通过allcate次数计算instruction的大小。
size_t koopa2RISCV::calc_inst_size(koopa_raw_value_t kval) {
    if(kval->kind.tag == KOOPA_RVT_ALLOC)
        return calc_type_size(kval->ty->data.pointer.base);
    return calc_type_size(kval->ty);
}
// static size_t函数，通过类型计算allcate的大小。如果是pointer或者int，则是4；若是array，则乘上array长度；若是unit或者其余，设成零。
size_t koopa2RISCV::calc_type_size(koopa_raw_type_t ty) {
    switch(ty->tag) {
        case KOOPA_RTT_ARRAY:
            return calc_type_size(ty->data.array.base) * ty->data.array.len;
        case KOOPA_RTT_POINTER:
//            return 4;
        case KOOPA_RTT_INT32:
            return 4;
        case KOOPA_RTT_UNIT:
//            return 0;
        default:// Some error here.
            return 0;
    }
}

