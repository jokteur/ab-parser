#include "t_testcases.h"

void ParserCheck::print_html_enter(AB::BLOCK_TYPE b_type, const std::vector<AB::Boundaries>& bounds, const AB::Attributes& attributes, AB::BlockDetailPtr detail) {
    if (b_type != AB::BLOCK_DOC)
        html << std::endl;
    has_entered = true;

    for (int i = 0;i < level;i++) {
        html << "  ";
    }

    html << "<" << AB::block_to_html(b_type);

    if (b_type == AB::BLOCK_DIV) {
        auto info = std::static_pointer_cast<AB::BlockDivDetail>(detail);
        html << " class=\"" << info->name << "\"";
    }
    else if (b_type == AB::BLOCK_CODE) {
        auto info = std::static_pointer_cast<AB::BlockCodeDetail>(detail);
        html << " lang=\"" << info->lang << "\"";
    }

    for (auto& pair : attributes) {
        html << " " << pair.first;
        if (!pair.second.empty())
            html << "=\"" << pair.second << "\"";
    }

    if (b_type == AB::BLOCK_HR) {
        html << "/>";
    }
    else {
        html << ">";
    }

    if (is_block_child(b_type)) {
        bool first = true;
        for (auto bound : bounds) {
            if (!first) {
                if (b_type != AB::BLOCK_LATEX)
                    html << "<br />";
                else
                    html << " ";
            }
            for (int i = bound.beg;i < bound.end;i++) {
                html << txt[i];
            }
            first = false;
        }
    }
}
void ParserCheck::print_ast(AB::BLOCK_TYPE b_type, const std::vector<AB::Boundaries>& bounds, const AB::Attributes& attributes, AB::BlockDetailPtr detail) {
    for (int i = 0;i < level;i++) {
        ast << "  ";
    }
    ast << AB::block_to_name(b_type);
    for (auto bound : bounds) {
        ast << " {" << bound.line_number;
        ast << ": " << bound.pre;
        ast << ", " << bound.beg;
        ast << ", " << bound.end;
        ast << ", " << bound.post << "} ";
    }
    ast << std::endl;
}
void ParserCheck::print_html_close(AB::BLOCK_TYPE b_type) {
    if (!is_block_child(b_type)) {
        html << std::endl;
        for (int i = 0;i < level;i++) {
            html << "  ";
        }

        html << "</" << AB::block_to_html(b_type) << ">";
    }
    else if (b_type != AB::BLOCK_HR)
        html << "</" << AB::block_to_html(b_type) << ">";
}

ParserCheck::ParserCheck() {
    parser.enter_block = [&](AB::BLOCK_TYPE b_type, const std::vector<AB::Boundaries>& bounds, const AB::Attributes& attributes, AB::BlockDetailPtr detail) -> bool {
        this->print_html_enter(b_type, bounds, attributes, detail);
        this->print_ast(b_type, bounds, attributes, detail);
        level++;
        return true;
    };
    parser.leave_block = [&](AB::BLOCK_TYPE b_type) -> bool {
        level--;
        this->print_html_close(b_type);
        has_entered = false;
        return true;
    };
}

int ParserCheck::check_ast(const std::string& txt_input, const std::string& expected_ast, const std::string& expected_html, std::string& out_ast, std::string& out_html) {
    txt = txt_input;
    AB::parse(txt_input.c_str(), (AB::SIZE)txt_input.length(), &parser);
    out_ast = ast.str();
    out_html = html.str();

    int ret = 0;
    if (out_ast != expected_ast)
        ret |= AST_FAILED;
    if (out_html != expected_html)
        ret |= HTML_FAILED;
    return ret;
}
