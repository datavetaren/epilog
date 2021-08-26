#include "mempool.hpp"

using namespace epilog::common;

namespace epilog { namespace node {

mempool::mempool() : interpreter("mempool"), id_count_(0) {
    setup_standard_lib();
}

void mempool::add_tx(const term_serializer::buffer_t &buf) {
    term h = term_serializer::hash_small(buf);
    auto &pred = get_predicate(con_cell("tx", 2));
    std::vector<common::term> existing;
    pred.get_matched_first_arg(h, existing);
    if (!existing.empty()) {
	return;
    }

    // Create new clause
    term_serializer ser(*this);
    term t = ser.read(buf);

    auto new_fact = new_term(con_cell("tx",2), { h, t });
    load_clause(new_fact, interp::LAST_CLAUSE);
    size_t id = ++id_count_;
    
    auto new_index = new_term(con_cell("index",2), { id, h });
    load_clause(new_index, interp::LAST_CLAUSE);
}
	
}}
