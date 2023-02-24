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

    static const int SELECT_ALL = ~0;
    static const int SELECT_ALL_LINKS = S_LINK & S_LINKDEF & S_AUTOLINK & S_REF & S_INSERTED_REF & S_IMG;

    struct Mark {
        char open1 = 0;
        char open2 = 0;
        char open3 = 0;
        char close1 = 0;
        char close2 = 0;
        char close3 = 0;
        int repeat = 0;
        int s_type = 0;
        int dont_allow_inside = 0;
    };

    bool parse_spans(Context* ctx, ContainerPtr ptr) {
        std::cout << block_to_html(ptr->b_type) << std::endl;
        return true;
    }
}