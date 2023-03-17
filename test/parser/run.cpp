#include "nlohmann/json.hpp"

#include "ast/ASTNode.h"
#include "ast/Expr.h"
#include "ast/Declaration.h"
#include "ast/Parser.h"
#include "ast/Statement.h"
#include "CompileContext.h"

using json = nlohmann::json;

enum class TestType {
    EXPRESSION,
    DECLARATIONS,
    PREPARSE,

    NUM
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
    { "typedef",            TestType::PREPARSE },
    { "var_decl",           TestType::PREPARSE },
    { "fun_decl",           TestType::PREPARSE },

    { "typedef",            TestType::DECLARATIONS },
    { "var_decl",           TestType::DECLARATIONS },
    { "fun_decl",           TestType::DECLARATIONS },
    { "stmt",               TestType::DECLARATIONS },
    { "string_literal",     TestType::EXPRESSION },
    { "lex",                TestType::EXPRESSION },
    { "int_literal",        TestType::EXPRESSION },
    { "conversion",         TestType::EXPRESSION },
    { "float_literal",      TestType::EXPRESSION },
    { "expr",               TestType::EXPRESSION },
};

static ostream& print_error(const string& name, const string& file, int line) {
    cerr << file << '(' << line << "): " << name << "\n";
    return cerr;
}

Expr* parse_expr(const string& input) {
    Parser parser(input, false);
    auto result = parser.parse_expr(0);
    if (!parser.check_eof()) return nullptr;
    return result;
}

ASTNodeVector parse_declarations(const string& input, bool preparse) {
    Parser parser(input, preparse);
    parser.parse_unit();
    return move(parser.declarations);
}

static bool test_case(TestType test_type, const string sections[NUM_SECTIONS], const string& name, const string& file, int line) {
    //try {
        stringstream message_stream;
        CompileContext compile_context(message_stream);

        const Type* type{};
        stringstream output_stream;
        if (test_type == TestType::EXPRESSION) {
            auto expr = parse_expr(sections[INPUT]);
            if (!sections[EXPECTED_TYPE].empty()) {
                type = expr->get_type();
            }
            output_stream << expr;
        } else {
            auto statements = parse_declarations(sections[INPUT], test_type == TestType::PREPARSE);
            output_stream << statements;
        }

        
        if (message_stream.str() != sections[EXPECTED_MESSAGE]) {
            print_error(name, file, line) << "Expected message:\n" << sections[EXPECTED_MESSAGE] << "\n  Actual message:\n" << message_stream.str();
        }

        if (!sections[EXPECTED_AST].empty()) {
            auto parsed_output = json::parse(output_stream);
            auto parsed_expected = json::parse(sections[EXPECTED_AST]);

            if (parsed_output != parsed_expected) {
                print_error(name, file, line) << "Expected AST: " << parsed_expected << "\n  Actual AST: " << parsed_output << "\n";
                return false;
            }
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
        bool enabled_types[unsigned(TestType::NUM)] = {};
        int num_enabled_types = 0;

        for (auto line_num = 1; !file_stream.eof() && !file_stream.fail(); ++line_num) {
            string line;
            getline(file_stream, line);

            if (file_stream.eof() || line.substr(0, 5) == "BEGIN") {
                if (section != INITIAL) {
                    if (num_enabled_types == 0 || enabled_types[unsigned(test.type)]) {
                        if (!test_case(test.type, sections, test_name, test_file_name, test_line_num)) {
                            ++num_failures;
                        }

                        ++num_tests;
                    }

                    for (auto i = 0; i < NUM_SECTIONS; ++i) sections[i].clear();
                }

                section = Section::INPUT;
                for (unsigned i = 0; i < unsigned(TestType::NUM); ++i) enabled_types[i] = false;
                num_enabled_types = 0;
                test_line_num = line_num;
                if (line.length() >= 6) test_name = line.substr(6);
            } else if (line.substr(0, 10) == "EXPECT_AST") {
                section = Section::EXPECTED_AST;
            } else if (line.substr(0, 11) == "EXPECT_TYPE") {
                section = Section::EXPECTED_TYPE;
            } else if (line.substr(0, 14) == "EXPECT_MESSAGE") {
                section = Section::EXPECTED_MESSAGE;
            } else if (line.substr(0, 10) == "EXPRESSION") {
                enabled_types[unsigned(TestType::EXPRESSION)] = true;
                ++num_enabled_types;
            } else if (line.substr(0, 8) == "PREPARSE") {
                enabled_types[unsigned(TestType::PREPARSE)] = true;
                ++num_enabled_types;
            } else if (line.substr(0, 12) == "DECLARATIONS") {
                enabled_types[unsigned(TestType::DECLARATIONS)] = true;
                ++num_enabled_types;
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
