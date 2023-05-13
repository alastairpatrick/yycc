#include "nlohmann/json.hpp"

#include "FileCache.h"
#include "parse/ASTNode.h"
#include "parse/Expr.h"
#include "parse/Declaration.h"
#include "parse/Parser.h"
#include "parse/Statement.h"
#include "TranslationUnitContext.h"
#include "visit/Emitter.h"
#include "visit/ResolvePass.h"

using json = nlohmann::json;

enum class TestType {
    PREPROCESS,
    SWEEP,
    EXPRESSION,
    STATEMENT,
    PREPARSE,
    PARSE,
    RESOLVE,
    EMIT,

    NUM
};

struct Test {
    const char* const name;
    const TestType type;
};

enum Section {
    INITIAL,

    EXPECT_AST,
    EXPECT_GLOBALS,
    EXPECT_IR,
    EXPECT_MESSAGE,
    EXPECT_TEXT,
    EXPECT_TYPE,
    FILE_SECTION,
    INPUT,
    NONE,

    NUM_SECTIONS,
};

static const Test tests[] = {
    { "parse/expr",                 TestType::EXPRESSION },











    { "parse/preprocess",           TestType::PREPROCESS },
    { "parse/sweep",                TestType::SWEEP },

    { "parse/string_literal",       TestType::EXPRESSION },
    { "parse/lex",                  TestType::EXPRESSION },
    { "parse/int_literal",          TestType::EXPRESSION },
    { "parse/float_literal",        TestType::EXPRESSION },
    { "parse/expr",                 TestType::EXPRESSION },

    { "parse/namespace",            TestType::PREPARSE},
    { "parse/namespace",            TestType::PARSE},
    { "resolve/namespace",          TestType::RESOLVE},

    { "parse/array",                TestType::PREPARSE },
    { "parse/array",                TestType::PARSE },
    { "resolve/array",              TestType::RESOLVE },

    { "parse/enum",                 TestType::PREPARSE },
    { "parse/enum",                 TestType::PARSE },

    { "resolve/expr",               TestType::RESOLVE},

    { "parse/function",             TestType::PREPARSE },
    { "parse/function",             TestType::PARSE },
    { "resolve/function",           TestType::RESOLVE },

    { "parse/statement",            TestType::STATEMENT },
    { "resolve/statement",          TestType::RESOLVE },

    { "parse/struct",               TestType::PREPARSE },
    { "parse/struct",               TestType::PARSE },
    { "resolve/struct",             TestType::RESOLVE },

    { "parse/typedef",              TestType::PREPARSE },
    { "parse/typedef",              TestType::PARSE },
    { "resolve/typedef",            TestType::RESOLVE},

    { "parse/union",                TestType::PREPARSE },
    { "parse/union",                TestType::PARSE },
    { "resolve/union",              TestType::RESOLVE },

    { "parse/variable",             TestType::PREPARSE },
    { "parse/variable",             TestType::PARSE },
    { "resolve/variable",           TestType::RESOLVE },

    { "parse/recovery",             TestType::PREPARSE },
    { "parse/recovery",             TestType::PARSE },

    { "emit/conversion_type",       TestType::EXPRESSION },

    { "emit/assignment",            TestType::EMIT },
    { "emit/binary_expr",           TestType::EMIT },
    { "emit/conversion",            TestType::EMIT },
    { "emit/emit",                  TestType::EMIT },
    { "emit/expr",                  TestType::EMIT },
    { "emit/statement",             TestType::EMIT },
    { "emit/variable",              TestType::EMIT },
};

static ostream& print_error(const string& name, const string& file, int line) {
    cerr << file << '(' << line << "): " << name << "\n";
    return cerr;
}

Expr* parse_expr(IdentifierMap& identifiers, const Input& input) {
    Preprocessor preprocessor(false);
    preprocessor.in(input);

    Parser parser(preprocessor, identifiers);
    auto result = parser.parse_standalone_expr();
    if (!parser.check_eof()) return nullptr;

    return result;
}

Statement* parse_statement(IdentifierMap& identifiers, const Input& input) {
    Preprocessor preprocessor(false);
    preprocessor.in(input);

    Parser parser(preprocessor, identifiers);
    auto result = parser.parse_standalone_statement();
    if (!parser.check_eof()) return nullptr;

    return result;
}

vector<Declaration*> parse_declarations(IdentifierMap& identifiers, bool preparse, const Input& input) {
    Preprocessor preprocessor(preparse);
    preprocessor.in(input);

    Parser parser(preprocessor, identifiers);
    auto declarations = parser.parse();

    return move(declarations);
}

void sweep(ostream& stream, const File& file);

static bool test_case(TestType test_type, const string sections[NUM_SECTIONS], const string& name, const string& file, int line) {
    //try {
        stringstream message_stream;

        TranslationUnitContext context(message_stream);

        IdentifierMap identifiers;

        const Type* type{};
        string module_ir;
        stringstream output_stream;
        if (test_type == TestType::EXPRESSION) {
            auto expr = parse_expr(identifiers, sections[INPUT]);
            if (!sections[EXPECT_TYPE].empty()) {
                type = get_expr_type(expr);
            }
            output_stream << expr;
        } else if (test_type == TestType::STATEMENT) {
            auto statement = parse_statement(identifiers, sections[INPUT]);
            output_stream << statement;
        } else if (test_type == TestType::PREPROCESS) {
            Preprocessor preprocessor(sections[INPUT], true);
            while (preprocessor.next_token() != TOK_EOF) {
            }
            output_stream << preprocessor.output();
        } else if (test_type == TestType::SWEEP) {
            File file;
            file.text = sections[INPUT];
            sweep(output_stream, file);
        } else {
            auto declarations = parse_declarations(identifiers, test_type == TestType::PREPARSE, sections[INPUT]);

            if (test_type >= TestType::RESOLVE) {
                auto resolved_module = resolve_pass(declarations, *identifiers.file_scope());

                if (test_type >= TestType::EMIT) {
                    EmitOptions options;
                    options.initialize_variables = false;

                    auto module = emit_pass(resolved_module, options);
                    char* module_string = LLVMPrintModuleToString(module);
                    module_ir = module_string;
                    LLVMDisposeMessage(module_string);
                    LLVMDisposeModule(module);

                    // Erase first three lines:
                    module_ir.erase(0, module_ir.find("\n") + 1);
                    module_ir.erase(0, module_ir.find("\n") + 1);
                    module_ir.erase(0, module_ir.find("\n") + 1);
                }
            }

            output_stream << declarations;
        }

        
        if (message_stream.str() != sections[EXPECT_MESSAGE]) {
            print_error(name, file, line) << "Expected message:\n" << sections[EXPECT_MESSAGE] << "\n  Actual message:\n" << message_stream.str() << '\n';
            return false;
        }

        if (!sections[EXPECT_AST].empty()) {
            auto parsed_output = json::parse(output_stream);
            auto parsed_expected = json::parse(sections[EXPECT_AST]);

            if (parsed_output != parsed_expected) {
                print_error(name, file, line) << "Expected AST: " << parsed_expected << "\n  Actual AST: " << parsed_output << "\n";
                return false;
            }
        }

        if (!sections[EXPECT_GLOBALS].empty()) {
            vector<Declarator*> declarators;
            for (auto& pair : identifiers.file_scope()->declarator_map) {
                declarators.push_back(pair.second);
            }

            sort(declarators.begin(), declarators.end(), [](Declarator* a, Declarator* b) {
                return *a->identifier < *b->identifier;
            });

            stringstream global_stream;
            global_stream << '[';
            bool separator = false;
            for (auto declarator : declarators) {
                if (separator) global_stream << ',';
                separator = true;
                global_stream << declarator;
            }
            global_stream << ']';

            auto parsed_globals = json::parse(global_stream);
            auto parsed_expected = json::parse(sections[EXPECT_GLOBALS]);

            if (parsed_globals != parsed_expected) {
                print_error(name, file, line) << "Expected globals: " << parsed_expected << "\n  Actual globals: " << parsed_globals << "\n";
                return false;
            }
        }
        
        if (!sections[EXPECT_IR].empty()) {
            if (module_ir != sections[EXPECT_IR]) {
                print_error(name, file, line) << "Expected IR:\n" << sections[EXPECT_IR] << "\n  Actual IR:\n" << module_ir << "\n";
                return false;
            }
        }

        if (!sections[EXPECT_TYPE].empty()) {
            stringstream type_stream;
            type_stream << type;

            if (type_stream.str() != sections[EXPECT_TYPE]) {
                print_error(name, file, line) << "Expected type: " << sections[EXPECT_TYPE] << "\n  Actual type: " << type_stream.str() << "\n";
                return false;
            }
        }

        if (!sections[EXPECT_TEXT].empty()) {
            if (output_stream.str() != sections[EXPECT_TEXT]) {
                print_error(name, file, line) << "Expected text:\n" << sections[EXPECT_TEXT] << "\n  Actual text:\n" << output_stream.str() << "\n";
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

        FileCache file_cache(false);
        File* file{};

        for (auto line_num = 1; !file_stream.fail(); ++line_num) {
            string line;
            getline(file_stream, line);

            if ((line.empty() && file_stream.eof()) || line.substr(0, 5) == "BEGIN") {
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
                file_cache.files.clear();
                test_line_num = line_num;
                if (line.length() >= 6) test_name = line.substr(6);
            } else if (line.substr(0, 4) == "FILE") {
                section = Section::FILE_SECTION;
                string header_name = line.substr(5);
                file = file_cache.add(header_name);
                file->path = header_name.substr(1, header_name.length() - 2);
            } else if (line == "END") {
                section = Section::NONE;
            } else if (line == "EXPECT_AST") {
                section = Section::EXPECT_AST;
            } else if (line == "EXPECT_GLOBALS") {
                section = Section::EXPECT_GLOBALS;
            } else if (line == "EXPECT_IR") {
                section = Section::EXPECT_IR;
            } else if (line == "EXPECT_MESSAGE") {
                section = Section::EXPECT_MESSAGE;
            } else if (line == "EXPECT_TEXT") {
                section = Section::EXPECT_TEXT;
            } else if (line == "EXPECT_TYPE") {
                section = Section::EXPECT_TYPE;
            } else if (line == "EMIT") {
                enabled_types[unsigned(TestType::EMIT)] = true;
                ++num_enabled_types;
            } else if (line == "EXPRESSION") {
                enabled_types[unsigned(TestType::EXPRESSION)] = true;
                ++num_enabled_types;
            } else if (line == "RESOLVE") {
                enabled_types[unsigned(TestType::RESOLVE)] = true;
                ++num_enabled_types;
            } else if (line == "PARSE") {
                enabled_types[unsigned(TestType::PARSE)] = true;
                ++num_enabled_types;
            } else if (line == "PREPARSE") {
                enabled_types[unsigned(TestType::PREPARSE)] = true;
                ++num_enabled_types;
            } else if (line == "PREPROCESS") {
                enabled_types[unsigned(TestType::PREPROCESS)] = true;
                ++num_enabled_types;
            } else if (line == "STATEMENT") {
                enabled_types[unsigned(TestType::STATEMENT)] = true;
                ++num_enabled_types;
            } else if (line == "SWEEP") {
                enabled_types[unsigned(TestType::SWEEP)] = true;
                ++num_enabled_types;
            } else if (line.substr(0, 3) == "REM") {
            } else {
                if (section == EXPECT_TYPE) {
                    if (line.length()) sections[section] += line;
                } else if (section == FILE_SECTION) {
                    file->text += line + "\n";
                } else {
                    sections[section] += line + "\n";
                }
            }

            if (line.empty() && file_stream.eof()) break;
        }

        if (file_stream.fail() && !file_stream.eof()) {
            cerr << "Error processing file " << test.name << ".test\n";
            ++num_failures;
        }
    }

    cerr << "Ran " << num_tests << " parser tests of which " << num_failures << " failed.\n";

    return num_failures == 0;
}
