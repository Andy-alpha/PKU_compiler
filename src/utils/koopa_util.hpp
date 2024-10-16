#ifndef KOOPA_UTIL_H
#define KOOPA_UTIL_H

#include <string>
#include <vector>
#include <koopa.h>

koopa_raw_slice_t empty_koopa_raw_slice(koopa_raw_slice_item_kind_t kind = KOOPA_RSIK_UNKNOWN);
koopa_raw_slice_t make_koopa_raw_slice(const std::vector<const void*> &vec, koopa_raw_slice_item_kind_t kind = KOOPA_RSIK_UNKNOWN); // vector
koopa_raw_slice_t make_koopa_raw_slice(const void *element, koopa_raw_slice_item_kind_t kind = KOOPA_RSIK_UNKNOWN); // single element
koopa_raw_slice_t add_element_to_koopa_raw_slice(koopa_raw_slice_t origin, const void *element);

koopa_raw_type_kind* make_array_type(const std::vector<int> &sz, int st_pos = 0);
koopa_raw_type_kind* simple_koopa_raw_type_kind(koopa_raw_type_tag_t tag);
koopa_raw_type_kind* make_int_pointer_type();
koopa_raw_type_kind* Init(koopa_raw_type_kind_t ty,
                            koopa_raw_type_tag_t _tag,
                            const struct koopa_raw_type_kind *_base = nullptr,
                            size_t _len = 0/*,
                            koopa_raw_slice_t _params*/
                            );
koopa_raw_value_kind_t make_koopa_raw_value_kind(koopa_raw_value_tag_t _tag,
                                                uint32_t _integer = 0,
                                                /*int _integer1 = 0,
                                                const void **_buffer = nullptr,*/
                                                koopa_raw_value_t krv1 = nullptr,
                                                koopa_raw_value_t krv2 = nullptr/*,
                                                koopa_raw_slice_t rs*/);

koopa_raw_value_data *make_koopa_interger(int x);
koopa_raw_value_data *JumpInst(koopa_raw_basic_block_t target);
koopa_raw_value_data *AllocIntInst(const std::string &name);
koopa_raw_value_data *AllocType(const std::string &name, koopa_raw_type_t ty);
koopa_raw_value_data *ZeroInit(koopa_raw_type_kind *_type = nullptr);
koopa_raw_value_data *Init(//koopa_raw_value_data* res,
                            koopa_raw_type_t _ty,
                            const char *_name,
                            koopa_raw_slice_t _used_by,
                            koopa_raw_value_kind_t _kind);

char *new_char_arr(std::string str);
#endif