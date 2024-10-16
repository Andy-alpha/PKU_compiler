#include "utils/koopa_util.hpp"

#include <cassert>
#include <cstring>

/// Parameter should be a kind (`koopa_raw_slice_item_kind_t`).
///
/// Returns `koopa_raw_slice_t` with nullptr as buffer and len=0.
koopa_raw_slice_t empty_koopa_raw_slice(koopa_raw_slice_item_kind_t kind) {
    koopa_raw_slice_t res;
    res.buffer = nullptr;
    res.kind = kind;
    res.len = 0;
    return res;
}

/// Parameters should be a vector<const void*> and a kind.
///
/// Returns `koopa_raw_slice_t` with the given vector as buffer and len=vec.size().
koopa_raw_slice_t make_koopa_raw_slice(const std::vector<const void *> &vec, koopa_raw_slice_item_kind_t kind) {
    koopa_raw_slice_t res;
    res.buffer = new const void *[vec.size()];
    std::copy(vec.begin(), vec.end(), res.buffer);
    res.kind = kind;
    res.len = vec.size();
    return res;
}

/// Parameters should be a single element(const void*) and a kind.
///
/// Returns `koopa_raw_slice_t` with the given element as buffer and len=vec.size().
koopa_raw_slice_t make_koopa_raw_slice(const void *element, koopa_raw_slice_item_kind_t kind) {
    // single element
    koopa_raw_slice_t res;
    res.buffer = new const void *[1];
    res.buffer[0] = element;
    res.kind = kind;
    res.len = 1;
    return res;
}

/// Parameters should be the orignal koopa raw slice and a single element(const void*).
///
/// Returns `koopa_raw_slice_t` which concatenates the origin koopa raw slice with the element.
koopa_raw_slice_t add_element_to_koopa_raw_slice(koopa_raw_slice_t origin, const void *element) {

    koopa_raw_slice_t res;
    res.buffer = new const void *[origin.len + 1];
    memcpy(res.buffer, origin.buffer, sizeof(void *) * origin.len);
    res.buffer[origin.len] = element;
    res.len = origin.len + 1;
    res.kind = origin.kind;
    delete origin.buffer;

    return res;
}

/// Parameters should be a vector<int> sz (whose value will not be modified) and int st_pos(starting position).
///
/// Return a manmade `koopa_raw_type_kind*` begin with st_pos and end with the origin sz.end().
///
/// It's a linked list. Actually res = ty_list[0], and ty_list[i]->data.array.base = ty_list[i+1].
koopa_raw_type_kind* make_array_type(const std::vector<int> &sz, int st_pos) {
    std::vector<koopa_raw_type_kind*> ty_list;
    for(size_t i = st_pos; i < sz.size(); ++i) {
        koopa_raw_type_kind *new_rt = new koopa_raw_type_kind();
        new_rt->tag = KOOPA_RTT_ARRAY;
        new_rt->data.array.len = sz[i];
        ty_list.push_back(new_rt);
    }
    size_t type_list_size = ty_list.size();
    ty_list[type_list_size - 1]->data.array.base = simple_koopa_raw_type_kind(KOOPA_RTT_INT32);
    for(size_t i = 0; i < type_list_size - 1; ++i)
        ty_list[i]->data.array.base = ty_list[i + 1];
    return ty_list[0];
}

/// Parameters should be tag `KOOPA_RTT_INT32` or `KOOPA_RTT_UINT`.
///
/// Returns `koopa_raw_type_kind` with either given tag.
koopa_raw_type_kind *simple_koopa_raw_type_kind(koopa_raw_type_tag_t tag) {

    assert(tag == KOOPA_RTT_INT32 || tag == KOOPA_RTT_UNIT);
    koopa_raw_type_kind *res = new koopa_raw_type_kind();
    res->tag = tag;
    return res;
}

/// Tag is set to `KOOPA_RTT_POINTER`.
///
/// Returns `koopa_raw_type_kind*` res, and res->data.pointer.base->tag = `KOOPA_RTT_INT32`.
koopa_raw_type_kind* make_int_pointer_type() {

    koopa_raw_type_kind *res = new koopa_raw_type_kind();
    res->tag = KOOPA_RTT_POINTER;
    res->data.pointer.base = simple_koopa_raw_type_kind(KOOPA_RTT_INT32);
    return res;
}

//
// Initialize koopa raw type kind.
//
//typedef struct koopa_raw_type_kind {
//  koopa_raw_type_tag_t tag;
//  union {
//    struct {
//      const struct koopa_raw_type_kind *base;
//      size_t len;
//    } array;
//    struct {
//      const struct koopa_raw_type_kind *base;
//    } pointer;
//    struct {
//      koopa_raw_slice_t params;
//      const struct koopa_raw_type_kind *ret;
//    } function;
//  } data;
//} koopa_raw_type_kind_t;
koopa_raw_type_kind* Init(koopa_raw_type_tag_t _tag,
                            const struct koopa_raw_type_kind *_base,
                            size_t _len/*,
                            koopa_raw_slice_t _params*/ 
                            )
{
    assert(_tag == KOOPA_RTT_INT32 || _tag == KOOPA_RTT_UNIT || _tag == KOOPA_RTT_ARRAY || _tag == KOOPA_RTT_POINTER || _tag == KOOPA_RTT_FUNCTION);
    koopa_raw_type_kind *ty = new koopa_raw_type_kind();
    switch (_tag) {
        case KOOPA_RTT_INT32:
            break;
        case KOOPA_RTT_UNIT:
            break;
        case KOOPA_RTT_ARRAY:
            break;
        case KOOPA_RTT_POINTER:
            break;
        case KOOPA_RTT_FUNCTION:
            break;
    }
    return ty;
}
// 
// Make koopa raw value kind.
//
// integer0 for regular argument, integer1 for additional information.
//
// krv1 and krv2 are of the same pattern.
koopa_raw_value_kind_t make_koopa_raw_value_kind(koopa_raw_value_tag_t _tag,
                                                uint32_t _integer,
                                                /*int _integer1,
                                                const void **_buffer,*/
                                                koopa_raw_value_t krv1,
                                                koopa_raw_value_t krv2/*,
                                                koopa_raw_slice_t rs = nullptr*/)
{
    koopa_raw_value_kind_t kind;
    kind.tag = _tag;
    switch (_tag) {
        /// Integer constant.
        case KOOPA_RVT_INTEGER:
        /// Zero initializer.
        case KOOPA_RVT_ZERO_INIT:
        /// Undefined value.
        case KOOPA_RVT_UNDEF:
            kind.data.integer.value = (int32_t)_integer;
            break;
        /// Aggregate constant.
        case KOOPA_RVT_AGGREGATE: {
            //koopa_raw_slice_t rs;
            //kind.data.aggregate.elems.buffer = _buffer;
            //kind.data.aggregate.elems.len = (uint32_t)_integer;
            //koopa_raw_slice_item_kind _enum = (koopa_raw_slice_item_kind)_integer1;
            //kind.data.aggregate.elems.kind = _enum;
            break;
        }
        /// Function argument reference.
        case KOOPA_RVT_FUNC_ARG_REF:
            kind.data.func_arg_ref.index = (size_t)_integer;
            break;
        /// Basic block argument reference.
        case KOOPA_RVT_BLOCK_ARG_REF:
            kind.data.block_arg_ref.index = (size_t)_integer;
            break;
        /// Local memory allocation.
        case KOOPA_RVT_ALLOC:
            kind.data.ret.value = krv1;
        /// Global memory allocation.
        case KOOPA_RVT_GLOBAL_ALLOC:
            kind.data.global_alloc.init = krv1;
            break;
        /// Memory load.
        case KOOPA_RVT_LOAD:
//            kind.data.global_alloc.init = (koopa_raw_value_t)Init(_tag, );
            kind.data.load.src = krv1;
            break;
        /// Memory store.
        case KOOPA_RVT_STORE: {
            kind.data.store.value = krv1;
            kind.data.store.dest = krv2;
            break;
        }
        /// Pointer calculation.
        case KOOPA_RVT_GET_PTR: {
            kind.data.get_ptr.src = krv1;
            kind.data.get_ptr.index = krv2;
            break;
        }
        /// Element pointer calculation.
        case KOOPA_RVT_GET_ELEM_PTR: {
            kind.data.get_elem_ptr.src = krv1;
            kind.data.get_elem_ptr.index = krv2;
            break;
        }
        /// Binary operation.
        case KOOPA_RVT_BINARY: {
            koopa_raw_binary_op_t _op = (koopa_raw_binary_op_t)_integer;
            kind.data.binary.op = _op;
            kind.data.binary.lhs = krv1;
            kind.data.binary.rhs = krv2;
            break;
        }
        /// Conditional branch.
        case KOOPA_RVT_BRANCH:

            break;
        /// Unconditional jump.
        case KOOPA_RVT_JUMP:

            break;
        /// Function call.
        case KOOPA_RVT_CALL:

            break;
        /// Function return.
        case KOOPA_RVT_RETURN:
            break;
    }
    return kind;
}

/// 
/// As it is.
///
koopa_raw_value_data *make_koopa_interger(int x) {

    koopa_raw_value_data *res = new koopa_raw_value_data();
    res->ty = simple_koopa_raw_type_kind(KOOPA_RTT_INT32);
    res->name = nullptr;
    res->used_by = empty_koopa_raw_slice(KOOPA_RSIK_VALUE);
    res->kind.tag = KOOPA_RVT_INTEGER;
    res->kind.data.integer.value = x;
    return res;
}

/// 
/// Parameter is a target (`koopa_raw_basic_block_t`).
///
/// Return an intrustion about 'jump' (`koopa_raw_value_data`).
koopa_raw_value_data *JumpInst(koopa_raw_basic_block_t target) {

    koopa_raw_value_data *res = new koopa_raw_value_data();
    res->ty = simple_koopa_raw_type_kind(KOOPA_RTT_UNIT);
    res->name = nullptr;
    res->used_by = empty_koopa_raw_slice(KOOPA_RSIK_VALUE);
    res->kind.tag = KOOPA_RVT_JUMP;
    res->kind.data.jump.args = empty_koopa_raw_slice(KOOPA_RSIK_VALUE);
    res->kind.data.jump.target = target;
    return res;
}

/// 
/// Parameter is a string.
///
/// Return an intrustion about 'alloc(int)' (`koopa_raw_value_data`).
koopa_raw_value_data *AllocIntInst(const std::string &name) {

    koopa_raw_value_data *res = new koopa_raw_value_data();
    res->ty = make_int_pointer_type();
    res->name = new_char_arr(name);
    res->used_by = empty_koopa_raw_slice(KOOPA_RSIK_VALUE);
    res->kind.tag = KOOPA_RVT_ALLOC;
    return res;
}

/// 
/// Parameters should be a string and a type (`koopa_raw_type_t`)
///
/// Return an intrustion about 'alloc(type)' (`koopa_raw_value_data`).
koopa_raw_value_data *AllocType(const std::string &name, koopa_raw_type_t ty) {

    koopa_raw_value_data *res = new koopa_raw_value_data();
    koopa_raw_type_kind *tty = new koopa_raw_type_kind();
    tty->tag = KOOPA_RTT_POINTER;
    tty->data.pointer.base = ty;
    res->ty = tty;
    res->name = new_char_arr(name);
    res->used_by = empty_koopa_raw_slice(KOOPA_RSIK_VALUE);
    res->kind.tag = KOOPA_RVT_ALLOC;
    return res;
}

/// 
/// Parameter is a type (koopa_raw_type_kind*).
///
/// Return an empty structure with res->ty = _type (or 'int'), res->kind.tag = `KOOPA_RVT_ZERO_INIT`.
koopa_raw_value_data *ZeroInit(koopa_raw_type_kind *_type) {

    koopa_raw_value_data *res = new koopa_raw_value_data();
    if(_type)
        res->ty = _type;
    else
        res->ty = simple_koopa_raw_type_kind(KOOPA_RTT_INT32);
    res->name = nullptr;
    res->used_by = empty_koopa_raw_slice(KOOPA_RSIK_VALUE);
    res->kind.tag = KOOPA_RVT_ZERO_INIT;
    return res;
}
// Initialize koopa ra value data.
//struct koopa_raw_value_data {
  /// Type of value.
//  koopa_raw_type_t ty;
  /// Name of value, null if no name.
//  const char *name;
  /// Values that this value is used by.
//  koopa_raw_slice_t used_by;
  /// Kind of value.
//  koopa_raw_value_kind_t kind;
//}
koopa_raw_value_data *Init(koopa_raw_type_t _ty,
                            const char *_name,
                            koopa_raw_slice_t _used_by,
                            koopa_raw_value_kind_t _kind)
{
    koopa_raw_value_data *res = new koopa_raw_value_data();
    res->ty = _ty;
    res->name = _name;
    res->used_by = _used_by;
    res->kind = _kind;

    return res;
}