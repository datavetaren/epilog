#include "../common/term_env.hpp"
#include "../common/garbage_collector.hpp"
#include "interpreter_base.hpp"
#include "builtins_fileio.hpp"
#include "wam_interpreter.hpp"
#include <boost/filesystem.hpp>
#include <boost/timer/timer.hpp>
#include <boost/algorithm/string.hpp>
#include "interpreter.hpp"

#define PROFILER 0

namespace epilog { namespace interp {

using namespace epilog::common;

const common::term code_point::fail_term_ = common::term(common::ref_cell(0));
const common::con_cell interpreter_base::COMMA = common::con_cell(",",2);
const common::con_cell interpreter_base::EMPTY_LIST = common::con_cell("[]",0);
const common::con_cell interpreter_base::IMPLIED_BY = common::con_cell(":-",2);
const common::con_cell interpreter_base::ACTION_BY = common::con_cell(":-",1);
const common::con_cell interpreter_base::USER_MODULE = common::con_cell("user",0);
const common::con_cell interpreter_base::COLON = common::con_cell(":",2);

void managed_clauses::add_clause(const managed_clause &mclause, clause_position pos) {
    if (pos == FIRST_CLAUSE) {
	clauses_.insert(clauses_.begin(), mclause);
	clear_index();
    } else {
	size_t i = clauses_.size();
	clauses_.push_back(mclause);
	add_index(i);
    }
    num_clauses_++;
}

bool managed_clauses::remove_clause(common::term clause) {
    auto head = interp_->clause_head(clause);
    auto &index = get_indexed(head);
    size_t n = index.size();
    for (size_t i = 0; i < n; i++) {
	size_t clause_index = index[i];
	if (clause_index == std::numeric_limits<size_t>::max()) continue;
	auto &mclause = clauses_[clause_index];
	if (mclause.is_erased()) {
	    continue;
	}
	if (clause == mclause.clause()) {
	    mclause.erase();
	    num_clauses_--;
	    return true;
	}
    }
    return false;
}

/*
bool managed_clauses::remove_all_clauses(common::term head) {
    bool ok = false;
    auto first_arg = interp_->get_first_arg(head);
    auto &index = get_indexed(first_arg);
    size_t n = index.size();
    for (size_t i = 0; i < n; i++) {
	size_t clause_index = index[i];
	if (clause_index == std::numeric_limits<size_t>::max()) continue;
	auto &mclause = clauses_[clause_index];
	auto clause = mclause.clause();
	if (interp_->can_unify(head, clause)) {
	    mclause.erase();
	    num_clauses_--;
	    ok = true;
	}
    }
    return ok;
}
*/

void managed_clauses::remove_clauses(const std::vector<size_t> &indices) {
    size_t n = indices.size();
    for (size_t i = 0; i < n; i++) {
	size_t index = indices[i];
	auto &mclause = get_clause(index);
	if (!mclause.is_erased()) {
	    remove_index(index);
	    mclause.erase();
	    num_clauses_--;
	}
    }
}
	
void managed_clauses::add_index(size_t i) {
    increment_performance_count();
    auto &mclause = clauses_[i];
    auto clause = mclause.clause();
    auto first_arg = interp_->clause_first_arg(clause);
    all_.push_back(i);
    if (first_arg == term()) {
	return;
    }
    auto tag = first_arg.tag();
    if (tag.is_ref()) {
	has_vars_ = true;
    } else {
        auto tag_value = static_cast<size_t>(tag);
	auto &map = index_[tag_value];
	auto h = interp_->simple_hash(first_arg);
	auto &indexed = map[h];
	indexed.push_back(i);
    }
}

void managed_clauses::remove_index(size_t i) {
    increment_performance_count();    
    auto &mclause = clauses_[i];
    auto clause = mclause.clause();
    auto first_arg = interp_->clause_first_arg(clause);
    if (first_arg == term()) {
	return;
    }
    all_[i] = std::numeric_limits<size_t>::max();
    auto tag = first_arg.tag();
    if (tag.is_ref()) {
	for (auto &map : index_) {
	    for (auto &p2 : map) {
		auto &indexed = p2.second;
		auto it = std::find(indexed.begin(), indexed.end(), i);
	        indexed.erase(it);
	    }
	}
    } else {
        auto tag_value = static_cast<size_t>(tag);
	auto &map = index_[tag_value];
	auto h = interp_->simple_hash(first_arg);
	auto &indexed = map[h];
	auto it = std::find(indexed.begin(), indexed.end(), i);
        if (it != indexed.end()) {
	    indexed.erase(it);
	}
    }
}

std::vector<size_t> managed_clauses::all_var_based_clauses() const {
    std::vector<size_t> var_based;
    auto n = clauses_.size();
    for (size_t i = 0; i < n; i++) {
	auto clause = clauses_[i].clause();
	auto first_arg = interp_->clause_first_arg(clause);
	if (first_arg == term()) {
	    continue;
	}
	auto tag = first_arg.tag();
	if (tag.is_ref()) {
	    var_based.push_back(i);
	}
    }
    return var_based;
}

void managed_clauses::do_index() const
{
    if (is_indexed_) {
	return;
    }
    has_vars_ = false;
    for (size_t i = 0; i < NUM_TAGS; i++) {
	index_[i].clear();
    }
    all_.clear();
    auto n = clauses_.size();
    auto var_based = all_var_based_clauses();
    for (size_t i = 0; i < n; i++) {
	auto mclause = clauses_[i];
	all_.push_back(i);
	if (mclause.is_erased()) continue;
	auto clause = mclause.clause();
	auto first_arg = interp_->clause_first_arg(clause);
	if (first_arg == term()) {
	    continue;
	}
	auto tag = first_arg.tag();
	if (tag.is_ref()) {
	    has_vars_ = true;
	    continue;
	}
	auto tag_value = static_cast<size_t>(tag);
	auto &second_indexing_map = index_[tag_value];
	auto h = interp_->simple_hash(first_arg);
	auto &indexed = second_indexing_map[h];
	for (auto j : var_based) {
	    if (i > j) {
		indexed.push_back(i);
	    }
	    indexed.push_back(j);
	}
    }
    is_indexed_ = true;
}
	
meta_context::meta_context(interpreter_base &i, meta_fn mfn)
{
    fn = mfn;
    old_m = i.m();
    old_top_b = i.top_b();
    old_b = i.b();
    old_b0 = i.b0();
    old_top_e = i.top_e();
    old_e = i.e();
    old_e_kind = i.e_kind();
    old_p = i.p();
    old_cp = i.cp();
    old_qr = i.qr();
    old_hb = i.get_register_hb();
}

interpreter_base::interpreter_base(const std::string &name) : retain_state_between_queries_(false), arith_(*this), locale_(*this), name_(name)
{
    init();
}

void interpreter_base::total_reset()
{
    retain_state_between_queries_ = false;
    term_env::reset();
    secondary_env_.reset();

    code_db_.clear();
    builtins_.clear();
    program_db_.clear();
    module_db_.clear();
    module_db_set_.clear();
    program_predicates_.clear();
    updated_predicates_.clear();
    module_meta_db_.clear();
    if (stack_) delete [] stack_;
    stack_ = nullptr;
    for (auto e : managed_data_) delete e.second;
    managed_data_.clear();

    init();
    reset_files();
    frozen_closures_.clear();
    reset_accumulated_cost();
}

void interpreter_base::init()
{
    delayed_ready_ = 0;
    has_updated_predicates_ = false;
    in_critical_section_ = false;
    arith_.total_reset();
    locale_.total_reset();
    current_module_ = con_cell("system",0);
    persistent_password_ = false;

    debug_ = false;
    track_cost_ = false;
    file_id_count_ = 3;
    num_of_args_= 0;
    memset(&register_ai_, 0, sizeof(register_ai_));
    stack_ = reinterpret_cast<word_t *>(new char[MAX_STACK_SIZE]);
    num_y_fn_ = &num_y;
    save_state_fn_ = &save_state;
    restore_state_fn_ = &restore_state;
    standard_output_ = nullptr;
    prepare_execution();

    // These will be loaded into the system module
    builtins::load(*this);

    register_top_b_ = nullptr;
    register_top_e_ = nullptr;
    register_b_ = nullptr;
    register_b0_ = nullptr;
    register_e_ = nullptr;
    register_e_kind_ = ENV_NAIVE;
    register_p_.reset();
    register_cp_.reset();
    register_m_ = nullptr;
    num_of_args_ = 0;
    for (size_t i = 0; i < MAX_ARGS; i++) {
       register_ai_[i] = term(0);
    }
    maximum_cost_ = std::numeric_limits<uint64_t>::max();
    heap_limit();
    register_top_hb_ = heap_size();
    register_top_tr_ = trail_size();

    set_current_module(con_cell("user",0));

    register_pr_ = qname(current_module(), EMPTY_LIST);
}

interpreter_base::~interpreter_base()
{
    for (auto &p : managed_data_) {
        auto md = p.second;
	delete md;
    }
    managed_data_.clear();
    arith_.unload();
    close_all_files();
    register_cp_.reset();
    register_qr_ = term();
    builtins_.clear();
    program_db_.clear();
    module_db_.clear();
    module_db_set_.clear();
    module_meta_db_.clear();
    program_predicates_.clear();
}

void interpreter_base::foreach_stack_frame(interpreter_base::stack_frame_visitor &callb)
{
    enum entry_kind_t {
	ENTRY_ENV_NAIVE,
	ENTRY_ENV_WAM,
	ENTRY_ENV_FROZEN,
	ENTRY_CHOICE_POINT,
	ENTRY_META_CONTEXT
    };
    struct entry_t {
	entry_kind_t kind;
	union {
	    void *ptr;
	    environment_base_t *env_base;
	    environment_naive_t *env_naive;
	    environment_t *env_wam;
	    environment_frozen_t *env_frozen;
	    choice_point_t *choice;
	    meta_context *meta;
	};
	entry_t(environment_saved_t sv) {
	    switch (sv.kind()) {
	    case ENV_NAIVE:
		kind = ENTRY_ENV_NAIVE;
		env_naive = reinterpret_cast<environment_naive_t *>(sv.ce0());
		break;
	    case ENV_WAM:
		kind = ENTRY_ENV_WAM;
		env_wam = reinterpret_cast<environment_t *>(sv.ce0());
		break;
	    case ENV_FROZEN:
		kind = ENTRY_ENV_FROZEN;
		env_frozen = reinterpret_cast<environment_frozen_t *>(sv.ce0());
		break;		
	    }
	}
	entry_t(environment_naive_t *e1) : kind(ENTRY_ENV_NAIVE), env_naive(e1) { }
	entry_t(environment_t *e1) : kind(ENTRY_ENV_WAM), env_wam(e1) { }
	entry_t(environment_frozen_t *e1) : kind(ENTRY_ENV_FROZEN), env_frozen(e1) { }
	entry_t(choice_point_t *cp) : kind(ENTRY_CHOICE_POINT), choice(cp) { }
	entry_t(meta_context *m) : kind(ENTRY_META_CONTEXT), meta(m) { }
    };
    
    std::unordered_set<void *> visited;
    std::stack<entry_t> worklist;


    auto do_push = [&](const entry_t &e) {
	worklist.push(e);
    };

    if (e0()) {
	switch (e_kind()) {
	case ENV_NAIVE: do_push(entry_t(ee())); break;
	case ENV_WAM: do_push(entry_t(e())); break;
	case ENV_FROZEN: do_push(entry_t(ef())); break;
	}
    }
    if (b()) do_push(entry_t(b()));
    if (b0()) do_push(entry_t(b0()));
    if (m()) do_push(entry_t(m()));

    while (!worklist.empty()) {
	auto ent = worklist.top();
	worklist.pop();
	if (ent.ptr == nullptr) {
	    continue;
	}
	if (visited.count(ent.ptr)) {
	    continue;
	}
	visited.insert(ent.ptr);
	switch (ent.kind) {
	    case ENTRY_ENV_NAIVE:
	        callb.visit_naive_environment(ent.env_naive);
		break;
	    case ENTRY_ENV_WAM:
	        callb.visit_wam_environment(ent.env_wam);
		break;
	    case ENTRY_ENV_FROZEN:
		callb.visit_frozen_environment(ent.env_frozen);
		break;
   	    case ENTRY_CHOICE_POINT:
		callb.visit_choice_point(ent.choice);
		break;
	    case ENTRY_META_CONTEXT:
	        callb.visit_meta_context(ent.meta);
		break;
	}
	switch (ent.kind) {
	    case ENTRY_ENV_FROZEN:
	    case ENTRY_ENV_NAIVE:
		do_push(ent.env_naive->b0);
		// Fall through
	    case ENTRY_ENV_WAM:
		do_push(entry_t(ent.env_base->ce));
		break;
	    case ENTRY_CHOICE_POINT:
		do_push(entry_t(ent.choice->ce));
		do_push(entry_t(ent.choice->m));
		do_push(entry_t(ent.choice->b));
		do_push(entry_t(ent.choice->b0));
		break;
	    case ENTRY_META_CONTEXT:
	        do_push(entry_t(ent.meta->old_m));
	        do_push(entry_t(ent.meta->old_b));
	        do_push(entry_t(ent.meta->old_b0));
	        do_push(entry_t(environment_saved_t(ent.meta->old_e, ent.meta->old_e_kind)));
		break;
	}
    }
}
	
void interpreter_base::set_current_module(con_cell mod)
{
    static con_cell SYSTEM("system",0);
    bool is_new_module = !is_existing_module(mod) && mod != SYSTEM;
    get_module(mod);
    current_module_ = mod;
    if (is_new_module) {
        use_module(SYSTEM);
    }
}
    
void interpreter_base::reset()
{
    unwind_to_top_choice_point();
    while (has_meta_context()) {
	auto *m = get_current_meta_context();
	m->fn(*this, meta_reason_t::META_DELETE);
	unwind_to_top_choice_point();
    }
    bool cont = false;
    do {
        cont = false;
        while (b() != top_b()) {
	    auto *ch = b();
	    reset_to_choice_point(ch);
	    set_b(ch->b);
	    set_register_hb(b());
	}
	if (top_b() != nullptr) {
	    reset_to_choice_point(top_b());	  
	    set_top_b(top_b()->b);
	    cont = true;
	}
    } while (cont);

    register_e_ = nullptr;
    register_e_kind_ = ENV_NAIVE;
    register_top_b_ = nullptr;
    register_top_e_ = nullptr;
    register_b_ = nullptr;
    register_b0_ = nullptr;

    if (!persistent_password_) {
        clear_secret();
    }

    // Try to cleanup all bindings that happened to variables at the top-level
    // (e.g. the entry variables for the query.)
    set_register_hb(static_cast<size_t>(0));
    tidy_trail();
}

void interpreter_base::update_pr() {
}
	
std::string code_point::to_string(const interpreter_base &interp) const
{
    if (has_wam_code()) {
	std::stringstream ss;
	wam_code()->print(ss, static_cast<wam_interpreter &>(const_cast<interpreter_base &>(interp)));
	return ss.str();
    } else if (is_fail()) {
	return "fail";
    } else {
        std::string s;
        if (module() != interpreter_base::USER_MODULE) {
	    s += interp.to_string(module());
	    s += ":";
        }
	auto t = term_code();
	s += interp.to_string(t);
	if (t.tag() == common::tag_t::CON) {
	    auto &c = reinterpret_cast<con_cell &>(t);
	    s += "/" + boost::lexical_cast<std::string>(c.arity());
	}
	return s;
    }
}

void interpreter_base::close_all_files()
{
    std::vector<size_t> ids;
    for (auto f : open_files_) {
	if (f.second->get_id() >= 3) {
	    ids.push_back(f.second->get_id());
	}
    }
    for (auto id : ids) {
	close_file_stream(id);
    }
}

void interpreter_base::reset_files()
{
    close_all_files();
    file_id_count_ = 3;
}

bool interpreter_base::is_file_id(size_t id) const
{
    return open_files_.find(id) != open_files_.end();
}

file_stream & interpreter_base::new_file_stream(const std::string &path)
{
    size_t new_id = file_id_count_;
    file_stream *fs = new file_stream(*this, file_id_count_, path);
    file_id_count_++;
    open_files_[new_id] = fs;
    return *fs;
}

void interpreter_base::close_file_stream(size_t id)
{
    if (open_files_.find(id) == open_files_.end()) {
        return;
    }
    file_stream *fs = open_files_[id];
    open_files_.erase(id);
    delete fs;
}

file_stream & interpreter_base::get_file_stream(size_t id)
{
    return *open_files_[id];
}

file_stream & interpreter_base::standard_output()
{
    if (standard_output_ == nullptr) {
	standard_output_ = new file_stream(*this, 0, "<stdout>");
	standard_output_->open(std::cout);
    }
    return *standard_output_;
}

void interpreter_base::tell_standard_output(file_stream &fs)
{
    standard_output_stack_.push(standard_output_);
    standard_output_ = &fs;
}

void interpreter_base::told_standard_output()
{
    standard_output_ = standard_output_stack_.top();
    standard_output_stack_.pop();
}

bool interpreter_base::has_told_standard_outputs()
{
    return !standard_output_stack_.empty();
}

void interpreter_base::preprocess_freeze(term t)
{
    qname old_pr = get_pr();
    auto head = clause_head(t);
    set_pr( qname(EMPTY_LIST, EMPTY_LIST) );
    if (is_functor(head)) {
	con_cell f = functor(head);
	con_cell name = f;
	con_cell module = current_module();
	if (f == COLON) {
	    auto m = arg(head, 0);
	    if (is_functor(m)) {
		module = functor(m);
	    }
	    if (is_functor(arg(head,1))) {
		name = functor(arg(head,1));
	    }
	}
	set_pr(qname(module, name));
    }

    try {
	term body = clause_body(t);
	preprocess_freeze_body(body);
    } catch (std::exception &) {
	set_pr(old_pr);
	throw;
    }
    set_pr(old_pr);
}

void interpreter_base::preprocess_freeze_body(term goal)
{
    static const common::con_cell op_comma(",", 2);
    static const common::con_cell op_semi(";", 2);
    static const common::con_cell op_imply("->", 2);
    static const common::con_cell freeze("freeze", 2);

    if (goal.tag() != common::tag_t::STR) {
        return;
    }
    
    auto f = functor(goal);
    if (f == op_comma || f == op_semi || f == op_imply) {
        preprocess_freeze_body(arg(goal, 0));
	preprocess_freeze_body(arg(goal, 1));
	return;
    }
    if (f == freeze) {
        // Rewrite
        // freeze(Var, Body)
        // freeze(Var, '$freeze':<id>(<Predicate>, <Var> <Closure Vars>))
        // where:
        // '$freeze':'<id>'(<Predicate>, <Var> <Closure Vars> ...) :- Body.
        term freezeVar = arg(goal, 0);
	term freezeBody = arg(goal, 1);

	preprocess_freeze_body(freezeBody);

	set_arg(goal, 1, rewrite_freeze_body(freezeVar, freezeBody));
    }
}

term interpreter_base::rewrite_freeze_body(term freezeVar, term freezeBody)
{
    static const common::con_cell freeze_module("$freeze",0);
    static const common::con_cell op_clause(":-", 2);
    static const common::con_cell op_colon(":", 2);

    // Not a functor?
    if (freezeBody.tag() != common::tag_t::STR) {
        return freezeBody;
    }

    // Already rewritten? 
    if (functor(freezeBody) == op_colon) {
        if (arg(freezeBody, 0) == freeze_module) {
   	    return freezeBody;
	}
    }

    std::unordered_set<term> vars;
    std::vector<term> vars_list;
    std::unordered_map<term,term> varmap;

    vars_list.push_back(freezeVar);
    vars.insert(freezeVar);
    for (auto t : iterate_over(freezeBody)) {
      if (t.tag().is_ref()) {
	t = deref(t);
	if (vars.count(t) == 0) {
	  vars.insert(t);
	  vars_list.push_back(t);
	}
      }
    }

    auto qname = gen_predicate(freeze_module, vars_list.size()+3);
	
    auto head = new_str(qname.second);
    for (size_t i = 0; i < vars_list.size(); i++) {
      varmap[vars_list[i]] = arg(head,i+3);
    }

    // Remap vars (closure vars to new vars)
    for (auto t : iterate_over(freezeBody)) {
      if (t.tag() == common::tag_t::STR) {
	size_t num_args = functor(t).arity();
	for (size_t i = 0; i < num_args; i++) {
	  term ai = arg(t, i);
	  if (ai.tag().is_ref()) {
	    set_arg(t, i, varmap[ai]);
	  }
	}
      }
    }

    term freeze_clause = new_str(op_clause);
    set_arg(freeze_clause, 0, head);
    set_arg(freeze_clause, 1, freezeBody);
    
    auto old_module = current_module();
    set_current_module(freeze_module);
    load_clause(freeze_clause, LAST_CLAUSE);
    set_current_module(old_module);
    
    // get_predicate(qname).add_clause(*this, freeze_clause);

    auto closure_head = new_str(qname.second);
    // Store current executing predicate in the closure call
    // (we'll update the pr register when woken up)
    set_arg(closure_head, 0, get_pr().first.to_atom());
    set_arg(closure_head, 1, get_pr().second.to_atom());
    set_arg(closure_head, 2, int_cell(static_cast<int64_t>(get_pr().second.arity())));
    for (size_t i = 0; i < vars_list.size(); i++) {
        set_arg(closure_head, i+3, vars_list[i]);
    }
    
    auto closure_mod = new_str(op_colon);
    set_arg(closure_mod, 0, qname.first);
    set_arg(closure_mod, 1, closure_head);

    return closure_mod;
}
    
void interpreter_base::load_clause(term t, clause_position pos)
{
    syntax_check_clause(t);

    // We want to prerocess freeze/2 calls:
    //    foo(A,X,Y) :-
    //       bar(A,Y,Z),
    //       freeze(Z, (baz(X,Y,Z), baz2(Z,Z1))).
    // becomes:
    //    foo(A,X,Y) :-
    //       bar(A,Y,Z),
    //       '$closure'('$frz123',Z,X,Y).
    //
    //    '$frz123'(Z,X,Y) :-
    //       baz(X,Y,Z), baz2(Z,Z1).
    //
    // Where '$closure' will create a closure on the heap and
    // register its first variable as "frozen." It'll automatically
    // awake the closure and use the closure vars to create a call
    // to the generated clause '$frz123' (which is unique for each
    // closure construction.) This will minimize the amount of stuff
    // created on the heap; just the closure will be created and not
    // a duplicate of the clause body. We can also WAM compile this
    // generated predicate separately for accelaration.
    //
    // However, it won't be able to manage higher order constructions
    // of freeze; those will still be rather inefficient, but this covers
    // the most common case.
    preprocess_freeze(t);

    con_cell module = current_module_;

    // This is a valid clause. Let's lookup the functor of its head.

    term head = clause_head(t);

    if (head == ACTION_BY) {
	return;
    }

    con_cell pn = functor(head);

    if (pn == ACTION_BY) {
        return;
    }

    if (pn == COLON) {
	// This is a head with a module definition
	term module_term = arg(head, 0);
	term predicate_term = arg(head, 1);
	module = reinterpret_cast<con_cell &>(module_term);
	pn = functor(predicate_term);
	term body = clause_body(t);
	if (body == EMPTY_LIST) {
	    t = predicate_term;
	} else {
	    set_arg(t, 0, predicate_term);
	}
    }

    auto qn = std::make_pair(module, pn);

    // Required so that global interpreter loads the predicate
    // into memory.
    get_predicate(qn);
    
    updated_predicate_pre(qn);
    
    auto found = program_db_.find(qn);
    if (found == program_db_.end()) {
        program_db_[qn] = predicate(*this, qn);
	program_db_[qn].set_id(program_predicates_.size()+1);
	program_predicates_.push_back(qn);
    }

    auto it = program_db_.find(qn);
    if (it == program_db_.end()) {
	program_db_.insert(std::make_pair(qn, predicate(*this, qn)));
    }
    
    auto &pred = program_db_[qn];
    pred.add_clause(*this, t, pos);

    if (module_db_set_[module].count(qn) == 0) {
        module_db_set_[module].insert(qn);
	module_db_[module].push_back(qn);
    }

    set_code(qn, code_point(module, pn));

    internal_updated_predicate_post(qn);

    heap_limit();
}

void interpreter_base::remove_clauses(const qname &qn)
{
    auto &pred = get_predicate(qn);

    if (pred.num_clauses()) {
	updated_predicate_pre(qn);
	pred.clear();
	updated_predicate_post(qn);
    }
}

void interpreter_base::load_builtin(const qname &qn, builtin b)
{
    auto found = builtins_.find(qn);
    if (found != builtins_.end()) {
	auto &mod = module_db_[qn.first];
	auto it = std::find(mod.begin(), mod.end(), qn);
	if (it != mod.end()) {
	    mod.erase(it);
	}
    }
    
    builtins_[qn] = b;
    module_db_[qn.first].push_back(qn);
    set_code(qn, code_point(qn.second, b.fn(), b.is_recursive()));
}

void interpreter_base::remove_builtin(const qname &qn)
{
    auto found = builtins_.find(qn);
    if (found == builtins_.end()) {
	return;
    }
    builtins_.erase(found);
    auto &mod = module_db_[qn.first];
    auto found_in_module = std::find(mod.begin(), mod.end(), qn);
    if (found_in_module != mod.end()) {
	mod.erase(found_in_module);
    }
    remove_code(qn);
}

void interpreter_base::set_debug_enabled()
{
    auto old_mod = current_module();
    set_current_module(con_cell("system",0));
    load_builtin(functor("debug_on",0), &builtins::debug_on_0);
    load_builtin(functor("debug_off",0), &builtins::debug_off_0);
    load_builtin(functor("debug_check",0), &builtins::debug_check_0);
    load_builtin(functor("debug_predicate",1), &interpreter::debug_predicate_1);
    load_builtin(functor("program_state",0), &builtins::program_state_0);
    load_builtin(functor("program_state",1), &builtins::program_state_1);

    set_current_module(old_mod);
    use_module(con_cell("system",0));
}

void interpreter_base::enable_file_io()
{
    load_builtins_file_io();
}

void interpreter_base::set_current_directory(const std::string &dir)
{
    current_dir_ = dir;
}

const std::string & interpreter_base::get_current_directory() const
{
    return current_dir_;
}

std::string interpreter_base::get_full_path(const std::string &path) const
{
    if (path.empty()) {
	return path;
    }
    boost::filesystem::path path1(path);
    if (path1.is_relative()) {
	boost::filesystem::path abspath(current_dir_);
	abspath /= path;
	return abspath.string();
    } else {
	return path1.string();
    }
}

void interpreter_base::load_builtins_file_io()
{
    con_cell old_module = current_module();
    set_current_module( con_cell("system",0) );
    
    load_builtin(con_cell("open", 3), &builtins_fileio::open_3);
    load_builtin(con_cell("close", 1), &builtins_fileio::close_1);
    load_builtin(con_cell("read", 2), &builtins_fileio::read_2);
    load_builtin(functor("at_end_of_stream", 1), &builtins_fileio::at_end_of_stream_1);
    load_builtin(con_cell("write", 1), &builtins_fileio::write_1);
    load_builtin(con_cell("write", 2), &builtins_fileio::write_2);
    load_builtin(con_cell("nl", 0), &builtins_fileio::nl_0);
    load_builtin(con_cell("tell",1), &builtins_fileio::tell_1);
    load_builtin(con_cell("told",0), &builtins_fileio::told_0);
    load_builtin(con_cell("format",2), builtin(&builtins_fileio::format_2,true));
    load_builtin(con_cell("sformat",3), builtin(&builtins_fileio::sformat_3,true));
    set_current_module(old_module);
}

qname interpreter_base::gen_predicate(const common::con_cell module, size_t arity) {
    static const char ALPHABET[64] = {
      'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p',
      'q','r','s','t','u','v','w','x','y','z','A','B','C','D','E','F',
      'G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V',
      'W','X','Y','Z','0','1','2','3','4','5','6','7','8','9','-','+'};
    
    size_t num = unique_predicate_id(module);
    char name[7];
    size_t i;
    for (i = 0; i < 7 && num != 0; i++) {
        name[6-i] = ALPHABET[num & 0x3f];
	num >>= 6;
    }
    std::string atom_name(&name[6-(i-1)], i);

    common::con_cell p = functor(atom_name, arity);
    qname pn(module, p);
    program_predicates_.push_back(pn);

    if (module_db_set_[module].count(pn) == 0) {
        module_db_set_[module].insert(pn);
	module_db_[module].push_back(pn);
    }

    return pn;
}

void interpreter_base::syntax_check_program(term clauses)
{
    if (!is_list(clauses)) {
	throw syntax_exception_program_not_a_list(clauses);
    }

    for (auto clause : list_iterator(*this, clauses)) {
        syntax_check_clause(clause);
    }
}

void interpreter_base::syntax_check_clause(term clause)
{
    static const con_cell def(":-", 2);

    if (!is_functor(clause)) {
        throw syntax_exception_clause_bad_head(clause, "Head of clause is not a functor; was " + to_string(clause));
    }
    
    auto f = functor(clause);
    if (f == def) {
	auto head = arg(clause, 0);
	auto body = arg(clause, 1);
	syntax_check_head(clause, head);
	syntax_check_body(clause, body);
	return;
    }

    // This is a head only clause.

    syntax_check_head(clause, clause);
}

void interpreter_base::syntax_check_head(term clause, term head)
{
    static con_cell def(":-", 2);
    static con_cell semi(";", 2);
    static con_cell comma(",", 2);
    static con_cell cannot_prove("\\+", 1);

    if (!is_functor(head)) {
         throw syntax_exception_clause_bad_head(clause, "For '" + to_string(clause) + "': Head of clause is not a functor; was " + to_string(head));
    }

    // Head cannot be functor ->, ; , or \+
    auto f = functor(head);

    if (f == def || f == semi || f == comma || f == cannot_prove) {
        throw syntax_exception_clause_bad_head(clause, "For '" + to_string(clause) + "': Clause has an invalid head; cannot be '->', ';', ',' or '\\+'; was " + to_string(f));
    }

    if (f == COLON) {
	// Check that operands are real constants
	if (arg(head, 0).tag() != tag_t::CON ||
	    !is_functor(arg(head, 1))) {
	    throw syntax_exception_clause_bad_head(clause, "For '" + to_string(clause)+ "': Clause has an invalid head; module and predicate name are not pure constants; was " + to_string(head));
	}
    }
}

void interpreter_base::syntax_check_body(term clause, term body)
{
    static con_cell imply("->", 2);
    static con_cell semi(";", 2);
    static con_cell comma(",", 2);
    static con_cell cannot_prove("\\+", 1);

    if (is_functor(body)) {
	auto f = functor(body);
	if (f == imply || f == semi || f == comma || f == cannot_prove) {
	    auto num_args = f.arity();
	    for (size_t i = 0; i < num_args; i++) {
		auto a = arg(body, i);
		syntax_check_body(clause, a);
	    }
	}
    }
    syntax_check_goal(clause, body);

}

void interpreter_base::syntax_check_goal(term clause, term goal)
{
    // Each goal must be a functor (e.g. a plain integer is not allowed)

    if (!is_functor(goal)) {
	auto tg = goal.tag();
	// We don't know what variables will be bound to, so we need
	// to conservatively skip the syntax check.
	if (tg.is_ref()) {
	    return;
	}
	throw syntax_exception_bad_goal(goal, "For '" + to_string(clause) + "': Goal is not callable; was " + to_string(goal));
    }
}

void interpreter_base::print_db() const
{
    print_db(std::cout);
}

void interpreter_base::print_db(std::ostream &out) const
{
    bool do_nl_p = false;
    for (const auto &qn : program_predicates_) {
	print_predicate(out, qn, do_nl_p);
    }
    out << "\n";
}

void interpreter_base::print_predicate(std::ostream &out, const qname &qn) const
{
    bool do_nl_p = false;
    print_predicate(out, qn, do_nl_p);
}

void interpreter_base::print_predicate(std::ostream &out, const qname &qn, bool &do_nl_p) const
{
    auto it = program_db_.find(qn);
    if (it == program_db_.end()) {
	return;
    }
    const predicate &pred = it->second;
    if (do_nl_p) {
	out << "\n";
    }
    emitter_options opt;
    opt.set(emitter_option::EMIT_PROGRAM);
    bool do_nl = false;
    for (auto &m_clause : pred.get_clauses()) {
	if (do_nl) out << "\n";
	std::string mod = "";
	if (qn.first != USER_MODULE) {
	    mod = to_string(qn.first)+":";
	}
	auto str = mod+to_string(m_clause.clause(), opt);
	out << str;
	do_nl = true;
    }
    do_nl_p = true;
}

void interpreter_base::print_profile() const
{
    print_profile(std::cout);
}

void interpreter_base::print_profile(std::ostream &out) const
{
    struct entry {
	con_cell f;
	uint64_t t;

	bool operator < (const entry &e) const {
	    if (t < e.t) {
		return true;
	    } else if (t > e.t) {
		return false;
	    } else {
		return f.value() < e.f.value();
	    }
	}

	bool operator == (const entry &e) const {
	    return t == e.t && f == e.f;
	}
    };

    std::vector<entry> all;

    for (auto prof : profiling_) {
	auto f = prof.first;
	auto t = prof.second;
	all.push_back(entry{f,t});
    }
    std::sort(all.begin(), all.end());
    for (auto p : all) {
	auto f = p.f;
	auto t = p.t;
	std::cout << to_string(f) << ": " << t << "\n";
    }

}

void interpreter_base::abort(const interpreter_exception &ex)
{
    throw ex;
}

void interpreter_base::prepare_execution()
{
    num_of_args_= 0;
    memset(&register_ai_, 0, sizeof(register_ai_));
    top_fail_ = false;
    complete_ = false;
    register_top_b_ = register_b_;
    register_b0_ = register_b_;
    register_top_e_ = register_e_;
    set_register_hb(heap_size());
    set_top_hb(heap_size());
    set_top_tr(trail_size());
    register_p_.reset();
}


void interpreter_base::tidy_trail()
{
    size_t from = (b() == nullptr) ? top_tr() : b()->tr;
    size_t to = trail_size();
    term_env::tidy_trail(from, to);
}

bool interpreter_base::definitely_inequal(const term a, const term b)
{
    using namespace common;
    if (a.tag().is_ref() || b.tag().is_ref()) {
        return false;
    }
    
    if (a.tag() != b.tag()) {
	return true;
    }
    switch (a.tag()) {
    case tag_t::REF: case tag_t::RFW: return false;
    case tag_t::CON: return a != b;
    case tag_t::STR: {
	con_cell fa = functor(a);
	con_cell fb = functor(b);
        return fa != fb;
    }
    case tag_t::INT: return a != b;
    case tag_t::BIG: {
        // Check big headers
        big_header ha = get_big_header(a);
        big_header hb = get_big_header(b);
	return ha != hb;
    }
    }

    return false;
}

void interpreter_base::unwind_to_top_choice_point()
{
    if (top_b() == nullptr) {
        if (!persistent_password_) clear_secret();
        return;
    }
    reset_to_choice_point(top_b());
    set_b(top_b());
}

choice_point_t * interpreter_base::reset_to_choice_point(choice_point_t *ch)
{
    // std::cout << "reset_to_choice_point(): b=" << b << std::endl;
    
    set_m(ch->m);
    set_e(ch->ce);
    set_cp(ch->cp);
    unwind(ch->tr);
    trim_heap_safe(ch->h);
    set_b0(ch->b0);
    set_register_hb(heap_size());
    
    set_qr(ch->qr);
    register_pr_ = ch->pr;

    size_t n = ch->arity;
    num_of_args_ = n;
    for (size_t i = 0; i < n; i++) {
	a(i) = ch->ai(i);
    }

    set_b(ch);

    return ch;
}

void interpreter_base::unwind(size_t from_tr)
{
    size_t n = trail_size();
    unwind_frozen_closures(from_tr, n);
    unwind_trail(from_tr, n);
    trim_trail(from_tr);
}

void interpreter_base::use_module(con_cell module_name)
{
    auto &qnames = get_module(module_name);

    for (auto &qn : qnames) {
	import_predicate(qn);
    }
}

void interpreter_base::import_predicate(const qname &qn) {
    auto module = current_module();
    qname imported_qn(module, qn.second);
    if (!get_code(imported_qn).is_fail()) {
	// We alreadty have a definition in current module
	// and we won't overwrite it.
	return;
    }
    auto &cp = get_code(qn);
    set_code(imported_qn, cp);
    if (cp.is_builtin()) {
	builtin &b = get_builtin(qn);
	load_builtin(imported_qn, b);
    }
}

void interpreter_base::save_program(con_cell module, std::ostream &out)
{
    std::unordered_set<qname> seen_predicates;

    for (auto &se : module_meta_db_[module].get_source_elements()) {
	switch (se.type()) {
	case source_element::SOURCE_NONE: break;
	case source_element::SOURCE_COMMENT: save_comment(se.comment(), out); break;
	case source_element::SOURCE_ACTION: save_clause(se.action(), out); break;
	case source_element::SOURCE_PREDICATE:
	    qname qn(module, se.predicate());
	    save_predicate(qn, out);
	    seen_predicates.insert(qn); break;
	}
    }

    for (auto &qn : module_db_[module]) {
        if (!seen_predicates.count(qn)) {
	    save_predicate(qn, out);
	}
    }

    module_meta_db_[module].clear_changed();
}

void interpreter_base::save_comment(const term_tokenizer::token &comment, std::ostream &out)
{
    std::string lexeme = comment.lexeme();
    boost::trim(lexeme);
    out << lexeme;
    out << std::endl;
}

void interpreter_base::save_clause(term t, std::ostream &out)
{
    term_emitter emit(out, *this);
    emit.set_var_naming(&var_naming());
    emit.options().set(emitter_option::EMIT_PROGRAM);
    emit.print(t);
    emit.nl();
}

void interpreter_base::save_predicate(const qname &qn, std::ostream &out)
{
    term_emitter emit(out, *this);
    emit.set_var_naming(&var_naming());    
    emit.options().set(emitter_option::EMIT_PROGRAM);
    bool empty = true;
    for (auto &managed : get_predicate(qn).get_clauses()) {
        if (managed.is_erased()) continue;
        empty = false;
	emit.print(managed.clause());
	emit.nl();
    }
    if (!empty) emit.nl();
}

// The secret area enables us to cache password derivatives, which are
// automatically cleaned up later. We can use "assert('$secret':blaha(42))"
// to store temporary things.
void interpreter_base::clear_secret()
{
    static con_cell SECRET("$secret",0);
    // Iterate through $secret module and clear all predicates

    auto &qnames = get_module(SECRET);
    for (auto &qn : qnames) {
	auto &pred = get_predicate(qn);
	pred.clear();
    }
}

void interpreter_base::add_delayed(delayed_t *dt)
{
    heap_limit_term(dt->query);
    heap_limit_term(dt->else_do);
    delayed_.push_back(dt);
    if (dt->has_timeout()) {
	boost::lock_guard<common::spinlock> lockit2(delayed_fast_lock_);
	if (delayed_next_timeout_.is_zero() ||
	    dt->timeout_at < delayed_next_timeout_) {
	    delayed_next_timeout_ = dt->timeout_at;
	}
    }
}

void interpreter_base::check_delayed()
{
    process_delayed( [this](const delayed_t &dt) {
	bool ok = false;
	if (dt.result_src) {
	    term result_copy = copy( dt.result, *dt.result_src);
	    ok = unify(dt.query, result_copy);
	    if (!ok) {
		std::cout << "FAILED DELAYED" << std::endl;
		std::cout << "QUERY: " << to_string(dt.query) << std::endl;
		std::cout << "RESULT:" << to_string(result_copy) << std::endl;
	    }
	}
	if (!ok) {
	    // Remote execution failed, so execute else clause (if present)
	    if (dt.else_do != EMPTY_LIST) {
		allocate_environment<ENV_FROZEN, environment_frozen_t *>();
		allocate_environment<ENV_NAIVE, environment_naive_t *>();
		set_cp(code_point(EMPTY_LIST));
		set_p(code_point(dt.else_do));
		set_qr(dt.else_do);
	    }
	}
    });
}

std::vector<common::ptr_cell *> interpreter_base::get_gc_roots() {
  std::vector<common::ptr_cell *> roots;
  get_stack_roots(roots);
  get_program_db_roots(roots);
  get_code_db_roots(roots);
  get_frozen_closure_roots(roots);
  get_meta_roots(roots);
  gc_roots_fn()(roots, this);
  return roots;
}

void interpreter_base::get_meta_roots(std::vector<common::ptr_cell *> &roots) {
  meta_context *current_meta = get_current_meta_context();
  while (current_meta != nullptr) {
    add_root(roots, &current_meta->old_qr);
    current_meta = current_meta->old_m;
  }
}

void interpreter_base::get_stack_roots(std::vector<common::ptr_cell *> &roots) {
  auto older_e = e0();
  auto cur_cp = b();
  auto kind = e_kind();
  environment_base_t *newer_e = nullptr;
  while(older_e != nullptr || newer_e != nullptr) {
    if(kind == ENV_NAIVE) {
    } else if (kind == ENV_WAM) {
      if (newer_e != nullptr) {
        // If we have interleaved choice point(s?) on the stack
        // make sure those are subtracted from the current environment
        // calculation.
        int choice_points_size = 0;
        while (base(cur_cp) < base(newer_e) && base(cur_cp) > base(older_e)) {
          choice_points_size += words<choice_point_t>();
          choice_points_size += words<term>()*cur_cp->arity;
	  // Process choice point.
	  // Assumption: there will always be an environment below
	  // the last choice point.
	  add_root(roots, &(cur_cp->qr));
	  for (size_t i = 0; i < cur_cp->arity; i++) {
	      add_root(roots, &cur_cp->ai(i));
	  }
          cur_cp = cur_cp->b;
        }
	auto newer_e_w  = base(newer_e);
	auto older_e_w = base(older_e);
        auto base_size_w = words<environment_t>();
        auto term_size_w = words<term>();
        auto num_y = ((newer_e_w - older_e_w) - base_size_w - choice_points_size)/term_size_w;

        auto wamenv = reinterpret_cast<environment_t*>(older_e);
	for(size_t i = 0; i < num_y; i++) {
	  add_root(roots, &wamenv->yn[i]);
	}
      }
    } else { // FROZEN
      auto frozen_env = reinterpret_cast<environment_frozen_t*>(older_e);
      auto num_extra = frozen_env->num_extra;
      for(size_t i = 0; i < num_extra; i++) {
	add_root(roots, &frozen_env->extra[i]);
      }
    }

    newer_e = older_e;
    if(older_e != nullptr) {
      kind = older_e->ce.kind();
      older_e = older_e->ce.ce0();
    }
  }
}

void interpreter_base::get_program_db_roots(std::vector<common::ptr_cell *> &roots)
{
  for (auto &db_entry : program_db_) {
    auto &predicate = db_entry.second;
    auto &managed_clauses = predicate.get_clauses();
    for (auto &managed_clause : managed_clauses) {
      add_root(roots, managed_clause.clause_ptr());
    }
  }
}

void interpreter_base::get_code_db_roots(std::vector<common::ptr_cell *> &roots)
{
  for (auto &code_db_entry : code_db_) {
    auto &code_point = code_db_entry.second;
    add_root(roots, &code_point.term_code_);
  }
}

void interpreter_base::get_meta_db_roots(std::vector<common::ptr_cell *> &roots)
{
  // FIXME: Add roots if needed.
  //  for (auto &meta_db_entry : module_meta_db_) {
  //    auto &module_meta = meta_db_entry.second;
  //    auto &source_elements = module_meta.source_elements_;
  //    for (auto &source_element : source_elements) {
  //    }
  //  }
}

void interpreter_base::get_frozen_closure_roots(std::vector<common::ptr_cell *> &roots)
{
  for (auto &frozen_closure_entry : frozen_closures_) {
    add_root(roots, &frozen_closure_entry.second);
  }
}

void interpreter_base::dump_roots()
{
  std::vector<common::ptr_cell *> roots = get_gc_roots();
  std::cout << "---- Roots Start ----\n";
  for(auto &ptr : roots) {
    std::cout << ptr->raw_value() << ", " << ptr->index() << " value: " << to_string(*ptr)
	      << " ptr: " << (void*)(((size_t)ptr->index())) << 3 << "\n";
  }
  std::cout << "---- Roots End ----\n";
}

void interpreter_base::dump_stack() {
  auto older_e = e0();
  auto cur_cp = b();
  size_t stack_size = older_e == nullptr ? 0 :
    (size_t)older_e - (size_t)stack_;
  std::cout << "---- Stack Start(" << stack_size << ") " << stack_
	    << " - " << older_e << " ---\n";
  auto kind = e_kind();
  environment_base_t *newer_e = nullptr;
  while(older_e != nullptr || newer_e != nullptr) {
    if(kind == ENV_NAIVE) {
      std::cout << "Naive env: " << older_e << "\n";
    } else if (kind == ENV_WAM) {
      if (newer_e == nullptr) {
        std::cout << "Top of stack - ignoring y-regs\n";
      } else {
        // If we have interleaved choice point(s?) on the stack
        // make sure those are subtracted from the current environment
        // calculation.
        int choice_points_size = 0;
        while (base(cur_cp) < base(newer_e) && base(cur_cp) > base(older_e)) {
          choice_points_size += words<choice_point_t>();
          choice_points_size += words<term>()*cur_cp->arity;
	  std::cout << "Choice Point: " << cur_cp << "\n";
          cur_cp = cur_cp->b;
        }
	auto newer_e_w  = base(newer_e);
	auto older_e_w = base(older_e);
        auto base_size_w = words<environment_t>();
        auto term_size_w = words<term>();
        auto num_y = ((newer_e_w - older_e_w) - base_size_w - choice_points_size)/term_size_w;

        auto wamenv = reinterpret_cast<environment_t*>(older_e);
	std::cout << "WAM env: " << wamenv << "\n";
	for(size_t i = 0; i < num_y; i++) {
	  common::term yi = (wamenv == nullptr) ? e()->yn[i] : wamenv->yn[i];
	  std::cout << "y[" << i << "]: " << yi.tag().str();
	  if(yi.tag() == common::tag_t::REF ||
	     yi.tag() == common::tag_t::RFW) {
	    std::cout << yi.raw_value() << ", " <<
	      reinterpret_cast<ptr_cell&>(yi).index() << "ptr: " <<
	      (void*)(((size_t)(reinterpret_cast<ptr_cell&>(yi).index())) << 3);
	  } else if (yi.tag() == common::tag_t::STR) {
	    std::cout << " " << reinterpret_cast<ptr_cell&>(yi).index() <<
	      ", " << to_string(yi);
	  } else if (yi.tag() == common::tag_t::CON) {
	    std::cout << ", " << to_string(yi);
	  }
	  std::cout << "\n";
	}
      }
    } else { // FROZEN
      std::cout << "Frozen env: " << older_e << "\n";
    }
    newer_e = older_e;
    if(older_e != nullptr) {
      kind = older_e->ce.kind();
      older_e = older_e->ce.ce0();
    }
  }
  std::cout << "---- Stack End ---\n";
}

void interpreter_base::dump_choice_points() {
  auto current_b = b();
  std::cout << "---- Choicepoints Start ---\n";
  while(current_b != nullptr) {
    auto barity = current_b->arity;
    std::cout << "CP(" << barity << "): ";
    bool is_wam = current_b->bp.has_wam_code();
    if(is_wam) {
      std::cout << "WAM\n";
    } else {
      std::cout << "Naive\n";
    }
    if(!is_wam) {
      std::cout << "Qr: " << to_string(current_b->qr) << "\n";
    }
    std::cout << "Meta: " << current_b->m << "\n";
    for(size_t i = 0; i < barity; i++) {
	std::cout << "Arg[" << i << "]: " << to_string(current_b->ai(i)) << "\n";
    }
    current_b = current_b->b;
  }
  std::cout << "---- Choicepoint End ---\n";
}

void interpreter_base::garbage_collect() {
  std::vector<common::ptr_cell *> roots = get_gc_roots();
  common::garbage_collector collector(get_heap());
  collector.do_collection(roots);
}

}}
