#pragma once

#include <unordered_map>
#include <vector>
#include <functional>
#include <string>
#include <memory>

namespace AB {
    typedef unsigned int SIZE;
    typedef int OFFSET;
    typedef char CHAR;

#define RECURSE_LIMIT 32

    /* A block represents a part of the herarchy like a paragraph
     * or a list
     */
    enum BLOCK_TYPE {
        BLOCK_DOC = 0,

        BLOCK_HIDDEN,

        BLOCK_QUOTE,

        BLOCK_UL,
        BLOCK_OL,
        BLOCK_LI,

        BLOCK_HR,
        BLOCK_H,

        BLOCK_SPECIAL,
        BLOCK_DIV,
        BLOCK_DEF,
        BLOCK_LATEX,

        BLOCK_CODE,
        BLOCK_P,

        BLOCK_TABLE,
        BLOCK_THEAD,
        BLOCK_TBODY,
        BLOCK_TR,
        BLOCK_TH,
        BLOCK_TD,

        BLOCK_EMPTY /* Empty block is for internal trickery, not display for outside use*/
    };
    /*
     * A sequence of spans constitute a block
     * It is generally inline
     */
    enum SPAN_TYPE {
        SPAN_EM,
        SPAN_STRONG,
        SPAN_A,
        SPAN_IMG,
        SPAN_CODE,
        SPAN_LATEXMATH,
        SPAN_LATEXMATH_DISPLAY,
        SPAN_WIKILINK,
        SPAN_SUP,
        SPAN_SUB,
        SPAN_U,
        SPAN_DEL,
        SPAN_HIGHLIGHT
    };

    enum TEXT_TYPE {
        TEXT_NORMAL = 0,

        TEXT_LATEX,

        /* Everything that is not considered content by markdown and is used to give information
         * about a block, i.e. delimitation markers, whitespace, special chars TEXT_BLOCK_MARKER_HIDDEN
         */
        TEXT_BLOCK_MARKER_HIDDEN,
        /* Everything that is not considered content by markdown and is used to give information
         * about a span, i.e. delimitation markers is TEXT_SPAN_MARKER_HIDDEN
         */
        TEXT_SPAN_MARKER_HIDDEN,
        /* Ascii chars can be escaped with \ */
        TEXT_ESCAPE_CHAR_HIDDEN
    };

    const char* block_to_name(BLOCK_TYPE type);
    const char* block_to_html(BLOCK_TYPE type);
    const char* span_to_name(SPAN_TYPE type);
    const char* text_to_name(TEXT_TYPE type);



    /* ======================
     * Block and span details
     * ====================== */

    struct Boundaries {
        OFFSET line_number;
        OFFSET pre = 0;
        OFFSET beg = 0;
        OFFSET end = 0;
        OFFSET post = 0;
    };

    typedef std::unordered_map<std::string, std::string> Attributes;

    struct BlockDetail {
        std::vector<Boundaries> bounds;
    };
    typedef std::shared_ptr<BlockDetail> BlockDetailPtr;

    struct BlockCodeDetail: public BlockDetail {
        std::string lang;
        int num_ticks;
    };

    struct BlockOlDetail: public BlockDetail {
        enum OL_TYPE { OL_NUMERIC, OL_ALPHABETIC, OL_ROMAN };
        char pre_marker;
        char post_marker;
        bool lower_case;
        OL_TYPE type;
    };

    struct BlockUlDetail: public BlockDetail {
        char marker;
    };

    struct BlockLiDetail: public BlockDetail {
        enum TASK_STATE { TASK_EMPTY, TASK_FAIL, TASK_SUCCESS };
        bool is_task;
        std::string number;
        TASK_STATE task_state;
        int level = 0;
    };

    struct BlockDefDetail: public BlockDetail {
        enum DEF_TYPE { DEF_FOOTNOTE, DEF_CITATION, DEF_LINK };
        std::string name;
        DEF_TYPE definition_type;
    };

    struct BlockHDetail: public BlockDetail {
        unsigned char level; /* Header level (1 to 6) */
    };
    // TODO, tables

    struct SpanDetail {
        /* A span can have pre and post information (before text begins)
         * E.g. [foo](my title) is an URL, where pre->start contains [
         * and end->post contains ](my title)
         */
        Boundaries bounds;
    };
    struct SpanADetail: public SpanDetail {
        std::string href;
        std::string title;
    };
    struct SpanImgDetail: public SpanDetail {
        std::string src;
        std::string title;
    };
    struct SpanWikiLink: public SpanDetail {
        std::string target;
    };


    /* =================
     * Parsing functions
     * ================= */

    typedef std::function<bool(BLOCK_TYPE type, const std::vector<Boundaries>& bounds, std::shared_ptr<BlockDetail> detail)> BlockFct;
    typedef std::function<bool(BLOCK_TYPE type)> LeaveBlockFct;
    typedef std::function<bool(SPAN_TYPE type, const Boundaries& bounds, std::shared_ptr<SpanDetail> detail)> SpanFct;
    typedef std::function<bool(SPAN_TYPE type)> LeaveSpanFct;
    typedef std::function<bool(TEXT_TYPE type, const OFFSET begin, const OFFSET end)> TextFct;

    struct Parser {
        BlockFct enter_block;
        LeaveBlockFct leave_block;
        SpanFct enter_span;
        LeaveSpanFct leave_span;
        TextFct text;
    };

}