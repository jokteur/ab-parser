#pragma once

#include <doctest/doctest.h>
#include <string>
#include <sstream>
#include <set>
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
        return b_type != AB::BLOCK_LI && b_type != AB::BLOCK_UL && b_type != AB::BLOCK_DOC
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
                html << "</" << AB::block_to_html(b_type) << ">";
            }
            else if (b_type != AB::BLOCK_HR)
                html << "</" << AB::block_to_html(b_type) << ">";
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
                bool ret = testcase.check_ast(txt_input, expected_ast.str(), expected_html.str(), out_ast, out_html);

                std::string outpath_ast(fs::path(cwd).append("ast_out").append(name + ".ast").generic_string());
                std::string outpath_html(fs::path(cwd).append("ast_out").append(name + ".html").generic_string());
                std::ofstream outfile_ast(outpath_ast);
                std::ofstream outfile_html(outpath_html);
                outfile_html << out_html;
                outfile_ast << out_ast;
                CHECK_MESSAGE(ret, "Failed for test case '", name, "' ; see outputs");
            }
        }
    }
}