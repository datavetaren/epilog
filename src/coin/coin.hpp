#pragma once
#ifndef _epilog_coin_coin_hpp
#define _epilog_coin_coin_hpp

#include "../interp/interpreter_base.hpp"

namespace epilog { namespace coin {

// Any term that has arity >= 2 and whose functor's name starts with '$'
// is a coin.
// The first argument must be an integer telling its value,
// The second argument is either unbound (unspent) or not (spent.)
static inline bool is_coin(interp::interpreter_base &interp, common::term t) {
    if (t.tag() != common::tag_t::STR) {
        return false;
    }
    auto f = interp.functor(t);
    if (f.arity() < 2) {
        return false;
    }
    return interp.is_dollar_atom_name(f);
}

static inline bool is_native_coin(interp::interpreter_base &interp, common::term t) {
    return interp.is_functor(t, common::con_cell("$coin",2));
}

static inline bool is_coin_spent(interp::interpreter_base &interp, common::term t) {
    assert(is_coin(interp, t));
    return !interp.arg(t, 1).tag().is_ref();
}

static inline bool spend_coin(interp::interpreter_base &interp, common::term t) {
    assert(is_coin(interp, t));
    assert(!is_coin_spent(interp, t));
    return interp.unify(interp.arg(t, 1), common::term_env::EMPTY_LIST);
}

static inline int64_t coin_value(interp::interpreter_base &interp, common::term t) {
    assert(is_coin(interp, t));
    common::term v = interp.arg(t, 0);
    assert(v.tag() == common::tag_t::INT);
    return reinterpret_cast<common::int_cell &>(v).value();
}

}}

#endif
	
