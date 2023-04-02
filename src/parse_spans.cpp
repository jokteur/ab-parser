#include "parse_spans.h"
#include "parse_commons.h"
#include <iostream>
#include <unordered_set>
#include <list>

#define MAX_VERB_OPENINGS 32

namespace AB {
    static const int S_EM_SIMPLE = 0x1;
    static const int S_EM = 0x2;
    static const int S_STRONG_SIMPLE = 0x4;
    static const int S_STRONG = 0x8;
    static const int S_HIGHLIGHT = 0x10;
    static const int S_UNDERLINE = 0x20;
    static const int S_DELETE = 0x40;
    static const int S_VERBATIME = 0x80;
    static const int S_LINK = 0x100;
    static const int S_LINKDEF = 0x200;
    static const int S_AUTOLINK = 0x400;
    static const int S_REF = 0x800;
    static const int S_INSERTED_REF = 0x1000;
    static const int S_IMG = 0x2000;
    static const int S_IMG_TITLE = 0x4000;
    static const int S_IMG_DEF = 0x8000;
    static const int S_LATEX = 0x10000;
    static const int S_ATTRIBUTE = 0x20000;

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
            return SPAN_URL;
        case S_AUTOLINK:
            return SPAN_URL;
        case S_REF:
        case S_INSERTED_REF:
            return SPAN_REF;
        case S_IMG:
        case S_IMG_TITLE:
        case S_IMG_DEF:
            return SPAN_IMG;
        case S_LATEX:
            return SPAN_LATEXMATH;
        default:
            return SPAN_EMPTY;
        }
    }

    static const int SELECT_ALL = ~0;
    static const int SELECT_REFS = S_REF | S_INSERTED_REF;
    static const int SELECT_IMGS = S_IMG | S_IMG_DEF | S_IMG_TITLE;
    static const int SELECT_LINKS = S_LINK | S_LINKDEF | S_AUTOLINK;
    static const int SELECT_ALL_LINKTYPE = SELECT_REFS | SELECT_IMGS | SELECT_LINKS;

    /**
     * @brief Mark stores the rules for span detection
     *
     * s_type
     *     name (flag) of the span
     * open
     *     rule for opening the span
     * closing
     *     rule for closing the span
     * need_ws_or_punct
     *     when true, the opening must be preceeded by a punctuation or a
     *     whitespace and after the closing a punctuation or a whitespace
     *     is needed
     * dont_allow_inside
     *     flags of spans not allowed inside. e.g. a link cannot contain other links
     * repeat
     *     if true, then the opening and closing chars can be repeated as many times
     *     as needed
     * count
     *     stores the number of repeat chars if the previous attribute is true
     * second_close
     *     sometimes, spans are defined by a second closing rule
     *     e.g. [abc](def)  -->  open = "[", close = "](", second_close =")"
     *
     * The other attributes are used for solving the spans
     *
     * solved:
     *     when true, it means that the mark has been solved (i.e. opening and closing found)
     * is_closing:
     *     if true, then mark is used as a closing landmark in the mark_chain
     * pre, beg, line_number:
     *     starting bounds of the span
     * true_bounds:
     *     once the span is solved, we calculate the true boundaries and store them there
     */
    struct Mark {
        int s_type = 0;
        std::string open;
        std::string close;
        bool need_ws_or_punct = false;
        int dont_allow_inside = 0;
        bool repeat = false;
        int count = 0;
        std::string second_close;
        bool no_self_nested = false;
        bool jump_after_match = false;

        /* Once solved, we store this information */
        bool solved = false;
        bool is_closing = false;
        OFFSET pre = 0;
        OFFSET beg = 0;
        int line_number = 0;
        std::vector<Boundaries> true_bounds;
        Attributes attributes;
        Mark* start_ptr = nullptr;
    };

    static const std::unordered_set<char> opening_marks{
        '!', '[', '*', '`', '_', '{', '$', 'h'
    };

    static const std::unordered_set<char> closing_marks{
        ']', '=', '*', '+', '-', '_', '`', '$', '}'
    };

    static const Mark marks[] = {
        /* The order at which declare here is of crucial importance
         * If `*` is checked before `{*`, then `*` will be closed
         * before `{*` and thus making it impossible in most cases
         * to detect `{*`
         * */
        Mark{ S_EM, "{_", "_}"},
        Mark{ S_EM_SIMPLE, "_", "_", true},
        Mark{ S_STRONG, "{*", "*}"},
        Mark{ S_STRONG_SIMPLE, "*", "*", true},
        Mark{ S_VERBATIME, "`", "`", false, SELECT_ALL, true},
        Mark{ S_HIGHLIGHT, "{=", "=}"},
        Mark{ S_UNDERLINE, "{+", "+}"},
        Mark{ S_DELETE, "{-", "-}"},
        Mark{ S_INSERTED_REF, "![[", "]]", false, SELECT_ALL},
        Mark{ S_REF, "[[", "]]", false, SELECT_ALL},
        Mark{ S_IMG_TITLE, "![", "](", false, SELECT_ALL_LINKTYPE, false, 0, ")"},
        Mark{ S_IMG_DEF, "![", "][", false, SELECT_ALL_LINKTYPE, false, 0, "]"},
        Mark{ S_IMG, "![", "]", false, SELECT_ALL},
        Mark{ S_LINK, "[", "](", false, SELECT_ALL_LINKTYPE, false, 0, ")", true},
        Mark{ S_LINKDEF, "[", "][", false, SELECT_ALL_LINKTYPE, false, 0, "]", true},
        Mark{ S_LATEX, "$$", "$$", false, SELECT_ALL, false, 0, "", false, true},
        Mark{ S_ATTRIBUTE, "{{", "}}", false, SELECT_ALL}
        /* Autolinks handled by lookahead_autolink*/
    };
#define M_EM 0
#define M_EM_SIMPLE 1
#define M_STRONG 2
#define M_STRONG_SIMPLE 3
#define M_VERBATIME 4
#define M_HIGHLIGHT 5
#define M_UNDERLINE 6
#define M_DELETE 7
#define M_INSERTED_REF 8
#define M_REF 9
#define M_IMG_TITLE 10
#define M_IMG_DEF 11
#define M_IMG 12
#define M_LINK 13
#define M_LINKDEF 14
#define M_LATEX 15
#define M_ATTRIBUTE 16

    typedef std::list<Mark> MarkChain;

    inline bool check_match(Context* ctx, const std::string& str, int& i, OFFSET off, OFFSET end) {
        int count = 0;
        for (;str[i] != 0 && off + i < end;i++) {
            if (str[i] != CH(off + i)) {
                return false;
            }
            count++;
        }
        if (count < str.length()) {
            return false;
        }
        return true;
    }

    void add_to_flag_count(std::unordered_map<int, int>& flag_count, int flag) {
        if (flag_count.find(flag) != flag_count.end()) {
            flag_count[flag]++;
        }
        else {
            flag_count[flag] = 1;
        }
    }
    /* No testing of bounds */
    void remove_from_flag_count(std::unordered_map<int, int>& flag_count, int flag) {
        flag_count[flag]--;
    }

    bool is_count_positive(std::unordered_map<int, int>& flag_count, int flag) {
        if (flag_count.find(flag) != flag_count.end() && flag_count[flag] > 0)
            return true;
        return false;
    }

    inline bool close_mark(Context* ctx, MarkChain& mark_chain, const Mark& mark, OFFSET* off, OFFSET end, const std::vector<AB::Boundaries>& content_bounds, std::unordered_map<int, int>& flag_count) {
        bool found_match = true;
        OFFSET i = 0;
        OFFSET jump_to = *off;
        int mark_count = 0;
        if (mark.repeat) {
            char ch = mark.close[0];
            for (;*off + i < end && i < MAX_VERB_OPENINGS;i++) {
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
                found_match = false;
                OFFSET tmp_off = *off + 1;
                /* Look ahead */
                while (tmp_off < end) {
                    if (CH(tmp_off) == mark.second_close[0]) {
                        OFFSET j = 0;
                        found_match = check_match(ctx, mark.second_close, j, tmp_off, end);
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

            while (it != mark_chain.rend()) {
                if (it->solved) {
                    if (!(it->s_type & mark.dont_allow_inside)) {
                        it++;
                        continue;
                    }
                }
                else if (it->s_type == mark.s_type && it->count == mark_count) {
                    break;
                }
                to_erase.push_back(std::next(it).base());
                it++;
            }
            if (it != mark_chain.rend() && !mark_chain.empty()) {
                it->solved = true;

                Mark tmp_mark(*it);
                /* Need to calculate the true boundaries of the span
                 * A span can be on multiple lines, this is why we need
                 * to use content_boundaries */
                OFFSET b_end = *off;
                OFFSET b_post = jump_to;
                int line_number = ctx->offset_to_line_number[b_end];
                if (line_number > tmp_mark.line_number) {
                    auto bound_it = content_bounds.begin();
                    while (bound_it != content_bounds.end()) {
                        if (bound_it->line_number == it->line_number)
                            break;
                        bound_it++;
                    }
                    /* Starting boundary */
                    it->true_bounds.push_back(Boundaries{ it->line_number, tmp_mark.pre, tmp_mark.beg, bound_it->end, bound_it->end });
                    bound_it++;
                    /* Inbetween boundaries */
                    while (bound_it != content_bounds.end()) {
                        if (bound_it->line_number == line_number)
                            break;
                        it->true_bounds.push_back(Boundaries{ bound_it->line_number, bound_it->beg, bound_it->beg, bound_it->end, bound_it->end });
                        bound_it++;
                    }
                    /* Last boundary */
                    it->true_bounds.push_back(Boundaries{ line_number, bound_it->beg, bound_it->beg, b_end, b_post });
                }
                else {
                    it->true_bounds.push_back(Boundaries{ line_number, tmp_mark.pre, tmp_mark.beg, b_end, b_post });
                }
                tmp_mark.solved = true;
                tmp_mark.is_closing = true;
                tmp_mark.start_ptr = &(*it);
                for (auto& m : to_erase) {
                    remove_from_flag_count(flag_count, m->s_type);
                    mark_chain.erase(m);
                }
                mark_chain.push_back(tmp_mark);
                *off = jump_to;
                return true;
            }
        }

        return false;
    }

    inline int open_mark(Context* ctx, MarkChain& mark_chain, const Mark& mark, OFFSET off, OFFSET end, bool ws_or_punct_before, std::unordered_map<int, int>& flag_count) {
        bool found_match = true;
        OFFSET i = 0;
        int mark_count = 0;
        if (mark.repeat) {
            char ch = mark.open[0];
            for (;off + i < end && i < MAX_VERB_OPENINGS;i++) {
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
            tmp_mark.line_number = ctx->offset_to_line_number[off];
            tmp_mark.pre = off;
            if (mark.repeat) {
                tmp_mark.count = mark_count;
                tmp_mark.beg = off + mark_count;
            }
            else {
                tmp_mark.beg = off + (OFFSET)mark.open.length();
            }
            mark_chain.push_back(tmp_mark);
            add_to_flag_count(flag_count, mark.s_type);
            if (mark.jump_after_match)
                return mark.open.length();
            else
                return mark_count;
        }
        return 0;
    }

    bool lookahead_autolink(Context* ctx, MarkChain& mark_chain, OFFSET* off, OFFSET end) {
        /* Basic efficient sanity check, look for http:// or https:// */
        if (*off + 7 >= end)
            return false;
        if (CH(*off + 1) != 't' || CH(*off + 2) != 't' || CH(*off + 3) != 'p')
            return false;
        bool is_https = false;
        if (CH(*off + 4) == 's')
            is_https = true;
        if (is_https && *off + 8 >= end)
            return false;
        if (CH(*off + 4 + (int)is_https) != ':' || CH(*off + 5 + (int)is_https) != '/' || CH(*off + 6 + (int)is_https) != '/')
            return false;

        OFFSET start = *off;
        while (*off < end) {
            if (ISWHITESPACE(*off) || CH(*off) == '[' || CH(*off) == ']')
                break;
            else if (ISPUNCT(*off) && CHECK_WS_OR_END(*off + 1)) {
                break;
            }
            (*off)++;
        }
        Mark autolink{ S_AUTOLINK, "http://", " ", false, SELECT_ALL };
        autolink.solved = true;
        autolink.true_bounds.push_back(Boundaries{ ctx->offset_to_line_number[start], start, start, *off, *off });
        mark_chain.push_back(autolink);
        auto ptr = &(mark_chain.back());
        autolink.is_closing = true;
        autolink.start_ptr = ptr;
        mark_chain.push_back(autolink);
        return true;
    }

    bool create_text(Context* ctx, std::vector<Boundaries>::iterator& b_it, std::vector<Boundaries>::iterator& b_end_it, TEXT_TYPE type, OFFSET start, OFFSET end) {
        bool ret = true;
        if (start == end || end > ctx->end)
            return true;
        std::vector<Boundaries> bounds;

        /* Edge case if the cursor just stopped on a new line */
        if (start == b_it->post) {
            b_it++;
            start = b_it->beg;
        }

        int last_line = ctx->offset_to_line_number[end];
        int diff = last_line - b_it->line_number;
        if (diff > 0) {
            bounds.push_back({ b_it->line_number, start, start, b_it->end, b_it->end });
            while (diff > 1) {
                b_it++;
                if (b_it == b_end_it)
                    break;
                start = b_it->beg;
                bounds.push_back({ b_it->line_number, start, start, b_it->end, b_it->end });
                diff = last_line - b_it->line_number;
            }
            b_it++;
            if (b_it->beg < end) {
                bounds.push_back({ b_it->line_number, b_it->beg, b_it->beg, end, end });
            }
        }
        else {
            bounds.push_back({ b_it->line_number, start, start, end, end });
        }

        CHECK_AND_RET(ctx->parser->text(type, bounds));

        return true;
    abort:
        return ret;
    }

    inline bool main_loop(Context* ctx, Container* ptr, MarkChain& mark_chain) {
        bool ret = true;
        std::unordered_map<int, int> flag_count;

#define OPEN_MARK(num) {int mark_count = open_mark(ctx, mark_chain, marks[(num)], off, bound.end, prev_is_punctuation || prev_is_whitespace, flag_count); \
                        if (mark_count > advance) advance = mark_count; }

#define CLOSE_MARK(num) if (!success && is_count_positive(flag_count, marks[(num)].s_type)) { \
                    success = close_mark(ctx, mark_chain, marks[(num)], &off, bound.end, ptr->content_boundaries, flag_count); \
                    if (success) { remove_from_flag_count(flag_count, marks[(num)].s_type); advance = 0; } \
                }

        for (auto& bound : ptr->content_boundaries) {
            bool prev_is_whitespace = true;
            bool prev_is_punctuation = false;
            /* Kind of a hack to avoid making ![[]] become !<ref />*/
            bool prev_is_exclamation_or_bracket = false;
            for (OFFSET off = bound.beg;off < bound.end;) {
                bool success = false;
                int advance = 1;

                if (CH(off) == '\\') {
                    /* Edge case for `\` */
                    if (!mark_chain.empty() && mark_chain.back().s_type & S_VERBATIME && CH(off + 1) == '`')
                        off++;

                    else
                        off += 2;
                    continue;
                }

                else if (ISWHITESPACE(off))
                    prev_is_whitespace = true;
                /* Opening marks, num based on table marks */
                else if (CH(off) == '{') {
                    OPEN_MARK(M_EM);
                    OPEN_MARK(M_STRONG);
                    OPEN_MARK(M_HIGHLIGHT);
                    OPEN_MARK(M_UNDERLINE);
                    OPEN_MARK(M_DELETE);
                    OPEN_MARK(M_ATTRIBUTE);
                }
                else if (CH(off) == '*') {
                    CLOSE_MARK(M_STRONG);
                    CLOSE_MARK(M_STRONG_SIMPLE);
                    if (!success) {
                        OPEN_MARK(M_STRONG_SIMPLE);
                    }
                }
                else if (CH(off) == '_') {
                    CLOSE_MARK(M_EM);
                    CLOSE_MARK(M_EM_SIMPLE);
                    if (!success) {
                        OPEN_MARK(M_EM_SIMPLE);
                    }
                }
                else if (CH(off) == '`') {
                    CLOSE_MARK(M_VERBATIME);
                    if (!success) {
                        OPEN_MARK(M_VERBATIME);
                    }
                }
                else if (CH(off) == '!') {
                    OPEN_MARK(M_INSERTED_REF);
                    OPEN_MARK(M_IMG_TITLE);
                    OPEN_MARK(M_IMG_DEF);
                    OPEN_MARK(M_IMG);
                }
                else if (CH(off) == '[') {
                    OPEN_MARK(M_REF);
                    OPEN_MARK(M_LINK);
                    OPEN_MARK(M_LINKDEF);
                }
                else if (CH(off) == '$') {
                    CLOSE_MARK(M_LATEX);
                    if (!success) {
                        OPEN_MARK(M_LATEX);
                    }
                }
                else if (CH(off) == 'h') {
                    bool success = lookahead_autolink(ctx, mark_chain, &off, bound.end);
                    if (success)
                        continue;
                }
                else if (CH(off) == '=') {
                    CLOSE_MARK(M_HIGHLIGHT);
                }
                else if (CH(off) == '+') {
                    CLOSE_MARK(M_UNDERLINE);
                }
                else if (CH(off) == '-') {
                    CLOSE_MARK(M_DELETE);
                }
                else if (CH(off) == ']') {
                    CLOSE_MARK(M_INSERTED_REF);
                    CLOSE_MARK(M_REF);
                    CLOSE_MARK(M_IMG_TITLE);
                    CLOSE_MARK(M_IMG_DEF);
                    CLOSE_MARK(M_IMG);
                    CLOSE_MARK(M_LINK);
                    CLOSE_MARK(M_LINKDEF);
                }
                else if (CH(off) == '}') {
                    CLOSE_MARK(M_ATTRIBUTE);
                }

                if (ISPUNCT(off))
                    prev_is_punctuation = true;

                if (!success)
                    off += advance;
            }
        }
        return ret;
    }

    inline bool mark_cleanup(Context* ctx, MarkChain& mark_chain) {
        bool ret = true;

        /* Cleanup of unmatched spans and attribute creation */
        std::vector<std::list<Mark>::iterator> to_erase;
        for (auto it = mark_chain.begin();it != mark_chain.end();it++) {
            if (!it->solved) {
                to_erase.push_back(it);
            }
            else if (it->s_type == S_ATTRIBUTE) {
                auto next = std::next(it);
                if (it != mark_chain.begin() && std::prev(it)->solved) {
                    auto prev = std::prev(it);
                    /* Need to test if btw prev and attribute there is only whitespace */
                    auto prev_bound = prev->start_ptr->true_bounds.back();
                    bool valid_attribute = true;
                    if (!prev->solved)
                        valid_attribute = false;
                    if (valid_attribute && prev_bound.line_number != it->line_number)
                        valid_attribute = false;
                    if (valid_attribute)
                        for (OFFSET off = prev_bound.post;off < it->pre;off++) {
                            if (!ISWHITESPACE(off)) {
                                valid_attribute = false;
                                break;
                            }
                        }

                    if (valid_attribute) {
                        OFFSET off = it->beg;
                        prev->start_ptr->attributes = parse_attributes(ctx, &off);
                        prev->start_ptr->true_bounds.back().post = it->true_bounds.back().post;
                    }
                }
                to_erase.push_back(it);
                to_erase.push_back(next);
                it++;
            }
        }
        for (auto it : to_erase) {
            mark_chain.erase(it);
        }
        return ret;
    }

    bool parse_text(Context* ctx, Container* ptr, MarkChain& mark_chain) {
        bool ret = true;

        /* Pass the spans to the caller of the library */
        auto bound_it = ptr->content_boundaries.begin();
        auto bound_end = ptr->content_boundaries.end();
        OFFSET text_off = bound_it->beg;

        TEXT_TYPE t_type = TEXT_NORMAL;
        for (auto it = mark_chain.begin();it != mark_chain.end();it++) {
            auto& mark = *it;
            if (!mark.is_closing) {
                /* Create href details for links */
                SpanDetailPtr detail = nullptr;
                if (mark.s_type & (S_LINK | S_LINKDEF)) {
                    OFFSET start = mark.true_bounds.back().end + 2;
                    OFFSET end = mark.true_bounds.back().post - 1;
                    auto tmp = std::make_shared<SpanADetail>();
                    for (OFFSET off = start;off < end;off++) {
                        tmp->href += CH(off);
                    }
                    if (mark.s_type & S_LINKDEF)
                        tmp->alias = true;
                    detail = tmp;
                }
                else if (mark.s_type == S_AUTOLINK) {
                    OFFSET start = mark.true_bounds.back().pre;
                    OFFSET end = mark.true_bounds.back().end;
                    auto tmp = std::make_shared<SpanADetail>();
                    for (OFFSET off = start;off < end;off++) {
                        tmp->href += CH(off);
                    }
                    detail = tmp;
                }

                else if (mark.s_type & SELECT_IMGS) {
                    OFFSET start = mark.true_bounds.back().beg;
                    OFFSET end = mark.true_bounds.back().end;
                    OFFSET post = mark.true_bounds.back().post;
                    auto tmp = std::make_shared<SpanImgDetail>();
                    if (mark.s_type & S_IMG) {
                        for (OFFSET off = start;off < end;off++) {
                            tmp->src += CH(off);
                        }
                    }
                    else {
                        for (OFFSET off = start;off < end;off++) {
                            tmp->title += CH(off);
                        }
                        for (OFFSET off = end + 2;off < post - 1;off++) {
                            tmp->src += CH(off);
                        }
                    }
                    if (mark.s_type & S_IMG_DEF)
                        tmp->alias = true;
                    detail = tmp;
                }
                else if (mark.s_type & SELECT_REFS) {
                    OFFSET start = mark.true_bounds.back().beg;
                    OFFSET end = mark.true_bounds.back().end;
                    auto tmp = std::make_shared<SpanRefDetail>();
                    for (OFFSET off = start;off < end;off++) {
                        tmp->name += CH(off);
                    }
                    if (mark.s_type & S_INSERTED_REF)
                        tmp->inserted = true;
                    detail = tmp;
                }

                /* Insert text left to span */
                CHECK_AND_RET(create_text(ctx, bound_it, bound_end, TEXT_NORMAL, text_off, mark.true_bounds.front().pre));
                text_off = mark.true_bounds.front().beg;

                CHECK_AND_RET(ctx->parser->enter_span(
                    flag_to_type(mark.s_type),
                    mark.true_bounds,
                    mark.attributes,
                    detail
                ));
            }
            else {
                /* Insert text from inside span */
                auto bound = mark.start_ptr->true_bounds.back();
                bool has_text = true;
                TEXT_TYPE type = TEXT_NORMAL;
                if (mark.s_type == S_LATEX) {
                    type = TEXT_LATEX;
                }
                else if (mark.s_type == S_VERBATIME) {
                    type = TEXT_CODE;
                }
                else if (mark.s_type & (SELECT_REFS | SELECT_IMGS))
                    has_text = false;

                if (has_text)
                    CHECK_AND_RET(create_text(ctx, bound_it, bound_end, type, text_off, bound.end));
                text_off = bound.post;

                CHECK_AND_RET(ctx->parser->leave_span(flag_to_type(mark.s_type)));
            }
        }
        CHECK_AND_RET(create_text(ctx, bound_it, bound_end, t_type, text_off, ptr->content_boundaries.back().end));

        return ret;
    abort:
        return ret;
    }

    bool parse_spans(Context* ctx, Container* ptr) {
        bool ret = true;

        MarkChain mark_chain;
        if (ptr->b_type != BLOCK_CODE && ptr->b_type != BLOCK_LATEX) {
            main_loop(ctx, ptr, mark_chain);
            mark_cleanup(ctx, mark_chain);
            parse_text(ctx, ptr, mark_chain);
        }
        else {
            TEXT_TYPE t_type;
            if (ptr->b_type == BLOCK_CODE) {
                t_type = TEXT_CODE;
            }
            else if (ptr->b_type == BLOCK_LATEX) {
                t_type = TEXT_LATEX;
            }
            auto& bounds = ptr->content_boundaries;
            auto b_start = bounds.begin();
            auto b_end = bounds.end();
            CHECK_AND_RET(create_text(ctx, b_start, b_end, t_type, bounds.front().beg, bounds.back().end));
        }
        return true;
    abort:
        return ret;
    }
}