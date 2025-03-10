/*
 * This file is part of the Micro Python project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2013, 2014 Damien P. George
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

// A Micro Python object is a machine word having the following form:
//  - xxxx...xxx1 : a small int, bits 1 and above are the value
//  - xxxx...xx10 : a qstr, bits 2 and above are the value
//  - xxxx...xx00 : a pointer to an mp_obj_base_t (unless a fake object)

// All Micro Python objects are at least this type
// It must be of pointer size

typedef machine_ptr_t mp_obj_t;
typedef machine_const_ptr_t mp_const_obj_t;

// Anything that wants to be a Micro Python object must have
// mp_obj_base_t as its first member (except small ints and qstrs)

struct _mp_obj_type_t;
struct _mp_obj_base_t {
    const struct _mp_obj_type_t *type;
};
typedef struct _mp_obj_base_t mp_obj_base_t;

// These fake objects are used to indicate certain things in arguments or return
// values, and should only be used when explicitly allowed.
//
//  - MP_OBJ_NULL : used to indicate the absence of an object, or unsupported operation.
//  - MP_OBJ_STOP_ITERATION : used instead of throwing a StopIteration, for efficiency.
//  - MP_OBJ_SENTINEL : used for various internal purposes where one needs
//    an object which is unique from all other objects, including MP_OBJ_NULL.
//
// For debugging purposes they are all different.  For non-debug mode, we alias
// as many as we can to MP_OBJ_NULL because it's cheaper to load/compare 0.

#if NDEBUG
#define MP_OBJ_NULL             ((mp_obj_t)0)
#define MP_OBJ_STOP_ITERATION   ((mp_obj_t)0)
#define MP_OBJ_SENTINEL         ((mp_obj_t)4)
#else
#define MP_OBJ_NULL             ((mp_obj_t)0)
#define MP_OBJ_STOP_ITERATION   ((mp_obj_t)4)
#define MP_OBJ_SENTINEL         ((mp_obj_t)8)
#endif

// These macros check for small int, qstr or object, and access small int and qstr values

// these macros have now become inline functions; see below
//#define MP_OBJ_IS_SMALL_INT(o) ((((mp_int_t)(o)) & 1) != 0)
//#define MP_OBJ_IS_QSTR(o) ((((mp_int_t)(o)) & 3) == 2)
//#define MP_OBJ_IS_OBJ(o) ((((mp_int_t)(o)) & 3) == 0)
#define MP_OBJ_IS_TYPE(o, t) (MP_OBJ_IS_OBJ(o) && (((mp_obj_base_t*)(o))->type == (t))) // this does not work for checking a string, use below macro for that
#define MP_OBJ_IS_INT(o) (MP_OBJ_IS_SMALL_INT(o) || MP_OBJ_IS_TYPE(o, &mp_type_int))
#define MP_OBJ_IS_STR(o) (MP_OBJ_IS_QSTR(o) || MP_OBJ_IS_TYPE(o, &mp_type_str))

#define MP_OBJ_SMALL_INT_VALUE(o) (((mp_int_t)(o)) >> 1)
#define MP_OBJ_NEW_SMALL_INT(small_int) ((mp_obj_t)(((small_int) << 1) | 1))

#define MP_OBJ_QSTR_VALUE(o) (((mp_int_t)(o)) >> 2)
#define MP_OBJ_NEW_QSTR(qstr) ((mp_obj_t)((((mp_uint_t)qstr) << 2) | 2))

// These macros are used to declare and define constant function objects
// You can put "static" in front of the definitions to make them local

#define MP_DECLARE_CONST_FUN_OBJ(obj_name) extern const mp_obj_fun_native_t obj_name

#define MP_DEFINE_CONST_FUN_OBJ_VOID_PTR(obj_name, is_kw, n_args_min, n_args_max, fun_name) const mp_obj_fun_native_t obj_name = {{&mp_type_fun_native}, is_kw, n_args_min, n_args_max, (void *)fun_name}
#define MP_DEFINE_CONST_FUN_OBJ_0(obj_name, fun_name) MP_DEFINE_CONST_FUN_OBJ_VOID_PTR(obj_name, false, 0, 0, (mp_fun_0_t)fun_name)
#define MP_DEFINE_CONST_FUN_OBJ_1(obj_name, fun_name) MP_DEFINE_CONST_FUN_OBJ_VOID_PTR(obj_name, false, 1, 1, (mp_fun_1_t)fun_name)
#define MP_DEFINE_CONST_FUN_OBJ_2(obj_name, fun_name) MP_DEFINE_CONST_FUN_OBJ_VOID_PTR(obj_name, false, 2, 2, (mp_fun_2_t)fun_name)
#define MP_DEFINE_CONST_FUN_OBJ_3(obj_name, fun_name) MP_DEFINE_CONST_FUN_OBJ_VOID_PTR(obj_name, false, 3, 3, (mp_fun_3_t)fun_name)
#define MP_DEFINE_CONST_FUN_OBJ_VAR(obj_name, n_args_min, fun_name) MP_DEFINE_CONST_FUN_OBJ_VOID_PTR(obj_name, false, n_args_min, MP_OBJ_FUN_ARGS_MAX, (mp_fun_var_t)fun_name)
#define MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(obj_name, n_args_min, n_args_max, fun_name) MP_DEFINE_CONST_FUN_OBJ_VOID_PTR(obj_name, false, n_args_min, n_args_max, (mp_fun_var_t)fun_name)
#define MP_DEFINE_CONST_FUN_OBJ_KW(obj_name, n_args_min, fun_name) MP_DEFINE_CONST_FUN_OBJ_VOID_PTR(obj_name, true, n_args_min, MP_OBJ_FUN_ARGS_MAX, (mp_fun_kw_t)fun_name)

// This macro is used to define constant dict objects
// You can put "static" in front of the definition to make it local

#define MP_DEFINE_CONST_DICT(dict_name, table_name) \
    const mp_obj_dict_t dict_name = { \
        .base = {&mp_type_dict}, \
        .map = { \
            .all_keys_are_qstrs = 1, \
            .table_is_fixed_array = 1, \
            .used = sizeof(table_name) / sizeof(mp_map_elem_t), \
            .alloc = sizeof(table_name) / sizeof(mp_map_elem_t), \
            .table = (mp_map_elem_t*)table_name, \
        }, \
    }

// These macros are used to declare and define constant staticmethond and classmethod objects
// You can put "static" in front of the definitions to make them local

#define MP_DECLARE_CONST_STATICMETHOD_OBJ(obj_name) extern const mp_obj_static_class_method_t obj_name
#define MP_DECLARE_CONST_CLASSMETHOD_OBJ(obj_name) extern const mp_obj_static_class_method_t obj_name

#define MP_DEFINE_CONST_STATICMETHOD_OBJ(obj_name, fun_name) const mp_obj_static_class_method_t obj_name = {{&mp_type_staticmethod}, fun_name}
#define MP_DEFINE_CONST_CLASSMETHOD_OBJ(obj_name, fun_name) const mp_obj_static_class_method_t obj_name = {{&mp_type_classmethod}, fun_name}

// Underlying map/hash table implementation (not dict object or map function)

typedef struct _mp_map_elem_t {
    mp_obj_t key;
    mp_obj_t value;
} mp_map_elem_t;

// TODO maybe have a truncated mp_map_t for fixed tables, since alloc=used
// put alloc last in the structure, so the truncated version does not need it
// this would save 1 ROM word for all ROM objects that have a locals_dict
// would also need a trucated dict structure

typedef struct _mp_map_t {
    mp_uint_t all_keys_are_qstrs : 1;
    mp_uint_t table_is_fixed_array : 1;
    mp_uint_t used : (8 * sizeof(mp_uint_t) - 2);
    mp_uint_t alloc;
    mp_map_elem_t *table;
} mp_map_t;

// These can be or'd together
typedef enum _mp_map_lookup_kind_t {
    MP_MAP_LOOKUP,                    // 0
    MP_MAP_LOOKUP_ADD_IF_NOT_FOUND,   // 1
    MP_MAP_LOOKUP_REMOVE_IF_FOUND,    // 2
} mp_map_lookup_kind_t;

static inline bool MP_MAP_SLOT_IS_FILLED(const mp_map_t *map, mp_uint_t pos) { return ((map)->table[pos].key != MP_OBJ_NULL && (map)->table[pos].key != MP_OBJ_SENTINEL); }

void mp_map_init(mp_map_t *map, int n);
void mp_map_init_fixed_table(mp_map_t *map, int n, const mp_obj_t *table);
mp_map_t *mp_map_new(int n);
void mp_map_deinit(mp_map_t *map);
void mp_map_free(mp_map_t *map);
mp_map_elem_t* mp_map_lookup(mp_map_t *map, mp_obj_t index, mp_map_lookup_kind_t lookup_kind);
void mp_map_clear(mp_map_t *map);
void mp_map_dump(mp_map_t *map);

// Underlying set implementation (not set object)

typedef struct _mp_set_t {
    mp_uint_t alloc;
    mp_uint_t used;
    mp_obj_t *table;
} mp_set_t;

static inline bool MP_SET_SLOT_IS_FILLED(const mp_set_t *set, mp_uint_t pos) { return ((set)->table[pos] != MP_OBJ_NULL && (set)->table[pos] != MP_OBJ_SENTINEL); }

void mp_set_init(mp_set_t *set, int n);
mp_obj_t mp_set_lookup(mp_set_t *set, mp_obj_t index, mp_map_lookup_kind_t lookup_kind);
mp_obj_t mp_set_remove_first(mp_set_t *set);
void mp_set_clear(mp_set_t *set);

// Type definitions for methods

typedef mp_obj_t (*mp_fun_0_t)(void);
typedef mp_obj_t (*mp_fun_1_t)(mp_obj_t);
typedef mp_obj_t (*mp_fun_2_t)(mp_obj_t, mp_obj_t);
typedef mp_obj_t (*mp_fun_3_t)(mp_obj_t, mp_obj_t, mp_obj_t);
typedef mp_obj_t (*mp_fun_t)(void);
typedef mp_obj_t (*mp_fun_var_t)(uint n, const mp_obj_t *);
typedef mp_obj_t (*mp_fun_kw_t)(uint n, const mp_obj_t *, mp_map_t *);

typedef enum {
    PRINT_STR = 0,
    PRINT_REPR = 1,
    PRINT_EXC = 2, // Special format for printing exception in unhandled exception message
    PRINT_EXC_SUBCLASS = 4, // Internal flag for printing exception subclasses
} mp_print_kind_t;

typedef void (*mp_print_fun_t)(void (*print)(void *env, const char *fmt, ...), void *env, mp_obj_t o, mp_print_kind_t kind);
typedef mp_obj_t (*mp_make_new_fun_t)(mp_obj_t type_in, uint n_args, uint n_kw, const mp_obj_t *args);
typedef mp_obj_t (*mp_call_fun_t)(mp_obj_t fun, uint n_args, uint n_kw, const mp_obj_t *args);
typedef mp_obj_t (*mp_unary_op_fun_t)(int op, mp_obj_t);
typedef mp_obj_t (*mp_binary_op_fun_t)(int op, mp_obj_t, mp_obj_t);
typedef void (*mp_load_attr_fun_t)(mp_obj_t self_in, qstr attr, mp_obj_t *dest); // for fail, do nothing; for attr, dest[0] = value; for method, dest[0] = method, dest[1] = self
typedef bool (*mp_store_attr_fun_t)(mp_obj_t self_in, qstr attr, mp_obj_t value); // return true if store succeeded; if value==MP_OBJ_NULL then delete
typedef mp_obj_t (*mp_subscr_fun_t)(mp_obj_t self_in, mp_obj_t index, mp_obj_t value);

typedef struct _mp_method_t {
    qstr name;
    mp_const_obj_t fun;
} mp_method_t;

// Buffer protocol
typedef struct _mp_buffer_info_t {
    // if we'd bother to support various versions of structure
    // (with different number of fields), we can distinguish
    // them with ver = sizeof(struct). Cons: overkill for *micro*?
    //int ver; // ?

    void *buf;
    mp_int_t len; // in bytes
    int typecode; // as per binary.h

    // Rationale: to load arbitrary-sized sprites directly to LCD
    // Cons: a bit adhoc usecase
    // int stride;
} mp_buffer_info_t;
#define MP_BUFFER_READ  (1)
#define MP_BUFFER_WRITE (2)
#define MP_BUFFER_RW (MP_BUFFER_READ | MP_BUFFER_WRITE)
typedef struct _mp_buffer_p_t {
    mp_int_t (*get_buffer)(mp_obj_t obj, mp_buffer_info_t *bufinfo, int flags);
} mp_buffer_p_t;
bool mp_get_buffer(mp_obj_t obj, mp_buffer_info_t *bufinfo, int flags);
void mp_get_buffer_raise(mp_obj_t obj, mp_buffer_info_t *bufinfo, int flags);

// Stream protocol
typedef struct _mp_stream_p_t {
    // On error, functions should return -1 and fill in *errcode (values are
    // implementation-dependent, but will be exposed to user, e.g. via exception).
    mp_int_t (*read)(mp_obj_t obj, void *buf, mp_uint_t size, int *errcode);
    mp_int_t (*write)(mp_obj_t obj, const void *buf, mp_uint_t size, int *errcode);
    // add seek() ?
    int is_bytes : 1;
} mp_stream_p_t;

struct _mp_obj_type_t {
    mp_obj_base_t base;
    qstr name;
    mp_print_fun_t print;
    mp_make_new_fun_t make_new;     // to make an instance of the type

    mp_call_fun_t call;
    mp_unary_op_fun_t unary_op;     // can return MP_OBJ_NULL if op not supported
    mp_binary_op_fun_t binary_op;   // can return MP_OBJ_NULL if op not supported

    mp_load_attr_fun_t load_attr;
    mp_store_attr_fun_t store_attr; // if value is MP_OBJ_NULL, then delete that attribute

    mp_subscr_fun_t subscr;         // implements load, store, delete subscripting
                                    // value=MP_OBJ_NULL means delete, value=MP_OBJ_SENTINEL means load, else store
                                    // can return MP_OBJ_NULL if op not supported

    mp_fun_1_t getiter;
    mp_fun_1_t iternext; // may return MP_OBJ_STOP_ITERATION as an optimisation instead of raising StopIteration() (with no args)

    mp_buffer_p_t buffer_p;
    const mp_stream_p_t *stream_p;

    // these are for dynamically created types (classes)
    mp_obj_t bases_tuple;
    mp_obj_t locals_dict;

    /*
    What we might need to add here:

    len             str tuple list map
    abs             float complex
    hash            bool int none str
    equal           int str

    unpack seq      list tuple
    */
};

typedef struct _mp_obj_type_t mp_obj_type_t;

// Constant types, globally accessible
extern const mp_obj_type_t mp_type_type;
extern const mp_obj_type_t mp_type_object;
extern const mp_obj_type_t mp_type_NoneType;
extern const mp_obj_type_t mp_type_bool;
extern const mp_obj_type_t mp_type_int;
extern const mp_obj_type_t mp_type_str;
extern const mp_obj_type_t mp_type_bytes;
extern const mp_obj_type_t mp_type_bytearray;
extern const mp_obj_type_t mp_type_float;
extern const mp_obj_type_t mp_type_complex;
extern const mp_obj_type_t mp_type_tuple;
extern const mp_obj_type_t mp_type_list;
extern const mp_obj_type_t mp_type_map; // map (the python builtin, not the dict implementation detail)
extern const mp_obj_type_t mp_type_enumerate;
extern const mp_obj_type_t mp_type_filter;
extern const mp_obj_type_t mp_type_dict;
extern const mp_obj_type_t mp_type_range;
extern const mp_obj_type_t mp_type_set;
extern const mp_obj_type_t mp_type_frozenset;
extern const mp_obj_type_t mp_type_slice;
extern const mp_obj_type_t mp_type_zip;
extern const mp_obj_type_t mp_type_array;
extern const mp_obj_type_t mp_type_super;
extern const mp_obj_type_t mp_type_gen_instance;
extern const mp_obj_type_t mp_type_fun_native;
extern const mp_obj_type_t mp_type_fun_bc;
extern const mp_obj_type_t mp_type_module;
extern const mp_obj_type_t mp_type_staticmethod;
extern const mp_obj_type_t mp_type_classmethod;
extern const mp_obj_type_t mp_type_property;
extern const mp_obj_type_t mp_type_stringio;
extern const mp_obj_type_t mp_type_bytesio;

// Exceptions
extern const mp_obj_type_t mp_type_BaseException;
extern const mp_obj_type_t mp_type_ArithmeticError;
extern const mp_obj_type_t mp_type_AssertionError;
extern const mp_obj_type_t mp_type_AttributeError;
extern const mp_obj_type_t mp_type_EOFError;
extern const mp_obj_type_t mp_type_Exception;
extern const mp_obj_type_t mp_type_GeneratorExit;
extern const mp_obj_type_t mp_type_IOError;
extern const mp_obj_type_t mp_type_ImportError;
extern const mp_obj_type_t mp_type_IndentationError;
extern const mp_obj_type_t mp_type_IndexError;
extern const mp_obj_type_t mp_type_KeyError;
extern const mp_obj_type_t mp_type_LookupError;
extern const mp_obj_type_t mp_type_MemoryError;
extern const mp_obj_type_t mp_type_NameError;
extern const mp_obj_type_t mp_type_NotImplementedError;
extern const mp_obj_type_t mp_type_OSError;
extern const mp_obj_type_t mp_type_OverflowError;
extern const mp_obj_type_t mp_type_RuntimeError;
extern const mp_obj_type_t mp_type_StopIteration;
extern const mp_obj_type_t mp_type_SyntaxError;
extern const mp_obj_type_t mp_type_SystemError;
extern const mp_obj_type_t mp_type_SystemExit;
extern const mp_obj_type_t mp_type_TypeError;
extern const mp_obj_type_t mp_type_ValueError;
extern const mp_obj_type_t mp_type_ZeroDivisionError;

// Constant objects, globally accessible
// The macros are for convenience only
#define mp_const_none ((mp_obj_t)&mp_const_none_obj)
#define mp_const_false ((mp_obj_t)&mp_const_false_obj)
#define mp_const_true ((mp_obj_t)&mp_const_true_obj)
#define mp_const_empty_tuple ((mp_obj_t)&mp_const_empty_tuple_obj)
extern const struct _mp_obj_none_t mp_const_none_obj;
extern const struct _mp_obj_bool_t mp_const_false_obj;
extern const struct _mp_obj_bool_t mp_const_true_obj;
extern const struct _mp_obj_tuple_t mp_const_empty_tuple_obj;
extern const struct _mp_obj_ellipsis_t mp_const_ellipsis_obj;
extern const struct _mp_obj_exception_t mp_const_MemoryError_obj;
extern const struct _mp_obj_exception_t mp_const_GeneratorExit_obj;

// General API for objects

mp_obj_t mp_obj_new_type(qstr name, mp_obj_t bases_tuple, mp_obj_t locals_dict);
mp_obj_t mp_obj_new_none(void);
mp_obj_t mp_obj_new_bool(bool value);
mp_obj_t mp_obj_new_cell(mp_obj_t obj);
mp_obj_t mp_obj_new_int(mp_int_t value);
mp_obj_t mp_obj_new_int_from_uint(mp_uint_t value);
mp_obj_t mp_obj_new_int_from_str_len(const char **str, uint len, bool neg, uint base);
mp_obj_t mp_obj_new_int_from_ll(long long val); // this must return a multi-precision integer object (or raise an overflow exception)
mp_obj_t mp_obj_new_str(const char* data, uint len, bool make_qstr_if_not_already);
mp_obj_t mp_obj_new_bytes(const byte* data, uint len);
#if MICROPY_PY_BUILTINS_FLOAT
mp_obj_t mp_obj_new_float(mp_float_t val);
mp_obj_t mp_obj_new_complex(mp_float_t real, mp_float_t imag);
#endif
mp_obj_t mp_obj_new_exception(const mp_obj_type_t *exc_type);
mp_obj_t mp_obj_new_exception_arg1(const mp_obj_type_t *exc_type, mp_obj_t arg);
mp_obj_t mp_obj_new_exception_args(const mp_obj_type_t *exc_type, uint n_args, const mp_obj_t *args);
mp_obj_t mp_obj_new_exception_msg(const mp_obj_type_t *exc_type, const char *msg);
mp_obj_t mp_obj_new_exception_msg_varg(const mp_obj_type_t *exc_type, const char *fmt, ...); // counts args by number of % symbols in fmt, excluding %%; can only handle void* sizes (ie no float/double!)
mp_obj_t mp_obj_new_fun_bc(uint scope_flags, qstr *args, uint n_pos_args, uint n_kwonly_args, mp_obj_t def_args, mp_obj_t def_kw_args, const byte *code);
mp_obj_t mp_obj_new_fun_asm(uint n_args, void *fun);
mp_obj_t mp_obj_new_gen_wrap(mp_obj_t fun);
mp_obj_t mp_obj_new_closure(mp_obj_t fun, uint n_closed, const mp_obj_t *closed);
mp_obj_t mp_obj_new_tuple(uint n, const mp_obj_t *items);
mp_obj_t mp_obj_new_list(uint n, mp_obj_t *items);
mp_obj_t mp_obj_new_dict(int n_args);
mp_obj_t mp_obj_new_set(int n_args, mp_obj_t *items);
mp_obj_t mp_obj_new_slice(mp_obj_t start, mp_obj_t stop, mp_obj_t step);
mp_obj_t mp_obj_new_super(mp_obj_t type, mp_obj_t obj);
mp_obj_t mp_obj_new_bound_meth(mp_obj_t meth, mp_obj_t self);
mp_obj_t mp_obj_new_getitem_iter(mp_obj_t *args);
mp_obj_t mp_obj_new_module(qstr module_name);

mp_obj_type_t *mp_obj_get_type(mp_const_obj_t o_in);
const char *mp_obj_get_type_str(mp_const_obj_t o_in);
bool mp_obj_is_subclass_fast(mp_const_obj_t object, mp_const_obj_t classinfo); // arguments should be type objects
mp_obj_t mp_instance_cast_to_native_base(mp_const_obj_t self_in, mp_const_obj_t native_type);

void mp_obj_print_helper(void (*print)(void *env, const char *fmt, ...), void *env, mp_obj_t o_in, mp_print_kind_t kind);
void mp_obj_print(mp_obj_t o, mp_print_kind_t kind);
void mp_obj_print_exception(mp_obj_t exc);

int mp_obj_is_true(mp_obj_t arg);

// TODO make these all lower case when they have proven themselves
static inline bool MP_OBJ_IS_OBJ(mp_const_obj_t o) { return ((((mp_int_t)(o)) & 3) == 0); }
static inline bool MP_OBJ_IS_SMALL_INT(mp_const_obj_t o) { return ((((mp_int_t)(o)) & 1) != 0); }
//static inline bool MP_OBJ_IS_TYPE(mp_const_obj_t o, const mp_obj_type_t *t) { return (MP_OBJ_IS_OBJ(o) && (((mp_obj_base_t*)(o))->type == (t))); } // this does not work for checking a string, use below macro for that
//static inline bool MP_OBJ_IS_INT(mp_const_obj_t o) { return (MP_OBJ_IS_SMALL_INT(o) || MP_OBJ_IS_TYPE(o, &mp_type_int)); } // returns true if o is a small int or long int
static inline bool mp_obj_is_integer(mp_const_obj_t o) { return MP_OBJ_IS_INT(o) || MP_OBJ_IS_TYPE(o, &mp_type_bool); } // returns true if o is bool, small int or long int
static inline bool MP_OBJ_IS_QSTR(mp_const_obj_t o) { return ((((mp_int_t)(o)) & 3) == 2); }
//static inline bool MP_OBJ_IS_STR(mp_const_obj_t o) { return (MP_OBJ_IS_QSTR(o) || MP_OBJ_IS_TYPE(o, &mp_type_str)); }

bool mp_obj_is_callable(mp_obj_t o_in);
mp_int_t mp_obj_hash(mp_obj_t o_in);
bool mp_obj_equal(mp_obj_t o1, mp_obj_t o2);

mp_int_t mp_obj_get_int(mp_const_obj_t arg);
bool mp_obj_get_int_maybe(mp_const_obj_t arg, mp_int_t *value);
#if MICROPY_PY_BUILTINS_FLOAT
mp_float_t mp_obj_get_float(mp_obj_t self_in);
void mp_obj_get_complex(mp_obj_t self_in, mp_float_t *real, mp_float_t *imag);
#endif
//qstr mp_obj_get_qstr(mp_obj_t arg);
void mp_obj_get_array(mp_obj_t o, uint *len, mp_obj_t **items);
void mp_obj_get_array_fixed_n(mp_obj_t o, uint len, mp_obj_t **items);
uint mp_get_index(const mp_obj_type_t *type, mp_uint_t len, mp_obj_t index, bool is_slice);
mp_obj_t mp_obj_len_maybe(mp_obj_t o_in); /* may return MP_OBJ_NULL */
mp_obj_t mp_obj_subscr(mp_obj_t base, mp_obj_t index, mp_obj_t val);

// bool
// TODO make lower case when it has proven itself
static inline mp_obj_t MP_BOOL(mp_int_t x) { return x ? mp_const_true : mp_const_false; }

// cell
mp_obj_t mp_obj_cell_get(mp_obj_t self_in);
void mp_obj_cell_set(mp_obj_t self_in, mp_obj_t obj);

// int
// For long int, returns value truncated to mp_int_t
mp_int_t mp_obj_int_get(mp_const_obj_t self_in);
#if MICROPY_PY_BUILTINS_FLOAT
mp_float_t mp_obj_int_as_float(mp_obj_t self_in);
#endif
// Will raise exception if value doesn't fit into mp_int_t
mp_int_t mp_obj_int_get_checked(mp_const_obj_t self_in);

// exception
#define mp_obj_is_native_exception_instance(o) (mp_obj_get_type(o)->make_new == mp_obj_exception_make_new)
bool mp_obj_is_exception_type(mp_obj_t self_in);
bool mp_obj_is_exception_instance(mp_obj_t self_in);
bool mp_obj_exception_match(mp_obj_t exc, const mp_obj_type_t *exc_type);
void mp_obj_exception_clear_traceback(mp_obj_t self_in);
void mp_obj_exception_add_traceback(mp_obj_t self_in, qstr file, mp_uint_t line, qstr block);
void mp_obj_exception_get_traceback(mp_obj_t self_in, mp_uint_t *n, mp_uint_t **values);
mp_obj_t mp_obj_exception_get_value(mp_obj_t self_in);
mp_obj_t mp_obj_exception_make_new(mp_obj_t type_in, uint n_args, uint n_kw, const mp_obj_t *args);

// str
mp_obj_t mp_obj_str_builder_start(const mp_obj_type_t *type, uint len, byte **data);
mp_obj_t mp_obj_str_builder_end(mp_obj_t o_in);
bool mp_obj_str_equal(mp_obj_t s1, mp_obj_t s2);
uint mp_obj_str_get_hash(mp_obj_t self_in);
uint mp_obj_str_get_len(mp_obj_t self_in);
qstr mp_obj_str_get_qstr(mp_obj_t self_in); // use this if you will anyway convert the string to a qstr
const char *mp_obj_str_get_str(mp_obj_t self_in); // use this only if you need the string to be null terminated
const char *mp_obj_str_get_data(mp_obj_t self_in, uint *len);
mp_obj_t mp_obj_str_intern(mp_obj_t str);
void mp_str_print_quoted(void (*print)(void *env, const char *fmt, ...), void *env, const byte *str_data, uint str_len, bool is_bytes);

#if MICROPY_PY_BUILTINS_FLOAT
// float
typedef struct _mp_obj_float_t {
    mp_obj_base_t base;
    mp_float_t value;
} mp_obj_float_t;
mp_float_t mp_obj_float_get(mp_obj_t self_in);
mp_obj_t mp_obj_float_binary_op(int op, mp_float_t lhs_val, mp_obj_t rhs); // can return MP_OBJ_NULL if op not supported

// complex
void mp_obj_complex_get(mp_obj_t self_in, mp_float_t *real, mp_float_t *imag);
mp_obj_t mp_obj_complex_binary_op(int op, mp_float_t lhs_real, mp_float_t lhs_imag, mp_obj_t rhs_in); // can return MP_OBJ_NULL if op not supported
#endif

// tuple
void mp_obj_tuple_get(mp_obj_t self_in, uint *len, mp_obj_t **items);
void mp_obj_tuple_del(mp_obj_t self_in);
mp_int_t mp_obj_tuple_hash(mp_obj_t self_in);

// list
struct _mp_obj_list_t;
void mp_obj_list_init(struct _mp_obj_list_t *o, uint n);
mp_obj_t mp_obj_list_append(mp_obj_t self_in, mp_obj_t arg);
void mp_obj_list_get(mp_obj_t self_in, uint *len, mp_obj_t **items);
void mp_obj_list_set_len(mp_obj_t self_in, uint len);
void mp_obj_list_store(mp_obj_t self_in, mp_obj_t index, mp_obj_t value);
mp_obj_t mp_obj_list_sort(uint n_args, const mp_obj_t *args, mp_map_t *kwargs);

// dict
typedef struct _mp_obj_dict_t {
    mp_obj_base_t base;
    mp_map_t map;
} mp_obj_dict_t;
void mp_obj_dict_init(mp_obj_dict_t *dict, int n_args);
uint mp_obj_dict_len(mp_obj_t self_in);
mp_obj_t mp_obj_dict_get(mp_obj_t self_in, mp_obj_t index);
mp_obj_t mp_obj_dict_store(mp_obj_t self_in, mp_obj_t key, mp_obj_t value);
mp_obj_t mp_obj_dict_delete(mp_obj_t self_in, mp_obj_t key);
mp_map_t *mp_obj_dict_get_map(mp_obj_t self_in);

// set
void mp_obj_set_store(mp_obj_t self_in, mp_obj_t item);

// slice
void mp_obj_slice_get(mp_obj_t self_in, mp_obj_t *start, mp_obj_t *stop, mp_obj_t *step);

// array
uint mp_obj_array_len(mp_obj_t self_in);
mp_obj_t mp_obj_new_bytearray_by_ref(uint n, void *items);

// functions
#define MP_OBJ_FUN_ARGS_MAX (0xffff) // to set maximum value in n_args_max below
typedef struct _mp_obj_fun_native_t { // need this so we can define const objects (to go in ROM)
    mp_obj_base_t base;
    bool is_kw : 1;
    uint n_args_min : 15; // inclusive
    uint n_args_max : 16; // inclusive
    void *fun;
    // TODO add mp_map_t *globals
    // for const function objects, make an empty, const map
    // such functions won't be able to access the global scope, but that's probably okay
} mp_obj_fun_native_t;

const char *mp_obj_fun_get_name(mp_const_obj_t fun);
const char *mp_obj_code_get_name(const byte *code_info);

mp_obj_t mp_identity(mp_obj_t self);
MP_DECLARE_CONST_FUN_OBJ(mp_identity_obj);

// module
typedef struct _mp_obj_module_t {
    mp_obj_base_t base;
    qstr name;
    mp_obj_dict_t *globals;
} mp_obj_module_t;
mp_obj_dict_t *mp_obj_module_get_globals(mp_obj_t self_in);

// staticmethod and classmethod types; defined here so we can make const versions
// this structure is used for instances of both staticmethod and classmethod
typedef struct _mp_obj_static_class_method_t {
    mp_obj_base_t base;
    mp_obj_t fun;
} mp_obj_static_class_method_t;

// property
const mp_obj_t *mp_obj_property_get(mp_obj_t self_in);

// sequence helpers

// slice indexes resolved to particular sequence
typedef struct {
    mp_uint_t start;
    mp_uint_t stop;
    mp_int_t step;
} mp_bound_slice_t;

void mp_seq_multiply(const void *items, uint item_sz, uint len, uint times, void *dest);
#if MICROPY_PY_BUILTINS_SLICE
bool mp_seq_get_fast_slice_indexes(mp_uint_t len, mp_obj_t slice, mp_bound_slice_t *indexes);
#endif
#define mp_seq_copy(dest, src, len, item_t) memcpy(dest, src, len * sizeof(item_t))
#define mp_seq_cat(dest, src1, len1, src2, len2, item_t) { memcpy(dest, src1, (len1) * sizeof(item_t)); memcpy(dest + (len1), src2, (len2) * sizeof(item_t)); }
bool mp_seq_cmp_bytes(int op, const byte *data1, uint len1, const byte *data2, uint len2);
bool mp_seq_cmp_objs(int op, const mp_obj_t *items1, uint len1, const mp_obj_t *items2, uint len2);
mp_obj_t mp_seq_index_obj(const mp_obj_t *items, uint len, uint n_args, const mp_obj_t *args);
mp_obj_t mp_seq_count_obj(const mp_obj_t *items, uint len, mp_obj_t value);
mp_obj_t mp_seq_extract_slice(uint len, const mp_obj_t *seq, mp_bound_slice_t *indexes);
// Helper to clear stale pointers from allocated, but unused memory, to preclude GC problems
#define mp_seq_clear(start, len, alloc_len, item_sz) memset((byte*)(start) + (len) * (item_sz), 0, ((alloc_len) - (len)) * (item_sz))
#define mp_seq_replace_slice_no_grow(dest, dest_len, beg, end, slice, slice_len, item_t) \
    /*printf("memcpy(%p, %p, %d)\n", dest + beg, slice, slice_len * sizeof(item_t));*/ \
    memcpy(dest + beg, slice, slice_len * sizeof(item_t)); \
    /*printf("memcpy(%p, %p, %d)\n", dest + (beg + slice_len), dest + end, (dest_len - end) * sizeof(item_t));*/ \
    memcpy(dest + (beg + slice_len), dest + end, (dest_len - end) * sizeof(item_t));

#define mp_seq_replace_slice_grow_inplace(dest, dest_len, beg, end, slice, slice_len, len_adj, item_t) \
    /*printf("memmove(%p, %p, %d)\n", dest + beg + len_adj, dest + beg, (dest_len - beg) * sizeof(item_t));*/ \
    memmove(dest + beg + len_adj, dest + beg, (dest_len - beg) * sizeof(item_t)); \
    memcpy(dest + beg, slice, slice_len * sizeof(item_t));
