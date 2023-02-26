#pragma once

#include <doctest/doctest.h>
#include <string>
#include <sstream>
#include <set>
#include <filesystem>
#include <fstream>
#include <vector>
#include "parser.h"

static const int AST_FAILED = 0x1;
static const int HTML_FAILED = 0x2;

class ParserCheck {
private:
    int level = 0;
    std::string txt;
    std::stringstream html;
    std::stringstream ast;
    struct AB::Parser parser;
    bool has_entered = false;
    bool is_block_child(AB::BLOCK_TYPE b_type) {
        return b_type != AB::BLOCK_LI && b_type != AB::BLOCK_UL && b_type != AB::BLOCK_DOC
            && b_type != AB::BLOCK_OL && b_type != AB::BLOCK_QUOTE && b_type != AB::BLOCK_DEF
            && b_type != AB::BLOCK_DIV;
    }

public:
    ParserCheck();
    void print_block_html_enter(AB::BLOCK_TYPE b_type, const std::vector<AB::Boundaries>& bounds, const AB::Attributes& attributes, AB::BlockDetailPtr detail);
    void print_block_html_close(AB::BLOCK_TYPE b_type);
    void print_block_ast(AB::BLOCK_TYPE b_type, const std::vector<AB::Boundaries>& bounds, const AB::Attributes& attributes, AB::BlockDetailPtr detail);

    void print_span_html_enter(AB::SPAN_TYPE s_type, const std::vector<AB::Boundaries>& bounds, const AB::Attributes& attributes, AB::SpanDetailPtr detail);
    void print_span_html_close(AB::SPAN_TYPE s_type);
    void print_span_ast(AB::SPAN_TYPE s_type, const std::vector<AB::Boundaries>& bounds, const AB::Attributes& attributes, AB::SpanDetailPtr detail);

    void print_text_ast(AB::TEXT_TYPE t_type, const std::vector<AB::Boundaries>& bounds);

    int check_ast(const std::string& txt_input, const std::string& expected_ast, const std::string& expected_html, std::string& out_ast, std::string& out_html);
};

TEST_SUITE("Parser") {
    TEST_CASE("AST check") {
        namespace fs = std::filesystem;

        fs::path cwd = fs::current_path();
        // ast_out is where all the outputs will be stored
        fs::remove_all("ast_out");
        fs::create_directory("ast_out");

        std::set<fs::path> sorted_files;

        for (auto& entry : fs::directory_iterator(cwd))
            sorted_files.insert(entry.path());

        for (const auto& file : sorted_files) {
            if (file.extension() == ".ab") {
                // Find test case name
                std::string name(file.filename().generic_string());
                name = name.substr(0, name.length() - 3);

                // Read content of input
                std::ifstream ifs1(file.generic_string());
                std::string txt_input((std::istreambuf_iterator<char>(ifs1)), (std::istreambuf_iterator<char>()));

                // Read content of AST
                std::string ast_file(file.parent_path().concat("/" + name + ".ast").generic_string());
                std::stringstream expected_ast;
                if (fs::exists(ast_file)) {
                    std::ifstream ifs2(ast_file);
                    expected_ast << ifs2.rdbuf();
                }

                // Read content of html
                std::string html_file(file.parent_path().concat("/" + name + ".html").generic_string());
                std::stringstream expected_html;
                if (fs::exists(html_file)) {
                    std::ifstream ifs3(html_file);
                    expected_html << ifs3.rdbuf();
                }

                ParserCheck testcase;
                std::string out_ast;
                std::string out_html;
                int ret = testcase.check_ast(txt_input, expected_ast.str(), expected_html.str(), out_ast, out_html);

                std::string outpath_ast(fs::path(cwd).append("ast_out").append(name + ".ast").generic_string());
                std::string outpath_html(fs::path(cwd).append("ast_out").append(name + ".html").generic_string());
                std::ofstream outfile_ast(outpath_ast);
                std::ofstream outfile_html(outpath_html);
                outfile_html << out_html;
                outfile_ast << out_ast;

                if (name != "_testbench") {
                    if ((ret & AST_FAILED) && (ret & HTML_FAILED)) {
                        CHECK_MESSAGE(false, "Failed both AST and HTML for test case '", name, "' ; see outputs");
                    }
                    else if ((ret & AST_FAILED)) {
                        CHECK_MESSAGE(false, "Failed AST for test case '", name, "' ; see outputs");
                    }
                    else if ((ret & HTML_FAILED)) {
                        CHECK_MESSAGE(false, "Failed HTML for test case '", name, "' ; see outputs");
                    }
                    else {
                        /* Count towards assertions */
                        CHECK_MESSAGE(true, "");
                    }
                }
            }
        }
    }
}