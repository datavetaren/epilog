#include "global_interpreter.hpp"
#include "builtins.hpp"
#include "../common/checked_cast.hpp"
#include "../ec/builtins.hpp"
#include "../coin/builtins.hpp"
#include "../coin/coin.hpp"
#include "global.hpp"

using namespace epilog::common;
using namespace epilog::interp;

namespace epilog { namespace global {

global_interpreter::global_interpreter(global &g)
    : interp::interpreter("global"),
      global_(g),
      current_block_index_(static_cast<size_t>(-2)),
      current_block_(nullptr),
      block_flusher_(),
      block_cache_(global::BLOCK_CACHE_SIZE / heap_block::MAX_SIZE / sizeof(cell), block_flusher_),
      new_predicates_(0),
      new_frozen_closures_(0),
      old_heap_size_(0),
      next_atom_id_(0),
      start_next_atom_id_(0),
      next_predicate_id_(0),
      start_next_predicate_id_(0)
{
    set_auto_wam(true);
}

void global_interpreter::init()
{
    heap_setup_get_block_function( call_get_heap_block, this );
    heap_setup_trim_function( call_trim_heap, this );

    init_from_heap_db();
    init_from_symbols_db();
    init_from_program_db();

    heap_setup_new_atom_function( call_new_atom, this );
    heap_setup_load_atom_name_function( call_load_atom_name, this );
    heap_setup_load_atom_index_function( call_load_atom_index, this );
    heap_setup_modified_block_function( call_modified_heap_block, this );

    setup_standard_lib();
    set_retain_state_between_queries(true);

    // TODO: Remove this two lines
    load_builtins_file_io();
    set_debug_enabled();

    setup_consensus_lib(*this);

    setup_builtins();

    // Clear updated/old predicates (side effect for initializing interpreter)
    updated_predicates_.clear();
    old_predicates_.clear();
    new_predicates_ = 0;
    old_heap_size_ = heap_size();

    // Setup empty goals
    get_global().init_empty_goals();

    get_global().get_blockchain().flush_db();
}

void global_interpreter::total_reset()
{
    block_cache_.clear();

    current_block_index_ = static_cast<size_t>(-2);
    current_block_ = nullptr;
    new_atoms_.clear();
    new_atom_indices_.clear();
    modified_blocks_.clear();
    updated_predicates_.clear();
    old_predicates_.clear();
    modified_closures_.clear();

    global::erase_db(get_global().data_dir());
    interpreter::total_reset();
    init();
}

void global_interpreter::load_predicate(const interp::qname &qn) {
    std::vector<term> clauses;
    if (get_global().db_get_predicate(qn, clauses)) {
	auto *pred = internal_get_predicate(qn);
	for (auto clause : clauses) {
	    pred->add_clause(*this, clause);
	}
    } else {
	// The predicate was not found, but maybe there's an inherited
	// system predicate available? Of course, the global interpreter
	// is not allowed to shadow system: predicates, but I'd like to keep
	// that rule separate from the general infrastructure.
	// Also note that this rule is quite different from standard Prolog
	// which only import system predicates at "use_module(...)", but
	// the global interpreter may have millions of predicates and we
	// don't want to load them all into memory (only by demand) so that's
	// why the rule is slightly different for the global interpreter.
	// Think "use_module(system)" but applied lazily.
	static const con_cell SYSTEM("system",0);
	interp::qname imported_qn(SYSTEM, qn.second);
	if (get_global().db_get_predicate(imported_qn, clauses)) {
	    auto *pred = internal_get_predicate(qn);
	    for (auto clause : clauses) {
		pred->add_clause(*this, clause);
	    }
	    import_predicate(qn); // Here's the lazy rule
	}
    }
}

size_t global_interpreter::unique_predicate_id(const common::con_cell /*module*/) {
    size_t predicate_id = next_predicate_id_;
    next_predicate_id_++;
    return predicate_id;
}

void global_interpreter::setup_consensus_lib(interpreter &interp) {
  ec::builtins::load_consensus(interp);
  coin::builtins::load_consensus(interp);
  builtins::load(interp); // Overrides reward_2 from coin
    
  std::string lib = R"PROG(

%
% Transaction predicates
%

%
% tx/5
%

tx(CoinIn, Hash, Script, Args, CoinOut) :-
    functor(CoinIn, Functor, Arity),
    arg(1, CoinIn, V),
    ground(V),
    arg(2, CoinIn, X),
    var(X),
    cmove(CoinIn, CoinX),
    freeze(Hash,
           (call(Script, Hash, Args),
            CoinOut = CoinX)).

tx1(Hash,args(Signature,PubKey,PubKeyAddr)) :-
    ec:address(PubKey,PubKeyAddr),
    ec:validate(PubKey,Hash,Signature).

reward(PubKeyAddr) :-
    reward(_, Coin),
    tx(Coin, _, tx1, args(_,_,PubKeyAddr), _).

)PROG";

    auto &tx_5 = interp.get_predicate(con_cell("user",0), con_cell("tx",5));
    if (tx_5.empty()) {
        // Nope, so load it
        interp.load_program(lib);
	auto &tx_5_verify = interp.get_predicate(con_cell("user",0), con_cell("tx",5));
	assert(!tx_5_verify.empty());

	// The non-existance of tx_5 means we've created a genesis block
	// So we'll capture the state so it cannot be removed/discarded.
        reinterpret_cast<global_interpreter &>(interp).get_global().advance();
    }
    interp.compile();
}

void global_interpreter::preprocess_hashes(term t) {
    static const common::con_cell op_comma(",", 2);
    static const common::con_cell op_semi(";", 2);
    static const common::con_cell op_imply("->", 2);
    static const common::con_cell op_clause(":-", 2);    

    if (t.tag() != common::tag_t::STR) {
        return;
    }
    
    auto f = functor(t);
    // Scan for :- and subsitute the hash for body
    if (f == op_clause) {
        auto head = arg(t, 0);
	if (head.tag() == tag_t::STR) {
	    // Name must be 'p', arity doesn't matter
	    if (atom_name(functor(head)) != "p") {
		return;
	    }
	    auto hash_var = arg(head, 0);
	    if (!hash_var.tag().is_ref()) {
		return;
	    }
	    uint8_t hash[ec::builtins::RAW_HASH_SIZE];
	    // Important that the hash covers the entire predicate clause
	    // p(X, ...) :- Body, as the user control what variables to
	    // expose in Body (typically signature variables and fees)
	    if (!ec::builtins::get_hashed_2_term(*this, t, hash)) {
		return;
	    }
	    term hash_term = new_big(ec::builtins::RAW_HASH_SIZE*8);
	    set_big(hash_term, hash, ec::builtins::RAW_HASH_SIZE);
	    if (!unify(hash_var, hash_term)) {
		return;
	    }
	    std::for_each(begin(head), end(head),
			  [this](term t) {
			      if (t.tag().is_ref()) {
				  ref_cell &v = reinterpret_cast<ref_cell &>(t);
				  p_vars_in_goal_.insert(v);
			      }
			  });
	}
	return;
    }

    if (f == op_comma || f == op_semi || f == op_imply) {
        preprocess_hashes(arg(t, 0));
	preprocess_hashes(arg(t, 1));
    }
}

//
// A goal is typically a transaction of the form:
// (Signature = <constant>, p(X, Signature, Fee) :- ... Body ...)
//
// The variables inside Body that do not occur in head of 'p' are considered
// hidden and should not be referenced outside 'p'.
//
// We keep track of what 'p' a hidden var is referenced from.
// var => {... of p's ... }  (if multiple p's then that an indication of
// cross referencing a hidden variable between p's which is not allowed)
//
void global_interpreter::hidden_vars(term t, std::unordered_map<term, std::unordered_set<term> > &vars)
{
    static const common::con_cell op_comma(",", 2);
    static const common::con_cell op_semi(";", 2);
    static const common::con_cell op_imply("->", 2);
    static const common::con_cell op_clause(":-", 2);

    if (t.tag() != common::tag_t::STR) {
        return;
    }
    
    auto f = functor(t);
    // Scan for :- and subsitute the hash for body
    if (f == op_clause) {
        auto head = arg(t, 0);
	if (head.tag() == tag_t::STR) {
	    // Name must be 'p', arity doesn't matter
	    auto h = functor(head);
	    if (atom_name(h) != "p") {
		return;
	    }
	    auto n = h.arity();
	    if (n < 1) {
		return;
	    }
	    auto body = arg(t, 1);
	    auto head_vars = vars_of(head);
	    // The hash (always first) var is also hidden despite
	    // it is in the head.
	    head_vars.erase(arg(head,0));

	    // For all variables in body that does not occur in head = hidden var
	    std::for_each( begin(body),
			   end(body),
			   [&vars,&head_vars,t](const term v) {
			       if (v.tag().is_ref()) {
				   if (!head_vars.count(v)) {
				       vars[v].insert(t);
				   }
			       }
			   });
	}
	return;
    }

    if (f == op_comma || f == op_semi || f == op_imply) {
        hidden_vars(arg(t, 0), vars);
	hidden_vars(arg(t, 1), vars);
	return;
    }
}

void global_interpreter::check_vars_cross_ref(term t, std::unordered_map<term, std::unordered_set<term > > &all_hidden) {
    for (auto &v : all_hidden) {
	if (v.second.size() != 1) {
	    std::stringstream ss;
	    ss << "Attempt to cross reference hidden variable " << to_string(t);
	    throw global_hidden_var_exception(ss.str());
	}
    }
}

// Make sure hidden vars in each p(...) :- Body are hidden, i.e.
// no other goal/term is allowed to access them.
// This method does not the cross reference checking between p's; there's
// a separate method for that (check_vars_cross_ref)
void global_interpreter::check_vars(term t, std::unordered_map<term, std::unordered_set<term > > &all_hidden) {
    static const common::con_cell op_comma(",", 2);
    static const common::con_cell op_semi(";", 2);
    static const common::con_cell op_imply("->", 2);
    static const common::con_cell op_clause(":-", 2);

    if (t.tag() != common::tag_t::STR) {
        return;
    }

    auto f = functor(t);

    // Skip checking for 'p' itself (it's already been checked
    // via check_vars_cross_ref)
    if (f == op_clause) {
        auto head = arg(t, 0);
	if (head.tag() == tag_t::STR) {
	    auto h = functor(head);
	    if (atom_name(h) == "p") {
		auto n = h.arity();
		if (n >= 1) {
		    return;
		}
	    }
	}
    }

    if (f == op_comma || f == op_semi || f == op_imply) {
        check_vars(arg(t, 0), all_hidden);
	check_vars(arg(t, 1), all_hidden);
	return;
    }

    std::for_each( begin(t),
		   end(t),
		   [this,&all_hidden](const term v) {
		      if (v.tag().is_ref()) {
			  auto it = all_hidden.find(v);
			  if (it != all_hidden.end()) {
			      if (!it->second.empty()) {
				  std::stringstream ss;
				  ss << "Attempt to reference hidden variable " << to_string(v);
				  throw global_hidden_var_exception(ss.str());
			      }
			  }
		      }
		   } );
}
	
bool global_interpreter::execute_goal(term goal) {
    std::unordered_map<term, std::unordered_set<term> > all_hidden;
    hidden_vars(goal, all_hidden);

    check_vars_cross_ref(goal, all_hidden);
    check_vars(goal, all_hidden);

    // Check if term is a clause:
    // p(X, ...) :- Body
    // Then we compute the hash of Body (with X unbound) and bind X to the
    // hashed value. Then we apply commit on Body. This enables us to solve
    // the malleability problem; no one can tamper with the transaction
    // body if we check the digital signature based on this hash (that can
    // be precomputed by the sender in advance.)
    //
    preprocess_hashes(goal);

    bool r = execute(goal, false);

    return r;
}

bool global_interpreter::execute_goal(buffer_t &serialized, bool silent)
{
    term_serializer ser(*this);

    clear_names();
    
    try {
        term goal = ser.read(serialized);

	if (!execute_goal(goal)) {
	    reset();
	    return false;
	}

	if (silent) {
	    stop();
	} else {
	    serialized.clear();
	    ser.write(serialized, goal);
	}
	return true;
    } catch (serializer_exception &ex) {
	reset();
        throw ex;
    } catch (interpreter_exception &ex) {
	reset();
        throw ex;
    }
}

void global_interpreter::execute_cut() {
    set_b0(nullptr); // Set cut point to top level
    interpreter_base::cut();
    interpreter_base::clear_trail();
}

int64_t global_interpreter::available_fees() {
    int64_t fees = 0;
    for (auto r : p_vars_in_goal()) {
	auto dr = deref(r);
	if (coin::is_native_coin(*this, dr) && !coin::is_coin_spent(*this, dr)) {
	    fees += coin::coin_value(*this, dr);
	}
    }
    return fees;
}

void global_interpreter::discard_changes() {
    // Discard program changes
    for (auto &qn : updated_predicates_) {
	clear_predicate(qn);
	remove_compiled(qn);
    }
    updated_predicates_.clear();

    // Restore old predictaes (if any)
    for (auto &p : old_predicates_) {
	restore_predicate(p);
    }
    old_predicates_.clear();

    // Tell that no program changes have occurred so that
    // heap can be properly trimmed.
    heap_limit(old_heap_size_);

    // Unwind everything on heap (unbind variables, etc.)
    reset();

    // Discard heap changes
    for (auto &m : modified_blocks_) {
	auto block_index = m.first;
	auto block = m.second;
	block->clear_changed();
	block_cache_.erase(block_index);
    }
    modified_blocks_.clear();

    // Discard new symbols
    for (auto &sym : new_atoms_) {
	clear_atom_index(sym.second, sym.first);
    }
    new_atom_indices_.clear();

    // Reset symbol counters
    next_atom_id_ = start_next_atom_id_;
    next_predicate_id_ = start_next_predicate_id_;
    new_predicates_ = 0;
    trim_heap_safe(old_heap_size_);
    current_block_ = nullptr;
    current_block_index_ = static_cast<size_t>(-2);
    set_head_block(nullptr);
    heap_get(old_heap_size_-1);
    current_block_ = get_head_block();
    modified_closures_.clear();
    get_stacks().reset(); // Clear trail and stacks
}
    
bool global_builtins::operator_clause_2(interpreter_base &interp0, size_t arity, term args[] )
{
    auto &interp = to_global(interp0);

    term head = args[0];
    term body = args[1];

    if (head.tag() != tag_t::STR) {
	auto p = interp.functor(head);
	if (interp.atom_name(p) != "p" || p.arity() < 1) {
	    throw interpreter_exception_wrong_arg_type(":-/2: Head of clause must be 'p(Hash,...)'; was " + interp.to_string(head));
	}
    }

    // Setup new environment and where to continue
    interp.allocate_environment<ENV_NAIVE>();
	
    interp.set_p(code_point(body));
    interp.set_cp(code_point(interpreter_base::EMPTY_LIST));

    return true;
}

void global_interpreter::setup_builtins()
{
    load_builtin(con_cell(":-",2), &global_builtins::operator_clause_2);
}

size_t global_interpreter::new_atom(const std::string &atom_name)
{
    // Let's see if this symbol is already known
    auto index = get_global().db_get_symbol_index(atom_name);
    if (index != 0) {
	return index;
    }

    size_t atom_id = next_atom_id_;
    next_atom_id_++;
    set_atom_index(atom_name, atom_id);
    new_atoms_.push_back(std::make_pair(atom_id, atom_name));
    new_atom_indices_[atom_name] = atom_id;
    return atom_id;
}

void global_interpreter::load_atom_name(size_t index)
{
    auto name = get_global().db_get_symbol_name(index);
    if (!name.empty()) {
	set_atom_index(name, index);
    }
}

void global_interpreter::load_atom_index(const std::string &name)
{
    auto it = new_atom_indices_.find(name);
    if (it != new_atom_indices_.end()) {
	return;
    }
    
    auto index = get_global().db_get_symbol_index(name);
    if (index != 0) {
	set_atom_index(name, index);
    } else {
	new_atom(name);
    }
}

heap_block * global_interpreter::db_get_heap_block(size_t block_index) {
    common::heap_block *block = get_global().db_get_heap_block(block_index);
    block_cache_.insert(block_index, block);
    current_block_ = block;
    return block;
}

term global_interpreter::get_frozen_closure(size_t addr)
{
    auto it = modified_closures_.find(addr);
    if (it != modified_closures_.end()) {
	return it->second;
    }
    return get_global().db_get_closure(addr);
}

void global_interpreter::clear_frozen_closure(size_t addr)
{
    term cl = get_frozen_closure(addr);
    bool is_frozen = cl != EMPTY_LIST;
    if (!is_frozen) {
	return;
    }
    internal_clear_frozen_closure(addr);
    
    // Check if frozen closure is in modified list, then remove it
    auto it = modified_closures_.find(addr);
    if (it != modified_closures_.end()) {
	new_frozen_closures_--;
	modified_closures_.erase(addr);
	return;
    }
    // We need to remove it from the database, so we annotate for it
    modified_closures_[addr] = term();
    new_frozen_closures_--;
}

void global_interpreter::set_frozen_closure(size_t addr, term closure)
{
    internal_set_frozen_closure(addr, closure);
    modified_closures_[addr] = closure;
    new_frozen_closures_++;
}

void global_interpreter::get_frozen_closures(size_t from_addr,
					     size_t to_addr,
					     size_t max_closures,
			     std::vector<std::pair<size_t,term> > &closures)
{
    auto root = get_global().get_blockchain().closure_root();
    size_t k = max_closures;

    bool reversed = heap_size() == from_addr && to_addr <= from_addr;

    const auto none = term();

    auto &closure_db = get_global().get_blockchain().closure_db();
    
    auto it1 = reversed ? closure_db.end(root) : 
	                  closure_db.begin(root, from_addr);
    if (reversed) --it1;
    size_t prev_last_addr = from_addr;
    size_t last_addr = from_addr;
    while (!it1.at_end() && k > 0) {
	auto &leaf = *it1;
	if (reversed) {
	    if (leaf.key() < to_addr) {
		break;
	    }
	} else {
	    if (leaf.key() >= to_addr) {
		break;
	    }
	}
	prev_last_addr = last_addr;
	last_addr = leaf.key();

	// Process modified closures in between
	if (reversed) {
	    auto it2 = modified_closures_.lower_bound(last_addr+1);
	    while (it2 != modified_closures_.end()) {
		if (it2->first >= prev_last_addr) {
		    break;
		}
		if (it2->second != none) {
		    closures.push_back(*it2);
		}
		++it2;
	    }
	} else {
	    auto it2 = modified_closures_.lower_bound(prev_last_addr+1);
	    while (it2 != modified_closures_.end()) {
		if (it2->first >= last_addr) {
		    break;
		}
		if (it2->second != none) {
		    closures.push_back(*it2);
		}
		++it2;
	    }
	}

	auto mod = modified_closures_.find(leaf.key());
	if (mod != modified_closures_.end()) {
	    ++it1;
	    if (mod->second == none) {
		continue;
	    }
	    k--;
	    closures.push_back(*mod);
	    continue;
	}

	assert(it1->custom_data_size() == sizeof(uint64_t));
	cell cl(common::read_uint64(it1->custom_data()));
	closures.push_back(std::make_pair(leaf.key(), cl));
	k--;
	if (reversed) --it1; else ++it1;
    }

    if (!reversed) {
	// At this point we check modified closures from last_addr
	auto mod = modified_closures_.lower_bound(last_addr);
	while (mod != modified_closures_.end() && k > 0) {
	    if (mod->first >= to_addr) {
		break;
	    }
	    if (mod->second != none) {
		closures.push_back(*mod);
		k--;
	    }
	    ++mod;
	}
    } else if (last_addr > 0) {
	auto mod0 = modified_closures_.lower_bound(last_addr-1);
	std::map<size_t, term>::reverse_iterator mod(mod0);
	while (mod != modified_closures_.rend() && k > 0) {
	    if (mod->first < to_addr) {
		break;
	    }
	    if (mod->second != none) {
		closures.push_back(*mod);
		k--;
	    }
	    ++mod;
	}
    }
}

void global_interpreter::commit_symbols()
{
    if (new_atoms_.empty()) {
	 return;
    }
    
    for (auto &a : new_atoms_) {
	size_t symbol_index = a.first;
        const std::string &symbol_name = a.second;
	get_global().db_set_symbol_index(symbol_index, symbol_name);
    }

    new_atoms_.clear();
    new_atom_indices_.clear();
}

void global_interpreter::commit_closures()
{
    const auto none = term();
    for (auto &cl : modified_closures_) {
	if (cl.second == none) {
	    get_global().db_remove_closure(cl.first);
	} else {
	    get_global().db_set_closure(cl.first, cl.second);
	}
    }
    modified_closures_.clear();
    new_frozen_closures_ = 0;
}

size_t global_interpreter::num_predicates() const {
    return get_global().db_num_predicates() + new_predicates_;
}

size_t global_interpreter::num_symbols() {
    return get_global().db_num_symbols() + new_atoms_.size();
}

size_t global_interpreter::num_frozen_closures() const {
    return get_global().db_num_frozen_closures() + new_frozen_closures_;
}

void global_interpreter::commit_heap()
{
    std::vector<heap_block*> blocks;
    for (auto &e : modified_blocks_) {
        auto *block = e.second;
	blocks.push_back(block);
    }
    std::sort(blocks.begin(), blocks.end(),
	   [](heap_block *a, heap_block *b) { return a->index() < b->index();});
    for (auto *block : blocks) {
	get_global().db_set_heap_block(block->index(), block);
	block->clear_changed();
	block_cache_.insert(block->index(), block);
    }
    modified_blocks_.clear();
    old_heap_size_ = heap_size();
}

void global_interpreter::init_from_heap_db()
{
    auto &hdb = get_global().get_blockchain().heap_db();
    if (hdb.is_empty()) {
	new_heap();
        return;
    }
    auto root = get_global().get_blockchain().heap_root();
    if (root.is_zero() ||
	get_global().get_blockchain().heap_db().num_entries(root) == 0) {
	new_heap();
	return;
    }
		
    auto it = hdb.end(root);
    --it;
    if (it.at_end()) {
	new_heap();
	return;
    }
    assert(!it.at_end());
    auto &leaf = *it;
    auto block_index = leaf.key();
    auto &block = get_heap_block(block_index);
    size_t heap_size = block_index * heap_block::MAX_SIZE + block.size();
    heap_set_size(heap_size);
    set_head_block(&block);
}

void global_interpreter::new_heap() {
    assert(heap_size() == 0);
    auto &block = get_heap_block(common::heap::NEW_BLOCK);
    set_head_block(&block);
} 

void global_interpreter::init_from_symbols_db()
{
    next_atom_id_ = get_global().db_num_symbols();
    start_next_atom_id_ = next_atom_id_;
}
    
void global_interpreter::init_from_program_db()
{
    next_predicate_id_ = get_global().db_num_predicates();
    start_next_predicate_id_ = next_predicate_id_;
}
    
void global_interpreter::commit_program()
{    
    if (updated_predicates_.empty()) {
	 return;
    }

    for (auto &qn : updated_predicates_) {
        auto &pred = get_predicate(qn);
	auto &mclauses = pred.get_clauses();
	std::vector<term> clauses;
	for (auto &mc : mclauses) {
	    clauses.push_back(mc.clause());
	}
	get_global().db_set_predicate(qn, clauses);
    }

    updated_predicates_.clear();
    old_predicates_.clear();
    new_predicates_ = 0;
}

}}
