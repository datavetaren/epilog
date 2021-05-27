#include <iomanip>
#include "term_tokenizer.hpp"

namespace epilog { namespace common {

static const bool T = true;
static const bool F = false;

const bool token_chars::IS_SYMBOL[256] = 
    { /* 00 */ F, F, F, F, F, F, F, F, F, F, F, F, F, F, F, F,
      /* 10 */ F, F, F, F, F, F, F, F, F, F, F, F, F, F, F, F,
      /* 20 */ F, F, F, T, T, F, T, F, F, F, T, T, F, T, T, T,
      /* 30 */ F, F, F, F, F, F, F, F, F, F, T, F, T, T, T, T,
      /* 40 */ T, F, F, F, F, F, F, F, F, F, F, F, F, F, F, F,
      /* 50 */ F, F, F, F, F, F, F, F, F, F, F, F, T, F, T, F,
      /* 60 */ T, F, F, F, F, F, F, F, F, F, F, F, F, F, F, F,
      /* 70 */ F, F, F, F, F, F, F, F, F, F, F, F, F, F, T, F,
      /* 80 */ F, F, F, F, F, F, F, F, F, F, F, F, F, F, F, F,
      /* 90 */ F, F, F, F, F, F, F, F, F, F, F, F, F, F, F, F,
      /* A0 */ T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,
      /* B0 */ T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,
      /* C0 */ F, F, F, F, F, F, F, F, F, F, F, F, F, F, F, F,
      /* D0 */ F, F, F, F, F, F, F, T, F, F, F, F, F, F, F, F,
      /* E0 */ F, F, F, F, F, F, F, F, F, F, F, F, F, F, F, F,
      /* F0 */ F, F, F, F, F, F, F, T, F, F, F, F, F, F, F, F
    };



std::string token_chars::escape(const std::string &str)
{
    std::stringstream ss;
    for (auto ch : str) {
	if (should_be_escaped(ch)) {
	    ss << "\\x";
	    ss << std::setfill('0') << std::setw(2) << std::setbase(16) << (int)ch;
	} else {
	    ss << ch;
	}
    }
    return ss.str();
}

std::string token_chars::escape_ascii(const std::string &str)
{
    std::stringstream ss;
    for (auto ch : str) {
        int cp = ((int)ch) & 0xff;
        if (cp >= 127 || should_be_escaped(cp)) {
	    ss << "\\x";
	    ss << std::setfill('0') << std::setw(2) << std::setbase(16) << cp;
	} else {
	    ss << ch;
	}
    }
    return ss.str();
}

std::string token_chars::escape_pretty(const std::string &str)
{
    std::stringstream ss;
    for (auto ch : str) {
	int cp = ((int)ch) & 0xff;
	switch (cp) {
	case 8: ss << "\\b"; break;
	case 9: ss << "\\t"; break;
	case 10: ss << "\\n"; break;
	case 11: ss << "\\v"; break;
	case 12: ss << "\\f"; break;
	case 13: ss << "\\r"; break;
	case 27: ss << "\\e"; break;
        case 32: ss << " "; break;
	case 127: ss << "\\d"; break;
	case 7: ss << "\\a"; break;
	case '\'': ss << "\\\'"; break;
	default:
	    if (cp >= 127 || should_be_escaped(cp)) {
		ss << "\\x";
		ss << std::setfill('0') << std::setw(2) << std::setbase(16) << cp;
	    } else {
		ss << ch;
	    }
	}
    }
    return ss.str();
}

}}
