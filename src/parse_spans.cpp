#include "parse_spans.h"
#include <iostream>

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
    static const int S_LATEX = 0x2000;
    static const int S_ATTRIBUTE = 0x4000;

    static const int SELECT_ALL = ~0;
    static const int SELECT_ALL_LINKS = S_LINK & S_LINKDEF & S_AUTOLINK & S_REF & S_INSERTED_REF & S_IMG;

    struct Mark {
        int s_type = 0;
        const char* open = 0;
        const char* close = 0;
        bool need_ws_or_punct = false;
        int dont_allow_inside = 0;
        bool repeat = false;
        int count = 0;
        const char* second_close = 0;
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

    bool parse_spans(Context* ctx, ContainerPtr ptr) {
        std::cout << block_to_html(ptr->b_type) << std::endl;
        for (auto& bound : ptr->content_boundaries) {
            bool prev_is_whitespace = true;
            bool prev_is_punctuation = false;
            for (OFFSET off = bound.beg;off < bound.end;off++) {
                if (CH(off) == '\\')
                    continue;
                else if (ISWHITESPACE(off))
                    prev_is_whitespace = true;
                else if (ISPUNCT(off))
                    prev_is_punctuation = true;

                for (auto& mark : marks) {
                    /* Search first for closing marks */
                    bool mark_closed = true;
                    OFFSET i = 0;
                    for (;mark.close[i] != 0;i++) {
                        if (mark.close[i] != CH(off + i)) {
                            mark_closed = false;
                            break;
                        }
                    }
                    if (mark_closed && off + i < bound.end && mark.need_ws_or_punct
                        && !ISWHITESPACE(off + i) && !ISPUNCT(off + i))
                        mark_closed = false;
                    // Do closing mark
                    if (mark_closed)
                        continue;

                    /* Search then for opening marks */
                    bool mark_open = true;
                    i = 0;
                    for (;mark.open[i] != 0;i++) {
                        if (mark.open[i] != CH(off + i)) {
                            mark_open = false;
                            break;
                        }
                    }
                    if (mark_closed && off + i < bound.end && mark.need_ws_or_punct
                        && !prev_is_whitespace && !prev_is_punctuation)
                        mark_open = false;
                    if (mark_open)
                        continue;
                }
            }
        }
        return true;
    }
}