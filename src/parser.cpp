/*
 * Part of this code is taken and inspired from MD4C: Markdown parser for C, http://github.com/mity/md4c
 * The licence for md4c is given below:
 *
 * Copyright (c) 2016-2020 Martin Mitas
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Terrible modifications by Jokteur, https://github.com/jokteur
 */

#include "parser.h"
#include "internal_helpers.h"
#include "definitions.h"
#include "helpers.h"

#include <iostream>
#include <memory>

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

#define CHECK_AND_RET(fct) \
    ret = (fct); \
    if (!ret) \
        goto abort;
#define ENTER_BLOCK(type, arg) ctx->parser.enter_block((type), (arg));
#define LEAVE_BLOCK() ctx->parser.leave_block();
#define ENTER_SPAN(type, arg) ctx->parser.enter_span((type), (arg));
#define LEAVE_SPAN() ctx->parser.leave_span();
#define TEXT(type, begin, end) ctx->parser.text((type), (arg));


    struct Container;
    typedef std::shared_ptr<Container> ContainerPtr;
    struct Container {
        bool closed = false;

        BLOCK_TYPE b_type;
        std::shared_ptr<BlockDetail> detail;
        ContainerPtr parent = nullptr;
        std::vector<ContainerPtr> children;
        std::vector<Boundaries> content_boundaries;
        int last_non_empty_child_line = -1;
        int flag = 0;
        unsigned indent = 0;
    };

    struct TemporaryBoundary {
        Boundaries bounds;
        ContainerPtr parent;
    };

    /**************
    *** Context ***
    ***************/
    /* The context is used throughout all the code and keeps stored
     * all the necessary informations for parsing
    */
    struct Context {
        /* Information given by the user */
        const CHAR* text;
        SIZE size;
        const Parser* parser;

        std::vector<ContainerPtr> containers;
        ContainerPtr current_container;
        ContainerPtr above_container = nullptr;

        std::vector<int> offset_to_line_number;
        std::vector<int> line_number_begs;
    };


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
        // unsigned indent = 0;
        bool blank_line = true;
        ContainerPtr current_container = nullptr;
        bool has_content_after = true;
        bool skip_segment = false;

        // List information
        char li_pre_marker = 0;
        char li_post_marker = 0;
        std::string li_number;
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
            return ctx->size;
        else
            return ctx->line_number_begs[current_line_number + 1] - 1;
    }
    int count_marks(Context* ctx, OFFSET off, char mark) {
        int counter = 0;
        while (CH(off) != '\n' && off < (OFFSET)ctx->size) {
            if (CH(off) == mark)
                counter++;
            else
                break;
            off++;
        }
        return counter;
    }
    bool check_for_whitespace_after(Context* ctx, OFFSET off) {
        while (CH(off) != '\n' && off < (OFFSET)ctx->size) {
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
        if (ctx->above_container != nullptr && !ctx->above_container->children.empty()) {
            ctx->above_container = *(ctx->above_container->children.end() - 1);
            if (ctx->above_container->b_type == BLOCK_UL || ctx->above_container->b_type == BLOCK_OL) {
                ctx->above_container = *(ctx->above_container->children.end() - 1);
            }
            ctx->current_container = ctx->above_container;
        }
    }

    // === Analysing ===

    static void analyse_make_p(Context* ctx, OFFSET off, OFFSET* end, SegmentInfo* seg) {
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
        if (flag & QUOTE_OPENER)
            return 1;
        else
            return 3;
    }

    bool analyse_segment(Context* ctx, OFFSET off, OFFSET* end, SegmentInfo* seg) {
        bool ret = true;

        *seg = SegmentInfo();

        seg->end = find_next_line_off(ctx, off);
        seg->line_number = ctx->offset_to_line_number[off];
        OFFSET this_segment_end = seg->end;
        ContainerPtr above_container = ctx->above_container;

        int line_start = off;

        seg->start = off;
        seg->first_non_blank = seg->end; // Unless otherwise, the first blank is defined at the end of the line
        seg->b_bounds.pre = off;
        seg->b_bounds.beg = off;
        seg->b_bounds.end = -1;
        seg->b_bounds.post = -1;
        seg->blank_line = true; // Unless otherwise, the default line is a blank line

        int local_indent = 0;
        int total_indent = 0;
        ContainerPtr corrected_above_quote = nullptr;
        if (above_container != nullptr) {
            local_indent = above_container->indent;
            total_indent = local_indent;
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
             * in process_segment) in the case of lists. Here is an example:
             *
             * - > abc
             *   > def
             * ^
         * cursor pos
             *
             * We are on the second line. above_container points to the LI, which has an
             * indent of 2 (you need two whitespace characters before the block to be part
             * of the LI). So if we detect enough whitespace characters before the block `>`,
             * then we move the above_container to the child of the LI, i.e. QUOTE. This way,
             * in process_segment(), the QUOTE can be continued as part of the LI.
             * */
             /* It means that there is enough indent to be part of the LI */
            if (local_indent > 0 && seg->blank_line && whitespace_counter >= local_indent) {
                above_container->content_boundaries.push_back({ seg->line_number, seg->start, off, seg->end, seg->end });
                above_container->parent->content_boundaries.push_back({ seg->line_number, seg->start, seg->start, seg->end, seg->end });
                // ctx->non_commited_boundaries.push_back({ {seg->line_number, seg->start, off, seg->end, seg->end}, ctx->above_container });
                seg->start = off;
                select_last_child_container(ctx);
                seg->above_list_depth++;
                above_container = ctx->above_container;
                total_indent += above_container->indent;
                local_indent = above_container->indent;
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
                if (off == seg->start) {
                    // Paragraph
                    analyse_make_p(ctx, seg->start, &this_segment_end, seg);
                    break;
                }
                else
                    next_utf8_char(&off);
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
                    if (off + 1 < (OFFSET)ctx->size && CH(off + 1) == ' ')
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
            next_utf8_char(&off);
        }

        if (!seg->blank_line && seg->flags == 0) {
            analyse_make_p(ctx, seg->start, &this_segment_end, seg);
        }
        if (whitespace_counter < local_indent && seg->first_non_blank - seg->start > get_allowed_ws(seg->flags)) {
            analyse_make_p(ctx, seg->start, &this_segment_end, seg);
        }
        /* Some blank lines can be transformed into boundaries of LI */
        if (seg->blank_line && whitespace_counter < local_indent
            && above_container != nullptr && above_container->b_type == BLOCK_LI) {
            // ctx->non_commited_boundaries.push_back({ {seg->line_number, seg->start, seg->end, seg->end, seg->end}, ctx->above_container });
            above_container->content_boundaries.push_back({ seg->line_number, seg->start, seg->end, seg->end, seg->end });
            above_container->parent->content_boundaries.push_back({ seg->line_number, seg->start, seg->start, seg->end, seg->end });
            seg->skip_segment = true;
        }

        // If potential list has been detected, verify if the enumeration makes sense
        if (seg->flags & LIST_OPENER && b_solved == PARTIAL) {
            if (seg->li_pre_marker == '(' && seg->li_post_marker != ')') {
                analyse_make_p(ctx, seg->start, &this_segment_end, seg);
                // TODO
                std::cout << "Make p from list" << std::endl;
            }
            else {
                /* Still need to verify if ordered list has valid enumeration
                 * Example: 'IM' is not a valid roman value */
                if ((verify_positiv_number(acc) && acc.length() < 10)
                    || validate_roman_enumeration(acc)
                    || alpha_to_decimal(acc) > 0 && acc.length() < 4) {
                    b_solved = FULL;
                    seg->li_number = acc;
                    seg->type = BLOCK_OL;
                }
                else {
                    analyse_make_p(ctx, seg->start, &this_segment_end, seg);
                    std::cout << "Make p from list" << std::endl;
                }
            }
        }

        if (seg->b_bounds.beg == seg->end && seg->flags & (QUOTE_OPENER | LIST_OPENER | H_OPENER)) {
            seg->has_content_after = false;
        }

        *end = this_segment_end;

        return ret;
    }

    // === Processing ===

    static void add_container(Context* ctx, BLOCK_TYPE block_type, const Boundaries& bounds, SegmentInfo* seg = nullptr, std::shared_ptr<BlockDetail> detail = nullptr) {
        ContainerPtr container = std::make_shared<Container>();
        container->b_type = block_type;
        container->content_boundaries.push_back(bounds);
        container->detail = detail;
        ContainerPtr parent = ctx->current_container;
        container->parent = parent;
        if (seg != nullptr) {
            container->indent = seg->indent;
            container->flag = seg->flags;
            /* This 'if' is not necessary because in the code, when block_type == BLOCK_HIDDEN we have
             * seg == nullptr, but we may put in the future other blocks with seg == nullptr */
            if (block_type != BLOCK_HIDDEN) {
                parent->last_non_empty_child_line = seg->line_number;
            }
        }
        ctx->containers.push_back(container);
        ctx->current_container = *(ctx->containers.end() - 1);
        parent->children.push_back(ctx->current_container);
    }

    /* Pass a non-null ptr and non root ptr to this function */
    static inline ContainerPtr select_parent(ContainerPtr ptr) {
        ContainerPtr parent = ptr->parent;
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
        ContainerPtr above_container = ctx->above_container;

        bool is_ul = seg->li_number.empty();
        char pre_marker = seg->li_pre_marker;
        char post_marker = seg->li_post_marker;
        ContainerPtr above_parent = (above_container != nullptr) ? above_container->parent : nullptr;
        bool is_above_ol = above_container != nullptr && above_parent->b_type == BLOCK_OL;
        bool is_above_ul = above_container != nullptr && above_parent->b_type == BLOCK_UL;

        BlockOlDetail::OL_TYPE type;
        int alpha = -1; int roman = -1;
        if (!is_ul) {
            alpha = alpha_to_decimal(seg->li_number);
            roman = roman_to_decimal(seg->li_number);
            if (verify_positiv_number(seg->li_number)) {
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
            if (seg->li_number.empty()) {
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
        else
            ctx->current_container = above_parent;

        // We can now add our list item
        auto detail = std::make_shared<BlockLiDetail>();
        if (!is_ul)
            detail->number = seg->li_number;
        add_container(ctx, BLOCK_LI, { seg->line_number, seg->b_bounds.pre, seg->b_bounds.beg, seg->end, seg->end }, seg, detail);

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
        ContainerPtr& above_container = ctx->above_container;

        bool set_above_to_nullptr = false;

        /* If the above_container doesn't match the current detected block, then we have to close
         * the current container. */
        if (above_container != nullptr) {
            int line_number_diff = seg->line_number - (above_container->content_boundaries.end() - 1)->line_number;
            if (line_number_diff > 1 || above_container->flag != seg->flags) {
                close_current_container(ctx);
                if (above_container->b_type == BLOCK_LI) {
                    close_current_container(ctx);
                }
                set_above_to_nullptr = true;
            }
        }

        /* Blank lines can depend on the future, so we have to temporarily store them */
        if (seg->blank_line) {
            ContainerPtr parent = ctx->containers[0]; // By default, blank lines belong to ROOT
            /* Blank lines should always be commited to parent above container */
            if (above_container != nullptr) {
                parent = select_parent(above_container);
            }
            ctx->current_container = parent;
            add_container(ctx, BLOCK_HIDDEN, { seg->line_number, seg->start, seg->start, seg->end, seg->end });
            close_current_container(ctx);
        }
        if (set_above_to_nullptr)
            above_container = nullptr;

#define IS_BLOCK_CONTINUED(type) (above_container != nullptr && above_container->b_type == type)

        if (seg->flags & P_OPENER) {
            if (IS_BLOCK_CONTINUED(BLOCK_P)) {
                above_container->content_boundaries.push_back({ seg->line_number, seg->start, seg->first_non_blank, seg->end, seg->end });
            }
            else {
                add_container(ctx, BLOCK_P, { seg->line_number, seg->start, seg->first_non_blank, seg->end, seg->end }, seg);
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
        else if (seg->flags & LIST_OPENER) {
            /* Lists are not trivial to handle, there are a lot of edge cases */
            make_list_item(ctx, seg, *off);
        }

        return ret;
    }

    bool parse_blocks(Context* ctx) {
        bool ret = true;

        OFFSET off = 0;

        // Add root container
        ContainerPtr doc_container = std::make_shared<Container>();
        doc_container->b_type = BLOCK_DOC;
        ctx->containers.push_back(doc_container);
        ctx->current_container = *(ctx->containers.end() - 1);

        SegmentInfo current_seg;

        int depth = -1;
        int indent = 0;
        while (off < (int)ctx->size) {
            select_last_child_container(ctx);
            CHECK_AND_RET(analyse_segment(ctx, off, &off, &current_seg));
            CHECK_AND_RET(process_segment(ctx, &off, &current_seg));

            // We arrived at a the end of a line
            if (off >= current_seg.end) {
                ctx->above_container = *ctx->containers.begin();
                ctx->current_container = ctx->above_container;
                off++;
            }
        }
        return ret;
    abort:
        return ret;
    }

    bool enter_block(Context* ctx, ContainerPtr ptr) {
        bool ret = true;
        CHECK_AND_RET(ctx->parser->enter_block(ptr->b_type, ptr->content_boundaries, ptr->detail));
        for (auto child : ptr->children) {
            CHECK_AND_RET(enter_block(ctx, child));
        }
        CHECK_AND_RET(ctx->parser->leave_block(ptr->b_type));
        return ret;
    abort:
        return ret;
    }

    bool parse_spans(Context* ctx) {
        bool ret = true;
        ret = enter_block(ctx, *(ctx->containers.begin()));

        // abort:
        //     return ret;
        return ret;
    }

    void generate_line_number_data(Context* ctx) {
        ctx->offset_to_line_number.reserve(ctx->size);
        /* The first seg always starts at 0 */
        ctx->line_number_begs.push_back(0);
        int line_counter = 0;
        for (unsigned int i = 0;i < ctx->size;i++) {
            ctx->offset_to_line_number.push_back(line_counter);
            if (ctx->text[i] == '\n') {
                line_counter++;
                if (i + 1 <= ctx->size)
                    ctx->line_number_begs.push_back(i + 1);
            }
        }
    }

    bool process_doc(Context* ctx) {
        bool ret = true;

        generate_line_number_data(ctx);

        /* First, process all the blocks that we
        * can find */
        CHECK_AND_RET(parse_blocks(ctx));
        CHECK_AND_RET(parse_spans(ctx));

    abort:
        return ret;
    }


    bool parse(const CHAR* text, SIZE size, const Parser* parser) {
        Context ctx;
        ctx.text = text;
        ctx.size = size;
        ctx.parser = parser;

        process_doc(&ctx);

        return 0;
    }
}