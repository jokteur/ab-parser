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
    std::stringstream sstream;
    struct AB::Parser parser;
public:
    ParserCheck() {
        parser.enter_block = [&](AB::BLOCK_TYPE b_type, const std::vector<AB::Boundaries>& bounds, AB::BlockDetailPtr detail) -> bool {
            for (int i = 0;i < level;i++) {
                sstream << "  ";
            }
            level++;

            sstream << AB::block_to_name(b_type);
            for (auto bound : bounds) {
                sstream << " {" << bound.line_number;
                sstream << ": " << bound.pre;
                sstream << ", " << bound.beg;
                sstream << ", " << bound.end;
                sstream << ", " << bound.post << "} ";
            }
            sstream << std::endl;

            return true;
        };
        parser.leave_block = [&]() -> bool {
            level--;
            return true;
        };
    }

    bool check_ast(const std::string& txt_input, const std::string& expected_ast, std::string& out_ast) {
        AB::parse(txt_input.c_str(), (AB::SIZE)txt_input.length(), &parser);
        out_ast = sstream.str();

        if (out_ast != expected_ast)
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
                std::string ast_file(file.path().parent_path().concat(name + ".ast").generic_string());

                // Read content of input
                std::ifstream ifs1(file.path().generic_string());
                std::string txt_input((std::istreambuf_iterator<char>(ifs1)), (std::istreambuf_iterator<char>()));

                // Read content of AST
                std::ifstream ifs2(file.path().generic_string());
                std::string expected_ast((std::istreambuf_iterator<char>(ifs2)), (std::istreambuf_iterator<char>()));

                ParserCheck testcase;
                std::string out_ast;
                bool ret = testcase.check_ast(txt_input, expected_ast, out_ast);

                std::string outpath(cwd.append("ast_out").append(name + ".ast").generic_string());
                std::ofstream outfile(outpath);
                std::cout << outpath << std::endl;
                outfile << out_ast;
                CHECK_MESSAGE(ret, "Failed for test case '", name, "' ; see outputs");
            }
        }
    }
}