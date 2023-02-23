#pragma once
#include "internal.h"
#include "definitions.h"

namespace AB {
    /**
     * Parses the attributes in the form of `{key1, key2=val2, key3:val3}`
     * Whitespaces are ignored in keys but not in values
     *
     * Returns an empty Attributes if syntax not correct, e.g. `{key1` or `{:}`
    */
    Attributes parse_attributes(Context* ctx, OFFSET* off);

    /**
     * Skips until the first non-whitespace character or end of line
    */
    void skip_whitespace(Context* ctx, OFFSET* off);

    /**
     * Counts the number of repeating marks (also advances the cursor)
    */
    int count_marks(Context* ctx, OFFSET* off, char mark);

    /**
     * Counts the number of repeating marks (doesn't advance the cursor)
    */
    int count_marks(Context* ctx, OFFSET off, char mark);

    /**
     * Advances the cursor until the character is found or end of line
     * Accumulates the characters in acc
    */
    bool advance_until(Context* ctx, OFFSET* off, std::string& acc, char ch);

    /* Returns true if the block type is a leaf (meaning it contains text)*/
    bool is_leaf_block(BLOCK_TYPE b_type);
}