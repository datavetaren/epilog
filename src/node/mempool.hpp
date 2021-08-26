#pragma once

#ifndef _node_mempool_hpp
#define _node_mempool_hpp

#include "../common/term_env.hpp"
#include "../common/utime.hpp"
#include "../common/term_serializer.hpp"
#include "../interp/interpreter.hpp"

namespace epilog { namespace node {

//
// We keep the mempool in a separate interpreter. This way the
// garbage collector becomes more efficient for the node (as we don't
// have a generational garbage collector.) Also, it's nice to have this
// completely separated from everything else. Whenever we want to retrieve
// data from the mempol we just do:
//
// tx(small hash of X, N, X) @ mempool
//
// where X then becomes bound to the transaction (which is a term.)
//
// and to be able to enumerate transactions we also provide:
//
// index(N, small hash of tx)
// 
//
class mempool : public interp::interpreter {
public:
    mempool();

    void add_tx(const common::term_serializer::buffer_t &buf);

private:
    size_t id_count_;
};

}}

#endif
