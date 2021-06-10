#include "mempool.hpp"

using namespace epilog::common;

namespace epilog { namespace node {

mempool::mempool() : interpreter("mempool") {
    setup_standard_lib();
}

void mempool::add_tx(const term_serializer::buffer_t &buf) {
    term h = term_serializer::hash_small(buf);

    auto &pred = get_predicate(con_cell("tx", 2));
    auto &existing = pred.get_clauses(*this, h);
    if (!existing.empty()) {
	return;
    }

    // Create new clause
    term_serializer ser(*this);
    term t = ser.read(buf);

    auto new_fact = new_term(con_cell("tx",2), { h, t });
    load_clause(new_fact, interp::LAST_CLAUSE);
    size_t id = pred.clauses().size();
    
    auto new_index = new_term(con_cell("index",2), { id, h });
    load_clause(new_index, interp::LAST_CLAUSE);
}
	
}}
