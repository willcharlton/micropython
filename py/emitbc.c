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

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "mpconfig.h"
#include "misc.h"
#include "qstr.h"
#include "lexer.h"
#include "parse.h"
#include "obj.h"
#include "emitglue.h"
#include "scope.h"
#include "runtime0.h"
#include "emit.h"
#include "bc0.h"

#if !MICROPY_EMIT_CPYTHON

#define BYTES_FOR_INT ((BYTES_PER_WORD * 8 + 6) / 7)
#define DUMMY_DATA_SIZE (BYTES_FOR_INT)

struct _emit_t {
    pass_kind_t pass : 8;
    uint last_emit_was_return_value : 8;
    byte dummy_data[DUMMY_DATA_SIZE];

    int stack_size;

    scope_t *scope;

    uint last_source_line_offset;
    uint last_source_line;

    uint max_num_labels;
    uint *label_offsets;

    uint code_info_offset;
    uint code_info_size;
    uint bytecode_offset;
    uint bytecode_size;
    byte *code_base; // stores both byte code and code info
};

STATIC void emit_bc_rot_two(emit_t *emit);
STATIC void emit_bc_rot_three(emit_t *emit);

emit_t *emit_bc_new(uint max_num_labels) {
    emit_t *emit = m_new0(emit_t, 1);
    emit->max_num_labels = max_num_labels;
    emit->label_offsets = m_new(uint, emit->max_num_labels);
    return emit;
}

void emit_bc_free(emit_t *emit) {
    m_del(uint, emit->label_offsets, emit->max_num_labels);
    m_del_obj(emit_t, emit);
}

// all functions must go through this one to emit code info
STATIC byte* emit_get_cur_to_write_code_info(emit_t* emit, int num_bytes_to_write) {
    //printf("emit %d\n", num_bytes_to_write);
    if (emit->pass < MP_PASS_EMIT) {
        emit->code_info_offset += num_bytes_to_write;
        return emit->dummy_data;
    } else {
        assert(emit->code_info_offset + num_bytes_to_write <= emit->code_info_size);
        byte *c = emit->code_base + emit->code_info_offset;
        emit->code_info_offset += num_bytes_to_write;
        return c;
    }
}

STATIC void emit_align_code_info_to_machine_word(emit_t* emit) {
    emit->code_info_offset = (emit->code_info_offset + sizeof(mp_uint_t) - 1) & (~(sizeof(mp_uint_t) - 1));
}

STATIC void emit_write_code_info_qstr(emit_t* emit, qstr qstr) {
    byte* c = emit_get_cur_to_write_code_info(emit, 4);
    // TODO variable length encoding for qstr
    c[0] = qstr & 0xff;
    c[1] = (qstr >> 8) & 0xff;
    c[2] = (qstr >> 16) & 0xff;
    c[3] = (qstr >> 24) & 0xff;
}

#if MICROPY_ENABLE_SOURCE_LINE
STATIC void emit_write_code_info_bytes_lines(emit_t* emit, uint bytes_to_skip, uint lines_to_skip) {
    assert(bytes_to_skip > 0 || lines_to_skip > 0);
    while (bytes_to_skip > 0 || lines_to_skip > 0) {
        uint b = MIN(bytes_to_skip, 31);
        uint l = MIN(lines_to_skip, 7);
        bytes_to_skip -= b;
        lines_to_skip -= l;
        *emit_get_cur_to_write_code_info(emit, 1) = b | (l << 5);
    }
}
#endif

// all functions must go through this one to emit byte code
STATIC byte* emit_get_cur_to_write_bytecode(emit_t* emit, int num_bytes_to_write) {
    //printf("emit %d\n", num_bytes_to_write);
    if (emit->pass < MP_PASS_EMIT) {
        emit->bytecode_offset += num_bytes_to_write;
        return emit->dummy_data;
    } else {
        assert(emit->bytecode_offset + num_bytes_to_write <= emit->bytecode_size);
        byte *c = emit->code_base + emit->code_info_size + emit->bytecode_offset;
        emit->bytecode_offset += num_bytes_to_write;
        return c;
    }
}

STATIC void emit_align_bytecode_to_machine_word(emit_t* emit) {
    emit->bytecode_offset = (emit->bytecode_offset + sizeof(mp_uint_t) - 1) & (~(sizeof(mp_uint_t) - 1));
}

STATIC void emit_write_bytecode_byte(emit_t* emit, byte b1) {
    byte* c = emit_get_cur_to_write_bytecode(emit, 1);
    c[0] = b1;
}

STATIC void emit_write_bytecode_byte_byte(emit_t* emit, byte b1, uint b2) {
    assert((b2 & (~0xff)) == 0);
    byte* c = emit_get_cur_to_write_bytecode(emit, 2);
    c[0] = b1;
    c[1] = b2;
}

STATIC void emit_write_bytecode_uint(emit_t* emit, uint num) {
    // We store each 7 bits in a separate byte, and that's how many bytes needed
    byte buf[BYTES_FOR_INT];
    byte *p = buf + sizeof(buf);
    // We encode in little-ending order, but store in big-endian, to help decoding
    do {
        *--p = num & 0x7f;
        num >>= 7;
    } while (num != 0);
    byte* c = emit_get_cur_to_write_bytecode(emit, buf + sizeof(buf) - p);
    while (p != buf + sizeof(buf) - 1) {
        *c++ = *p++ | 0x80;
    }
    *c = *p;
}

// Similar to emit_write_bytecode_uint(), just some extra handling to encode sign
STATIC void emit_write_bytecode_byte_int(emit_t* emit, byte b1, mp_int_t num) {
    emit_write_bytecode_byte(emit, b1);

    // We store each 7 bits in a separate byte, and that's how many bytes needed
    byte buf[BYTES_FOR_INT];
    byte *p = buf + sizeof(buf);
    // We encode in little-ending order, but store in big-endian, to help decoding
    do {
        *--p = num & 0x7f;
        num >>= 7;
    } while (num != 0 && num != -1);
    // Make sure that highest bit we stored (mask 0x40) matches sign
    // of the number. If not, store extra byte just to encode sign
    if (num == -1 && (*p & 0x40) == 0) {
        *--p = 0x7f;
    } else if (num == 0 && (*p & 0x40) != 0) {
        *--p = 0;
    }

    byte* c = emit_get_cur_to_write_bytecode(emit, buf + sizeof(buf) - p);
    while (p != buf + sizeof(buf) - 1) {
        *c++ = *p++ | 0x80;
    }
    *c = *p;
}

STATIC void emit_write_bytecode_byte_uint(emit_t* emit, byte b, uint num) {
    emit_write_bytecode_byte(emit, b);
    emit_write_bytecode_uint(emit, num);
}

// aligns the pointer so it is friendly to GC
STATIC void emit_write_bytecode_byte_ptr(emit_t* emit, byte b, void *ptr) {
    emit_write_bytecode_byte(emit, b);
    emit_align_bytecode_to_machine_word(emit);
    mp_uint_t *c = (mp_uint_t*)emit_get_cur_to_write_bytecode(emit, sizeof(mp_uint_t));
    *c = (mp_uint_t)ptr;
}

/* currently unused
STATIC void emit_write_bytecode_byte_uint_uint(emit_t* emit, byte b, uint num1, uint num2) {
    emit_write_bytecode_byte(emit, b);
    emit_write_bytecode_byte_uint(emit, num1);
    emit_write_bytecode_byte_uint(emit, num2);
}
*/

STATIC void emit_write_bytecode_byte_qstr(emit_t* emit, byte b, qstr qstr) {
    emit_write_bytecode_byte_uint(emit, b, qstr);
}

// unsigned labels are relative to ip following this instruction, stored as 16 bits
STATIC void emit_write_bytecode_byte_unsigned_label(emit_t* emit, byte b1, uint label) {
    uint bytecode_offset;
    if (emit->pass < MP_PASS_EMIT) {
        bytecode_offset = 0;
    } else {
        bytecode_offset = emit->label_offsets[label] - emit->bytecode_offset - 3;
    }
    byte *c = emit_get_cur_to_write_bytecode(emit, 3);
    c[0] = b1;
    c[1] = bytecode_offset;
    c[2] = bytecode_offset >> 8;
}

// signed labels are relative to ip following this instruction, stored as 16 bits, in excess
STATIC void emit_write_bytecode_byte_signed_label(emit_t* emit, byte b1, uint label) {
    int bytecode_offset;
    if (emit->pass < MP_PASS_EMIT) {
        bytecode_offset = 0;
    } else {
        bytecode_offset = emit->label_offsets[label] - emit->bytecode_offset - 3 + 0x8000;
    }
    byte* c = emit_get_cur_to_write_bytecode(emit, 3);
    c[0] = b1;
    c[1] = bytecode_offset;
    c[2] = bytecode_offset >> 8;
}

STATIC void emit_bc_set_native_types(emit_t *emit, bool do_native_types) {
}

STATIC void emit_bc_start_pass(emit_t *emit, pass_kind_t pass, scope_t *scope) {
    emit->pass = pass;
    emit->stack_size = 0;
    emit->last_emit_was_return_value = false;
    emit->scope = scope;
    emit->last_source_line_offset = 0;
    emit->last_source_line = 1;
    if (pass < MP_PASS_EMIT) {
        memset(emit->label_offsets, -1, emit->max_num_labels * sizeof(uint));
    }
    emit->bytecode_offset = 0;
    emit->code_info_offset = 0;

    // write code info size; use maximum space (4 bytes) to write it; TODO possible optimise this
    {
        byte* c = emit_get_cur_to_write_code_info(emit, 4);
        mp_uint_t s = emit->code_info_size;
        c[0] = s & 0xff;
        c[1] = (s >> 8) & 0xff;
        c[2] = (s >> 16) & 0xff;
        c[3] = (s >> 24) & 0xff;
    }

    // code info
    emit_write_code_info_qstr(emit, scope->source_file);
    emit_write_code_info_qstr(emit, scope->simple_name);

    // bytecode prelude: local state size and exception stack size; 16 bit uints for now
    {
        byte* c = emit_get_cur_to_write_bytecode(emit, 4);
        uint n_state = scope->num_locals + scope->stack_size;
        if (n_state == 0) {
            // Need at least 1 entry in the state, in the case an exception is
            // propagated through this function, the exception is returned in
            // the highest slot in the state (fastn[0], see vm.c).
            n_state = 1;
        }
        c[0] = n_state & 0xff;
        c[1] = (n_state >> 8) & 0xff;
        c[2] = scope->exc_stack_size & 0xff;
        c[3] = (scope->exc_stack_size >> 8) & 0xff;
    }

    // bytecode prelude: initialise closed over variables
    int num_cell = 0;
    for (int i = 0; i < scope->id_info_len; i++) {
        id_info_t *id = &scope->id_info[i];
        if (id->kind == ID_INFO_KIND_CELL) {
            num_cell += 1;
        }
    }
    assert(num_cell <= 255);
    emit_write_bytecode_byte(emit, num_cell); // write number of locals that are cells
    for (int i = 0; i < scope->id_info_len; i++) {
        id_info_t *id = &scope->id_info[i];
        if (id->kind == ID_INFO_KIND_CELL) {
            emit_write_bytecode_byte(emit, id->local_num); // write the local which should be converted to a cell
        }
    }
}

STATIC void emit_bc_end_pass(emit_t *emit) {
    // check stack is back to zero size
    if (emit->stack_size != 0) {
        printf("ERROR: stack size not back to zero; got %d\n", emit->stack_size);
    }

    *emit_get_cur_to_write_code_info(emit, 1) = 0; // end of line number info
    emit_align_code_info_to_machine_word(emit); // align so that following bytecode is aligned

    if (emit->pass == MP_PASS_CODE_SIZE) {
        // calculate size of code in bytes
        emit->code_info_size = emit->code_info_offset;
        emit->bytecode_size = emit->bytecode_offset;
        emit->code_base = m_new0(byte, emit->code_info_size + emit->bytecode_size);

    } else if (emit->pass == MP_PASS_EMIT) {
        qstr *arg_names = m_new(qstr, emit->scope->num_pos_args + emit->scope->num_kwonly_args);
        for (int i = 0; i < emit->scope->num_pos_args + emit->scope->num_kwonly_args; i++) {
            arg_names[i] = emit->scope->id_info[i].qstr;
        }
        mp_emit_glue_assign_bytecode(emit->scope->raw_code, emit->code_base,
            emit->code_info_size + emit->bytecode_size,
            emit->scope->num_pos_args, emit->scope->num_kwonly_args, arg_names,
            emit->scope->scope_flags);
    }
}

STATIC bool emit_bc_last_emit_was_return_value(emit_t *emit) {
    return emit->last_emit_was_return_value;
}

STATIC void emit_bc_adjust_stack_size(emit_t *emit, int delta) {
    emit->stack_size += delta;
}

STATIC void emit_bc_set_source_line(emit_t *emit, int source_line) {
    //printf("source: line %d -> %d  offset %d -> %d\n", emit->last_source_line, source_line, emit->last_source_line_offset, emit->bytecode_offset);
#if MICROPY_ENABLE_SOURCE_LINE
    if (mp_optimise_value >= 3) {
        // If we compile with -O3, don't store line numbers.
        return;
    }
    if (source_line > emit->last_source_line) {
        uint bytes_to_skip = emit->bytecode_offset - emit->last_source_line_offset;
        uint lines_to_skip = source_line - emit->last_source_line;
        emit_write_code_info_bytes_lines(emit, bytes_to_skip, lines_to_skip);
        //printf("  %d %d\n", bytes_to_skip, lines_to_skip);
        emit->last_source_line_offset = emit->bytecode_offset;
        emit->last_source_line = source_line;
    }
#endif
}

STATIC void emit_bc_load_id(emit_t *emit, qstr qstr) {
    emit_common_load_id(emit, &emit_bc_method_table, emit->scope, qstr);
}

STATIC void emit_bc_store_id(emit_t *emit, qstr qstr) {
    emit_common_store_id(emit, &emit_bc_method_table, emit->scope, qstr);
}

STATIC void emit_bc_delete_id(emit_t *emit, qstr qstr) {
    emit_common_delete_id(emit, &emit_bc_method_table, emit->scope, qstr);
}

STATIC void emit_bc_pre(emit_t *emit, int stack_size_delta) {
    assert((int)emit->stack_size + stack_size_delta >= 0);
    emit->stack_size += stack_size_delta;
    if (emit->stack_size > emit->scope->stack_size) {
        emit->scope->stack_size = emit->stack_size;
    }
    emit->last_emit_was_return_value = false;
}

STATIC void emit_bc_label_assign(emit_t *emit, uint l) {
    emit_bc_pre(emit, 0);
    assert(l < emit->max_num_labels);
    if (emit->pass < MP_PASS_EMIT) {
        // assign label offset
        assert(emit->label_offsets[l] == -1);
        emit->label_offsets[l] = emit->bytecode_offset;
    } else {
        // ensure label offset has not changed from MP_PASS_CODE_SIZE to MP_PASS_EMIT
        //printf("l%d: (at %d vs %d)\n", l, emit->bytecode_offset, emit->label_offsets[l]);
        assert(emit->label_offsets[l] == emit->bytecode_offset);
    }
}

STATIC void emit_bc_import_name(emit_t *emit, qstr qstr) {
    emit_bc_pre(emit, -1);
    emit_write_bytecode_byte_qstr(emit, MP_BC_IMPORT_NAME, qstr);
}

STATIC void emit_bc_import_from(emit_t *emit, qstr qstr) {
    emit_bc_pre(emit, 1);
    emit_write_bytecode_byte_qstr(emit, MP_BC_IMPORT_FROM, qstr);
}

STATIC void emit_bc_import_star(emit_t *emit) {
    emit_bc_pre(emit, -1);
    emit_write_bytecode_byte(emit, MP_BC_IMPORT_STAR);
}

STATIC void emit_bc_load_const_tok(emit_t *emit, mp_token_kind_t tok) {
    emit_bc_pre(emit, 1);
    switch (tok) {
        case MP_TOKEN_KW_FALSE: emit_write_bytecode_byte(emit, MP_BC_LOAD_CONST_FALSE); break;
        case MP_TOKEN_KW_NONE: emit_write_bytecode_byte(emit, MP_BC_LOAD_CONST_NONE); break;
        case MP_TOKEN_KW_TRUE: emit_write_bytecode_byte(emit, MP_BC_LOAD_CONST_TRUE); break;
        case MP_TOKEN_ELLIPSIS: emit_write_bytecode_byte(emit, MP_BC_LOAD_CONST_ELLIPSIS); break;
        default: assert(0);
    }
}

STATIC void emit_bc_load_const_small_int(emit_t *emit, mp_int_t arg) {
    emit_bc_pre(emit, 1);
    emit_write_bytecode_byte_int(emit, MP_BC_LOAD_CONST_SMALL_INT, arg);
}

STATIC void emit_bc_load_const_int(emit_t *emit, qstr qstr) {
    emit_bc_pre(emit, 1);
    emit_write_bytecode_byte_qstr(emit, MP_BC_LOAD_CONST_INT, qstr);
}

STATIC void emit_bc_load_const_dec(emit_t *emit, qstr qstr) {
    emit_bc_pre(emit, 1);
    emit_write_bytecode_byte_qstr(emit, MP_BC_LOAD_CONST_DEC, qstr);
}

STATIC void emit_bc_load_const_str(emit_t *emit, qstr qstr, bool bytes) {
    emit_bc_pre(emit, 1);
    if (bytes) {
        emit_write_bytecode_byte_qstr(emit, MP_BC_LOAD_CONST_BYTES, qstr);
    } else {
        emit_write_bytecode_byte_qstr(emit, MP_BC_LOAD_CONST_STRING, qstr);
    }
}

STATIC void emit_bc_load_null(emit_t *emit) {
    emit_bc_pre(emit, 1);
    emit_write_bytecode_byte(emit, MP_BC_LOAD_NULL);
};

STATIC void emit_bc_load_fast(emit_t *emit, qstr qstr, uint id_flags, int local_num) {
    assert(local_num >= 0);
    emit_bc_pre(emit, 1);
    switch (local_num) {
        case 0: emit_write_bytecode_byte(emit, MP_BC_LOAD_FAST_0); break;
        case 1: emit_write_bytecode_byte(emit, MP_BC_LOAD_FAST_1); break;
        case 2: emit_write_bytecode_byte(emit, MP_BC_LOAD_FAST_2); break;
        default: emit_write_bytecode_byte_uint(emit, MP_BC_LOAD_FAST_N, local_num); break;
    }
}

STATIC void emit_bc_load_deref(emit_t *emit, qstr qstr, int local_num) {
    emit_bc_pre(emit, 1);
    emit_write_bytecode_byte_uint(emit, MP_BC_LOAD_DEREF, local_num);
}

STATIC void emit_bc_load_name(emit_t *emit, qstr qstr) {
    emit_bc_pre(emit, 1);
    emit_write_bytecode_byte_qstr(emit, MP_BC_LOAD_NAME, qstr);
}

STATIC void emit_bc_load_global(emit_t *emit, qstr qstr) {
    emit_bc_pre(emit, 1);
    emit_write_bytecode_byte_qstr(emit, MP_BC_LOAD_GLOBAL, qstr);
}

STATIC void emit_bc_load_attr(emit_t *emit, qstr qstr) {
    emit_bc_pre(emit, 0);
    emit_write_bytecode_byte_qstr(emit, MP_BC_LOAD_ATTR, qstr);
}

STATIC void emit_bc_load_method(emit_t *emit, qstr qstr) {
    emit_bc_pre(emit, 1);
    emit_write_bytecode_byte_qstr(emit, MP_BC_LOAD_METHOD, qstr);
}

STATIC void emit_bc_load_build_class(emit_t *emit) {
    emit_bc_pre(emit, 1);
    emit_write_bytecode_byte(emit, MP_BC_LOAD_BUILD_CLASS);
}

STATIC void emit_bc_load_subscr(emit_t *emit) {
    emit_bc_pre(emit, -1);
    emit_write_bytecode_byte(emit, MP_BC_LOAD_SUBSCR);
}

STATIC void emit_bc_store_fast(emit_t *emit, qstr qstr, int local_num) {
    assert(local_num >= 0);
    emit_bc_pre(emit, -1);
    switch (local_num) {
        case 0: emit_write_bytecode_byte(emit, MP_BC_STORE_FAST_0); break;
        case 1: emit_write_bytecode_byte(emit, MP_BC_STORE_FAST_1); break;
        case 2: emit_write_bytecode_byte(emit, MP_BC_STORE_FAST_2); break;
        default: emit_write_bytecode_byte_uint(emit, MP_BC_STORE_FAST_N, local_num); break;
    }
}

STATIC void emit_bc_store_deref(emit_t *emit, qstr qstr, int local_num) {
    emit_bc_pre(emit, -1);
    emit_write_bytecode_byte_uint(emit, MP_BC_STORE_DEREF, local_num);
}

STATIC void emit_bc_store_name(emit_t *emit, qstr qstr) {
    emit_bc_pre(emit, -1);
    emit_write_bytecode_byte_qstr(emit, MP_BC_STORE_NAME, qstr);
}

STATIC void emit_bc_store_global(emit_t *emit, qstr qstr) {
    emit_bc_pre(emit, -1);
    emit_write_bytecode_byte_qstr(emit, MP_BC_STORE_GLOBAL, qstr);
}

STATIC void emit_bc_store_attr(emit_t *emit, qstr qstr) {
    emit_bc_pre(emit, -2);
    emit_write_bytecode_byte_qstr(emit, MP_BC_STORE_ATTR, qstr);
}

STATIC void emit_bc_store_subscr(emit_t *emit) {
    emit_bc_pre(emit, -3);
    emit_write_bytecode_byte(emit, MP_BC_STORE_SUBSCR);
}

STATIC void emit_bc_delete_fast(emit_t *emit, qstr qstr, int local_num) {
    emit_write_bytecode_byte_uint(emit, MP_BC_DELETE_FAST, local_num);
}

STATIC void emit_bc_delete_deref(emit_t *emit, qstr qstr, int local_num) {
    emit_write_bytecode_byte_uint(emit, MP_BC_DELETE_DEREF, local_num);
}

STATIC void emit_bc_delete_name(emit_t *emit, qstr qstr) {
    emit_bc_pre(emit, 0);
    emit_write_bytecode_byte_qstr(emit, MP_BC_DELETE_NAME, qstr);
}

STATIC void emit_bc_delete_global(emit_t *emit, qstr qstr) {
    emit_bc_pre(emit, 0);
    emit_write_bytecode_byte_qstr(emit, MP_BC_DELETE_GLOBAL, qstr);
}

STATIC void emit_bc_delete_attr(emit_t *emit, qstr qstr) {
    emit_bc_load_null(emit);
    emit_bc_rot_two(emit);
    emit_bc_store_attr(emit, qstr);
}

STATIC void emit_bc_delete_subscr(emit_t *emit) {
    emit_bc_load_null(emit);
    emit_bc_rot_three(emit);
    emit_bc_store_subscr(emit);
}

STATIC void emit_bc_dup_top(emit_t *emit) {
    emit_bc_pre(emit, 1);
    emit_write_bytecode_byte(emit, MP_BC_DUP_TOP);
}

STATIC void emit_bc_dup_top_two(emit_t *emit) {
    emit_bc_pre(emit, 2);
    emit_write_bytecode_byte(emit, MP_BC_DUP_TOP_TWO);
}

STATIC void emit_bc_pop_top(emit_t *emit) {
    emit_bc_pre(emit, -1);
    emit_write_bytecode_byte(emit, MP_BC_POP_TOP);
}

STATIC void emit_bc_rot_two(emit_t *emit) {
    emit_bc_pre(emit, 0);
    emit_write_bytecode_byte(emit, MP_BC_ROT_TWO);
}

STATIC void emit_bc_rot_three(emit_t *emit) {
    emit_bc_pre(emit, 0);
    emit_write_bytecode_byte(emit, MP_BC_ROT_THREE);
}

STATIC void emit_bc_jump(emit_t *emit, uint label) {
    emit_bc_pre(emit, 0);
    emit_write_bytecode_byte_signed_label(emit, MP_BC_JUMP, label);
}

STATIC void emit_bc_pop_jump_if_true(emit_t *emit, uint label) {
    emit_bc_pre(emit, -1);
    emit_write_bytecode_byte_signed_label(emit, MP_BC_POP_JUMP_IF_TRUE, label);
}

STATIC void emit_bc_pop_jump_if_false(emit_t *emit, uint label) {
    emit_bc_pre(emit, -1);
    emit_write_bytecode_byte_signed_label(emit, MP_BC_POP_JUMP_IF_FALSE, label);
}

STATIC void emit_bc_jump_if_true_or_pop(emit_t *emit, uint label) {
    emit_bc_pre(emit, -1);
    emit_write_bytecode_byte_signed_label(emit, MP_BC_JUMP_IF_TRUE_OR_POP, label);
}

STATIC void emit_bc_jump_if_false_or_pop(emit_t *emit, uint label) {
    emit_bc_pre(emit, -1);
    emit_write_bytecode_byte_signed_label(emit, MP_BC_JUMP_IF_FALSE_OR_POP, label);
}

STATIC void emit_bc_unwind_jump(emit_t *emit, uint label, int except_depth) {
    if (except_depth == 0) {
        emit_bc_pre(emit, 0);
        if (label & MP_EMIT_BREAK_FROM_FOR) {
            // need to pop the iterator if we are breaking out of a for loop
            emit_write_bytecode_byte(emit, MP_BC_POP_TOP);
        }
        emit_write_bytecode_byte_signed_label(emit, MP_BC_JUMP, label & ~MP_EMIT_BREAK_FROM_FOR);
    } else {
        emit_write_bytecode_byte_signed_label(emit, MP_BC_UNWIND_JUMP, label & ~MP_EMIT_BREAK_FROM_FOR);
        emit_write_bytecode_byte(emit, ((label & MP_EMIT_BREAK_FROM_FOR) ? 0x80 : 0) | except_depth);
    }
}

STATIC void emit_bc_setup_with(emit_t *emit, uint label) {
    emit_bc_pre(emit, 7);
    emit_write_bytecode_byte_unsigned_label(emit, MP_BC_SETUP_WITH, label);
}

STATIC void emit_bc_with_cleanup(emit_t *emit) {
    emit_bc_pre(emit, -7);
    emit_write_bytecode_byte(emit, MP_BC_WITH_CLEANUP);
}

STATIC void emit_bc_setup_except(emit_t *emit, uint label) {
    emit_bc_pre(emit, 0);
    emit_write_bytecode_byte_unsigned_label(emit, MP_BC_SETUP_EXCEPT, label);
}

STATIC void emit_bc_setup_finally(emit_t *emit, uint label) {
    emit_bc_pre(emit, 0);
    emit_write_bytecode_byte_unsigned_label(emit, MP_BC_SETUP_FINALLY, label);
}

STATIC void emit_bc_end_finally(emit_t *emit) {
    emit_bc_pre(emit, -1);
    emit_write_bytecode_byte(emit, MP_BC_END_FINALLY);
}

STATIC void emit_bc_get_iter(emit_t *emit) {
    emit_bc_pre(emit, 0);
    emit_write_bytecode_byte(emit, MP_BC_GET_ITER);
}

STATIC void emit_bc_for_iter(emit_t *emit, uint label) {
    emit_bc_pre(emit, 1);
    emit_write_bytecode_byte_unsigned_label(emit, MP_BC_FOR_ITER, label);
}

STATIC void emit_bc_for_iter_end(emit_t *emit) {
    emit_bc_pre(emit, -1);
}

STATIC void emit_bc_pop_block(emit_t *emit) {
    emit_bc_pre(emit, 0);
    emit_write_bytecode_byte(emit, MP_BC_POP_BLOCK);
}

STATIC void emit_bc_pop_except(emit_t *emit) {
    emit_bc_pre(emit, 0);
    emit_write_bytecode_byte(emit, MP_BC_POP_EXCEPT);
}

STATIC void emit_bc_unary_op(emit_t *emit, mp_unary_op_t op) {
    if (op == MP_UNARY_OP_NOT) {
        emit_bc_pre(emit, 0);
        emit_write_bytecode_byte_byte(emit, MP_BC_UNARY_OP, MP_UNARY_OP_BOOL);
        emit_bc_pre(emit, 0);
        emit_write_bytecode_byte(emit, MP_BC_NOT);
    } else {
        emit_bc_pre(emit, 0);
        emit_write_bytecode_byte_byte(emit, MP_BC_UNARY_OP, op);
    }
}

STATIC void emit_bc_binary_op(emit_t *emit, mp_binary_op_t op) {
    bool invert = false;
    if (op == MP_BINARY_OP_NOT_IN) {
        invert = true;
        op = MP_BINARY_OP_IN;
    } else if (op == MP_BINARY_OP_IS_NOT) {
        invert = true;
        op = MP_BINARY_OP_IS;
    }
    emit_bc_pre(emit, -1);
    emit_write_bytecode_byte_byte(emit, MP_BC_BINARY_OP, op);
    if (invert) {
        emit_bc_pre(emit, 0);
        emit_write_bytecode_byte(emit, MP_BC_NOT);
    }
}

STATIC void emit_bc_build_tuple(emit_t *emit, int n_args) {
    assert(n_args >= 0);
    emit_bc_pre(emit, 1 - n_args);
    emit_write_bytecode_byte_uint(emit, MP_BC_BUILD_TUPLE, n_args);
}

STATIC void emit_bc_build_list(emit_t *emit, int n_args) {
    assert(n_args >= 0);
    emit_bc_pre(emit, 1 - n_args);
    emit_write_bytecode_byte_uint(emit, MP_BC_BUILD_LIST, n_args);
}

STATIC void emit_bc_list_append(emit_t *emit, int list_stack_index) {
    assert(list_stack_index >= 0);
    emit_bc_pre(emit, -1);
    emit_write_bytecode_byte_uint(emit, MP_BC_LIST_APPEND, list_stack_index);
}

STATIC void emit_bc_build_map(emit_t *emit, int n_args) {
    assert(n_args >= 0);
    emit_bc_pre(emit, 1);
    emit_write_bytecode_byte_uint(emit, MP_BC_BUILD_MAP, n_args);
}

STATIC void emit_bc_store_map(emit_t *emit) {
    emit_bc_pre(emit, -2);
    emit_write_bytecode_byte(emit, MP_BC_STORE_MAP);
}

STATIC void emit_bc_map_add(emit_t *emit, int map_stack_index) {
    assert(map_stack_index >= 0);
    emit_bc_pre(emit, -2);
    emit_write_bytecode_byte_uint(emit, MP_BC_MAP_ADD, map_stack_index);
}

STATIC void emit_bc_build_set(emit_t *emit, int n_args) {
    assert(n_args >= 0);
    emit_bc_pre(emit, 1 - n_args);
    emit_write_bytecode_byte_uint(emit, MP_BC_BUILD_SET, n_args);
}

STATIC void emit_bc_set_add(emit_t *emit, int set_stack_index) {
    assert(set_stack_index >= 0);
    emit_bc_pre(emit, -1);
    emit_write_bytecode_byte_uint(emit, MP_BC_SET_ADD, set_stack_index);
}

STATIC void emit_bc_build_slice(emit_t *emit, int n_args) {
    assert(n_args >= 0);
    emit_bc_pre(emit, 1 - n_args);
    emit_write_bytecode_byte_uint(emit, MP_BC_BUILD_SLICE, n_args);
}

STATIC void emit_bc_unpack_sequence(emit_t *emit, int n_args) {
    assert(n_args >= 0);
    emit_bc_pre(emit, -1 + n_args);
    emit_write_bytecode_byte_uint(emit, MP_BC_UNPACK_SEQUENCE, n_args);
}

STATIC void emit_bc_unpack_ex(emit_t *emit, int n_left, int n_right) {
    assert(n_left >=0 && n_right >= 0);
    emit_bc_pre(emit, -1 + n_left + n_right + 1);
    emit_write_bytecode_byte_uint(emit, MP_BC_UNPACK_EX, n_left | (n_right << 8));
}

STATIC void emit_bc_make_function(emit_t *emit, scope_t *scope, uint n_pos_defaults, uint n_kw_defaults) {
    if (n_pos_defaults == 0 && n_kw_defaults == 0) {
        emit_bc_pre(emit, 1);
        emit_write_bytecode_byte_ptr(emit, MP_BC_MAKE_FUNCTION, scope->raw_code);
    } else {
        emit_bc_pre(emit, -1);
        emit_write_bytecode_byte_ptr(emit, MP_BC_MAKE_FUNCTION_DEFARGS, scope->raw_code);
    }
}

STATIC void emit_bc_make_closure(emit_t *emit, scope_t *scope, uint n_closed_over, uint n_pos_defaults, uint n_kw_defaults) {
    if (n_pos_defaults == 0 && n_kw_defaults == 0) {
        emit_bc_pre(emit, -n_closed_over + 1);
        emit_write_bytecode_byte_ptr(emit, MP_BC_MAKE_CLOSURE, scope->raw_code);
        emit_write_bytecode_byte(emit, n_closed_over);
    } else {
        assert(n_closed_over <= 255);
        emit_bc_pre(emit, -2 - n_closed_over + 1);
        emit_write_bytecode_byte_ptr(emit, MP_BC_MAKE_CLOSURE_DEFARGS, scope->raw_code);
        emit_write_bytecode_byte(emit, n_closed_over);
    }
}

STATIC void emit_bc_call_function_method_helper(emit_t *emit, int stack_adj, uint bytecode_base, int n_positional, int n_keyword, uint star_flags) {
    if (star_flags) {
        if (!(star_flags & MP_EMIT_STAR_FLAG_SINGLE)) {
            // load dummy entry for non-existent pos_seq
            emit_bc_load_null(emit);
            emit_bc_rot_two(emit);
        } else if (!(star_flags & MP_EMIT_STAR_FLAG_DOUBLE)) {
            // load dummy entry for non-existent kw_dict
            emit_bc_load_null(emit);
        }
        emit_bc_pre(emit, stack_adj - n_positional - 2 * n_keyword - 2);
        emit_write_bytecode_byte_uint(emit, bytecode_base + 1, (n_keyword << 8) | n_positional); // TODO make it 2 separate uints?
    } else {
        emit_bc_pre(emit, stack_adj - n_positional - 2 * n_keyword);
        emit_write_bytecode_byte_uint(emit, bytecode_base, (n_keyword << 8) | n_positional); // TODO make it 2 separate uints?
    }
}

STATIC void emit_bc_call_function(emit_t *emit, int n_positional, int n_keyword, uint star_flags) {
    emit_bc_call_function_method_helper(emit, 0, MP_BC_CALL_FUNCTION, n_positional, n_keyword, star_flags);
}

STATIC void emit_bc_call_method(emit_t *emit, int n_positional, int n_keyword, uint star_flags) {
    emit_bc_call_function_method_helper(emit, -1, MP_BC_CALL_METHOD, n_positional, n_keyword, star_flags);
}

STATIC void emit_bc_return_value(emit_t *emit) {
    emit_bc_pre(emit, -1);
    emit->last_emit_was_return_value = true;
    emit_write_bytecode_byte(emit, MP_BC_RETURN_VALUE);
}

STATIC void emit_bc_raise_varargs(emit_t *emit, int n_args) {
    assert(0 <= n_args && n_args <= 2);
    emit_bc_pre(emit, -n_args);
    emit_write_bytecode_byte_byte(emit, MP_BC_RAISE_VARARGS, n_args);
}

STATIC void emit_bc_yield_value(emit_t *emit) {
    emit_bc_pre(emit, 0);
    emit->scope->scope_flags |= MP_SCOPE_FLAG_GENERATOR;
    emit_write_bytecode_byte(emit, MP_BC_YIELD_VALUE);
}

STATIC void emit_bc_yield_from(emit_t *emit) {
    emit_bc_pre(emit, -1);
    emit->scope->scope_flags |= MP_SCOPE_FLAG_GENERATOR;
    emit_write_bytecode_byte(emit, MP_BC_YIELD_FROM);
}

STATIC void emit_bc_start_except_handler(emit_t *emit) {
    emit_bc_adjust_stack_size(emit, 6); // stack adjust for the 3 exception items, +3 for possible UNWIND_JUMP state
}

STATIC void emit_bc_end_except_handler(emit_t *emit) {
    emit_bc_adjust_stack_size(emit, -5); // stack adjust
}

const emit_method_table_t emit_bc_method_table = {
    emit_bc_set_native_types,
    emit_bc_start_pass,
    emit_bc_end_pass,
    emit_bc_last_emit_was_return_value,
    emit_bc_adjust_stack_size,
    emit_bc_set_source_line,

    emit_bc_load_id,
    emit_bc_store_id,
    emit_bc_delete_id,

    emit_bc_label_assign,
    emit_bc_import_name,
    emit_bc_import_from,
    emit_bc_import_star,
    emit_bc_load_const_tok,
    emit_bc_load_const_small_int,
    emit_bc_load_const_int,
    emit_bc_load_const_dec,
    emit_bc_load_const_str,
    emit_bc_load_null,
    emit_bc_load_fast,
    emit_bc_load_deref,
    emit_bc_load_name,
    emit_bc_load_global,
    emit_bc_load_attr,
    emit_bc_load_method,
    emit_bc_load_build_class,
    emit_bc_load_subscr,
    emit_bc_store_fast,
    emit_bc_store_deref,
    emit_bc_store_name,
    emit_bc_store_global,
    emit_bc_store_attr,
    emit_bc_store_subscr,
    emit_bc_delete_fast,
    emit_bc_delete_deref,
    emit_bc_delete_name,
    emit_bc_delete_global,
    emit_bc_delete_attr,
    emit_bc_delete_subscr,
    emit_bc_dup_top,
    emit_bc_dup_top_two,
    emit_bc_pop_top,
    emit_bc_rot_two,
    emit_bc_rot_three,
    emit_bc_jump,
    emit_bc_pop_jump_if_true,
    emit_bc_pop_jump_if_false,
    emit_bc_jump_if_true_or_pop,
    emit_bc_jump_if_false_or_pop,
    emit_bc_unwind_jump,
    emit_bc_unwind_jump,
    emit_bc_setup_with,
    emit_bc_with_cleanup,
    emit_bc_setup_except,
    emit_bc_setup_finally,
    emit_bc_end_finally,
    emit_bc_get_iter,
    emit_bc_for_iter,
    emit_bc_for_iter_end,
    emit_bc_pop_block,
    emit_bc_pop_except,
    emit_bc_unary_op,
    emit_bc_binary_op,
    emit_bc_build_tuple,
    emit_bc_build_list,
    emit_bc_list_append,
    emit_bc_build_map,
    emit_bc_store_map,
    emit_bc_map_add,
    emit_bc_build_set,
    emit_bc_set_add,
    emit_bc_build_slice,
    emit_bc_unpack_sequence,
    emit_bc_unpack_ex,
    emit_bc_make_function,
    emit_bc_make_closure,
    emit_bc_call_function,
    emit_bc_call_method,
    emit_bc_return_value,
    emit_bc_raise_varargs,
    emit_bc_yield_value,
    emit_bc_yield_from,

    emit_bc_start_except_handler,
    emit_bc_end_except_handler,
};

#endif // !MICROPY_EMIT_CPYTHON
