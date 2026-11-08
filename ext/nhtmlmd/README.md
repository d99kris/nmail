nhtmlmd
=======

nhtmlmd converts html to markdown or plain text, and markdown to html. It
is tuned for email: hidden preheaders, layout tables, tracking images and
quotes are handled to give readable text. It is developed as part of
[nmail](https://github.com/d99kris/nmail) and can be built standalone.

Library Usage
=============

    #include "nhtmlmd.h"

    std::string md = Nhtmlmd::HtmlToMarkdown(html);
    std::string text = Nhtmlmd::HtmlToText(html);

    Nhtmlmd::MarkdownOptions options;
    options.m_HardBreaks = true;
    std::string html = Nhtmlmd::MarkdownToHtml(md, options);

Input shall be utf-8. Links are rendered as their text by default, see
`Nhtmlmd::HtmlOptions` for numbered references or inline links.

Utility Usage
=============

The `nhtmlmd` utility exposes the library on the command line, for testing
or for use as an html filter by other programs:

    Usage: nhtmlmd [OPTION]... [FILE]

    nhtmlmd mail.html          convert html to markdown, links as text
    nhtmlmd -t mail.html       convert html to plain text
    nhtmlmd -l refs mail.html  convert html to markdown with numbered links
    nhtmlmd -m -b reply.md     convert markdown to html as nmail sends it

It reads FILE, or stdin if FILE is omitted or `-`, and writes the result to
stdout. See `nhtmlmd --help` for all options.

Build
=====

    ./make.sh tests

or with plain cmake:

    cmake -S . -B build && cmake --build build && (cd build && ctest)

This builds the static library and the `nhtmlmd` utility (in the build
directory, not installed). System lexbor / md4c may be used instead of the
bundled copies with `-DNHTMLMD_USE_SYSTEM_LEXBOR=ON` and
`-DNHTMLMD_USE_SYSTEM_MD4C=ON`.

Tests
=====

Golden output tests in `tests/` are run by `./make.sh tests` (debug and
release) or `tests/run.sh <nhtmlmd>`, use
`tests/run.sh --update <nhtmlmd>` to regenerate expected output after an
intended change.

Third-party
===========

- [lexbor](https://github.com/lexbor/lexbor) v3.0.0 (core, dom, html,
  ns, tag), Apache-2.0, see `ext/lexbor/LICENSE` and `ext/lexbor/NOTICE`
- [md4c](https://github.com/mity/md4c) v0.6.0, MIT, see
  `ext/md4c/LICENSE.md`

Bundled sources are updated with `utils/update-ext.sh`. Source code is
formatted with `./make.sh src` (uncrustify).

License
=======

nhtmlmd is distributed under the MIT license. See LICENSE file.
