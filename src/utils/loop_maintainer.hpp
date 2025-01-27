#ifndef LOOP_MAINTAINER_H
#define LOOP_MAINTAINER_H

#include <string>
#include <vector>
#include <memory>

#include "utils/koopa_util.hpp"

struct KoopaWhile {
    koopa_raw_basic_block_data_t *while_entry;
    koopa_raw_basic_block_data_t *while_body;
    koopa_raw_basic_block_data_t *end_block;
};

class LoopMaintainer {
    std::vector<KoopaWhile> loop_stk;

public:
    void AddLoop(koopa_raw_basic_block_data_t *while_entry, koopa_raw_basic_block_data_t *while_body, koopa_raw_basic_block_data_t *end_block) {
        KoopaWhile kw;
        kw.while_entry = while_entry;
        kw.while_body = while_body;
        kw.end_block = end_block;
        loop_stk.push_back(kw);
    }
    KoopaWhile GetLoop() {
        return loop_stk.back();
    }
    void PopLoop() {
        loop_stk.pop_back();
    }
};
#endif