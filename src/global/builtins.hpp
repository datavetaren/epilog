#pragma once

#ifndef _global_builtins_hpp
#define _global_builtins_hpp

#include "../common/term.hpp"
#include "../interp/interpreter.hpp"

namespace epilog { namespace global {

using interpreter_exception = ::epilog::interp::interpreter_exception;

class global;
    
class builtins {
public:
    using term = epilog::common::term;
    using interpreter_base = epilog::interp::interpreter_base;

    static global & get_global(interpreter_base &interp);
    static void load(interpreter_base &interp, common::con_cell *module = nullptr);
    // reward(Height, Coin)
    static bool reward_2(interpreter_base &interp, size_t arity, term args[] );

    static bool current_height_1(interpreter_base &interp, size_t arity, term args[] );

    // ref(?X, ?HeapAddr)
    // We'll add a new predicate "test(on)", "test(off)" to toggle
    // global interpreter in testing mode. ref_2 will not be available
    // during test(off), which is what will automatically happen when a new
    // block is comitted.
    static bool ref_2(interpreter_base &interp, size_t arity, common::term args[] );
};


}}

#endif

