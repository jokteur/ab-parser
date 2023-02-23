#include "parse_spans.h"
#include <iostream>

namespace AB {
    bool parse_spans(Context* ctx, ContainerPtr ptr) {
        std::cout << block_to_html(ptr->b_type) << std::endl;
        return true;
    }
}