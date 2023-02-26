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
#include "internal.h"
#include "definitions.h"
#include "parse_blocks.h"
#include "parse_spans.h"
#include "parse_commons.h"
#include "helpers.h"

#include <iostream>
#include <memory>

namespace AB {
    bool enter_block(Context* ctx, ContainerPtr ptr) {
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

    bool send_blocks(Context* ctx) {
        bool ret = true;
        ret = enter_block(ctx, *(ctx->containers.begin()));

        // abort:
        //     return ret;
        return ret;
    }

    void generate_line_number_data(Context* ctx) {
        ctx->offset_to_line_number.reserve(ctx->size + 1);
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
        ctx->offset_to_line_number.push_back(line_counter);
    }

    bool process_doc(Context* ctx) {
        bool ret = true;

        generate_line_number_data(ctx);

        /* First, process all the blocks that we
        * can find */
        CHECK_AND_RET(parse_blocks(ctx));
        CHECK_AND_RET(send_blocks(ctx));

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