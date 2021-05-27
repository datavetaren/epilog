#pragma once

#ifndef _wallet_wallet_hpp
#define _wallet_wallet_hpp

#include "../common/term_env.hpp"
#include "../terminal/terminal.hpp"
#include "wallet_interpreter.hpp"

namespace epilog { namespace wallet {

//
// The wallet is a pretty isolated piece of code. It loads in prolog source
// code (the wallet) in its own interpreter and communicate to the node
// via a (non-interactive) terminal. Terminals are socket based. This make
// the load balancing somewhat easier, so that the wallet can download and
// process things and thus will not starve computer threads to death on
// initial synchronizatons/wallet sweeps. It also enables wallets being run
// on a different computer (which could be on a different network.)
//
class wallet {
public:
    using terminal = epilog::terminal::terminal;
    using remote_return_t = interp::remote_return_t;

    // For testing purposes
    static void erase(const std::string &wallet_file);
    static void erase_all_test_wallets(const std::string &dir);
  
    // Wallet file is some Prolog source code (.pl) representing a wallet.
    wallet(const std::string &wallet_file = "");
    ~wallet();

    void set_current_directory(const std::string &dir);

    void total_reset();

    inline common::term_env & env() { return interp_; }
    inline wallet_interpreter & interp() { return interp_; }

    const std::string & get_file() const;
    void set_file(const std::string &wallet_file);
    bool using_new_file() const { return new_file_; }
    void load();
    void save();
    void set_auto_save(bool on) { auto_save_ = on; }
    bool is_auto_save() const { return auto_save_; }
    void create(const std::string &passwd, common::term sentence);
    void check_dirty();

    // Start the thread that will talk to the node.
    void connect_node(terminal *node_terminal);
    void node_pulse();
    void print();

    common::term parse(const std::string &cmd);
    std::string to_string(common::term t);
    std::string execute(const std::string &cmd);
    bool execute(common::term query);
    std::string get_result();
    common::term get_result_term();
    common::term get_result_term(const std::string &varname);
    void reset();
    bool has_more();
    bool next();

    remote_return_t execute_at(common::term query, common::term else_do, interp::interpreter_base &query_interp, const std::string &where, interp::remote_execute_mode mode, size_t timeout);
    remote_return_t continue_at(common::term query, common::term else_do, interp::interpreter_base &query_interp, const std::string &where, interp::remote_execute_mode mode, size_t timeout);
    bool delete_instance_at(common::term_env &query_src, const std::string &where);
  
private:
    std::string wallet_file_;
    bool new_file_;
    bool auto_save_;
    wallet_interpreter interp_;
    bool killed_;

    // This is the terminal to the node.
    terminal *terminal_;
};
    
}}

#endif
