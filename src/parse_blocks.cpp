#include "parse_blocks.h"
#include "parse_spans.h" 
#include "parse_commons.h"
#include "internal.h"

namespace AB {
    static const int LIST_OPENER = 0x1;
    static const int CODE_OPENER = 0x2;
    static const int HR_OPENER = 0x4;
    static const int H_OPENER = 0x8;
    static const int P_OPENER = 0x10;
    static const int QUOTE_OPENER = 0x20;
    static const int DIV_OPENER = 0x40;
    static const int DEFINITION_OPENER = 0x080;
    static const int LATEX_OPENER = 0x100;

    /******************
    *** SegmentInfo ***
    *******************/
    /**
     * SegmentInfo is used to determine the blocks on each line
     */
    struct SegmentInfo {
        int flags = 0;
        BLOCK_TYPE type = BLOCK_DOC;
        Boundaries b_bounds;
        OFFSET start = 0;
        OFFSET end = 0;
        OFFSET first_non_blank = 0;
        OFFSET indent = 0;
        OFFSET line_number = -1;
        int above_list_depth = 0;
        bool blank_line = true;
        bool skip_segment = false;
        std::string acc; /* Accumulator */
        bool close_block = false;
        bool no_content_after = false;
        int count = 0; /* Repeated marker counts */

        Attributes attributes;

        // List information
        char li_pre_marker = 0;
        char li_post_marker = 0;
    };


    /***********************
    *** Helper functions ***
    ************************/

    // TODO: correct utf8 implementation
    inline void next_utf8_char(OFFSET* text_pos) {
        *text_pos += 1;
    }

    /**
     * Verifies if a str can be converted into a positiv number
    */
    bool verify_positiv_number(const std::string& str) {
        if (str.empty())
            return false;
        if (str.length() == 1 && ISDIGIT_(str[0]))
            return true;
        for (int i = 0;i < str.length();i++) {
            if (!ISDIGIT_(str[i]) || (i == 0 && str[i] == '0'))
                return false;
        }
        return true;
    }
    static std::string to_string(Context* ctx, OFFSET start, OFFSET end) {
        std::string out;
        for (OFFSET i = start;i < end;i++) {
            out += CH(i);
        }
        return out;
    }
    static OFFSET find_next_line_off(Context* ctx, OFFSET off) {
        OFFSET current_line_number = ctx->offset_to_line_number[off];
        if (current_line_number + 1 >= ctx->line_number_begs.size())
            return ctx->end;
        else
            return ctx->line_number_begs[current_line_number + 1] - 1;
    }
    bool check_for_whitespace_after(Context* ctx, OFFSET off) {
        while (CH(off) != '\n' && off < (OFFSET)ctx->end) {
            if (!ISWHITESPACE(off))
                return false;
            off++;
        }
        return true;
    }

    /**************
    *** Parsing ***
    ***************/

    static void select_last_child_container(Context* ctx) {
        if (ctx->above_container != nullptr) {
            if (!ctx->above_container->children.empty()) {
                ctx->above_container = ctx->above_container->children.back();
                if (ctx->above_container->b_type == BLOCK_UL || ctx->above_container->b_type == BLOCK_OL) {
                    ctx->above_container = ctx->above_container->children.back();
                }
                ctx->current_container = ctx->above_container;
            }
            else {
                ctx->above_container = nullptr;
            }
        }
    }

    // === Analysing ===

    static void analyse_make_p(Context* ctx, OFFSET off, OFFSET* end, SegmentInfo* seg) {
        seg->attributes.clear();
        seg->b_bounds.pre = off;
        seg->b_bounds.beg = off;
        seg->flags = P_OPENER;
        seg->type = BLOCK_P;
        *end = seg->end;
    }
    static void analyse_make_ul(Context* ctx, OFFSET off, OFFSET* end, SegmentInfo* seg, int indent) {
        seg->b_bounds.pre = seg->start;
        seg->b_bounds.beg = (off + 1 == seg->end) ? off + 1 : off + 2;
        seg->indent = off + 2 - seg->start + indent;
        *end = (CHECK_SPACE_AFTER(off)) ? off + 2 : off + 1;
        seg->flags = LIST_OPENER;
        seg->type = BLOCK_UL;
        seg->li_pre_marker = CH(off);
    }
    static inline int get_allowed_ws(int flag) {
        if (flag & (QUOTE_OPENER | DEFINITION_OPENER))
            return 1;
        else
            return 3;
    }

    static inline void get_name_and_attributes(Context* ctx, OFFSET* off, std::string& name, Attributes& attributes) {
        for (;(SIZE)*off < ctx->end && CH(*off) != '\n';(*off)++) {
            if (CH(*off) == '{' && CH(*off + 1) == '{') {
                *off += 2;
                attributes = parse_attributes(ctx, off);
                break;
            }
            else if (ISWHITESPACE(*off))
                continue;
            else
                name += CH(*off);
        }
    }

    /**
     * Checks at off if there are closing delimiters given the set of rules in the arguments
     *
     * It optionally also parses the attributes
     *
     * Returns the number of closing delimiters found
     * If after the closing delimiters there are non-authorised chars, then it returns -1
     * If it hasn't found any closing delimiter, then it return 0
    */
    static inline int check_for_closing_delimiters(Context* ctx, OFFSET* off, SegmentInfo* seg, char marker, int num_markers, bool allow_greater_number, bool allow_chars_before_closing, bool allow_attributes) {
        bool check_ws_before = allow_chars_before_closing
            || !allow_chars_before_closing && CHECK_WS_BEFORE(*off);


        int count = 0;
        for (;(SIZE)*off < ctx->end && CH(*off) != '\n';(*off)++) {
            if (CH(*off) == '\\')
                count = -1;
            else if (CH(*off) == marker)
                count++;
            else if (count < num_markers)
                count = 0;
            else
                break;
        }
        bool is_count_right = allow_greater_number && count >= num_markers
            || !allow_greater_number && count == num_markers;
        bool non_authorized_text_after = false;
        /* Check for attributes after ending */

        OFFSET tmp_off = *off;

        skip_whitespace(ctx, &tmp_off);

        if (CH(tmp_off) == '{' && allow_attributes) {
            tmp_off++;
            if (CH(tmp_off) == '{') {
                tmp_off++;
                seg->attributes = parse_attributes(ctx, &tmp_off);
                if (seg->attributes.empty()) {
                    non_authorized_text_after = true;
                }
            }
            else {
                non_authorized_text_after = true;
            }
        }
        else if (!CHECK_WS_OR_END(*off)) {
            non_authorized_text_after = true;
        }

        if (is_count_right && check_ws_before && !non_authorized_text_after) {
            return count;
        }
        else if (non_authorized_text_after)
            return -1;
        else
            return 0;
    }

    bool analyse_segment(Context* ctx, OFFSET off, OFFSET* end, SegmentInfo* seg) {
        bool ret = true;

        *seg = SegmentInfo();

        seg->end = find_next_line_off(ctx, off);
        seg->line_number = ctx->offset_to_line_number[off];
        OFFSET this_segment_end = seg->end;
        Container* above_container = ctx->above_container;

        int line_start = off;

        seg->start = off;
        seg->first_non_blank = seg->end; // Unless otherwise, the first blank is defined at the end of the line
        seg->b_bounds.pre = off;
        seg->b_bounds.beg = off;
        seg->b_bounds.end = -1;
        seg->b_bounds.post = -1;
        seg->blank_line = true; // Unless otherwise, the default line is a blank line

        RepeatedMarker repeated_markers;
        int local_indent = 0;
        int total_indent = 0;
        ContainerPtr corrected_above_quote = nullptr;
        if (above_container != nullptr) {
            local_indent = above_container->indent;
            total_indent = local_indent;
            if (above_container->repeated_markers.marker && !above_container->closed)
                repeated_markers = above_container->repeated_markers;
        }

        /* If above is a block which has open/close delimiters (repeating markers),
         * then all standard detection rules should be ignored and one should only
         * look to closing delimiters.
         * We already know that the segment will be of the flag of above */
        if (repeated_markers.marker && !above_container->closed) {
            seg->flags = above_container->flag;
            seg->blank_line = false;
            seg->b_bounds.end = seg->end;
            seg->b_bounds.post = seg->end;
        }

        int whitespace_counter = 0;
        std::string acc; // Acc is for accumulator
        enum SOLVED { NONE, PARTIAL, FULL };
        SOLVED b_solved = NONE;

        // Useful macros for segment analysis
#define CHECK_INDENT(allowed_ws) (whitespace_counter - total_indent < allowed_ws)

        // Segment analysis
        while (off < seg->end) {
            acc += CH(off);

            /* Indent is useful for knowing when to move above_container (see explanations
             * in process_segment) in the case of lists or definitions. Here is an example:
             *
             * - > abc
             *   > def
             * ^
         * cursor pos
                        seg->b_bounds.end = off + num_markers;
                        seg->b_bounds.post = off + num_markers;
             *
             * We are on the second line. above_container points to the LI, which has an
             * indent of 2 (you need two whitespace characters before the block to be part
             * of the LI). So if we detect enough whitespace characters before the block `>`,
             * then we move the above_container to the child of the LI, i.e. QUOTE. This way,
             * in process_segment(), the QUOTE can be continued as part of the LI.
             * */
             /* It means that there is enough indent to be part of the LI or DEF */
            if (local_indent > 0 && seg->blank_line && whitespace_counter >= local_indent
                && !above_container->closed) {
                above_container->content_boundaries.push_back({ seg->line_number, seg->start, off, seg->end, seg->end });
                if (above_container->b_type == BLOCK_LI)
                    above_container->parent->content_boundaries.push_back({ seg->line_number, seg->start, seg->start, seg->end, seg->end });
                // ctx->non_commited_boundaries.push_back({ {seg->line_number, seg->start, off, seg->end, seg->end}, ctx->above_container });
                seg->start = off;
                select_last_child_container(ctx);
                seg->above_list_depth++;
                above_container = ctx->above_container;
                total_indent += above_container->indent;
                local_indent = above_container->indent;
                repeated_markers = above_container->repeated_markers;
            }

            if (!(ISWHITESPACE(off) || CH(off) == '\n') && seg->blank_line) {
                seg->blank_line = false;
                seg->first_non_blank = off;
                acc = CH(off);
            }

            if (CH(off) == ' ') {
                whitespace_counter++;
            }
            else if (CH(off) == '\t') {
                whitespace_counter += 4;
            }
            else if (CH(off) == '\\') {
                if (off == seg->start && !repeated_markers.marker) {
                    // Paragraph
                    analyse_make_p(ctx, seg->start, &this_segment_end, seg);
                    break;
                }
                else
                    next_utf8_char(&off);
            }
            else if (repeated_markers.marker) {
                if (CH(off) == repeated_markers.marker) {
                    /* Try to see if the block is closed with pre-defined rules */
                    OFFSET off_before = off;
                    int num = check_for_closing_delimiters(
                        ctx, &off, seg,
                        repeated_markers.marker,
                        repeated_markers.count,
                        repeated_markers.allow_greater_number,
                        repeated_markers.allow_chars_before_closing,
                        repeated_markers.allow_attributes
                    );
                    if (num) {
                        seg->close_block = true;
                        seg->flags = above_container->flag;
                        seg->b_bounds.end = off_before;
                        seg->b_bounds.post = off;
                        break;
                    }
                }
            }
            else if (CH(off) == '#') {
                int count = count_marks(ctx, off, '#');
                if (CHECK_WS_BEFORE(off) && count > 0 && count < 7
                    && CHECK_WS_OR_END(off + count) && CHECK_INDENT(3)) {
                    // Valid header
                    seg->flags = H_OPENER;
                    seg->b_bounds.pre = seg->start;
                    seg->b_bounds.beg = off + count;
                    b_solved = FULL;
                    seg->type = BLOCK_H;
                    break;
                }
                else {
                    // Paragraph
                    analyse_make_p(ctx, seg->start, &this_segment_end, seg);
                    break;
                }
            }
            else if (CH(off) == '>') {
                if (CHECK_WS_BEFORE(off) && CHECK_INDENT(get_allowed_ws(QUOTE_OPENER))) {
                    // Valid quote
                    seg->b_bounds.pre = seg->start; seg->b_bounds.beg = off + 1;
                    seg->flags = QUOTE_OPENER;
                    this_segment_end = off + 1;
                    b_solved = FULL;
                    seg->type = BLOCK_QUOTE;
                    if (CHECK_SPACE_AFTER(off)) {
                        seg->b_bounds.beg = off + 2;
                        this_segment_end = off + 2;
                    }
                    break;
                }
                else {
                    analyse_make_p(ctx, seg->start, &this_segment_end, seg);
                    break;
                }
            }
            // Block detection
            else if (CH(off) == '*') { // Potential bullet lists
                if (CHECK_WS_BEFORE(off) && CHECK_WS_OR_END(off + 1) && !(seg->flags & LIST_OPENER)) {
                    // Make list
                    analyse_make_ul(ctx, off, &this_segment_end, seg, whitespace_counter);
                    break;
                }
                else {
                    // Paragraph
                    analyse_make_p(ctx, seg->start, &this_segment_end, seg);
                    break;
                }
            }
            else if (CH(off) == '-') {
                int count = count_marks(ctx, off, '-');
                if (CHECK_WS_BEFORE(off) && count > 2 && check_for_whitespace_after(ctx, off + count)) {
                    seg->flags = HR_OPENER;
                    seg->b_bounds.pre = off;
                    seg->b_bounds.beg = off + count;
                    this_segment_end = seg->end;
                    b_solved = FULL;
                    seg->type = BLOCK_HR;
                    break;
                }
                else if (CHECK_WS_BEFORE(off) && CHECK_WS_OR_END(off + 1) && !(seg->flags & LIST_OPENER
                    && CHECK_INDENT(get_allowed_ws(LIST_OPENER)))) {
                    // Unordered list
                    analyse_make_ul(ctx, off, &this_segment_end, seg, whitespace_counter);
                    break;
                }
                else {
                    // Paragraph
                    analyse_make_p(ctx, seg->start, &this_segment_end, seg);
                    break;
                }
            }
            else if (CH(off) == '+') {
                if (CHECK_WS_BEFORE(off) && CHECK_WS_OR_END(off + 1) && !(seg->flags & LIST_OPENER)
                    && CHECK_INDENT(get_allowed_ws(LIST_OPENER))) {
                    // Make list
                    analyse_make_ul(ctx, off, &this_segment_end, seg, whitespace_counter);
                    break;
                }
            }
            // Potential ordered lists
            else if (CH(off) == '(') {
                if (seg->flags & LIST_OPENER || !CHECK_WS_BEFORE(off)) {
                    // Paragraph
                    analyse_make_p(ctx, seg->start, &this_segment_end, seg);
                    break;
                }
                b_solved = PARTIAL;
                acc.clear();
                seg->flags |= LIST_OPENER;
                seg->li_pre_marker = '(';
            }
            else if (ISANYOF2(off, ')', '.')) {
                std::string str = acc.substr(0, acc.size() - 1);
                if (str.length() > 0 && str.length() < 12 && CHECK_WS_OR_END(off + 1)
                    && !(seg->li_pre_marker == '(' && CH(off) == '.')) { // Potential list
                    // The validity of enumeration should still be checked
                    seg->b_bounds.pre = seg->start;
                    seg->b_bounds.beg = off + 1;
                    seg->indent = off + 2 - seg->start + whitespace_counter;
                    this_segment_end = off + 2;
                    if (off + 1 < (OFFSET)ctx->end && CH(off + 1) == ' ')
                        seg->b_bounds.beg++;

                    seg->flags = LIST_OPENER;
                    b_solved = PARTIAL;
                    seg->li_post_marker = CH(off);
                    acc = str;
                    break;
                }
                else {
                    analyse_make_p(ctx, seg->start, &this_segment_end, seg);
                    break;
                }
            }
            else if (CH(off) == '[') {
                if (!CHECK_INDENT(get_allowed_ws(DEFINITION_OPENER)) ||
                    above_container != nullptr && above_container->parent->b_type != BLOCK_DOC) {
                    analyse_make_p(ctx, seg->start, &this_segment_end, seg);
                    break;
                }
                OFFSET start_off = off;
                acc = "";
                bool found_end = advance_until(ctx, &off, acc, ']');
                if (!found_end || CH(off + 1) != ':' || off - start_off < 2) {
                    analyse_make_p(ctx, seg->start, &this_segment_end, seg);
                    break;
                }
                else {
                    seg->flags = DEFINITION_OPENER;
                    b_solved = FULL;
                    seg->indent = 4 + whitespace_counter;
                    seg->b_bounds.pre = seg->start;
                    seg->b_bounds.beg = off + 2;
                    seg->b_bounds.end = off + 2;
                    seg->b_bounds.post = off + 2;
                    this_segment_end = off + 2;
                    seg->acc = acc;
                    break;
                }
            }

            else if (CH(off) == ':') {
                OFFSET before_off = off;
                int count = count_marks(ctx, &off, ':');

                skip_whitespace(ctx, &off);
                if (CHECK_WS_BEFORE(before_off) && count == 3 && off < seg->end) {
                    seg->flags = DIV_OPENER;
                    seg->b_bounds.pre = seg->start;
                    seg->b_bounds.beg = seg->end;
                    seg->b_bounds.end = seg->end;
                    seg->b_bounds.post = seg->end;
                    seg->indent = 4 + ctx->current_container->indent;
                    this_segment_end = seg->end;
                    seg->acc = "";
                    get_name_and_attributes(ctx, &off, seg->acc, seg->attributes);
                    break;
                }
                else {
                    analyse_make_p(ctx, seg->start, &this_segment_end, seg);
                    break;
                }
            }
            else if (CH(off) == '$') {
                OFFSET tmp_off = off;
                int count = count_marks(ctx, &off, '$');
                /* Try to advance until the end of the line to see if the block is already closed */
                advance_until(ctx, &off, acc, '$');
                int closing = check_for_closing_delimiters(ctx, &off, seg, '$', 2, false, true, true);
                if (CHECK_WS_BEFORE(tmp_off) && count == 2 && closing >= 0) {
                    seg->flags = LATEX_OPENER;
                    seg->b_bounds.beg = tmp_off + count;
                    seg->b_bounds.end = seg->end;
                    seg->b_bounds.post = seg->end;
                    if (closing) {
                        seg->close_block = true;
                        seg->b_bounds.end = off - closing;
                    }
                    off++;
                    break;
                }

                else {
                    analyse_make_p(ctx, seg->start, &this_segment_end, seg);
                    break;
                }

            }
            else if (CH(off) == '`') {
                int count = count_marks(ctx, off, '`');
                if (CHECK_WS_BEFORE(off) && count > 2 && CHECK_INDENT(get_allowed_ws(CODE_OPENER))) {
                    seg->flags = CODE_OPENER;
                    seg->b_bounds.beg = seg->end;
                    seg->b_bounds.end = seg->end;
                    seg->b_bounds.post = seg->end;
                    seg->count = count;
                    off += count;
                    seg->acc = "";
                    get_name_and_attributes(ctx, &off, seg->acc, seg->attributes);
                }
                else {
                    analyse_make_p(ctx, seg->start, &this_segment_end, seg);
                    break;
                }

            }
            next_utf8_char(&off);
        }

        if (!seg->blank_line && seg->flags == 0) {
            analyse_make_p(ctx, seg->start, &this_segment_end, seg);
        }
        if (whitespace_counter < local_indent && seg->first_non_blank - seg->start > get_allowed_ws(seg->flags)) {
            analyse_make_p(ctx, seg->start, &this_segment_end, seg);
        }
        /* Some blank lines can be transformed into boundaries of indented blocks (LI, DEF) */
        if (seg->blank_line && whitespace_counter <= local_indent
            && above_container != nullptr && (above_container->b_type == BLOCK_LI
                || above_container->b_type == BLOCK_DEF || above_container->b_type == BLOCK_DIV)) {
            // ctx->non_commited_boundaries.push_back({ {seg->line_number, seg->start, seg->end, seg->end, seg->end}, ctx->above_container });
            above_container->content_boundaries.push_back({ seg->line_number, seg->start, seg->end, seg->end, seg->end });
            if (above_container->b_type == BLOCK_LI)
                above_container->parent->content_boundaries.push_back({ seg->line_number, seg->start, seg->start, seg->end, seg->end });
            seg->skip_segment = true;
        }

        // If potential list has been detected, verify if the enumeration makes sense
        if (seg->flags & LIST_OPENER && b_solved == PARTIAL) {
            if (seg->li_pre_marker == '(' && seg->li_post_marker != ')') {
                analyse_make_p(ctx, seg->start, &this_segment_end, seg);
            }
            else {
                /* Still need to verify if ordered list has valid enumeration
                 * Example: 'IM' is not a valid roman value */
                if ((verify_positiv_number(acc) && acc.length() < 10)
                    || validate_roman_enumeration(acc)
                    || alpha_to_decimal(acc) > 0 && acc.length() < 4) {
                    b_solved = FULL;
                    seg->acc = acc;
                    seg->type = BLOCK_OL;
                }
                else {
                    analyse_make_p(ctx, seg->start, &this_segment_end, seg);
                }
            }
        }

        if (seg->flags & LIST_OPENER && seg->b_bounds.beg == seg->end) {
            seg->no_content_after = true;
        }

        *end = this_segment_end;

        return ret;
    }

    // === Processing ===
    bool enter_block(Context* ctx, Container* ptr) {
        bool ret = true;
        CHECK_AND_RET(ctx->parser->enter_block(ptr->b_type, ptr->content_boundaries, ptr->attributes, ptr->detail));
        for (auto child : ptr->children) {
            if (child->b_type == BLOCK_EMPTY)
                continue;
            CHECK_AND_RET(enter_block(ctx, child));
        }
        if (is_leaf_block(ptr->b_type)) {
            parse_spans(ctx, ptr);
        }
        CHECK_AND_RET(ctx->parser->leave_block(ptr->b_type));
        return ret;
    abort:
        return ret;
    }

    bool send_previous_blocks(Context* ctx) {
        bool ret = true;
        Container* root = ctx->containers.front();
        for (auto child : root->children)
            CHECK_AND_RET(enter_block(ctx, child));
        root->children.clear();
        ctx->above_container = root;
        ctx->last_free_mem_it = ctx->containers.begin() + 1;
        /* Reset memory for all "freed" memory */
        for (auto it = ctx->last_free_mem_it; it != ctx->containers.end();it++) {
            (*it)->children.clear();
            (*it)->closed = false;
            (*it)->content_boundaries.clear();
            (*it)->repeated_markers = RepeatedMarker{};
            (*it)->last_non_empty_child_line = -1;
        }
        return ret;
    abort:
        return ret;
    }

    static void add_container(Context* ctx, BLOCK_TYPE block_type, const Boundaries& bounds, SegmentInfo* seg, std::shared_ptr<BlockDetail> detail = nullptr) {
        Container* parent = ctx->current_container;
        /* This means there is some already allocated memory
         * to be used */
        bool is_free_memory = ctx->last_free_mem_it != ctx->containers.end();

        Container* container;
        if (is_free_memory) {
            container = *ctx->last_free_mem_it;
        }
        else {
            container = new Container(); /* New containers will be deleted at the end of parsing in parse.cpp */
        }

        container->b_type = block_type;
        container->content_boundaries.push_back(bounds);
        container->detail = detail;
        container->parent = parent;
        container->indent = seg->indent;
        container->flag = seg->flags;
        container->attributes = seg->attributes;
        if (block_type != BLOCK_HIDDEN) {
            parent->last_non_empty_child_line = seg->line_number;
        }
        if (is_free_memory) {
            ctx->last_free_mem_it++;
        }
        else {
            ctx->containers.push_back(container);
            ctx->last_free_mem_it = ctx->containers.end();
        }
        ctx->current_container = container;
        parent->children.push_back(container);
    }

    /* Pass a non-null ptr and non root ptr to this function */
    static inline Container* select_parent(Container* ptr) {
        Container* parent = ptr->parent;
        if (parent->b_type == BLOCK_UL || parent->b_type == BLOCK_OL)
            parent = parent->parent;
        return parent;
    }

    /* To avoid `if (ctx->current_container == nullptr)`, we have to make
    * sure to never call this function when current_container is BLOCK_DOCUMENT */
    static void close_current_container(Context* ctx) {
        ctx->current_container->closed = true;
        ctx->current_container = ctx->current_container->parent;
    }

    bool make_list_item(Context* ctx, SegmentInfo* seg, OFFSET off) {
        Container* above_container = ctx->above_container;

        bool is_ul = seg->acc.empty();
        char pre_marker = seg->li_pre_marker;
        char post_marker = seg->li_post_marker;
        Container* above_parent = (above_container != nullptr) ? above_container->parent : nullptr;
        bool is_above_ol = above_container != nullptr && above_parent->b_type == BLOCK_OL;
        bool is_above_ul = above_container != nullptr && above_parent->b_type == BLOCK_UL;

        BlockOlDetail::OL_TYPE type;
        int alpha = -1; int roman = -1;
        if (!is_ul) {
            alpha = alpha_to_decimal(seg->acc);
            roman = roman_to_decimal(seg->acc);
            if (verify_positiv_number(seg->acc)) {
                type = BlockOlDetail::OL_NUMERIC;
            }
            /* With this simple rule, we can decide between cases that are valid in both roman
             * and alpha case, e.g. 'i)' */
            else if (alpha > 0 && roman > 0) {
                if (alpha < roman)
                    type = BlockOlDetail::OL_ALPHABETIC;
                else
                    type = BlockOlDetail::OL_ROMAN;
            }
            else if (roman > 0) {
                type = BlockOlDetail::OL_ROMAN;
            }
            else {
                type = BlockOlDetail::OL_ALPHABETIC;
            }
        }

        bool make_new_list = false;
        /* If above at the same level there is not list, then we must
         * create a new list */
        if (!is_above_ul && !is_above_ol) {
            make_new_list = true;
        }
        else if (is_above_ul) {
            auto detail = std::static_pointer_cast<BlockUlDetail>(above_parent->detail);
            /* Even there is an above list, if the marker don't match then a new list
             * is still created */
            if (pre_marker != detail->marker)
                make_new_list = true;
        }
        else if (is_above_ol && !is_ul) {
            auto detail = std::static_pointer_cast<BlockOlDetail>(above_parent->detail);
            if (detail->pre_marker != pre_marker || detail->post_marker != post_marker)
                make_new_list = true;

            /* By default, we choose the enumeration type of the one that is lowest in decimal
             * However, if we are already in a list that is either alpha or roman, then the
             * current list item must inherit the alpha or roman property */
            if (type != BlockOlDetail::OL_NUMERIC) {
                if (detail->type == BlockOlDetail::OL_ALPHABETIC && roman > 0 && alpha > 0)
                    type = BlockOlDetail::OL_ALPHABETIC;
                else if (detail->type == BlockOlDetail::OL_ROMAN && roman > 0 && alpha > 0)
                    type = BlockOlDetail::OL_ROMAN;
            }
            if (type != detail->type)
                make_new_list = true;
        }
        else if (is_above_ol && is_ul) {
            make_new_list = true;
        }

        /* If above we have a list (at the same level at the current),
         * we need to close the last inserted list element */
        if (is_above_ul || is_above_ol) {
            for (auto ptr : above_container->children) {
                ptr->closed = true;
            }
            ctx->current_container->closed = true;
            /* Determine the number of blank lines between the last LI and this one */
            if (seg->line_number - above_container->last_non_empty_child_line > 2)
                make_new_list = true;

            if (make_new_list) {
                ctx->current_container = above_container->parent->parent;
            }
            else {
                ctx->current_container = above_container->parent;
            }
        }
        if (make_new_list) {
            if (seg->acc.empty()) {
                auto detail = std::make_shared<BlockUlDetail>();
                detail->marker = pre_marker;
                add_container(ctx, BLOCK_UL, { seg->line_number, seg->b_bounds.pre, seg->b_bounds.pre, seg->end, seg->end }, seg, detail);
            }
            else {
                auto detail = std::make_shared<BlockOlDetail>();
                detail->pre_marker = pre_marker;
                detail->post_marker = post_marker;
                detail->type = type;
                detail->lower_case = ISLOWER(seg->b_bounds.beg);
                add_container(ctx, BLOCK_OL, { seg->line_number,seg->b_bounds.pre, seg->b_bounds.pre, seg->end, seg->end }, seg, detail);
            }
        }
        else {
            above_parent->content_boundaries.push_back({ seg->line_number, seg->b_bounds.pre, seg->b_bounds.pre, seg->end, seg->end });
            ctx->current_container = above_parent;
        }

        // We can now add our list item
        auto detail = std::make_shared<BlockLiDetail>();
        if (!is_ul)
            detail->number = seg->acc;
        add_container(ctx, BLOCK_LI, { seg->line_number, seg->b_bounds.pre, seg->b_bounds.beg, seg->end, seg->end }, seg, detail);
        if (seg->no_content_after) {
            add_container(ctx, BLOCK_EMPTY, {}, seg);
        }

        ctx->above_container = nullptr;
        return true;
    }

    /**
     * @brief Once a segment of a line has been analysed by the analyse_segment
     * function, we need to decide were to place the block in the AST.
     * This process is context dependent.
     */
    bool process_segment(Context* ctx, OFFSET* off, SegmentInfo* seg) {
        bool ret = true;

        if (seg->skip_segment)
            return true;

        /* Above container is the critical part to take decisions on how the place
         * the newly analysed block. This pointer points to where we would be if we
         * are looking at the same level on the above line.
         * Here is an example:
         *
         *     > - item1
         *     > abc
         *     ^
         *  cursor pos
         *
         * We are on the second line. We just detected a quote character. above_container
         * points to the QUOTE of the previous line. This way we know we have to prolong
         * the QUOTE and not create a new one. For the next loop, we use select_last_child_container()
         * so the above pointer will point to the next block in the above line, i.e. list item (LI).
         *
         *     > - item1
         *     > abc
         *       ^
         *   cursor pos
         *
         * On the second pass, we detected a paragraph. However, as above we point to the LI,
         * we know that it should be closed and that the paragraph should be placed on the same
         * level as the UL containing the LI.
         *
         * There is a second pointer called ctx->current_container. Whenever add_container() is
         * called, the new block is insert as a child to ctx->current_container. This pointer
         * closely follows above_container, until above_container is set to nullptr, in which
         * case current_container always points to the last inserted block.
         */
        Container* above_container = ctx->above_container;

        bool set_above_to_nullptr = false;

        /* If the above_container doesn't match the current detected block, then we have to close
         * the current container. */
        int line_number_diff = 0;
        if (above_container != nullptr && above_container->b_type != BLOCK_DOC) {
            line_number_diff = seg->line_number - (above_container->content_boundaries.end() - 1)->line_number;
            if (line_number_diff > 1 || above_container->flag != seg->flags || above_container->flag & DEFINITION_OPENER) {
                close_current_container(ctx);
                if (above_container->b_type == BLOCK_LI) {
                    close_current_container(ctx);
                }
                set_above_to_nullptr = true;

                if (above_container->parent->b_type == BLOCK_DOC && !seg->blank_line) {
                    send_previous_blocks(ctx);
                }
            }
        }

        if (seg->blank_line) {
            Container* parent = ctx->containers.back(); // By default, blank lines belong to ROOT
            /* Blank lines should always be commited to parent above container */
            if (above_container != nullptr) {
                parent = select_parent(above_container);
            }
            ctx->current_container = parent;
            add_container(ctx, BLOCK_HIDDEN, { seg->line_number, seg->start, seg->start, seg->end, seg->end }, seg);
            close_current_container(ctx);
        }
        if (set_above_to_nullptr) {
            ctx->above_container = nullptr;
            above_container = nullptr;
        }

#define IS_BLOCK_CONTINUED(type) (above_container != nullptr && above_container->b_type == type && line_number_diff > 0)

        if (seg->flags & P_OPENER) {
            if (IS_BLOCK_CONTINUED(BLOCK_P)) {
                above_container->content_boundaries.push_back({ seg->line_number, seg->start, seg->first_non_blank, seg->end, seg->end });
            }
            else {
                add_container(ctx, BLOCK_P, { seg->line_number, seg->start, seg->first_non_blank, seg->end, seg->end }, seg);
            }
        }
        else if (seg->flags & HR_OPENER) {
            add_container(ctx, BLOCK_HR, { seg->line_number, seg->start, seg->start, seg->end, seg->end }, seg);
        }
        else if (seg->flags & H_OPENER) {
            bool new_header = true;
            int level = seg->b_bounds.beg - seg->b_bounds.pre;
            /* Headers can be empty, e.g. `##`
             * In this case, the mandatory space after is not taken into account
             * When there is the mandatory space, b_beg should begin one char after */
            if (seg->b_bounds.beg < seg->end) {
                seg->b_bounds.beg++;
            }

            if (IS_BLOCK_CONTINUED(BLOCK_H)) {
                auto detail = std::static_pointer_cast<BlockHDetail>(above_container->detail);
                if (detail->level == level) {
                    new_header = false;
                    above_container->content_boundaries.push_back({ seg->line_number, seg->b_bounds.pre, seg->b_bounds.beg, seg->end, seg->end });
                }
                else {
                    close_current_container(ctx);
                }
            }
            if (new_header) {
                auto detail = std::make_shared<BlockHDetail>();
                detail->level = level;
                add_container(ctx, BLOCK_H, { seg->line_number, seg->b_bounds.pre, seg->b_bounds.beg, seg->end, seg->end }, seg, detail);
            }
        }
        else if (seg->flags & QUOTE_OPENER) {
            if (IS_BLOCK_CONTINUED(BLOCK_QUOTE)) {
                above_container->content_boundaries.push_back({ seg->line_number, seg->b_bounds.pre, seg->b_bounds.beg, seg->end, seg->end });
            }
            else {
                add_container(ctx, BLOCK_QUOTE, { seg->line_number, seg->b_bounds.pre, seg->b_bounds.beg, seg->end, seg->end }, seg);
            }
        }
        else if (seg->flags & DEFINITION_OPENER) {
            auto detail = std::make_shared<BlockDefDetail>();
            detail->name = seg->acc;
            if (seg->acc[0] == '^')
                detail->definition_type = BlockDefDetail::DEF_FOOTNOTE;
            else if (seg->acc[0] == 'c' && seg->acc.length() > 3 && seg->acc[1] == ':')
                detail->definition_type = BlockDefDetail::DEF_CITATION;
            else
                detail->definition_type = BlockDefDetail::DEF_LINK;
            add_container(ctx, BLOCK_DEF, { seg->line_number, seg->b_bounds.pre, seg->b_bounds.beg, seg->b_bounds.end, seg->b_bounds.post }, seg, detail);
        }
        else if (seg->flags & LIST_OPENER) {
            /* Lists are not trivial to handle, there are a lot of edge cases */
            make_list_item(ctx, seg, *off);
        }
        else if (seg->flags & DIV_OPENER) {
            auto detail = std::make_shared<BlockDivDetail>();
            detail->name = seg->acc;
            add_container(ctx, BLOCK_DIV, { seg->line_number, seg->b_bounds.pre, seg->b_bounds.beg, seg->b_bounds.end, seg->b_bounds.post }, seg, detail);
            seg->flags = 0;
            add_container(ctx, BLOCK_EMPTY, { seg->line_number }, seg);
        }
        else if (seg->flags & LATEX_OPENER) {
            if (IS_BLOCK_CONTINUED(BLOCK_LATEX)) {
                ctx->current_container->content_boundaries.push_back({ seg->line_number, seg->b_bounds.pre, seg->b_bounds.beg, seg->b_bounds.end, seg->b_bounds.post });
                ctx->current_container->attributes = seg->attributes;
            }
            else {
                add_container(ctx, BLOCK_LATEX, { seg->line_number, seg->b_bounds.pre, seg->b_bounds.beg, seg->b_bounds.end, seg->b_bounds.post }, seg);
                auto& r = ctx->current_container->repeated_markers;
                r.marker = '$';
                r.count = 2;
                r.allow_greater_number = true;
                r.allow_chars_before_closing = true;
                r.allow_attributes = true;
            }
            if (seg->close_block) {
                ctx->current_container->closed = true;
            }
        }
        else if (seg->flags & CODE_OPENER) {
            if (IS_BLOCK_CONTINUED(BLOCK_CODE)) {
                ctx->current_container->content_boundaries.push_back({ seg->line_number, seg->b_bounds.pre, seg->b_bounds.beg, seg->b_bounds.end, seg->b_bounds.post });
            }
            else {
                auto detail = std::make_shared<BlockCodeDetail>();
                detail->lang = seg->acc;
                add_container(ctx, BLOCK_CODE, { seg->line_number, seg->b_bounds.pre, seg->b_bounds.beg, seg->b_bounds.end, seg->b_bounds.post }, seg, detail);
                auto& r = ctx->current_container->repeated_markers;
                r.marker = '`';
                r.count = seg->count;
                r.allow_greater_number = false;
                r.allow_chars_before_closing = false;
                r.allow_attributes = false;
            }
            if (seg->close_block) {
                ctx->current_container->closed = true;
            }
        }

        return ret;
    }

    bool parse_blocks(Context* ctx) {
        bool ret = true;


        OFFSET off = ctx->start;
        SegmentInfo current_seg;

        // Add root container
        Container* doc_container = new Container();
        doc_container->b_type = BLOCK_DOC;
        ctx->containers.push_back(doc_container);
        ctx->current_container = ctx->containers.front();
        /* Enter directly into DOC */
        CHECK_AND_RET(ctx->parser->enter_block(BLOCK_DOC, {}, {}, nullptr));

        ctx->last_free_mem_it = ctx->containers.begin() + 1;

        while (off < (int)ctx->end) {
            select_last_child_container(ctx);
            CHECK_AND_RET(analyse_segment(ctx, off, &off, &current_seg));
            CHECK_AND_RET(process_segment(ctx, &off, &current_seg));

            // We arrived at a the end of a line
            if (off >= current_seg.end) {
                ctx->above_container = ctx->containers.front();
                ctx->current_container = ctx->above_container;
                off++;
            }
        }

        CHECK_AND_RET(send_previous_blocks(ctx));
        CHECK_AND_RET(ctx->parser->leave_block(BLOCK_DOC));

        return ret;
    abort:
        return ret;
    }
};