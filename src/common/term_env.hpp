#pragma once

#ifndef _common_term_env_hpp
#define _common_term_env_hpp

#include <iterator>
#include <map>
#include "term.hpp"
#include "term_emitter.hpp"
#include "term_parser.hpp"
#include "term_tokenizer.hpp"
#include "checked_cast.hpp"

namespace epilog { namespace common {

//
// This is the environment that terms need when dealing with
// some basic operations.  It consists of a heap, a constant table
// (if constant names are bigger than 6 characters,) a stack and a trail.
//
// One of the core operations is unificiation. Variables may become
// bound during the unification process and thus need to be recorded so
// they can become unbound.
//
class term_exception_not_list : public std::runtime_error {
public:
    inline term_exception_not_list(term t)
       : std::runtime_error("term is not a list"), term_(t) { }
    inline ~term_exception_not_list() noexcept(true) { }

    inline term get_term() { return term_; }

private:
    term term_;
};

class term_env;
	
class term_iterator : public std::iterator<std::forward_iterator_tag,
					   term,
					   term,
					   const term *,
					   const term &> {
public:
    inline term_iterator(term_env &env, const term t)
	 : env_(env), current_(t) { elem_ = first_of(t); }

    inline bool operator == (const term_iterator &other) const;
    inline bool operator != (const term_iterator &other) const
        { return ! operator == (other); }

    inline term_iterator & operator ++ ()
        { term_iterator &it = *this; it.advance(); return it; }
    inline reference operator * () const { return elem_; }
    inline pointer operator -> () const { return &elem_; }

    inline term_env & env() { return env_; }
    inline const term_env & env() const { return env_; }

private:
    inline term first_of(const term t);
    void advance();

    term_env &env_;
    term current_;
    term elem_;
};
	
class term_dfs_iterator : public std::iterator<std::forward_iterator_tag,
					       term,
					       term,
					       const term *,
					       const term &> {
public:
    inline term_dfs_iterator(term_env &env, const term t)
           : env_(env) { elem_ = first_of(t); }

    inline term_dfs_iterator(term_env &env)
	   : env_(env), elem_(0) { }

    inline bool operator == (const term_dfs_iterator &other) const;
    inline bool operator != (const term_dfs_iterator &other) const
        { return ! operator == (other); }

    inline term_dfs_iterator & operator ++ ()
        { term_dfs_iterator &it = *this;
	  it.advance(); return it; }
    inline reference operator * () const { return elem_; }
    inline pointer operator -> () const { return &elem_; }
    inline size_t depth() const { return stack_.size(); }

    inline term_env & env() { return env_; }

private:
    inline term first_of(const term t);
    void advance();

    term_env &env_;
    term elem_;

    struct dfs_pos {
	dfs_pos(const term &p, size_t i, size_t n) :
	    parent(p), index(i), arity(n) { }

	term parent;
	size_t index;
	size_t arity;
    };

    std::vector<dfs_pos> stack_;
};

class const_term_dfs_iterator : public term_dfs_iterator
{
public:
    inline const_term_dfs_iterator(const term_env &env, const term t)
  	   : term_dfs_iterator(const_cast<term_env &>(env), t) { }

    inline const_term_dfs_iterator(const term_env &env)
  	   : term_dfs_iterator(const_cast<term_env &>(env)) { }

    inline bool operator == (const const_term_dfs_iterator &other) const;
    inline bool operator != (const const_term_dfs_iterator &other) const
        { return ! operator == (other); }

    inline const_term_dfs_iterator & operator ++ ()
        { return static_cast<const_term_dfs_iterator &>(
		   term_dfs_iterator::operator ++ ()); }
};

template<typename T> class heap_dock : public T {
public:
    inline heap_dock() { }

    // Heap management
    inline void heap_setup_get_block_function( heap::get_block_fn fn, void *context)
        { T::get_heap().setup_get_block_fn(fn, context); }
    inline void heap_setup_modified_block_function( heap::modified_block_fn fn, void *context)
        { T::get_heap().setup_modified_block_fn(fn, context); }
    inline void heap_setup_new_atom_function( heap::new_atom_fn fn, void *context)
        { T::get_heap().setup_new_atom_fn(fn, context); }
    inline void heap_setup_load_atom_name_function( heap::load_atom_name_fn fn, void *context) { T::get_heap().setup_load_atom_name_fn(fn, context); }
    inline void heap_setup_load_atom_index_function( heap::load_atom_index_fn fn, void *context) { T::get_heap().setup_load_atom_index_fn(fn, context); }

    inline void heap_setup_trim_function( heap::trim_fn fn, void *context)
        { T::get_heap().setup_trim_fn(fn, context); }

    inline void heap_set_size(size_t sz)
        { T::get_heap().set_size(sz);  }

  
    inline void heap_set(size_t index, term t)
        { T::get_heap()[index] = t; }
    inline term heap_get(size_t index) const
        { return T::get_heap()[index]; }
    inline untagged_cell heap_get_untagged(size_t index)
        { return T::get_heap().untagged_at(index); }

    inline bool check_term(const term t) const
        { return T::get_heap().check_term(t); }

    // Term management
    inline term new_ref()
        { return T::get_heap().new_ref(); }
    inline void new_ref(size_t cnt)
        { T::get_heap().new_ref(cnt); }
    inline term deref(const term t) const
        { return T::get_heap().deref(t); }
    inline term deref_with_cost(const term t, uint64_t &cost) const
        { return T::get_heap().deref_with_cost(t, cost); }
    inline con_cell functor(const term t) const
        { return T::get_heap().functor(t); }
    inline term arg(const term t, size_t index) const
        { return T::get_heap().arg(t, index); }
    inline void set_arg(term t, size_t index, const term arg)
        { return T::get_heap().set_arg(t, index, arg); }
    inline untagged_cell get_big(term t, size_t index) const
        { return T::get_heap().get_big(t, index); }
    inline size_t num_bits(big_cell b) const
        { return T::get_heap().num_bits(b); }
    inline bool big_equal(term t1, term t2, uint64_t &cost) const
        { return T::get_heap().big_equal(reinterpret_cast<big_cell &>(t1),
					 reinterpret_cast<big_cell &>(t2),
					 cost);
	}
    inline int big_compare(term t1, term t2, uint64_t &cost) const
        { return T::get_heap().big_compare(reinterpret_cast<big_cell &>(t1),
					   reinterpret_cast<big_cell &>(t2),
					   cost);
        }
    inline void get_big(term t, boost::multiprecision::cpp_int &i,
			size_t &nbits) const
        { T::get_heap().get_big(t, i, nbits); }
    inline void get_big(term t, uint8_t *bytes, size_t n) const
        { T::get_heap().get_big(t, bytes, n); }
    inline big_header get_big_header(term t) const
        { return T::get_heap().get_big_header(t); }
    inline void set_big(term t, size_t index, const untagged_cell cell)
        {  T::get_heap().set_big(t, index, cell); }
    inline void set_big(term t, const boost::multiprecision::cpp_int &i)
        { T::get_heap().set_big(t, i); }
    inline void set_big(term t, const uint8_t *bytes, size_t n)
        { T::get_heap().set_big(t, bytes, n); }
    inline void trim_heap(size_t new_size)
        { return T::get_heap().trim(new_size); }
    inline size_t heap_size() const
        { return T::get_heap().size(); }

    // Term creation
    inline con_cell functor(const std::string &name, size_t arity)
        { return T::get_heap().functor(name, arity); }
    inline term new_term(con_cell functor)
        { return T::get_heap().new_str(functor); }
    inline term new_term_con(con_cell functor)
        { return T::get_heap().new_con0(functor); }
    inline term new_term_str(con_cell functor)
        { return T::get_heap().new_str0(functor); }
    inline term new_big(size_t nbits)
        { return T::get_heap().new_big(nbits); }
    inline term new_big(const uint8_t *data, size_t num_bytes)
        { auto b = T::get_heap().new_big(num_bytes*8);
	  set_big(b, data, num_bytes);
	  return b;
	}

    inline void new_term_copy_cell(term t)
        { T::get_heap().new_cell0(t); }
    inline term new_term(con_cell functor, const std::vector<term> &args)
        { term t = new_term(functor);
          size_t i = 0;
	  for (auto arg : args) {
	      T::get_heap().set_arg(t, i, arg);
	      i++;
	  }
	  return t;
        }
    inline term new_term(con_cell functor, std::initializer_list<term> args)
        { term t = new_term(functor);
          size_t i = 0;
	  for (auto arg : args) {
	      T::get_heap().set_arg(t, i, arg);
	      i++;
	  }
	  return t;
	}
    inline term new_dotted_pair()
        { return T::get_heap().new_dotted_pair(); }
    inline term new_dotted_pair(term a, term b)
        { return T::get_heap().new_dotted_pair(a,b); }
    inline con_cell to_atom(con_cell functor)
        { return T::get_heap().to_atom(functor); }
    inline con_cell to_functor(con_cell atom, size_t arity)
        { return T::get_heap().to_functor(atom, arity); }

    // Term functions
    inline size_t list_length(term lst)
        { return T::get_heap().list_length(lst); }
    inline std::string atom_name(con_cell f) const
        { return T::get_heap().atom_name(f); }
    inline bool is_dollar_atom_name(con_cell f) const
        { return T::get_heap().is_dollar_atom_name(f); }

    template<typename U> inline bool get_number(term t, U &num) const
        { if (t.tag() != tag_t::INT && t.tag() != tag_t::BIG) {
	      return false;
          }
	  if (t.tag() == tag_t::INT) {
	      auto v = reinterpret_cast<int_cell &>(t).value();
	      try {
		  num = checked_cast<U>(v);
		  return true;
	      } catch (checked_cast_exception &ex) {
		  return false;
	      }
	  } else {
	      boost::multiprecision::cpp_int i;
	      size_t nbits = 0;
	      big_cell big = reinterpret_cast<big_cell &>(t);
	      get_big(big, i, nbits);
	      try {
		  num = checked_cast<U>(i);
		  return true;
	      } catch (checked_cast_exception &ex) {
		  return false;
	      }
	  }
        }

    // Term predicates
    inline bool is_dotted_pair(term t) const
       { return T::get_heap().is_dotted_pair(t); }
    inline bool is_empty_list(term t) const
       { return T::get_heap().is_empty_list(t); }
    inline bool is_comma(term t) const
       { return T::get_heap().is_comma(t); }
    inline bool is_list(term t) const
       { return T::get_heap().is_list(t); }

    // Watch addresses
    inline void heap_add_watched(size_t addr)
       { T::get_heap().add_watched(addr); }
    inline void heap_watch(size_t addr, bool b)
       { T::get_heap().watch(addr, b); }
    inline const std::vector<size_t> & heap_watched() const
       { return T::get_heap().watched(); }
    inline bool heap_watched(size_t addr) const
       { return T::get_heap().watched(addr); }  
    inline void heap_clear_watched()
       { T::get_heap().clear_watched(); }

    // Disable coin security
    inline typename heap::disabled_coin_security disable_coin_security()
       { return T::get_heap().disable_coin_security(); }
};

class stacks {
public:
    inline stacks & get_stacks() { return *this; }
    inline const stacks & get_stacks() const { return *this; }
    inline std::vector<term> & get_stack() { return stack_; }
    inline const std::vector<term> & get_stack() const { return stack_; }
    inline std::vector<size_t> & get_trail() { return trail_; }
    inline const std::vector<size_t> & get_trail() const { return trail_; }
    inline std::vector<term> & get_temp() { return temp_; }
    inline const std::vector<term> & get_temp() const { return temp_; }
    inline std::vector<size_t> & get_temp_trail() { return temp_trail_; }
    inline const std::vector<size_t> & get_temp_trail() const { return temp_trail_; }
    inline size_t get_register_hb() const { return register_hb_; }
    inline void set_register_hb(size_t hb) { register_hb_ = hb; }

    inline void reset() {
	stack_.clear();
	trail_.clear();
	temp_.clear();
	temp_trail_.clear();
	register_hb_ = 0;
    }

private:
    std::vector<term> stack_;
    std::vector<size_t> trail_;
    std::vector<term> temp_;
    std::vector<size_t> temp_trail_;
    size_t register_hb_;
};

template<typename T> class stacks_dock : public T {
public:
  inline stacks_dock() { }

  inline stacks & get_stacks()
      { return T::get_stacks(); }

  inline size_t allocate_stack(size_t num_cells)
      { size_t at_index = T::get_stack().size();
	T::get_stack().resize(at_index+num_cells);
	return at_index;
      }
  inline void ensure_stack(size_t at_index, size_t num_cells)
      { if (at_index + num_cells > T::get_stack().size()) {
	    allocate_stack(at_index+num_cells-T::get_stack().size());
	}
      }
  inline term * stack_ref(size_t at_index)
      { return &T::get_stack()[at_index]; }

  inline void push(const term t)
      { T::get_stack().push_back(t); }
  inline term pop()
      { term t = T::get_stack().back(); T::get_stack().pop_back(); return t; }
  inline size_t stack_size() const
      { return T::get_stack().size(); }
  inline void trim_stack(size_t new_size)
      { T::get_stack().resize(new_size); }

  inline void push_trail(size_t i)
      { T::get_trail().push_back(i); }
  inline size_t pop_trail()
      { auto p = T::get_trail().back(); T::get_trail().pop_back(); return p; }
  inline size_t trail_size() const
      { return T::get_trail().size(); }
  inline void trim_trail(size_t to)
      { T::get_trail().resize(to); }
  inline size_t trail_get(size_t i) const
      { return T::get_trail()[i]; }
  inline void trail_set(size_t i, size_t val)
      { T::get_trail()[i] = val; }
  inline void trail(size_t index)
      { if (index < T::get_register_hb()) {
	    push_trail(index);
	}
      }
  inline void tidy_trail(size_t from, size_t to)
  {
      size_t i = from;
      auto hb = T::get_register_hb();
      while (i < to) {
   	  if (trail_get(i) < hb) {
   	      // This variable recording happened before the choice point.
	      // We can't touch it.
	      i++;
          } else {
	      // Remove this trail point, move one trail point we haven't
	      // visited to this location.
  	      trail_set(i, trail_get(to-1));
	      to--;
          }
      }

      // We're done. Trim the trail to the new end
      trim_trail(to);
  }

  inline void clear_trail() {
      T::get_trail().clear();
  }

  inline size_t temp_size() const
     { return T::get_temp().size(); }
  inline void temp_clear()
     { T::get_temp().clear();   }
  inline void temp_push(const term t)
      { T::get_temp().push_back(t); }
  inline term temp_pop()
      { auto t = T::get_temp().back(); T::get_temp().pop_back(); return t; }

  inline size_t temp_trail_size() const
      { return T::get_temp_trail().size(); }
  inline void temp_trail_push(const size_t i)
      { T::get_temp_trail().push_back(i); }
  inline size_t temp_trail_pop()
      { auto i = T::get_temp_trail().back(); T::get_temp_trail().pop_back(); return i; }

};

template<typename T> class ops_dock : public T
{
public:
    ops_dock() : T() { }
    ops_dock(T &t) : T(t) { }
};

class term_env;

class term_utils {
public:
    term_utils(term_env &env) : env_(env), dont_copy_big_(false) { }

    heap & get_heap();
    term_ops & get_ops();
    
    term new_ref();
    term new_dotted_pair(term a, term b);
	
    term heap_get(size_t index) const;
    void heap_set(size_t index, term t);
    size_t heap_size() const;
    void heap_watch(size_t addr, bool b);
    term new_term(con_cell functor);

    std::string atom_name(con_cell f) const;
    con_cell functor(const term t) const;
    con_cell functor(const std::string &name, size_t arity);
    term arg(const term t, size_t index) const;
    void set_arg(term t, size_t index, const term arg);
    size_t stack_size() const;
    void trim_stack(size_t new_size);
    size_t trail_size() const;
    void trim_trail(size_t new_size);
    void tidy_trail(size_t from, size_t to);
    size_t get_register_hb() const;
    void set_register_hb(size_t h);
    
    size_t temp_size() const;
    void temp_clear();
    void temp_push(const term t);
    term temp_pop();
    size_t temp_trail_size() const;
    void temp_trail_push(const size_t i);
    size_t temp_trail_pop();
    
    void push(const term t);
    term pop();
    term deref(const term t) const;
    term deref_with_cost(const term t, uint64_t &cost) const;

    size_t num_bits(big_cell b) const;
    term new_big(size_t nbits);
    void get_big(term t, boost::multiprecision::cpp_int &i, size_t &nbits) const;
    int big_compare(term t1, term t2, uint64_t &cost) const;
    
    const_big_iterator begin(const big_cell b) const;
    const_big_iterator end(const big_cell b) const;
    
    bool unify(term a, term b, uint64_t &cost);
    term copy(const term t, naming_map &names, uint64_t &cost);
    term copy(const term t, naming_map &names,
	      heap &src, naming_map *src_names, uint64_t &cost);
    bool equal(term a, term b, uint64_t &cost);
    bool big_equal(term t1, term t2, uint64_t &cost) const;
    uint64_t hash(term t);
    size_t simple_hash(term t) const;
    uint64_t cost(term t);

    // Return -1, 0 or 1 when comparing standard order for 'a' and 'b'
    int standard_order(const term a, const term b, uint64_t &cost);

    std::string list_to_string(const term t, heap &src);
    term string_to_list(const std::string &str);
    bool is_string(const term t, heap &src);

    std::vector<std::string> get_expected(const term_parse_exception &ex);
    std::vector<std::string> get_expected_simplified(const term_parse_exception &ex);

    std::vector<std::string> to_error_messages(const token_exception &ex);
    std::vector<std::string> to_error_messages(const term_parse_exception &ex);

    void print_error_messages(std::ostream &out, const token_exception &ex) {
	for (auto &s : to_error_messages(ex)) {
	    out << s << std::endl;
	}
    }

    void print_error_messages(std::ostream &out, const term_parse_exception &ex) {
	for (auto &s : to_error_messages(ex)) {
	    out << s << std::endl;
	}
    }

    void error_excerpt(const std::string &line, size_t column,
		       std::vector<std::string> &msgs);
    

    inline void set_dont_copy_big(bool b) {
        dont_copy_big_ = b;
    }
  
private:
    void restore_cells_after_unify();
    bool unify_helper(term a, term b, uint64_t &cost);
    int functor_standard_order(con_cell a, con_cell b);

    void bind(const ref_cell &a, term b);
    void unwind_trail(size_t from, size_t to);

    inline bool dont_copy_big() {
        return dont_copy_big_;
    }

    term_env &env_;
    bool dont_copy_big_;
};

class term_env
    : public heap_dock<heap>, public stacks_dock<stacks>, public ops_dock<term_ops>
{
private:
    typedef heap HT;
    typedef stacks ST;
    typedef term_ops OT;
public:
  inline term_env() { }

  void reset() {
      heap_dock<HT>::reset();
      stacks_dock<ST>::reset();
      ops_dock<OT>::reset();
      clear_names();
  }

  inline void heap_set_size(size_t sz)
      { heap_dock<HT>::heap_set_size(sz);
	stacks_dock<ST>::set_register_hb(sz);
      }

  inline bool is_atom(const term t) const
      { return is_functor(t) && heap_dock<HT>::functor(t).arity() == 0; }

  inline std::string atom_name(const term t) const
      { return heap_dock<HT>::atom_name(heap_dock<HT>::functor(t)); }

  inline bool is_functor(const term t) const
      { auto tt = heap_dock<HT>::deref(t).tag(); return tt == tag_t::CON || tt == tag_t::STR; }
	 
  inline bool is_functor(const term t, con_cell f)
     { return is_functor(t) && heap_dock<HT>::functor(t) == f; }

  inline bool is_ground(const term t) const
     {
        auto range = const_cast<term_env &>(*this).iterate_over(t);
        for (auto t1 : range) {
	    if (t1.tag().is_ref()) {
	        return false;
	    }
        }
        return true;    
     }

  inline naming_map & var_naming()
     { return var_naming_; }
  inline void clear_names()
     { var_naming_.clear_names(); }
  inline void clear_name(ref_cell r)
     { var_naming_.clear_name(r); }
  inline bool has_name(ref_cell r) const
     { return var_naming_.has_name(r); }
  inline void set_name(ref_cell r, const std::string &name)
     { var_naming_.set_name(r, name); }
  inline const std::string & get_name(ref_cell r) const
     { return var_naming_.get_name(r); }

  inline void unwind_trail(size_t from, size_t to)
  {
      for (size_t i = from; i < to; i++) {
	  size_t index = stacks_dock<ST>::trail_get(i);
          heap_dock<HT>::heap_set(index, ref_cell(index));
      }
  }

  inline void clear_trail() {
      stacks_dock<ST>::trim_trail(0);
  }

  inline bool unify(term a, term b, uint64_t &cost)
  {
      term_utils utils(*this);
      return utils.unify(a, b, cost);
  }

  inline term copy_without_names(term t, uint64_t &cost)
  {
      term_utils utils(*this);
      return utils.copy(t, var_naming(), heap_dock<HT>::get_heap(),
			nullptr, cost);
  }
    
  inline term copy(term t, uint64_t &cost)
  {
      term_utils utils(*this);
      return utils.copy(t, var_naming(), heap_dock<HT>::get_heap(),
			&var_naming(), cost);
  }

  inline term copy(term t, term_env &src, uint64_t &cost)
  {
      term_utils utils(*this);
      return utils.copy(t, var_naming(), src.get_heap(), &src.var_naming(), cost);
  }

  inline term copy_except_big(term t, uint64_t &cost)
  {
      term_utils utils(*this);
      utils.set_dont_copy_big(true);
      return utils.copy(t, var_naming(), heap_dock<HT>::get_heap(),
			nullptr, cost);
  }

  inline std::string list_to_string(term t, term_env &src)
  {
      term_utils utils(*this);
      return utils.list_to_string(t, src.get_heap());
  }

  inline std::string list_to_string(term t)
  {
      return list_to_string(t, *this);
  }

  inline term string_to_list(const std::string &str)
  {
      term_utils utils(*this);
      return utils.string_to_list(str);
  }

  inline bool is_string(term t)
  {
      term_utils utils(*this);
      return utils.is_string(t, *this);
  }

  inline uint64_t hash(term t)
  {
      term_utils utils(*this);
      return utils.hash(t);
  }

  inline size_t simple_hash(term t)
  {
      term_utils utils(*this);
      return utils.simple_hash(t);
  }

  inline uint64_t cost(const term t)
  {
      term_utils utils(*this);
      return utils.cost(t);
  }

  inline bool equal(const term a, const term b, uint64_t &cost)
  {
      term_utils utils(*this);
      return utils.equal(a, b, cost);
  }

  inline void bind(const ref_cell &a, term b)
  {
      size_t index = a.index();
      if (a.tag() == tag_t::RFW) {
	  heap_dock<HT>::heap_add_watched(index);
      }      
      heap_dock<HT>::heap_set(index, b);
      stacks_dock<ST>::trail(index);
  }

  inline int standard_order(const term a, const term b, uint64_t &cost)
  {
      term_utils utils(*this);
      return utils.standard_order(a, b, cost);
  }

  class term_range {
  public:
      inline term_range(term_env &env, const term t) : env_(env), term_(t) { }

      inline term_dfs_iterator begin() { return env_.begin(term_); }
      inline term_dfs_iterator end() { return env_.end(term_); }
  private:
      term_env &env_;
      term term_;
  };

  class const_term_range {
  public:
      inline const_term_range(const term_env &env, const term t)
           : env_(env), term_(t) { }

      inline const_term_dfs_iterator begin() const
      { return env_.begin(term_); }
      inline const_term_dfs_iterator end() const
      { return env_.end(term_); }
  private:
      const term_env &env_;
      term term_;
  };

  inline term_range iterate_over(const term t)
      { return term_range(*this, t); }

  inline const_term_range iterate_over(const term t) const
      { return const_term_range(*this, t); }

  inline std::string status() const
  { 
    std::stringstream ss;
    ss << "term_env::status() { heap_size=" << heap_dock<HT>::heap_size()
       << ",stack_size=" << stacks_dock<ST>::stack_size() << ",trail_size="
       << stacks_dock<ST>::trail_size() <<"}";
    return ss.str();
  }

  inline term parse(std::istream &in)
  {
      term_tokenizer tokenizer(in);
      term_parser parser(tokenizer, heap_dock<HT>::get_heap(),
			 ops_dock<OT>::get_ops());
      term r = parser.parse();

      // Once parsing is done we'll copy over the var-name bindings
      // so we can pretty print the variable names.
      parser.for_each_var_name( [this](ref_cell ref,
				    const std::string &name)
          { this->var_naming_.set_name(ref, name); } );
      return r;
  }
 
  inline term parse(const std::string &str)
  {
      std::stringstream ss(str);
      return parse(ss);
  }

  inline std::string to_string(const term t) const
  {
      emitter_options default_opt;
      return to_string(t, default_opt);
  }

  inline std::string to_string(const term t,const emitter_options &opt) const
  {
      term t1 = heap_dock<HT>::deref(t);
      std::stringstream ss;
      term_emitter emitter(ss, heap_dock<HT>::get_heap(),
			   ops_dock<OT>::get_ops());
      emitter.set_options(opt);
      emitter.set_var_naming(&var_naming_);
      emitter.print(t1);
      return ss.str();
  }

  inline std::string safe_to_string(const term t, const emitter_options &opt) const
  {
      return to_string(t, opt);
  }

  inline std::string safe_to_string(const term t) const
  {
      emitter_options default_opt;
      return to_string(t, default_opt);
  }

  const_big_iterator begin(const big_cell b)
  {
      return heap_dock<HT>::begin(b);
  }

  const_big_iterator end(const big_cell b)
  {
      return heap_dock<HT>::end(b);
  }  

  term_dfs_iterator begin(const term t)
  {
      return term_dfs_iterator(*this, t);
  }
  
  term_dfs_iterator end(const term t)
  {
      return term_dfs_iterator(*this);
  }

  const_term_dfs_iterator begin(const term t) const
  {
      return const_term_dfs_iterator(*this, t);
  }
  
  const_term_dfs_iterator end(const term t) const
  {
      return const_term_dfs_iterator(*this);
  }

  struct state_context {
      state_context(size_t tr, size_t st, size_t hb) :
	  trail_(tr), stack_(st), hb_(hb) { }
      size_t trail_;
      size_t stack_;
      size_t hb_;
  };

  state_context save_state() {
      return state_context(stacks_dock<ST>::trail_size(), stacks_dock<ST>::stack_size(), stacks_dock<ST>::get_register_hb());
  }

  void restore_state(state_context &context) {
      unwind_trail(context.trail_, stacks_dock<ST>::trail_size());
      stacks_dock<ST>::trim_trail(context.trail_);
      stacks_dock<ST>::trim_stack(context.stack_);
      stacks_dock<ST>::set_register_hb(context.hb_);
  }

  std::vector<std::pair<std::string, term> > find_vars(const term t0) {
      std::vector<std::pair<std::string, term> > vars;
      std::unordered_set<term> seen;
      // Record all vars for this query
      std::for_each( begin(t0),
		     end(t0),
		     [this,&seen,&vars](const term t) {
		         if (t.tag().is_ref()) {
			     const std::string name = this->to_string(t);
			     if (!seen.count(t)) {
				 vars.push_back(std::make_pair(name,t));
				 seen.insert(t);
			     }
			 }
		     } );
      return vars;
  }

  std::unordered_set<term> vars_of(term t0) {
      std::unordered_set<term> vars;
      // Record all vars for this query
      std::for_each( begin(t0),
		     end(t0),
		     [&vars](const term t) {
		         if (t.tag().is_ref()) {
			     vars.insert(t);
			 }
		     } );
      return vars;
  }

  void prettify_var_names(const term t0, std::vector<ref_cell> &touched)
  {
      std::map<ref_cell, size_t> count_occurrences;
      std::unordered_set<term> seen;
      std::unordered_set<std::string> used_names;
      stacks_dock<ST>::temp_clear();
      stacks_dock<ST>::temp_push(t0);

      while(stacks_dock<ST>::temp_size() > 0) {
	auto t = heap_dock<HT>::deref(stacks_dock<ST>::temp_pop());
	if (t.tag().is_ref()) {
	  auto r = reinterpret_cast<ref_cell &>(t);
	  if (!this->has_name(r)) {
	      ++count_occurrences[r];
	  } else {
	      used_names.insert(get_name(r));
	  }
	  continue;
	}

	if(seen.find(t) != seen.end()) {
	  continue;
	}
	seen.insert(t);
	
	if(t.tag() == tag_t::STR) {
	  auto f = heap_dock<HT>::functor(t);
	  auto num_args = f.arity();
	  for (size_t i = 0; i < num_args; i++) {
	      stacks_dock<ST>::temp_push(heap_dock<HT>::arg(t, num_args-i-1));
	  }
	}
      }

      // Those vars with a singleton occurrence will be named
      // '_'.
      size_t named_var_count = 0;
      for (auto v : count_occurrences) {
	  touched.push_back(v.first);
	  if (v.second == 1) {
	      set_name(v.first, "_");
	  } else { // v.second > 1
	      // Try to use friendly names A, B, C, D, ...
	      std::string name;
	      if (named_var_count < 26) {
		  name = ('A' + named_var_count);
		  if (used_names.count(name)) {
		      name = "G_" + boost::lexical_cast<std::string>(named_var_count);
		  }
	      } else {
		  name = "G_" + boost::lexical_cast<std::string>(named_var_count);
	      }
	      named_var_count++;
	      set_name(v.first, name);
	  }
      }
  }

  inline std::vector<std::string> get_expected(const term_parse_exception &ex)
  {
      term_utils utils(*this);
      return utils.get_expected(ex);
  }

  inline std::vector<std::string> get_expected_simplified(const term_parse_exception &ex)
  {
      term_utils utils(*this);
      return utils.get_expected_simplified(ex);
  }

  inline std::vector<std::string> to_error_messages(const token_exception &ex)
  {
      term_utils utils(*this);
      return utils.to_error_messages(ex);
  }

  inline std::vector<std::string> to_error_messages(const term_parse_exception &ex)
  {
      term_utils utils(*this);
      return utils.to_error_messages(ex);
  }

  inline void print_error_messages(std::ostream &out, const token_exception &ex)
  {
      term_utils utils(*this);
      return utils.print_error_messages(out, ex);
  }

  inline void print_error_messages(std::ostream &out, const term_parse_exception &ex)
  {
      term_utils utils(*this);
      return utils.print_error_messages(out, ex);
  }

  inline std::string to_string_debug(const term t) const
  {
      return to_string(t);
  }

private:
  naming_map var_naming_;
};

inline bool term_iterator::operator == (const term_iterator &other) const
{
    uint64_t cost = 0;
    return env_.equal(current_, other.current_, cost);
}

inline term term_iterator::first_of(const term t)
{
    if (env_.is_empty_list(t)) {
	return t;
    } else {
	if (!env_.is_dotted_pair(t)) {
	    throw term_exception_not_list(t);
	}
	return env_.arg(t, 0);
    }
}

inline void term_iterator::advance()
{
    term t = env_.deref(current_);
    if (t.tag() != common::tag_t::STR) {
	throw term_exception_not_list(current_);
    }
    current_ = env_.arg(current_, 1);
    elem_ = first_of(current_);
}

inline bool term_dfs_iterator::operator == (const term_dfs_iterator &other) const
{
    return elem_ == other.elem_ && stack_.size() == other.stack_.size();
}

inline bool const_term_dfs_iterator::operator == (const const_term_dfs_iterator &other) const
{
    return term_dfs_iterator::operator == (other);
}

inline term term_dfs_iterator::first_of(const term t)
{
    term p = env_.deref(t);
    while (p.tag() == tag_t::STR) {
	con_cell f = env_.functor(p);
	size_t arity = f.arity();
	if (arity > 0) {
	    stack_.push_back(dfs_pos(p, 0, arity));
	    p = env_.arg(p, 0);
	} else {
	    return p;
	}
    }
    return p;
}

inline void term_dfs_iterator::advance()
{
    if (stack_.empty()) {
	elem_ = term();
	return;
    }

    size_t new_index = ++stack_.back().index;
    if (new_index == stack_.back().arity) {
	elem_ = stack_.back().parent;
	stack_.pop_back();
	return;
    }
    elem_ = first_of(env_.arg(stack_.back().parent, new_index));
}

//
// This is handy for having unordered sets with equal terms.
// (Good for common sub-term recognition.) We also need this for
// good register allocation in the WAM compiler.
//
class eq_term {
public:
    inline eq_term(const eq_term &other)
        : env_(other.env_), term_(other.term_) { }
    inline eq_term(term_env &env, term t) : env_(env), term_(t) { }

    inline bool operator == (const eq_term &other) const {
        uint64_t cost = 0;
	bool eq = env_.equal(term_, other.term_, cost);
	return eq;
    }

    inline uint64_t hash() const {
        return env_.hash(term_);
    }

private:
    term_env &env_;
    term term_;
};

inline heap & term_utils::get_heap() {
    return env_.get_heap();
}

inline term_ops & term_utils::get_ops() {
    return env_.get_ops();
}	
	
inline term term_utils::new_ref() {
    return env_.new_ref();
}

inline term term_utils::new_dotted_pair(term a, term b) {
    return env_.new_dotted_pair(a,b);
}
	
inline term term_utils::heap_get(size_t index) const {
    return env_.heap_get(index);
}

inline void term_utils::heap_set(size_t index, term t) {
    env_.heap_set(index, t);
}

inline size_t term_utils::heap_size() const {
    return env_.heap_size();
}

inline term term_utils::new_term(con_cell functor) {
    return env_.new_term(functor);
}
	
inline size_t term_utils::num_bits(big_cell b) const {
    return env_.num_bits(b);
}

inline term term_utils::new_big(size_t nbits) {
    return env_.new_big(nbits);
}

inline void term_utils::get_big(term t, boost::multiprecision::cpp_int &i, size_t &nbits) const {
    env_.get_big(t, i, nbits);
}

inline int term_utils::big_compare(term t1, term t2, uint64_t &cost) const {
    return env_.big_compare(t1, t2, cost);
}

inline con_cell term_utils::functor(const term t) const {
    return env_.functor(t);
}

inline con_cell term_utils::functor(const std::string &name, size_t arity) {
    return env_.functor(name, arity);
}

inline std::string term_utils::atom_name(con_cell f) const {
    return env_.atom_name(f);
}

inline term term_utils::arg(const term t, size_t arg_index) const {
    return env_.arg(t, arg_index);
}

inline void term_utils::set_arg(term t, size_t index, const term arg) {
    env_.set_arg(t, index, arg);
}

inline size_t term_utils::stack_size() const {
    return env_.stack_size();
}

inline void term_utils::trim_stack(size_t new_size) {
    env_.trim_stack(new_size);
}

inline void term_utils::push(const term t) {
    env_.push(t);
}

inline term term_utils::pop() {
    return env_.pop();
}

inline bool term_utils::big_equal(term t1, term t2, uint64_t &cost) const
{
    return env_.big_equal(t1, t2, cost);
}

inline term term_utils::deref(const term t) const {
    return env_.deref(t);
}
	
inline term term_utils::deref_with_cost(const term t, uint64_t &cost) const {
    return env_.deref_with_cost(t, cost);
}

inline void term_utils::bind(const ref_cell &a, term b)
{
    if (a.tag() == tag_t::RFW) {
	env_.heap_add_watched(a.index());
    }
    size_t index = a.index();
    env_.heap_set(index, b);
    env_.trail(index);
}

inline void term_utils::unwind_trail(size_t from, size_t to)
{
    for (size_t i = from; i < to; i++) {
	size_t index = env_.trail_get(i);
	env_.heap_set(index, ref_cell(index));
    }
}

inline const_big_iterator term_utils::begin(const big_cell b) const {
    return env_.begin(b);
}
	
inline const_big_iterator term_utils::end(const big_cell b) const {
    return env_.end(b);
}

inline size_t term_utils::trail_size() const {
    return env_.trail_size();
}

inline void term_utils::trim_trail(size_t new_size) {
    env_.trim_trail(new_size);
}

inline void term_utils::tidy_trail(size_t from, size_t to) {
    env_.tidy_trail(from, to);
}

inline size_t term_utils::get_register_hb() const {
    return env_.get_register_hb();
}

inline void term_utils::set_register_hb(size_t h) {
    env_.set_register_hb(h);
}

inline size_t term_utils::temp_size() const {
    return env_.temp_size();
}

inline void term_utils::temp_clear() {
    env_.temp_clear();
}

inline void term_utils::temp_push(const term t) {
    env_.temp_push(t);
}

inline term term_utils::temp_pop() {
    return env_.temp_pop();
}

inline size_t term_utils::temp_trail_size() const {
    return env_.temp_trail_size();
}

inline void term_utils::temp_trail_push(const size_t i) {
    env_.temp_trail_push(i);
}

inline size_t term_utils::temp_trail_pop() {
    return env_.temp_trail_pop();
}

inline void term_utils::heap_watch(size_t addr, bool b) {
    env_.heap_watch(addr, b);
}
	
}}

namespace std {
    template<> struct hash<epilog::common::eq_term> {
        size_t operator()(const epilog::common::eq_term& eqt) const {
 	    return eqt.hash();
	}
    };
}

#endif
