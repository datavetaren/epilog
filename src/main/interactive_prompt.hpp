#pragma once

#ifndef _main_interactive_prompt_hpp
#define _main_interactive_prompt_hpp

#include "../common/readline.hpp"
#include "../terminal/terminal.hpp"
#include "../wallet/wallet.hpp"
#include "meta_interpreter.hpp"

namespace epilog { namespace main {

//
// This is the main terminal window that acts like a Prolog prompt
// and establishes a direct link to the node on the local machine.
//
class interactive_prompt {
private:
    using wallet = wallet::wallet;
    using terminal = terminal::terminal;
    
public:
    interactive_prompt();
    ~interactive_prompt();

    bool connect_node(unsigned short port);
    void connect_wallet(wallet *w);
    void connect_meta(meta_interpreter *meta);

    inline terminal * node_terminal()  { return node_terminal_; }
  
    void run();

private:
    using term = common::term;
    using readline = common::readline;

    bool key_callback(int ch);

    void halt();
    void pulse();

    term parse(const std::string &str);
    void execute(term t);
  
    void reset();
    bool has_more();
    bool next();
    std::string flush_text();
    void add_error(const std::string &msg);
    void add_text_output(const std::string &str);
    void add_text_output_no_nl(const std::string &str);

    std::string prompt_;
    bool stopped_;
    readline readline_;
    bool ctrl_c_;
    common::utime last_pulse_;
    bool in_wallet_;

    terminal *node_terminal_;
    wallet *wallet_;
    meta_interpreter *meta_;

    std::string text_;
};

}}

#endif
