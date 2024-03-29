#include "parse_commons.h"
#include "internal.h"

namespace AB {
    Attributes parse_attributes(Context* ctx, OFFSET* off) {
        Attributes attributes;
        bool start_collection = false;
        bool is_key = true;
        bool is_complete = false;
        std::string acc;
        std::string prev_key;
        for (;(SIZE)*off < ctx->end && CH(*off) != '\n';(*off)++) {
            char ch = CH(*off);
            if (ch == '\\')
                continue;
            if (ch == '}') {
                if (is_key)
                    attributes[acc] = "";
                else
                    attributes[prev_key] = acc;

                if (start_collection)
                    is_complete = true;
                break;
            }
            bool is_assignement = ch == ':' || ch == '=';
            if (!start_collection && !is_assignement)
                start_collection = true;
            if (ch == ',') {
                if (is_key)
                    attributes[acc] = "";
                else
                    attributes[prev_key] = acc;
                is_key = true;
                prev_key = "";
                acc = "";
                continue;
            }
            else if (is_assignement) {
                is_key = false;
                attributes[acc] = "";
                prev_key = acc;
                acc = "";
                continue;
            }
            else if (ISWHITESPACE(*off) && is_key)
                continue;
            else
                acc += ch;
        }
        if (!is_complete)
            attributes.clear();

        return attributes;
    }

    void skip_whitespace(Context* ctx, OFFSET* off) {
        while ((SIZE)*off < ctx->end && CH(*off) != '\n') {
            if (!ISWHITESPACE(*off))
                break;
            (*off)++;
        }
    }

    int count_marks(Context* ctx, OFFSET* off, char mark) {
        int counter = 0;
        while (CH(*off) != '\n' && *off < (OFFSET)ctx->end) {
            if (CH(*off) == mark)
                counter++;
            else
                break;
            (*off)++;
        }
        return counter;
    }

    int count_marks(Context* ctx, OFFSET off, char mark) {
        int counter = 0;
        while (CH(off) != '\n' && off < (OFFSET)ctx->end) {
            if (CH(off) == mark)
                counter++;
            else
                break;
            (off)++;
        }
        return counter;
    }

    bool advance_until(Context* ctx, OFFSET* off, std::string& acc, char ch) {
        bool found_end_char = false;
        for (;(SIZE)*off < ctx->end && CH(*off) != '\n';(*off)++) {
            if (CH(*off) == '\\')
                continue;
            acc += CH(*off);
            if (CH(*off) == ch) {
                found_end_char = true;
                break;
            }
        }
        return found_end_char;
    }

    bool is_leaf_block(BLOCK_TYPE b_type) {
        switch (b_type) {
        case BLOCK_CODE:
        case BLOCK_H:
        case BLOCK_LATEX:
        case BLOCK_P:
            return true;
        default:
            return false;
        }
    }
}