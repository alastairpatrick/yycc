#include "nlohmann/json.hpp"

#include "std.h"
#include "ast.h"
#include "expr.h"
#include "decl.h"
#include "stmt.h"

using json = nlohmann::json;

enum class TestType {
	EXPR,
	DECL,
};

struct Test {
	const char* const name;
	const TestType type;
};

enum Section {
	INITIAL,
	INPUT,
	EXPECTED_AST,
	EXPECTED_TYPE,
	EXPECTED_MESSAGE,
	NUM_SECTIONS,
};

static const Test tests[] = {
	{ "decl",				TestType::DECL },
	{ "stmt",				TestType::DECL },
	{ "string_literal",		TestType::EXPR },
	{ "lex",				TestType::EXPR },
	{ "int_literal",		TestType::EXPR },
	{ "conversion",			TestType::EXPR },
	{ "float_literal",		TestType::EXPR },
	{ "expr",				TestType::EXPR },
};

shared_ptr<Expr> parse_expr(const string& input, ostream& message_stream);
DeclStatementList parse_statements(const string& input, ostream& message_stream);

static bool compare_json(const string& l, const string& r) {
}

static ostream& print_error(const string& name, const string& file, int line) {
	cerr << file << '(' << line << "): " << name << "\n";
	return cerr;
}

static bool test_case(TestType test_type, const string sections[NUM_SECTIONS], const string& name, const string& file, int line) {
	//try {
		const Type* type = nullptr;
		stringstream message_stream;
		stringstream output_stream;
		if (test_type == TestType::EXPR) {
			auto expr = parse_expr(sections[INPUT], message_stream);
			if (!sections[EXPECTED_TYPE].empty()) {
				type = expr->get_type();
			}
			output_stream << expr;
		} else {
			auto statements = parse_statements(sections[INPUT], message_stream);
			output_stream << statements;
		}

		
		if (message_stream.str() != sections[EXPECTED_MESSAGE]) {
			print_error(name, file, line) << "Expected message:\n" << sections[EXPECTED_MESSAGE] << "\n  Actual message:\n" << message_stream.str();
		}

		if (!sections[EXPECTED_AST].empty() && json::parse(output_stream) != json::parse(sections[EXPECTED_AST])) {
			print_error(name, file, line) << "Expected AST: " << sections[EXPECTED_AST] << "\n  Actual AST: " << output_stream.str() << "\n";
			return false;
		}

		if (!sections[EXPECTED_TYPE].empty()) {
			stringstream type_stream;
			type_stream << type;

			if (type_stream.str() != sections[EXPECTED_TYPE]) {
				print_error(name, file, line) << "Expected type: " << sections[EXPECTED_TYPE] << "\n  Actual type: " << type_stream.str() << "\n";
				return false;
			}
		}
	//} catch (exception& e) {
	//	print_error(name, file, line) << "Exception thrown: " << e.what() << "\n";
	//	return false;
	//}

	return true;
}

bool run_parser_tests() {
	string test_dir = __FILE__;
	auto slash_idx = test_dir.find_last_of("/\\");
	test_dir  = test_dir.substr(0, slash_idx + 1);

	auto num_tests = 0;
	auto num_failures = 0;
	for (auto& test: tests) {
		string test_name;
		auto test_file_name = test_dir + test.name + ".test";
		fstream file_stream(test_file_name, ios_base::in);
		string sections[NUM_SECTIONS];

		auto test_line_num = 0;
		auto section = INITIAL;

		for (auto line_num = 1; !file_stream.eof() && !file_stream.fail(); ++line_num) {
			string line;
			getline(file_stream, line);

			if (file_stream.eof() || line.substr(0, 5) == "BEGIN") {
				if (section != INITIAL) {
					if (!test_case(test.type, sections, test_name, test_file_name, test_line_num)) {
						++num_failures;
					}

					++num_tests;
					for (auto i = 0; i < NUM_SECTIONS; ++i) sections[i].clear();
				}

				section = Section::INPUT;
				test_line_num = line_num;
				if (line.length() >= 6) test_name = line.substr(6);
			} else if (line.substr(0, 10) == "EXPECT_AST") {
				section = Section::EXPECTED_AST;
			} else if (line.substr(0, 11) == "EXPECT_TYPE") {
				section = Section::EXPECTED_TYPE;
			} else if (line.substr(0, 14) == "EXPECT_MESSAGE") {
				section = Section::EXPECTED_MESSAGE;
			} else {
				if (!line.empty()) {
					if (section == INPUT || section == EXPECTED_MESSAGE) line += '\n';  // getline removes \n
					sections[section] += line;
				}
			}
		}

		if (file_stream.fail() && !file_stream.eof()) {
			cerr << "Error processing file " << test.name << ".parser_test\n";
			++num_failures;
		}
	}

	cerr << "Ran " << num_tests << " parser tests of which " << num_failures << " failed.\n";

	return num_failures == 0;
}
