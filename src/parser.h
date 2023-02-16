#pragma once
#include <string>
#include <functional>
#include <memory>
#include <vector>

#include "definitions.h"
#include "helpers.h"


// Implementation is inspired from http://github.com/mity/md4c
namespace AB {
    struct Boundaries {
        OFFSET line_number;
        OFFSET pre = 0;
        OFFSET beg = 0;
        OFFSET end = 0;
        OFFSET post = 0;
    };

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

    bool parse(const CHAR* text, SIZE size, const Parser* parser);
};