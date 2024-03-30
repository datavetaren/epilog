#include <iostream>
#include <boost/filesystem.hpp>
#include <common/test/test_home_dir.hpp>

int main(int argc, char *argv[])
{
    boost::filesystem::path root(find_home_dir(argv[0]));
    boost::filesystem::path bin_path(argv[1]);

    auto src_file = root / "src" / "main" / "startup.pl";
    auto dst_file = bin_path;

    // If this is the executable, then get its parent directory
    if (boost::filesystem::exists(dst_file) &&
	boost::filesystem::is_regular_file(dst_file)) {
	dst_file = dst_file.parent_path();
    }

    dst_file = dst_file / "epilog-data" / "startup.pl";

    std::cout << "Copy file: " << src_file.string() << std::endl;
    std::cout << "       to: " << dst_file.string() << std::endl;

    try {
      boost::filesystem::create_directories(dst_file.parent_path());
      boost::filesystem::copy_file(src_file, dst_file,
		   boost::filesystem::copy_options::overwrite_existing);
    } catch (std::exception &ex) {
      std::cout << "Failed: " << ex.what() << std::endl;
      return 1;
    }

    return 0;
}
