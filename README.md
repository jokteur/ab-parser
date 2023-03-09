This is a C++17 parser for the Markdown-like language called `Annote-Bas` (pronounced /a/n/o/t̪/ /b/a/), or `AB`. 

# Another Markdown-like parser ?

This project was created for the program [MathNotes](https://github.com/jokteur/MathNotes), which is a WYSIWIG editor for taking math notes. The idea was to take notes in Markdown with the excellent [md4c](https://github.com/mity/md4c) Markdown parser. However, despites its great performance, *md4c* only goes from raw text to tree representation: it doesn't provide any informations concerning the positions of the delimiters and text. AB-parser solves this problem
with reasonable performance.

## Why a new Markdown-like language ?
As this parser is specifically designed for the *MathNotes* program, some features from the standard markdown were dropped, and other were added. This language keeps a lot of the original `Markdown` rules, so it is still possible to read `.md` files with this parser and have decent results. Here are some key differences:

- **No lazy lines**:
  ```Markdown
  > abc
  def
  ```
  here `def` won't continue the quote
- **No ATX Style headers**:
  ATX headers are kind of confusing in a WYSWIG editor. All headers should be pre-followed with `#`
  As a consequence of the last two rules, it is allowed to defined headers on multiple lines like the following:
  ```Markdown
  ## Title
  ## is finished here
  ```
- **No HTML and no entities**:
  HTML is not considered in this parser, has it has no use for *MathNotes*.
- **Math mode is only with double $**:
  Math (LaTeX) mode is specified via the double dollar sign: `$$ f(x) = x^2$$`. Depending on the context, the Math will be inline or display.
- **Attributes**:
  It is possible to specify attributes to block and spans with the following syntax: `{{key:value}}`. This allows for the user to customise certains blocks, like
- **Special blocks**:
  To extend the language, special blocks can be create with three `:`. Here is an example that mixes attributes and special blocks:
  ```Markdown
  ::: figure {{center,ncols=2}}
      ![image1.png]
      ![image2.png]
  ```
- **`*` for `strong`, `_` for `em`**:
  No more ambiguity between the mix of `*`, `**`, `_` and `__`
- **`{x x}` syntax for spans**:
  A feature stolen from [djot](https://github.com/jgm/djot), which removes ambiguity for `em` and `strong`. It is possible to have a strong char in the middle of a word, like `wo{*o*}rd`. Other spans have added, such as highlight `{=abc=}`, underline `{+abc+}` and deleted `{-deleted-}`.
- **General references system**:
  Anything that can be labeled with the attribute `{{l:name}}` (such as figures, latex equations, tables, code blocks, citations, ...) can be referenced with the syntax `[[name]]`. Inserted references are references that insert their content at the referenced place, the syntax used is `![[name]]`. It is up to the caller to verify the correctness of the references.

The specific rules are written in [rules.ab](rules.ab).

## The problem of the marker delimitation problem
Let's say that we have the following Markdown example:

```Markdown
- >> [abc
  >> def](example.com)
```

This example would generate an abstract syntax tree (AST) like:
```
DOC
  UL
    LI
      QUOTE
        QUOTE
          P
            URL
```

How do we attribute each non-text markers (like `-`, `>`, `[`, ...) to the correct block / span ? This information is crucial for *MathNotes*, because it needs to show temporarily the marker delimitations for each block.

This parser was created to solve this specific problem, while keeping reasonable performance. To do this, each object (BLOCK or SPAN) is represented by an array of boundaries. A boundary is defined as follows:

```C++
struct Boundary {
    int line_number;
    int pre;
    int beg;
    int end;
    int post;
};
```

This struct designates offsets in the raw text which form its structure. `line_number` is the line number in the raw text on which the boundary is currently operating. Offsets between `pre` and `beg` are the  pre-delimiters, and offsets between `end` and `post` are the post-delimiters. Everything between `beg` and `end` is the content of the block / span.

Here is a simple example. Suppose we have the following text: `_italic_`, which starts at line 0 and offset 0 then the boundary struct would look like `{0, 0, 1, 7, 8}`.

Going back to the first example, we now use the following notation to illustrate ownership of markers: if there is `x`, it indicates a delimiter, if there is `_` it indicates content, and `.` indicates not in boundary. Here are the ownership for each block and span:

```Markdown
- >> [abc
  >> def](example.com)

UL:
_________
______________________

LI:
xx_______
xx____________________

QUOTE (1st):
..x______
..x___________________

QUOTE (2nd):
...xx____
...xx_________________

P:
.....____
....._________________

URL:
.....x___
.....___xxxxxxxxxxxxxx
```

So, AB-parser provides boundary information for each block. 

## Reasonable performance ?

The current code has been inspired by *md4c*. Because of my programmation lazyness, the code is a mix between C-style and C++ style which is kind of ugly. It is clearly not the fastest parser in the world, but it is sufficient for the use case of *MathNotes*. 

Here are some very rough non-scientific benchmarks, done with one core of a Ryzen 5950 and a with  an 8MB example file:
| Library  | Parsing speed      | Speed-up compared to previous line |
| ---------| ------------------ |-------------------------------|
| [Tree Sitter Markdown](https://github.com/MDeiml/tree-sitter-markdown/)  | 2.7 MB/s  | ~ |
| AB-Parser  | 35 MB/s  | 14x |
| [md4c](https://github.com/mity/md4c) | 200 MB/s | ~5.7x |

It is clear that this library can still benefit from some optimisation in the future.

### Memory consumption
Like *md4c*, this parser does not hold the whole AST in memory when parsing. It only keeps what
is necessary to infer the correct syntax from the context. Once a root block is finished (i.e. the block can't expand in the next lines), the library passes the information of the current parsed blocks to the caller. The caller may interrupt the parsing at any time, so it is possible to scan half a document and still fetch some useful information from it.

In the case of deeply nested blocks, like `>>>>>>>>>>>>>>>>>>>>>>>>>>>>...`, the library may consume more memory to keep track of the AST.

## Why `Annote-Bas` ?
Annote-bas comes from the French, and it is a (intentional) bad literal translation of Markdown.