#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <iomanip>
#include <common/random.hpp>
#include <node/self_node.hpp>
#include <node/session.hpp>
#include <main/meta_interpreter.hpp>
#include <terminal/terminal.hpp>
#include "../../interp/test/test_files_infrastructure.hpp"

using namespace epilog::common;
using namespace epilog::interp;
using namespace epilog::terminal;
using namespace epilog::node;
using namespace epilog::main;
using namespace epilog::global;

std::string test_dir;

static void header( const std::string &str )
{
    std::cout << "\n";
    std::cout << "--- [" + str + "] " + std::string(60 - str.length(), '-') << "\n";
    std::cout << "\n";
}


static bool is_full(int argc, char *argv[])
{
    for (int i = 0; i < argc; i++) {
	if (strcmp(argv[i], "-full") == 0) {
	    return true;
	}
    }
    return false;
}

int main( int argc, char *argv[] )
{
    header( "test_main_pl_files" );

    random::set_for_testing(true);
    
    std::string home_dir = find_home_dir(argv[0]);
    test_dir = (boost::filesystem::path(home_dir) / "bin" / "test" / "main").string();
    
    full_mode = is_full(argc, argv);

    const char *name = NAME;

    const std::string dir = "/src/main/test/pl_files";

    test_interpreter_files<meta_interpreter>(
	      dir,
	      [&](meta_interpreter &mi){mi.set_home_dir(test_dir);},
	      [&](const std::string &cmd) { return false; },
	      name);

    return 0;
}

