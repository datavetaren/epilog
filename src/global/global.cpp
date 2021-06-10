#include <boost/filesystem.hpp>
#include "global.hpp"
#include "meta_entry.hpp"
#include "global_interpreter.hpp"
#include "../coin/coin.hpp"

using namespace epilog::common;
using namespace epilog::interp;

namespace epilog { namespace global {

global::global(const std::string &data_dir)
    : data_dir_(data_dir),
      blockchain_(data_dir),
      interp_(nullptr),
      commit_version_(blockchain::VERSION),
      commit_height_(0),
      commit_time_(),
      commit_goals_() {
    if (!blockchain_.tip().is_partial() || blockchain_.tip().is_zero()) {
	interp_ = std::unique_ptr<global_interpreter>(new global_interpreter(*this));
	interp_->init();
    } else {
	interp_ = nullptr;
    }
}

bool global::set_tip(const meta_id &id)
{
    auto entry = blockchain_.get_meta_entry(id);
    if (!entry) {
	return false;
    }
    blockchain_.set_tip(*entry);

    interp_ = nullptr;
    if (!blockchain_.tip().is_partial()) {
	interp_ = std::unique_ptr<global_interpreter>(new global_interpreter(*this));
	interp_->init();
    }
    return true;
}
	
void global::erase_db(const std::string &data_dir)
{
    auto dir_path = boost::filesystem::path(data_dir) / "db";
    boost::system::error_code ec;
    boost::filesystem::remove_all(dir_path, ec);
    if (ec) {
        throw global_db_exception( "Failed while attempting to erase all files at " + dir_path.string() + "; " + ec.message());
    }
}

void global::total_reset() {
    interp_ = nullptr;
    erase_db(data_dir_);
    blockchain_.init();
    interp_ = std::unique_ptr<global_interpreter>(new global_interpreter(*this));
    interp_->init();
}

bool global::db_get_predicate(const qname &qn,
			      std::vector<term> &clauses) {

    if (blockchain_.program_root().is_zero()) {
	return false;
    }
    
    // Max 10 collisions
    size_t i;
    const uint8_t *p = nullptr;
    size_t custom_data_size = 0;
    for (i = 0; i < 10; i++) {
	common::fast_hash h;
	h << qn.first.raw_value() << qn.second.raw_value() << i;
	auto key = h.finalize();
	auto leaf = blockchain_.program_db().find(
						  blockchain_.program_root(), key);
	if (leaf == nullptr) {
	    return false;
	}
	p = leaf->custom_data();
	custom_data_size = leaf->custom_data_size();
	common::untagged_cell qfirst = common::read_uint64(p); p += sizeof(uint64_t);
	common::untagged_cell qsecond = common::read_uint64(p); p += sizeof(uint64_t);
	common::con_cell &qfirst_con = reinterpret_cast<common::con_cell &>(qfirst);
	common::con_cell &qsecond_con = reinterpret_cast<common::con_cell &>(qsecond);
	interp::qname qn_entry(qfirst_con, qsecond_con);
	if (qn == qn_entry) {
	    break;
	}
    }
    if (i == 10) {
	return false;
    }
    size_t n_bytes = custom_data_size - 2*sizeof(uint64_t);
    size_t n_cells = n_bytes / sizeof(common::cell);
    for (i = 0; i < n_cells; i++) {
	common::cell c(common::read_uint64(p)); p += sizeof(uint64_t);
	clauses.push_back(c);
    }
    return true;
}
	
bool global::db_set_predicate(const qname &qn,
			      const std::vector<term> &clauses) { 
    static const size_t MAX_CLAUSES = 32;
    if (clauses.size() > MAX_CLAUSES) {
	return false;
    }   

    // Max 10 collisions
    size_t i;
    uint64_t key;
    for (i = 0; i < 10; i++) {
	common::fast_hash h;
	h << qn.first.raw_value() << qn.second.raw_value() << i;
	key = h.finalize();
	auto leaf = blockchain_.program_db().find(
						  blockchain_.program_root(), key);
	if (leaf == nullptr) {
	    break;
	}
	auto p = leaf->custom_data();
	common::untagged_cell qfirst = common::read_uint64(p); p += sizeof(uint64_t);
	common::untagged_cell qsecond = common::read_uint64(p); p += sizeof(uint64_t);
	common::con_cell &qfirst_con = reinterpret_cast<common::con_cell &>(qfirst);
	common::con_cell &qsecond_con = reinterpret_cast<common::con_cell &>(qsecond);
	interp::qname qn_entry(qfirst_con, qsecond_con);
	if (qn == qn_entry) {
	    break;
	}
    }
    if (i == 10) {
	return false;
    }
    uint8_t custom_data[2*sizeof(uint64_t)+MAX_CLAUSES*sizeof(common::cell)];
    uint8_t *p = &custom_data[0];
    common::write_uint64(p, qn.first.raw_value()); p += sizeof(uint64_t);
    common::write_uint64(p, qn.second.raw_value()); p += sizeof(uint64_t);
    for (auto &c : clauses) {
	common::write_uint64(p, c.raw_value()); p += sizeof(cell);
    }
    size_t custom_data_size = 2*sizeof(uint64_t)+clauses.size()*sizeof(cell);
    blockchain_.program_db().update(blockchain_.program_root(), key,
				    custom_data, custom_data_size);
    return true;
}

term global::db_get_block(term_env &dst, const meta_id &root_id, bool raw) {
    auto *e = blockchain_.get_meta_entry(root_id);
    if (e == nullptr) {
	return common::heap::EMPTY_LIST;
    }
    return db_get_block(dst, *e, raw);
}
	
term global::db_get_block(term_env &dst, const meta_entry &e, bool raw) {
    size_t height = e.get_height();
    auto leaf = blockchain_.blocks_db().find(e.get_root_id_blocks(), height);
    if (leaf == nullptr) {
	return common::heap::EMPTY_LIST;
    }
    if (raw) {
	term_serializer::buffer_t buf(leaf->serialization_size());
	leaf->write(&buf[0]);
	auto data_term = dst.new_big(&buf[0], buf.size());
	return data_term;
    }
    term_serializer ser(dst);
    try {
	term_serializer::buffer_t buf(leaf->custom_data_size());
	std::copy(leaf->custom_data(), leaf->custom_data()+leaf->custom_data_size(), &buf[0]);
        term blk = ser.read(buf, buf.size());
	return blk;
    } catch (serializer_exception &ex) {
	if (&dst == &(*interp_)) reset();
	throw ex;
    }
}

void global::db_set_block(size_t height, const term_serializer::buffer_t &buf)
{
    blockchain_.blocks_db().update(blockchain_.blocks_root(), height,
				   &buf[0], buf.size());
}

bool global::db_get_block_hash(term_env &src, term meta_info, db::node_hash &hash) {
    if (meta_info.tag() != tag_t::STR) {
	return false;
    }
    if (src.functor(meta_info) != con_cell("meta",1)) {
	return false;
    }
    auto lst = src.arg(meta_info, 0);
    while (src.is_dotted_pair(lst)) {
	auto prop = src.arg(lst, 0);
	lst = src.arg(lst, 1);
	if (!src.is_functor(prop, con_cell("db",3))) {
	    continue;
	}
	auto name = src.arg(prop, 0);
	if (!src.is_functor(name, con_cell("block",0))) {
	    continue;
	}
	int32_t height = 0;
	if (!src.get_number(src.arg(prop, 1), height)) {
	    return false;
        }
        term big_term = src.arg(prop, 2);
	if (big_term.tag() != tag_t::BIG) {
	    return false;
	}
	auto &big = reinterpret_cast<big_cell &>(big_term);
	auto n = src.num_bytes(big);
	if (n > 128) {
	    return false;
	}
	uint8_t data[128];
	src.get_big(big, data, n);
	hash.set_hash(data, n);
	return true;
    }
    return false;
}

term global::db_get_meta(term_env &dst, const meta_id &root_id, bool more) {
    auto *e = blockchain_.get_meta_entry(root_id);
    if (e == nullptr) {
	return common::heap::EMPTY_LIST;
    }
    term lst = common::heap::EMPTY_LIST;
    auto id = dst.new_term( con_cell("id",1), { root_id.to_term(dst) } );
    auto previd = dst.new_term(con_cell("previd",1), { e->get_previous_id().to_term(dst) });
    auto ver = dst.new_term( con_cell("version",1), { int_cell(static_cast<int64_t>(e->get_version())) });
    auto height = dst.new_term( con_cell("height",1), { int_cell(static_cast<int64_t>(e->get_height())) });
    auto time = dst.new_term( con_cell("time",1), { int_cell(static_cast<int64_t>(e->get_timestamp().in_ss())) });
    auto diff = dst.new_term( con_cell("diff",2), { int_cell(static_cast<int64_t>(e->get_pow_difficulty().value().exponent())), int_cell(static_cast<int64_t>(e->get_pow_difficulty().value().mantissa().raw_value())) });

    uint8_t pow_data[pow::pow_proof::TOTAL_SIZE_BYTES];
    e->get_pow_proof().write(pow_data);
    
    auto pow = dst.new_term( con_cell("pow",1), { dst.new_big(pow_data, pow::pow_proof::TOTAL_SIZE_BYTES) } );

    if (more) {
	static con_cell BLOCK = con_cell("block",0);
	static con_cell HEAP = con_cell("heap",0);
	static con_cell CLOSURE = con_cell("closure",0);
	static con_cell SYMBOLS = con_cell("symbols",0);
	static con_cell PROGRAM = con_cell("program",0);

	auto &b = get_blockchain();

	auto *block = b.blocks_db().find(e->get_root_id_blocks(),
					 e->get_height());
	assert(block != nullptr);
	
	auto &heap_root_hash = b.heap_db().get_root_hash(e->get_root_id_heap());
	auto num_heap = b.heap_db().num_entries(e->get_root_id_heap());

	auto &closure_root_hash = b.closure_db().get_root_hash(e->get_root_id_closure());
	auto num_closure = b.closure_db().num_entries(e->get_root_id_closure());

	auto &symbols_root_hash = b.symbols_db().get_root_hash(e->get_root_id_symbols());
	auto num_symbols = b.symbols_db().num_entries(e->get_root_id_symbols());
	
	auto &program_root_hash = b.program_db().get_root_hash(e->get_root_id_program());
	auto num_program = b.program_db().num_entries(e->get_root_id_program());
	
	auto block_term = dst.new_term(
          con_cell("db",3),
	  { BLOCK, int_cell(static_cast<int64_t>(e->get_height())),
	    dst.new_big(block->hash(), block->hash_size()) });
	auto heap_term = dst.new_term(
	  con_cell("db",3),
	  { HEAP, int_cell(static_cast<int64_t>(num_heap)),
	    dst.new_big(heap_root_hash.hash(), heap_root_hash.hash_size()) });
	auto closure_term = dst.new_term(
	  con_cell("db",3),
	  { CLOSURE, int_cell(static_cast<int64_t>(num_closure)),
	    dst.new_big(closure_root_hash.hash(), closure_root_hash.hash_size()) });
	auto symbols_term = dst.new_term(
	  con_cell("db",3),
	  { SYMBOLS, int_cell(static_cast<int64_t>(num_symbols)),
	    dst.new_big(symbols_root_hash.hash(), symbols_root_hash.hash_size()) });
	auto program_term = dst.new_term(
	  con_cell("db",3),
	  { PROGRAM, int_cell(static_cast<int64_t>(num_program)),
	    dst.new_big(program_root_hash.hash(), program_root_hash.hash_size()) });
	lst = dst.new_dotted_pair(program_term, lst);
	lst = dst.new_dotted_pair(symbols_term, lst);
	lst = dst.new_dotted_pair(closure_term, lst);
	lst = dst.new_dotted_pair(heap_term, lst);
	lst = dst.new_dotted_pair(block_term, lst);
    }
    lst = dst.new_dotted_pair(pow, lst);
    lst = dst.new_dotted_pair(diff, lst);
    lst = dst.new_dotted_pair(time, lst);
    lst = dst.new_dotted_pair(height, lst);
    lst = dst.new_dotted_pair(ver, lst);
    lst = dst.new_dotted_pair(previd, lst);
    lst = dst.new_dotted_pair(id, lst);
    auto meta = dst.new_term(con_cell("meta",1), { lst } );
    return meta;
}

static bool get_meta_id(term_env &src, term term_id, meta_id &id) {
    return id.from_term(src, term_id);
}

bool global::db_parse_meta(term_env &src, term meta_term, meta_entry &entry)
{
    if (meta_term.tag() != tag_t::STR) {
	return false;
    }
    if (src.functor(meta_term) != con_cell("meta",1)) {
	return false;
    }
    term lst = src.arg(meta_term,0);

    while (src.is_dotted_pair(lst)) {
	term prop = src.arg(lst, 0);
	if (prop.tag() != tag_t::STR) {
	    throw global_db_exception("Unexpected property in meta: " + src.to_string(prop));
	}
	term f = src.functor(prop);
	if (f == con_cell("id",1)) {
	    meta_id id;
	    if (!get_meta_id(src, src.arg(prop,0), id)) {
		throw global_db_exception("Unexpected identifier in meta: " + src.to_string(prop));
	    }
	    entry.set_id(id);
	} else if (f == con_cell("previd",1)) {
	    meta_id previd;
	    if (!get_meta_id(src, src.arg(prop,0), previd)) {
		throw global_db_exception("Unexpected identifier in meta: " + src.to_string(prop));
	    }
	    entry.set_previous_id(previd);
	} else if (f == con_cell("version",1)) {
	    uint64_t ver;
	    term ver_term = src.arg(prop,0);
	    if (!src.get_number(ver_term, ver)) {
		throw global_db_exception("Expected non-negative integer version number; was " + src.to_string(prop));
	    }
	    if (ver != 1) {
		throw global_db_exception("Expected integer version 1; was " + src.to_string(prop));
	    }
	    entry.set_version(ver);
	} else if (f == con_cell("height",1)) {
	    term height_term  = src.arg(prop,0);
	    uint32_t height;
	    if (!src.get_number(height_term, height)) {
		throw global_db_exception("Expected non-negative integer for height: " + src.to_string(prop));
	    }
	    entry.set_height(height);
	} else if (f == con_cell("time",1)) {
	    term time_term = src.arg(prop, 0);
	    uint64_t t;
	    if (!src.get_number(time_term, t)) {
		throw global_db_exception("Time is not a valid unsigned 64-bit number: " + src.to_string(prop));
	    }
	    entry.set_timestamp(utime(t));
	} else if (f == con_cell("diff",2)) {
	    term exp_part = src.arg(prop, 0);
	    term mantissa_part = src.arg(prop, 1);
	    uint64_t exp;
	    uint64_t mantissa;
	    if (!src.get_number(exp_part, exp)) {
		throw global_db_exception("Exponent of difficulty must be a number: " + src.to_string(exp_part));
	    }
	    if (!src.get_number(mantissa_part, mantissa)) {
		throw global_db_exception("Mantissa of difficulty must be a number: " + src.to_string(mantissa_part));
	    }
	    auto mantissa_fxp1648 = pow::fxp1648::raw(mantissa);
	    entry.set_pow_difficulty(pow::flt1648(static_cast<int16_t>(exp), mantissa_fxp1648));
	} else if (f == con_cell("pow",1)) {
	    term pow_term = src.arg(prop, 0);
	    if (pow_term.tag() != tag_t::BIG) {
		throw global_db_exception("POW must be a bignum, was: " + src.to_string(pow_term));
	    }
	    auto &big = reinterpret_cast<big_cell &>(pow_term);
	    auto n = src.num_bytes(big);
	    if (n != pow::pow_proof::TOTAL_SIZE_BYTES) {
		std::stringstream ss;
		ss << "POW must be exactly " << pow::pow_proof::TOTAL_SIZE_BYTES << " bytes in size, was " << n << " bytes.";
		throw global_db_exception(ss.str());
	    }
	    uint8_t pow_data[pow::pow_proof::TOTAL_SIZE_BYTES];
	    src.get_big(big, pow_data, n);
	    pow::pow_proof pow_proof;
	    pow_proof.read(pow_data);
	    entry.set_pow_proof(pow_proof);
	} else {
	    // Ignore unrecognized fields
	}
	lst = src.arg(lst, 1);
    }
    return true;
}

bool global::db_put_meta(term_env &src, term meta_term) {
    meta_entry entry;
    
    if (!db_parse_meta(src, meta_term, entry)) {
	return false;
    }
    
    if (!entry.validate_pow(pow_mode_)) {
	std::string short_id = boost::lexical_cast<std::string>(entry.get_height()) + "(" + hex::to_string(entry.get_id().hash(), 2) + ")";
	throw global_db_exception("PoW check failed for " + short_id);
    }

    auto &previd = entry.get_previous_id();
    if (!previd.is_zero() &&
	blockchain_.get_meta_entry(previd) == nullptr) {
	auto previd_str = src.to_string(previd.to_term(src));
	throw global_db_exception("Couldn't find previoius identifier: " + previd_str);
    }    

    // We never want to corrupt the genesis state or existing entries
    if (entry.get_height() > 0) {
	if (!blockchain_.get_meta_entry(entry.get_id())) {
	    blockchain_.add_meta_entry(entry);
	}
    }

    return true;
}

size_t global::db_get_meta_length(const meta_id &root_id, size_t lookahead_n) {
    auto follows = blockchain_.follows(root_id);
    if (lookahead_n == 0 || follows.empty()) {
	return 1;
    }
    size_t max_len = 0;
    for (auto &child_id : follows) {
	size_t len = 1 + db_get_meta_length(child_id, lookahead_n-1);
	if (len > max_len) max_len = len;
    }
    return max_len;
}

bool global::db_validate_meta(term_env &src, term meta_info, const meta_entry &entry) {

    db::node_hash block_hash, heap_hash, closure_hash, symbols_hash, program_hash;
    uint64_t block_num = 0, heap_num = 0, closure_num = 0, symbols_num = 0, program_num = 0;

    term lst = src.arg(meta_info, 0);
    while (src.is_dotted_pair(lst)) {
	auto prop = src.arg(lst, 0);
	if (prop.tag() == tag_t::STR && src.functor(prop) == con_cell("db",3)) {
	    db::node_hash *hash_p = nullptr;
	    uint64_t *num_p = nullptr;
	    auto dbname = src.arg(prop, 0);
	    if (dbname == con_cell("block",0)) {
		hash_p = &block_hash;
		num_p = &block_num;
	    } else if (dbname == con_cell("heap",0)) {
		hash_p = &heap_hash;
		num_p = &heap_num;
	    }  else if (dbname == con_cell("closure",0)) {
		hash_p = &closure_hash;
		num_p = &closure_num;		
	    } else if (dbname == con_cell("symbols",0)) {
		hash_p = &symbols_hash;
		num_p = &symbols_num;
	    } else if (dbname == con_cell("program",0)) {
		hash_p = &program_hash;
		num_p = &program_num;		
	    }

	    if (hash_p && num_p) {
		auto num_term = src.arg(prop, 1);
		if (num_term.tag() != tag_t::INT) {
		    return false;
		}
		*num_p = static_cast<uint64_t>(reinterpret_cast<int_cell &>(num_term).value());
		auto hash_term = src.arg(prop, 2);
		if (hash_term.tag() != tag_t::BIG) {
		    return false;
		}
		auto big = reinterpret_cast<big_cell &>(hash_term);
		size_t hash_size = src.num_bytes(big);
		if (hash_size > 128) {
		    return false;
		}
		uint8_t hash_data[128];
		src.get_big(hash_term, hash_data, hash_size);
		hash_p->set_hash(hash_data, hash_size);
	    }
	}
	lst = src.arg(lst, 1);
    }

    blake2b_state s;
    blake2b_init(&s, BLAKE2B_OUTBYTES);

    // Add previous id
    blake2b_update(&s, entry.get_previous_id().hash(), meta_id::HASH_SIZE);

    auto const &block_hash_const = block_hash;
    auto const &heap_hash_const = heap_hash;
    auto const &closure_hash_const = closure_hash;
    auto const &symbols_hash_const = symbols_hash;
    auto const &program_hash_const = program_hash;
    blake2b_update(&s, block_hash_const.hash(), block_hash_const.hash_size());
    blake2b_update(&s, heap_hash_const.hash(), heap_hash_const.hash_size());
    blake2b_update(&s, closure_hash_const.hash(), closure_hash_const.hash_size());
    blake2b_update(&s, symbols_hash_const.hash(), symbols_hash_const.hash_size());
    blake2b_update(&s, program_hash_const.hash(), program_hash_const.hash_size());

    uint8_t data[1024];

    // Add number of entries in heap, closures, symbols and program.
    common::write_uint64(data, heap_num);
    blake2b_update(&s, data, sizeof(uint64_t));
    common::write_uint64(data, closure_num);
    blake2b_update(&s, data, sizeof(uint64_t));
    common::write_uint64(data, symbols_num);
    blake2b_update(&s, data, sizeof(uint64_t));
    common::write_uint64(data, program_num);
    blake2b_update(&s, data, sizeof(uint64_t));
    
    // Add version
    common::write_uint64(data, entry.get_version());
    blake2b_update(&s, data, sizeof(uint64_t));

    // Add height
    common::write_uint32(data, entry.get_height());
    blake2b_update(&s, data, sizeof(uint32_t));

    // Add timestamp
    auto &ts = entry.get_timestamp();
    ts.write(data);
    blake2b_update(&s, data, ts.serialization_size());

    // Add pow difficulty
    entry.get_pow_difficulty().write(data);
    blake2b_update(&s, data, entry.get_pow_difficulty().serialization_size());

    memset(data, 0, sizeof(data));
    blake2b_final(&s, data, BLAKE2B_OUTBYTES);

    // This hash should equal the id of the entry
    return memcmp(data, entry.get_id().hash(), entry.get_id().hash_size()) == 0;
    
}

term global::db_get_meta_roots(term_env &dst, const meta_id &root_id, size_t spacing, size_t n) {
    term lst = common::heap::EMPTY_LIST;

    if (n == 0) {
	return lst;
    }

    auto id = root_id;
    auto id_term = id.to_term(dst);
    term head  = dst.new_dotted_pair(id_term, common::heap::EMPTY_LIST);
    term tail = head;
    n--;
    
    while (n > 0) {
	bool found = false;
	for (size_t i = 0; i < spacing; i++) {
	    auto follows = blockchain_.follows(id);
	    if (follows.empty()) {
		break;
	    }
	    found = true;
	    // Pick the longest branch with 10 blocks as max length
	    // Note that this is not really an attack vector. Yes, idiot miners
	    // can mine spurious branches (that are long), but this only impacts
	    // the parallelization of downloading meta records.
	    size_t max_len = 0;
	    meta_id best_id;
	    if (follows.size() > 1) {
		for (auto &follow_id : follows) {
		    auto len = db_get_meta_length(follow_id, 10);
		    if (len > max_len) {
			max_len = len;
			best_id = follow_id;
		    }
		}
	    } else {
		best_id = *follows.begin();
	    }
	    id = best_id;
	}
	if (!found) {
	    break;
	}

	id_term = id.to_term(dst);
	
	term new_term = dst.new_dotted_pair(id_term, common::heap::EMPTY_LIST);
	dst.set_arg(tail, 1, new_term);
	tail = new_term;
	
	n--;
    }

    return head;
}

term global::db_get_metas(term_env &dst, const meta_id &root_id, size_t n)
{
    term lst = common::heap::EMPTY_LIST;

    std::vector<meta_id> branches;
    branches.push_back(root_id);

    term m = db_get_meta(dst, root_id, false);
    lst = dst.new_dotted_pair(m, lst);
    term head = lst, tail = lst;
    bool has_more = true;
    if (n > 0) n--;
    
    while (n > 0 && has_more) {
	has_more = false;
	size_t num_branches = branches.size();
	for (size_t i = 0; i < num_branches; i++) {
	    auto const &id = branches[i];
	    auto follows = blockchain_.follows(id);
	    if (follows.empty()) {
		branches.erase(branches.begin()+i);
		num_branches--;
		i--;
		continue;
	    }
	    bool first = true;
	    for (auto const &fid : follows) {
		has_more = true;
		term m = db_get_meta(dst, fid, false);
		term new_term = dst.new_dotted_pair(m, common::heap::EMPTY_LIST);
		dst.set_arg(tail, 1, new_term);
		tail = new_term;
		
		if (!first) branches.push_back(fid);
		if (first) {
		    branches[i] = fid;
		    first = false;
		}
		if (n > 0) n--;
	    }
	}
    }

    return head;
}

//
// Extract ids of the path that we'd like to follow
//
term global::db_best_path(term_env &dst, const meta_id &root_id, size_t n)
{
    term lst = dst.EMPTY_LIST;
    if (n == 0) {
	return lst;
    }
    auto id = root_id;
    auto id_term = id.to_term(dst);
    term head = dst.new_dotted_pair(id_term, dst.EMPTY_LIST);
    term tail = head;
    n--;
    while (n > 0) {
	auto follows = blockchain_.follows(id);
	if (follows.empty()) {
	    break;
	}
	if (follows.size() == 1) {
	    id = *follows.begin();
	} else {
	    meta_id best_id;
	    size_t best_len = 0;
	    for (auto &fid : follows) {
		auto len = db_get_meta_length(fid, 100);
		if (len > best_len) {
		    best_len = len;
		    best_id = fid;
		}
	    }
	    id = best_id;
	}
	n--;

	term new_list = dst.new_dotted_pair(id.to_term(dst), dst.EMPTY_LIST);
	dst.set_arg(tail, 1, new_list);
	tail = new_list;
    }
    return head;
}

void global::db_put_metas(term_env &src, term lst)
{
    while (src.is_dotted_pair(lst)) {
	auto meta_term = src.arg(lst, 0);
	db_put_meta(src, meta_term);
	lst = src.arg(lst, 1);
    }
}

size_t global::current_height() const {
    return blockchain_.tip().get_height();
}

size_t global::max_height() const {
    return blockchain_.max_height();
}

void global::increment_height()
{
    get_blockchain().set_version(commit_version_);
    get_blockchain().set_height(commit_height_);
    get_blockchain().set_time(commit_time_);
    get_blockchain().advance();
    interp().commit_heap();
    interp().commit_closures();
    interp().commit_symbols();
    interp().commit_program();
    db_set_block(current_height(), commit_goals_);
    init_empty_goals();
    get_blockchain().update_tip();
}

void global::init_empty_goals() {
    commit_goals_.clear();
    term_env env;
    term_serializer ser(env);
    ser.write(commit_goals_, env.EMPTY_LIST);
}


void global::setup_commit() {
    commit_version_ = blockchain::VERSION;
    commit_time_ = utime::now();
    commit_height_ = current_height();
}

void global::setup_commit(const meta_entry &e) {
    commit_version_ = e.get_version();
    commit_height_ = e.get_height();
    commit_time_ = e.get_timestamp();
}

void global::setup_commit(const term_serializer::buffer_t &buf) {
    term_env env;
    term_serializer ser(env);
    term m = ser.read(buf);
    if (m.tag() != tag_t::STR || env.functor(m) != con_cell("meta",1)) {
	throw global_db_exception( "expected meta/1; was " + env.to_string(m));
    }
    term lst = env.arg(m, 0);
    while (env.is_dotted_pair(lst)) {
	auto el = env.arg(lst, 0);
	if (env.functor(el) == con_cell("version",1)) {
	    auto val = env.arg(el, 0);
	    if (val.tag() != tag_t::INT) {
		throw global_db_exception( "version must be an integer; was " + env.to_string(val));
	    }
	    commit_version_ = static_cast<uint64_t>(reinterpret_cast<int_cell &>(val).value());
	} else if (env.functor(el) == con_cell("height",1)) {
	    auto val = env.arg(el, 0);
	    if (val.tag() != tag_t::INT) {
		throw global_db_exception( "height must be an integer; was " + env.to_string(val));
	    }
	    commit_height_ = static_cast<uint32_t>(reinterpret_cast<int_cell &>(val).value());
	} else if (env.functor(el) == con_cell("time",1)) {
	    auto val = env.arg(el, 0);
	    if (val.tag() != tag_t::INT) {
		throw global_db_exception("time must be an integer; was " + env.to_string(val));
	    }
	    auto timestamp = reinterpret_cast<int_cell &>(val).value();
	    commit_time_ = utime(static_cast<uint64_t>(timestamp));
	}
	lst = env.arg(lst, 1);
    }
}

bool global::execute_commit(const term_serializer::buffer_t &buf) {
    if (!execute_goal_silent(buf)) {
	discard();
	return false;
    }
    commit_goals_ = buf;
    advance();
    
    return true;
}

bool global::wrap_fees(term_env &src, term &goals, term fee_coin, term to_add) {
    term_serializer::buffer_t buf;
    term_serializer ser(src);
    ser.write(buf, goals);

    term_serializer dser(*interp_);
    term dst_goal = dser.read(buf);

    term to_add_pair = src.new_dotted_pair(fee_coin, to_add);
    buf.clear();
    ser.write(buf, to_add_pair);

    term dst_to_add_pair = dser.read(buf);
    term dst_fee_coin = interp_->arg(dst_to_add_pair, 0);
    term dst_to_add = interp_->arg(dst_to_add_pair, 1);

    // Allocate choice point before executing goal so we can
    // unbind all variables.

    interp_->set_top_e();
    interp_->allocate_choice_point(code_point::fail());
    interp_->set_top_b(interp_->b());

    bool r = execute_goal(dst_goal);

    std::unordered_set<ref_cell> coins;
    for (auto v : interp_->singleton_vars_in_goal()) {
	auto dv = interp_->deref(v);
	if (coin::is_native_coin(*interp_, dv) && !coin::is_coin_spent(*interp_, dv)) {
	    coins.insert(v);
	}
    }
    if (!r) {
	discard();
	return false;
    }

    reset(); // Unbind vars in goal

    // Build list of all vars that become fee coins
    term lst = term_env::EMPTY_LIST;
    term tail = lst;
    for (auto c : coins) {
	term new_tail = interp_->new_dotted_pair(c, term_env::EMPTY_LIST);
	if (interp_->is_empty_list(tail)) {
	    lst = new_tail;
	} else {
	    interp_->set_arg(tail, 1, new_tail);
	}
	tail = new_tail;
    }

    // Create a cjoin with all the fee coins
    term collect_fees = interp_->new_term( con_cell("cjoin", 2), { lst, dst_fee_coin } );
    term extra = interp_->new_term( con_cell(",", 2), { collect_fees, dst_to_add } );

    // Patch in extra
    term result;
    if (interp_->is_functor(dst_goal, con_cell(",",2))) {
	term cma = dst_goal;
	while (interp_->is_functor(interp_->arg(cma, 1), con_cell(",",2))) {
	    cma = interp_->arg(cma, 1);
	}
        term tail = interp_->arg(cma, 1);
	result = interp_->new_term( con_cell(",",2), { tail, extra });
	interp_->set_arg(cma, 1, result);
	result = dst_goal;
    } else {
	result = interp_->new_term( con_cell(",",2), { dst_goal, extra });
    }

    // Now let's serialize dst_goal which includes the extra
    buf.clear();
    dser.write(buf, result);

    goals = ser.read(buf);

    discard();

    return true;
}
    
}}

