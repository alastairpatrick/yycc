#include "nlohmann/json.hpp"

#include "FileCache.h"
#include "parser/ASTNode.h"
#include "parser/Expr.h"
#include "parser/Declaration.h"
#include "parser/Parser.h"
#include "parser/Statement.h"
#include "TranslationUnitContext.h"
#include "visitor/Emitter.h"
#include "visitor/ResolvePass.h"

using json = nlohmann::json;

enum class TestType {
    EXPRESSION,
    DECLARATIONS,
    PREPARSE,
    PREPROCESS,

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
    EXPECT_MESSAGE,
    EXPECT_TEXT,
    EXPECT_TYPE,
    FILE_SECTION,
    INPUT,
    NONE,

    NUM_SECTIONS,
};

static const Test tests[] = {
    { "preprocess",         TestType::PREPROCESS },
    { "sweep",              TestType::PREPROCESS },

    { "typedef",            TestType::PREPARSE },
    { "typedef_comp",       TestType::PREPARSE },
    { "var_decl",           TestType::PREPARSE },
    { "var_comp_decl",      TestType::PREPARSE },
    { "fun_decl",           TestType::PREPARSE },
    { "fun_comp_decl",      TestType::PREPARSE },
    { "struct",             TestType::PREPARSE },
    { "union",              TestType::PREPARSE },
    { "enum",               TestType::PREPARSE },
    { "array",              TestType::PREPARSE },

    { "typedef",            TestType::DECLARATIONS },
    { "typedef_comp",       TestType::DECLARATIONS },
    { "var_decl",           TestType::DECLARATIONS },
    { "var_comp_decl",      TestType::DECLARATIONS },
    { "fun_decl",           TestType::DECLARATIONS },
    { "fun_comp_decl",      TestType::DECLARATIONS },
    { "struct",             TestType::DECLARATIONS },
    { "union",              TestType::DECLARATIONS },
    { "enum",               TestType::DECLARATIONS },
    { "stmt",               TestType::DECLARATIONS },
    { "array",              TestType::DECLARATIONS },

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

Expr* parse_expr(IdentifierMap& identifiers, const Input& input) {
    Preprocessor preprocessor(false);
    preprocessor.in(input);

    Parser parser(preprocessor, identifiers);
    auto result = parser.parse_standalone_expr();
    if (!parser.check_eof()) return nullptr;

    return result;
}

ASTNodeVector parse_declarations(IdentifierMap& identifiers, const Input& input) {
    Preprocessor preprocessor(identifiers.preparse);
    preprocessor.in(input);

    Parser parser(preprocessor, identifiers);
    auto declarations = parser.parse();

    if (!identifiers.preparse) resolve_pass(identifiers.scopes.front(), declarations);

    return move(declarations);
}

void sweep(ostream& stream, const File& file);

static void init_emitter(Emitter& context) {
    extern LLVMTargetRef g_target;
    extern LLVMTargetMachineRef g_target_machine;
    extern LLVMTargetDataRef g_target_data;

    context.target = g_target;
    context.target_machine = g_target_machine;
    context.target_data = g_target_data;
}

static bool test_case(TestType test_type, const string sections[NUM_SECTIONS], const string& name, const string& file, int line) {
    //try {
        stringstream message_stream;

        TranslationUnitContext tu_context(message_stream);
        init_emitter(tu_context.type_emitter);
        init_emitter(tu_context.fold_emitter);

        IdentifierMap identifiers(test_type == TestType::PREPARSE);

        const Type* type{};
        stringstream output_stream;
        if (test_type == TestType::EXPRESSION) {
            auto expr = parse_expr(identifiers, sections[INPUT]);
            if (!sections[EXPECT_TYPE].empty()) {
                type = expr->get_type();
            }
            output_stream << expr;
        } else if (test_type == TestType::PREPROCESS) {
            File file;
            file.text = sections[INPUT];
            sweep(output_stream, file);
        } else {
            auto statements = parse_declarations(identifiers, sections[INPUT]);
            output_stream << statements;
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
            for (auto& p : identifiers.scopes.front().declarators) {
                declarators.push_back(p.second);
            }

            sort(declarators.begin(), declarators.end(), [](Declarator* a, Declarator* b) {
                return *a->identifier.name < *b->identifier.name;
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
            } else if (line == "EXPECT_MESSAGE") {
                section = Section::EXPECT_MESSAGE;
            } else if (line == "EXPECT_TEXT") {
                section = Section::EXPECT_TEXT;
            } else if (line == "EXPECT_TYPE") {
                section = Section::EXPECT_TYPE;
            } else if (line == "EXPRESSION") {
                enabled_types[unsigned(TestType::EXPRESSION)] = true;
                ++num_enabled_types;
            } else if (line == "DECLARATIONS") {
                enabled_types[unsigned(TestType::DECLARATIONS)] = true;
                ++num_enabled_types;
            } else if (line == "PREPARSE") {
                enabled_types[unsigned(TestType::PREPARSE)] = true;
                ++num_enabled_types;
            } else if (line == "PREPROCESS") {
                enabled_types[unsigned(TestType::PREPROCESS)] = true;
                ++num_enabled_types;
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
