#include "t_testcases.h"
#include <map>

void ParserCheck::print_block_html_enter(AB::BLOCK_TYPE b_type, const std::vector<AB::Boundaries>& bounds, const AB::Attributes& attributes, AB::BlockDetailPtr detail) {
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

    std::map<std::string, std::string> ordered_attrs;
    for (auto& pair : attributes)
        ordered_attrs[pair.first] = pair.second;
    for (auto& pair : ordered_attrs) {
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

    if (b_type == AB::BLOCK_HIDDEN) {
        auto& bound = bounds.front();
        for (int i = bound.beg;i < bound.end;i++)
            html << txt[i];
    }
}
void ParserCheck::print_block_ast(AB::BLOCK_TYPE b_type, const std::vector<AB::Boundaries>& bounds, const AB::Attributes& attributes, AB::BlockDetailPtr detail) {
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
void ParserCheck::print_block_html_close(AB::BLOCK_TYPE b_type) {
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

void ParserCheck::print_span_html_enter(AB::SPAN_TYPE s_type, const std::vector<AB::Boundaries>& bounds, const AB::Attributes& attributes, AB::SpanDetailPtr detail) {
    html << "<" << AB::span_to_html(s_type);

    if (s_type == AB::SPAN_URL) {
        auto info = std::static_pointer_cast<AB::SpanADetail>(detail);
        html << " href=\"" << info->href << "\"";
        if (info->alias)
            html << " alias";
    }
    else if (s_type == AB::SPAN_IMG) {
        auto info = std::static_pointer_cast<AB::SpanImgDetail>(detail);
        html << " href=\"" << info->href << "\"";
    }
    else if (s_type == AB::SPAN_REF) {
        auto info = std::static_pointer_cast<AB::SpanRefDetail>(detail);
        html << " name=\"" << info->name << "\"";
        if (info->inserted)
            html << " inserted";
    }

    std::map<std::string, std::string> ordered_attrs;
    for (auto& pair : attributes)
        ordered_attrs[pair.first] = pair.second;
    for (auto& pair : ordered_attrs) {
        html << " " << pair.first;
        if (!pair.second.empty())
            html << "=\"" << pair.second << "\"";
    }

    if (s_type == AB::SPAN_IMG || s_type == AB::SPAN_REF) {
        html << "/>";
    }
    else {
        html << ">";
    }
}
void ParserCheck::print_span_ast(AB::SPAN_TYPE s_type, const std::vector<AB::Boundaries>& bounds, const AB::Attributes& attributes, AB::SpanDetailPtr detail) {
    for (int i = 0;i < level;i++) {
        ast << "  ";
    }
    ast << AB::span_to_name(s_type);
    for (auto bound : bounds) {
        ast << " {" << bound.line_number;
        ast << ": " << bound.pre;
        ast << ", " << bound.beg;
        ast << ", " << bound.end;
        ast << ", " << bound.post << "} ";
    }
    ast << std::endl;
}
void ParserCheck::print_span_html_close(AB::SPAN_TYPE s_type) {
    if (s_type != AB::SPAN_IMG && s_type != AB::SPAN_REF) {
        html << "</" << AB::span_to_html(s_type) << ">";
    }
}

void ParserCheck::print_text_ast(AB::TEXT_TYPE t_type, const std::vector<AB::Boundaries>& bounds) {
    for (int i = 0;i < level;i++) {
        ast << "  ";
    }
    ast << AB::text_to_name(t_type);
    for (auto bound : bounds) {
        ast << " {" << bound.line_number;
        ast << ": " << bound.pre;
        ast << ", " << bound.beg;
        ast << ", " << bound.end;
        ast << ", " << bound.post << "} ";
    }
    ast << std::endl;
}

ParserCheck::ParserCheck() {
    parser.enter_block = [&](AB::BLOCK_TYPE b_type, const std::vector<AB::Boundaries>& bounds, const AB::Attributes& attributes, AB::BlockDetailPtr detail) -> bool {
        this->print_block_html_enter(b_type, bounds, attributes, detail);
        this->print_block_ast(b_type, bounds, attributes, detail);
        level++;
        return true;
    };
    parser.leave_block = [&](AB::BLOCK_TYPE b_type) -> bool {
        level--;
        this->print_block_html_close(b_type);
        has_entered = false;
        return true;
    };
    parser.enter_span = [&](AB::SPAN_TYPE s_type, const std::vector<AB::Boundaries>& bounds, const AB::Attributes& attributes, AB::SpanDetailPtr detail) {
        this->print_span_html_enter(s_type, bounds, attributes, detail);
        this->print_span_ast(s_type, bounds, attributes, detail);
        level++;
        return true;
    };
    parser.leave_span = [&](AB::SPAN_TYPE s_type) {
        level--;
        this->print_span_html_close(s_type);
        return true;
    };
    parser.text = [&](AB::TEXT_TYPE t_type, const std::vector<AB::Boundaries>& bounds) {
        this->print_text_ast(t_type, bounds);
        int j = 0;
        for (auto bound : bounds) {
            if (t_type == AB::TEXT_LATEX) {
                if (j > 0)
                    html << " ";
            }
            else if (t_type == AB::TEXT_CODE) {
                if (j > 0)
                    html << '\n';
            }
            else if (j > 0)
                html << "<br />";
            for (int i = bound.beg;i < bound.end;i++) {
                html << txt[i];
            }
            j++;
        }
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
