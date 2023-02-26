#include "definitions.h"

namespace AB {
    const char* block_to_name(BLOCK_TYPE type) {
        switch (type) {
        case BLOCK_DOC:
            return "DOCUMENT";
        case BLOCK_QUOTE:
            return "B_QUOTE";
        case BLOCK_HIDDEN:
            return "B_HIDDEN";
        case BLOCK_UL:
            return "B_UL";
        case BLOCK_OL:
            return "B_OL";
        case BLOCK_LI:
            return "B_LI";
        case BLOCK_HR:
            return "B_HR";
        case BLOCK_H:
            return "B_H";
        case BLOCK_CODE:
            return "B_CODE";
        case BLOCK_LATEX:
            return "B_LATEX";
        case BLOCK_DEF:
            return "B_DEF";
        case BLOCK_DIV:
            return "B_DIV";
        case BLOCK_P:
            return "B_P";
        case BLOCK_TABLE:
            return "B_TABLE";
        case BLOCK_THEAD:
            return "B_THEAD";
        case BLOCK_TBODY:
            return "B_TBODY";
        case BLOCK_TR:
            return "B_TR";
        case BLOCK_TH:
            return "B_TH";
        case BLOCK_TD:
            return "B_TD;";
        };
        return "";
    }
    const char* block_to_html(BLOCK_TYPE type) {
        switch (type) {
        case BLOCK_DOC:
            return "doc";
        case BLOCK_QUOTE:
            return "blockquote";
        case BLOCK_HIDDEN:
            return "hidden";
        case BLOCK_UL:
            return "ul";
        case BLOCK_OL:
            return "ol";
        case BLOCK_LI:
            return "li";
        case BLOCK_HR:
            return "hr";
        case BLOCK_H:
            return "h";
        case BLOCK_CODE:
            return "code";
        case BLOCK_LATEX:
            return "math";
        case BLOCK_DEF:
            return "definition";
        case BLOCK_DIV:
            return "div";
        case BLOCK_P:
            return "p";
        case BLOCK_TABLE:
            return "table";
        case BLOCK_THEAD:
            return "thead";
        case BLOCK_TBODY:
            return "tbody";
        case BLOCK_TR:
            return "tr";
        case BLOCK_TH:
            return "th";
        case BLOCK_TD:
            return "td;";
        };
        return "";
    }
    const char* span_to_name(SPAN_TYPE type) {
        switch (type) {
        case SPAN_EM:
            return "S_EM";
        case SPAN_STRONG:
            return "S_STRONG";
        case SPAN_URL:
            return "S_URL";
        case SPAN_IMG:
            return "S_IMG";
        case SPAN_CODE:
            return "S_CODE";
        case SPAN_DEL:
            return "S_DEL";
        case SPAN_LATEXMATH:
            return "S_LATEXMATH";
        case SPAN_REF:
            return "S_REF";
        case SPAN_UNDERLINE:
            return "S_UNDERLINE";
        case SPAN_HIGHLIGHT:
            return "S_HIGHLIGHT";
        };
        return "";
    }
    const char* span_to_html(SPAN_TYPE type) {
        switch (type) {
        case SPAN_EM:
            return "em";
        case SPAN_STRONG:
            return "strong";
        case SPAN_URL:
            return "a";
        case SPAN_IMG:
            return "img";
        case SPAN_CODE:
            return "code";
        case SPAN_DEL:
            return "del";
        case SPAN_LATEXMATH:
            return "math";
        case SPAN_REF:
            return "ref";
        case SPAN_UNDERLINE:
            return "u";
        case SPAN_HIGHLIGHT:
            return "highlight";
        };
        return "";
    }
}