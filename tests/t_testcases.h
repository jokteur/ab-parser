#pragma once

#include <doctest/doctest.h>
#include <string>
#include <sstream>
#include <filesystem>
#include <fstream>
#include <vector>
#include "parser.h"

class ParserCheck {
private:
    int level = 0;
    std::string txt;
    std::stringstream html;
    std::stringstream ast;
    struct AB::Parser parser;
    bool has_entered = false;
    bool is_block_child(AB::BLOCK_TYPE b_type) {
        return b_type != AB::BLOCK_LI && b_type != AB::BLOCK_UL & b_type != AB::BLOCK_DOC
            && b_type != AB::BLOCK_OL && b_type != AB::BLOCK_QUOTE;
    }
public:
    ParserCheck() {
        parser.enter_block = [&](AB::BLOCK_TYPE b_type, const std::vector<AB::Boundaries>& bounds, AB::BlockDetailPtr detail) -> bool {
            if (b_type != AB::BLOCK_DOC)
                html << std::endl;
            has_entered = true;

            for (int i = 0;i < level;i++) {
                ast << "  ";
                html << "  ";
            }
            level++;

            ast << AB::block_to_name(b_type);
            html << "<" << AB::block_to_html(b_type);

            if (b_type == AB::BLOCK_HR) {
                html << "/>";
            }
            else {
                html << ">";
            }

            if (is_block_child(b_type)) {
                bool first = true;
                for (auto bound : bounds) {
                    if (!first)
                        html << "<br />";
                    for (int i = bound.beg;i < bound.end;i++) {
                        html << txt[i];
                    }
                    first = false;
                }
            }

            for (auto bound : bounds) {
                ast << " {" << bound.line_number;
                ast << ": " << bound.pre;
                ast << ", " << bound.beg;
                ast << ", " << bound.end;
                ast << ", " << bound.post << "} ";
            }
            ast << std::endl;

            return true;
        };
        parser.leave_block = [&](AB::BLOCK_TYPE b_type) -> bool {
            level--;
            if (!is_block_child(b_type)) {
                html << std::endl;
                for (int i = 0;i < level;i++) {
                    html << "  ";
                }
                html << "<" << AB::block_to_html(b_type) << "/>";
            }
            else if (b_type != AB::BLOCK_HR)
                html << "<" << AB::block_to_html(b_type) << "/>";
            has_entered = false;
            return true;
        };
    }

    bool check_ast(const std::string& txt_input, const std::string& expected_ast, const std::string& expected_html, std::string& out_ast, std::string& out_html) {
        txt = txt_input;
        AB::parse(txt_input.c_str(), (AB::SIZE)txt_input.length(), &parser);
        out_ast = ast.str();
        out_html = html.str();

        if (out_ast != expected_ast || expected_html != out_html)
            return false;
        else
            return true;
    }
};

TEST_SUITE("Parser") {
    TEST_CASE("AST check") {
        std::filesystem::path cwd = std::filesystem::current_path();
        // ast_out is where all the outputs will be stored
        std::filesystem::remove_all("ast_out");
        std::filesystem::create_directory("ast_out");

        for (const auto& file : std::filesystem::directory_iterator(cwd)) {
            if (file.path().extension() == ".ab") {
                // Find test case name
                std::string name(file.path().filename().generic_string());
                name = name.substr(0, name.length() - 3);

                // Read content of input
                std::ifstream ifs1(file.path().generic_string());
                std::string txt_input((std::istreambuf_iterator<char>(ifs1)), (std::istreambuf_iterator<char>()));

                // Read content of AST
                std::string ast_file(file.path().parent_path().concat(name + ".ast").generic_string());
                std::string expected_ast;
                if (std::filesystem::exists(ast_file)) {
                    std::ifstream ifs2(ast_file);
                    std::string expected_ast((std::istreambuf_iterator<char>(ifs2)), (std::istreambuf_iterator<char>()));
                }

                // Read content of html
                std::string html_file(file.path().parent_path().concat(name + ".html").generic_string());
                std::string expected_html;
                if (std::filesystem::exists(html_file)) {
                    std::ifstream ifs3(html_file);
                    std::string expected_html((std::istreambuf_iterator<char>(ifs3)), (std::istreambuf_iterator<char>()));
                }

                ParserCheck testcase;
                std::string out_ast;
                std::string out_html;
                bool ret = testcase.check_ast(txt_input, expected_ast, expected_html, out_ast, out_html);

                std::string outpath_ast(std::filesystem::path(cwd).append("ast_out").append(name + ".ast").generic_string());
                std::string outpath_html(std::filesystem::path(cwd).append("ast_out").append(name + ".html").generic_string());
                std::ofstream outfile_ast(outpath_ast);
                std::ofstream outfile_html(outpath_html);
                outfile_html << out_html;
                outfile_ast << out_ast;
                CHECK_MESSAGE(ret, "Failed for test case '", name, "' ; see outputs");
            }
        }
    }
}