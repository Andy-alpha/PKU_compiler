#ifndef BLOCK_H
#define BLOCK_H

#include <map>
#include <string>
#include <vector>
#include <memory>

#include "utils/koopa_util.hpp"
// This part was intended to maintain large blocks, which repeat lots of times.
class Block {
    koopa_raw_function_t current_func; // Record current function we are in.
    std::vector<const void *> current_insts_buf; // Pointer to current instructions.
    std::vector<const void *> *basic_block_buf; // Pointer to pointer to basic block buffers.

public:
    Block() = default;
    ~Block() = default;
    void SetCurrentFunction(koopa_raw_function_t _cur_func) {
        // Just like Constructor
        current_func = _cur_func;
    }
    void SetBasicBlockBuf(std::vector<const void *> *_basic_block_buf) {
        basic_block_buf = _basic_block_buf;
    }
    void FinishCurrentBlock() {
        if (basic_block_buf->size() == 0) {  // if so, clear the buffer and return directly.
            current_insts_buf.clear();
            return;
        }
        koopa_raw_basic_block_data_t *last_block = (koopa_raw_basic_block_data_t *)(*basic_block_buf)[basic_block_buf->size() - 1];
        bool found = false;
        // Find if there are BRANCH/RETURN/JUMP instructions in current buffers.
        // They are end of basic blocks, so resize them.
        size_t len = current_insts_buf.size();
        for (size_t i = 0; i < len; ++i) {
            koopa_raw_value_t t = (koopa_raw_value_t)current_insts_buf[i];
            switch (t->kind.tag) {
                case KOOPA_RVT_BRANCH:
                case KOOPA_RVT_RETURN:
                case KOOPA_RVT_JUMP:
                    current_insts_buf.resize(i + 1);
                    found = true;
                default:
                break;
            }
            if (found) break;
        }
        if(!found) { // Not found
            koopa_raw_value_data *ret = Init(simple_koopa_raw_type_kind(KOOPA_RTT_UNIT),
                nullptr,
                empty_koopa_raw_slice(KOOPA_RSIK_VALUE),
                make_koopa_raw_value_kind(KOOPA_RVT_RETURN)
            );
            // Add return value manually. If raw_type_tag == UNIT -> NULL; else return value -> 0.
            auto &value = ret->kind.data.ret.value;
            if(current_func->ty->data.function.ret->tag == KOOPA_RTT_UNIT)
                value = nullptr;
            else
                value = make_koopa_interger(0);
            current_insts_buf.push_back(ret);
        }
        if (last_block->insts.buffer == nullptr)
            last_block->insts = make_koopa_raw_slice(current_insts_buf, KOOPA_RSIK_VALUE);
            // make koopa raw slice from vector
        
        current_insts_buf.clear();
    }
    void Push_back(koopa_raw_basic_block_data_t *basic_block) {
        FinishCurrentBlock();
        basic_block->insts.buffer = nullptr;
        basic_block_buf->push_back(basic_block);
    }
    void Push_back(const void *inst) {
        current_insts_buf.push_back(inst);
    }
};
#endif