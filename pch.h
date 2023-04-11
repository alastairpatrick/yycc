#include "llvm-c/Core.h"
#include "llvm-c/Analysis.h"
#include "llvm-c/BitWriter.h"
#include "llvm-c/ExecutionEngine.h"
#include "llvm-c/Target.h"
#include "llvm-c/TargetMachine.h"

#include "reflex/matcher.h"
#include "reflex/abslexer.h"

#include <algorithm>
#include <cassert>
#include <charconv>
#include <exception>
#include <filesystem>
#include <functional>
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <set>
#include <strstream>
#include <stack>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using reflex::Input;

using std::cerr;
using std::chars_format;
using std::cin;
using std::cout;
using std::dec;
using std::errc;
using std::exception;
using std::from_chars;
using std::fstream;
using std::function;
using std::hash;
using std::hex;
using std::hexfloat;
using std::ios_base;
using std::istream;
using std::list;
using std::make_pair;
using std::map;
using std::max;
using std::min;
using std::move;
using std::ostream;
using std::set;
using std::strstream;
using std::pair;
using std::remove_if;
using std::stack;
using std::string;
using std::stringstream;
using std::string_view;
using std::unique_ptr;
using std::unordered_map;
using std::unordered_set;
using std::vector;
using std::wstring;

namespace filesystem = std::filesystem;