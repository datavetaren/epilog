#include <common/test/test_home_dir.hpp>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <iomanip>
#include <node/self_node.hpp>
#include <terminal/terminal.hpp>
#include <boost/filesystem.hpp>

using namespace epilog::common;
using namespace epilog::node;
using namespace epilog::terminal;
using namespace epilog::global;

std::string test_dir;

static void header( const std::string &str )
{
    std::cout << "\n";
    std::cout << "--- [" + str + "] " + std::string(60 - str.length(), '-') << "\n";
    std::cout << "\n";
}

static void test_terminal()
{
    header("test_terminal()");

    std::cout << "Start node at port 8000" << std::endl;

    global::erase_db(test_dir);
    
    self_node node(test_dir, 8000);
    node.start();

    std::cout << "Open terminal..." << std::endl;

    terminal tm(8000);

    if (!tm.connect()) {
	std::cout << "Failed to open connection." << std::endl;
    }

    bool r = tm.execute("member(X, [1,2,3,4]).");
    assert(r);

    const size_t N = 4;
    std::string expect [N] = { "1", "2", "3", "4" };

    for (size_t i = 0; i < N; i++) {
	auto actual = tm.get_result_string("X");
	std::cout << "Expect: X = " << expect[i] << std::endl;
	if (actual.empty()) {
	    actual = "<no value>";
	}
	std::cout << "Actual: X = " << actual << std::endl;
	assert(expect[i] == actual);

	std::cout << "Text  : " << tm.flush_text() << std::endl;

	if (i < N - 1) {
	    assert(tm.has_more());
	    assert(tm.next());
	}
    }
    // Check that we don't have more solutions...
    assert(!tm.next());

    // Shutdown terminal
    tm.close();

    // Shutdown node
    node.stop();
    node.join();
}

int main(int argc, char *argv[])
{
    std::string home_dir = find_home_dir(argv[0]);
    test_dir = (boost::filesystem::path(home_dir) / "bin" / "test" / "node" / "triedb").string();
    
    test_terminal();
    return 0;
}
