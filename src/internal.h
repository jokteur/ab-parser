#pragma once
#include <string.h>
#include <memory>

#include "definitions.h"
#include "profiling.h"
#include <iostream>

namespace AB {
      /*****************
       ***  Helpers  ***
       *****************/

       /* Character accessors. */
#define CH(off)                 (ctx->text[(off)])
#define STR(off)                (ctx->text + (off))

     /* Character classification.
      * Note we assume ASCII compatibility of code points < 128 here. */
#define _T(t) t
#define ISIN_(ch, ch_min, ch_max)       ((ch_min) <= (unsigned)(ch) && (unsigned)(ch) <= (ch_max))
#define ISANYOF_(ch, palette)           ((ch) != _T('\0')  &&  strchr((palette), (ch)) != NULL)
#define ISANYOF2_(ch, ch1, ch2)         ((ch) == (ch1) || (ch) == (ch2))
#define ISANYOF3_(ch, ch1, ch2, ch3)    ((ch) == (ch1) || (ch) == (ch2) || (ch) == (ch3))
#define ISASCII_(ch)                    ((unsigned)(ch) <= 127)
#define ISBLANK_(ch)                    (ISANYOF2_((ch), _T(' '), _T('\t')))
#define ISNEWLINE_(ch)                  (ISANYOF2_((ch), _T('\r'), _T('\n')))
#define ISWHITESPACE_(ch)               (ISBLANK_(ch) || ISANYOF2_((ch), _T('\v'), _T('\f')))
#define ISCNTRL_(ch)                    ((unsigned)(ch) <= 31 || (unsigned)(ch) == 127)
#define ISPUNCT_(ch)                    (ISIN_(ch, 33, 47) || ISIN_(ch, 58, 64) || ISIN_(ch, 91, 96) || ISIN_(ch, 123, 126))
#define ISUPPER_(ch)                    (ISIN_(ch, _T('A'), _T('Z')))
#define ISLOWER_(ch)                    (ISIN_(ch, _T('a'), _T('z')))
#define ISALPHA_(ch)                    (ISUPPER_(ch) || ISLOWER_(ch))
#define ISDIGIT_(ch)                    (ISIN_(ch, _T('0'), _T('9')))
#define ISXDIGIT_(ch)                   (ISDIGIT_(ch) || ISIN_(ch, _T('A'), _T('F')) || ISIN_(ch, _T('a'), _T('f')))
#define ISALNUM_(ch)                    (ISALPHA_(ch) || ISDIGIT_(ch))

#define ISANYOF(off, palette)           ISANYOF_(CH(off), (palette))
#define ISANYOF2(off, ch1, ch2)         ISANYOF2_(CH(off), (ch1), (ch2))
#define ISANYOF3(off, ch1, ch2, ch3)    ISANYOF3_(CH(off), (ch1), (ch2), (ch3))
#define ISASCII(off)                    ISASCII_(CH(off))
#define ISBLANK(off)                    ISBLANK_(CH(off))
#define ISNEWLINE(off)                  ISNEWLINE_(CH(off))
#define ISWHITESPACE(off)               ISWHITESPACE_(CH(off))
#define ISCNTRL(off)                    ISCNTRL_(CH(off))
#define ISPUNCT(off)                    ISPUNCT_(CH(off))
#define ISUPPER(off)                    ISUPPER_(CH(off))
#define ISLOWER(off)                    ISLOWER_(CH(off))
#define ISALPHA(off)                    ISALPHA_(CH(off))
#define ISDIGIT(off)                    ISDIGIT_(CH(off))
#define ISXDIGIT(off)                   ISXDIGIT_(CH(off))
#define ISALNUM(off)                    ISALNUM_(CH(off))  


#define NEXT_LOOP() off++;
#define CHECK_WS_OR_END(off) ((off) >= (int)ctx->size || ((off) < (int)ctx->size && ISWHITESPACE((off))) || CH((off)) == '\n')
#define CHECK_WS_BEFORE(off) (seg->first_non_blank >= (off))
#define CHECK_SPACE_AFTER(off) (off < (OFFSET)ctx->size && CH(off + 1) == ' ')

      /* Parsing functions*/
#define CHECK_AND_RET(fct) \
    ret = (fct); \
    if (!ret) \
        goto abort;
#define ENTER_BLOCK(type, arg) ctx->parser.enter_block((type), (arg));
#define LEAVE_BLOCK() ctx->parser.leave_block();
#define ENTER_SPAN(type, arg) ctx->parser.enter_span((type), (arg));
#define LEAVE_SPAN() ctx->parser.leave_span();
#define TEXT(type, begin, end) ctx->parser.text((type), (arg));


      /* For blocks that require fencing, e.g.
       * ```
       * Code block
       * ```
       */
      struct RepeatedMarker {
            char marker = 0;
            int count = 0;
            bool allow_greater_number = false;
            bool allow_chars_before_closing = false;
            bool allow_attributes = true;
      };

      struct Container;
      typedef std::shared_ptr<Container> ContainerPtr;
      struct Container {
            bool closed = false;
            bool erase_block = false;
            RepeatedMarker repeated_markers;

            Attributes attributes;

            BLOCK_TYPE b_type;
            std::shared_ptr<BlockDetail> detail;
            Container* parent = nullptr;
            std::vector<Container*> children;
            std::vector<Boundaries> content_boundaries;
            int last_non_empty_child_line = -1;
            int flag = 0;
            int indent = 0;

            static int alloc_count;

            void* operator new(std::size_t count) {
                  auto ptr = malloc(count);
                  TracyAlloc(ptr, count);
                  return ptr;
            }
            void operator delete (void* ptr) noexcept {
                  TracyFree(ptr);
                  free(ptr);
            }
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

            std::vector<Container*> containers;
            /* This allows us to reuse previously allocated
             * memory */
            std::vector<Container*>::iterator last_free_mem_it;
            Container* current_container;
            Container* above_container = nullptr;

            std::vector<int> offset_to_line_number;
            std::vector<int> line_number_begs;
      };

}
