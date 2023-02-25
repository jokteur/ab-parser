#include "parse_spans.h"
#include <iostream>
#include <list>

namespace AB {
    static const int S_EM_SIMPLE = 0x1;
    static const int S_EM = 0x2;
    static const int S_STRONG_SIMPLE = 0x4;
    static const int S_STRONG = 0x8;
    static const int S_VERBATIME = 0x10;
    static const int S_HIGHLIGHT = 0x20;
    static const int S_UNDERLINE = 0x40;
    static const int S_DELETE = 0x080;
    static const int S_LINK = 0x100;
    static const int S_LINKDEF = 0x200;
    static const int S_AUTOLINK = 0x400;
    static const int S_REF = 0x800;
    static const int S_INSERTED_REF = 0x1000;
    static const int S_IMG = 0x2000;
    static const int S_LATEX = 0x4000;
    static const int S_ATTRIBUTE = 0x8000;

    SPAN_TYPE flag_to_type(int flag) {
        switch (flag) {
        case S_EM_SIMPLE:
        case S_EM:
            return SPAN_EM;
        case S_STRONG_SIMPLE:
        case S_STRONG:
            return SPAN_STRONG;
        case S_VERBATIME:
            return SPAN_CODE;
        case S_HIGHLIGHT:
            return SPAN_HIGHLIGHT;
        case S_UNDERLINE:
            return SPAN_UNDERLINE;
        case S_DELETE:
            return SPAN_DEL;
        case S_LINK:
            return SPAN_URL;
        case S_LINKDEF:
            return SPAN_URL; // TODO
        case S_AUTOLINK:
            return SPAN_URL;
        case S_REF:
        case S_INSERTED_REF:
            return SPAN_REF;
        case S_IMG:
            return SPAN_IMG;
        case S_LATEX:
            return SPAN_LATEXMATH;
        default:
            return SPAN_EMPTY;
        }
    }

    static const int SELECT_ALL = ~0;
    static const int SELECT_ALL_LINKS = S_LINK | S_LINKDEF | S_AUTOLINK | S_REF | S_INSERTED_REF | S_IMG;

    struct Mark {
        int s_type = 0;
        std::string open;
        std::string close;
        bool need_ws_or_punct = false;
        int dont_allow_inside = 0;
        bool repeat = false;
        int count = 0;
        std::string second_close;

        /* Once solved, we store this information */
        bool solved = false;
        bool is_closing = false;
        Boundaries bounds;
    };

    static const Mark marks[] = {
        Mark{ S_EM_SIMPLE, "_", "_", true},
        Mark{ S_EM, "{_", "_}"},
        Mark{ S_STRONG_SIMPLE, "*", "*", true},
        Mark{ S_STRONG, "{*", "*}"},
        Mark{ S_VERBATIME, "`", "`", false, SELECT_ALL, true},
        Mark{ S_HIGHLIGHT, "{=", "=}"},
        Mark{ S_UNDERLINE, "{+", "+}"},
        Mark{ S_DELETE, "{-", "-}"},
        Mark{ S_LINK, "[", "](", false, SELECT_ALL_LINKS, false, 0, ")"},
        Mark{ S_LINKDEF, "[", "][", false, SELECT_ALL_LINKS, false, 0, "]"},
        Mark{ S_AUTOLINK, "http://", " ", false, SELECT_ALL},
        Mark{ S_AUTOLINK, "https://", " ", false, SELECT_ALL},
        Mark{ S_REF, "[[", "]]", false, SELECT_ALL},
        Mark{ S_INSERTED_REF, "![[", "]]", false, SELECT_ALL},
        Mark{ S_IMG, "![", "]", false, SELECT_ALL},
        Mark{ S_LATEX, "$$", "$$", false, SELECT_ALL},
        Mark{ S_ATTRIBUTE, "{{", "}}", false, SELECT_ALL}
    };

    typedef std::list<Mark> MarkChain;

    inline bool check_match(Context* ctx, const std::string& str, int& i, OFFSET off, OFFSET end) {
        for (;str[i] != 0 && off + i < end;i++) {
            if (str[i] != CH(off + i)) {
                return false;
            }
        }
        if (i < str.length() - 1) {
            return false;
        }
        return true;
    }

    inline bool close_mark(Context* ctx, MarkChain& mark_chain, const Mark& mark, OFFSET* off, OFFSET end, const std::vector<AB::Boundaries>& content_bounds) {
        bool found_match = true;
        OFFSET i = 0;
        OFFSET jump_to = *off;
        int mark_count = 0;
        if (mark.repeat) {
            char ch = mark.close[0];
            for (;*off + i < end;i++) {
                if (ch != CH(*off + i)) {
                    break;
                }
                mark_count++;
            }
            if (mark_count > 0) {
                jump_to = jump_to + i;
            }
            else {
                found_match = false;
            }
        }
        else {
            found_match = check_match(ctx, mark.close, i, *off, end);
            jump_to = jump_to + i;
            if (!mark.second_close.empty() && found_match) {
                OFFSET tmp_off = *off;
                /* Look ahead */
                while (tmp_off < end) {
                    if (CH(tmp_off) == mark.second_close[0]) {
                        OFFSET j = 1;
                        found_match = check_match(ctx, mark.close, j, tmp_off, end);
                        if (found_match) {
                            jump_to = tmp_off + j;
                        }
                        break;
                    }
                    tmp_off++;
                }
            }
        }
        if (found_match && jump_to < end && mark.need_ws_or_punct
            && !ISWHITESPACE(jump_to) && !ISPUNCT(jump_to))
            found_match = false;

        if (found_match) {
            auto it = mark_chain.rbegin();
            auto current = mark_chain.rbegin();
            std::vector<std::list<Mark>::iterator> to_erase;
            for (;it != mark_chain.rend();it++) {
                if (it->solved && !(it->s_type & mark.dont_allow_inside))
                    continue;
                if (it->s_type == mark.s_type
                    || it->s_type == mark.s_type && it->repeat && it->count == mark.repeat) {
                    break;
                }
                to_erase.push_back(std::next(it).base());
            }
            if (it != mark_chain.rend() && !mark_chain.empty()) {
                it->solved = true;

                Mark tmp_mark(*it);
                tmp_mark.bounds.end = *off + i;
                tmp_mark.bounds.post = jump_to;
                tmp_mark.solved = true;
                tmp_mark.is_closing = true;
                for (auto tmp_it : to_erase) {
                    mark_chain.erase(tmp_it);
                }
                mark_chain.push_back(tmp_mark);
                *off = jump_to;
                return true;
            }
        }

        return false;
    }

    inline bool open_mark(Context* ctx, MarkChain& mark_chain, const Mark& mark, OFFSET off, OFFSET end, bool ws_or_punct_before, int* opened_flags) {
        bool found_match = true;
        OFFSET i = 0;
        int mark_count = 0;
        if (mark.repeat) {
            char ch = mark.open[0];
            for (;off + i < end;i++) {
                if (ch != CH(off + i)) {
                    break;
                }
                mark_count++;
            }
            if (mark_count == 0) {
                found_match = false;
            }
        }
        else {
            found_match = check_match(ctx, mark.open, i, off, end);
        }
        if (mark.need_ws_or_punct && !ws_or_punct_before)
            found_match = false;

        if (found_match) {
            Mark tmp_mark(mark);
            if (mark.repeat) {
                tmp_mark.count = mark_count;
                tmp_mark.bounds = Boundaries{ ctx->offset_to_line_number[off], off, off + mark_count };
            }
            else {
                tmp_mark.bounds = Boundaries{ ctx->offset_to_line_number[off], off, off + (OFFSET)mark.open.length() };
            }
            mark_chain.push_back(tmp_mark);
            *opened_flags |= mark.s_type;
            return true;
        }
        else {
            return false;
        }
    }
    bool parse_spans(Context* ctx, ContainerPtr ptr) {
        bool ret = true;

        MarkChain mark_chain;
        int opened_flags = 0;

        for (auto& bound : ptr->content_boundaries) {
            bool prev_is_whitespace = true;
            bool prev_is_punctuation = false;
            for (OFFSET off = bound.beg;off < bound.end;) {
                if (CH(off) == '\\')
                    continue;
                else if (ISWHITESPACE(off))
                    prev_is_whitespace = true;
                else if (ISPUNCT(off))
                    prev_is_punctuation = true;

                int a = 0;

                bool success = false;
                for (auto& mark : marks) {
                    /* Search first for closing marks */
                    if (opened_flags & mark.s_type) {
                        success = close_mark(ctx, mark_chain, mark, &off, bound.end, ptr->content_boundaries);
                        if (success) {
                            break;
                        }
                    }
                    /* Search then for opening marks */
                    bool success = open_mark(ctx, mark_chain, mark, off, bound.end, prev_is_punctuation || prev_is_whitespace, &opened_flags);
                }
                if (!success)
                    off++;
            }
        }

        /* Cleanup of unmatched spans */
        std::vector<std::list<Mark>::iterator> to_erase;
        for (auto it = mark_chain.begin();it != mark_chain.end();it++) {
            if (!it->solved) {
                to_erase.push_back(it);
            }
        }
        for (auto it : to_erase) {
            mark_chain.erase(it);
        }

        for (auto& mark : mark_chain) {
            if (!mark.is_closing) {
                CHECK_AND_RET(ctx->parser->enter_span(
                    flag_to_type(mark.s_type),
                    { mark.bounds },
                    Attributes{},
                    nullptr
                ));
            }
            else {
                CHECK_AND_RET(ctx->parser->leave_span(flag_to_type(mark.s_type)));
            }
        }

        return true;
    abort:
        return ret;
    }
}