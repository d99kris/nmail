// test001.cpp - word wrap, quote prefix and trim/pad handling
//
// Copyright (c) 2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include <clocale>
#include <iostream>
#include <string>
#include <vector>

#include <wchar.h>

#include "util.h"

#include "unittest.h"

typedef std::vector<std::wstring> WLines;

static void TestGetQuotePrefix()
{
  std::wstring prefix;
  std::wstring line;

  unittest::ExpectTrue(Util::GetQuotePrefix(L"> hello", prefix, line));
  unittest::ExpectEqual(std::wstring, prefix, L"> ");
  unittest::ExpectEqual(std::wstring, line, L"hello");

  unittest::ExpectTrue(Util::GetQuotePrefix(L">> hello", prefix, line));
  unittest::ExpectEqual(std::wstring, prefix, L">> ");
  unittest::ExpectEqual(std::wstring, line, L"hello");

  unittest::ExpectTrue(Util::GetQuotePrefix(L"> > hello", prefix, line));
  unittest::ExpectEqual(std::wstring, prefix, L"> > ");
  unittest::ExpectEqual(std::wstring, line, L"hello");

  unittest::ExpectTrue(Util::GetQuotePrefix(L"  > x", prefix, line));
  unittest::ExpectEqual(std::wstring, prefix, L"  > ");
  unittest::ExpectEqual(std::wstring, line, L"x");

  unittest::ExpectTrue(Util::GetQuotePrefix(L">", prefix, line));
  unittest::ExpectEqual(std::wstring, prefix, L">");
  unittest::ExpectEqual(std::wstring, line, L"");

  unittest::ExpectFalse(Util::GetQuotePrefix(L"hello", prefix, line));
  unittest::ExpectEqual(std::wstring, prefix, L"");
  unittest::ExpectEqual(std::wstring, line, L"hello");

  unittest::ExpectFalse(Util::GetQuotePrefix(L"", prefix, line));
  unittest::ExpectEqual(std::wstring, prefix, L"");
  unittest::ExpectEqual(std::wstring, line, L"");
}

static void TestWordWrap()
{
  // plain text wraps at spaces
  const WLines plain = { L"the quick brown fox", L"jumps over the lazy", L"dog" };
  unittest::ExpectEqual(WLines,
                        Util::WordWrap(L"the quick brown fox jumps over the lazy dog", 20,
                                          false, false, false, 2), plain);

  // text without spaces is hard wrapped
  const WLines longWord = { L"abcdefghij", L"klmnopqrst", L"uvwxyz" };
  unittest::ExpectEqual(WLines,
                        Util::WordWrap(L"abcdefghijklmnopqrstuvwxyz", 10,
                                          false, false, false, 2), longWord);

  // empty text and empty lines
  unittest::ExpectEqual(WLines, Util::WordWrap(L"", 20, false, false, false, 2), WLines());
  const WLines emptyLines = { L"a", L"", L"b" };
  unittest::ExpectEqual(WLines, Util::WordWrap(L"a\n\nb", 20, false, false, false, 2), emptyLines);

  // tabs are expanded to next tab stop
  const WLines tabs = { L"ab   cd  e" };
  unittest::ExpectEqual(WLines, Util::WordWrap(L"ab\tcd\te", 40, false, false, false, 4), tabs);

  // format flowed input is reflowed before wrapping
  const WLines flowed = { L"one two three four", L"five" };
  unittest::ExpectEqual(WLines,
                        Util::WordWrap(L"one two \nthree four\nfive", 20,
                                          true, false, false, 2), flowed);
}

static void TestWordWrapQuoted()
{
  // quote prefix is repeated on each wrapped line
  const WLines quoted = { L"> the quick brown fox jumps over the", L"> lazy dog" };
  unittest::ExpectEqual(WLines,
                        Util::WordWrap(L"> the quick brown fox jumps over the lazy dog", 40,
                                          false, false, true, 2), quoted);

  // without quote wrap the prefix is treated as regular text
  const WLines unquoted = { L"> the quick brown fox jumps over the", L"lazy dog" };
  unittest::ExpectEqual(WLines,
                        Util::WordWrap(L"> the quick brown fox jumps over the lazy dog", 40,
                                          false, false, false, 2), unquoted);

  // nested quote prefix is normalized and repeated
  const WLines nested = { L">> deeply quoted text that", L">> needs wrapping here" };
  unittest::ExpectEqual(WLines,
                        Util::WordWrap(L"> > deeply quoted text that needs wrapping here", 30,
                                          false, false, true, 2), nested);

  // quoted text without spaces must hard wrap, and not wrap at the prefix space
  // (which previously caused an infinite loop, hanging export)
  const WLines quotedUrl =
  {
    L"> https://open.spotify.com/artist/1NwpmqCXeqQ4xrE6WEP4np?si=abcdefgh1234",
    L"> 5678"
  };
  unittest::ExpectEqual(WLines,
                        Util::WordWrap(
                          L"> https://open.spotify.com/artist/1NwpmqCXeqQ4xrE6WEP4np?si=abcdefgh12345678", 72,
                          false, false, true, 2), quotedUrl);
}

static void TestWordWrapPos()
{
  // position 25 of the input is on wrapped line 1, at position 5
  int wrapLine = -1;
  int wrapPos = -1;
  const WLines lines = { L"the quick brown fox", L"jumps over the lazy", L"dog" };
  unittest::ExpectEqual(WLines,
                        Util::WordWrap(L"the quick brown fox jumps over the lazy dog", 20,
                                          false, false, false, 2, 25, wrapLine, wrapPos), lines);
  unittest::ExpectEqual(int, wrapLine, 1);
  unittest::ExpectEqual(int, wrapPos, 5);
}

static void TestClusters()
{
  // emoji sequences are kept whole: base + variation selector, skin tone,
  // zero width joiner sequence, keycap and flag (regional indicator pair)
  const std::wstring heart = L"\u2764\uFE0F";
  const std::wstring biker = L"\U0001F6B4\U0001F3FF\u200D\u2642\uFE0F";
  const std::wstring keycap = L"1\uFE0F\u20E3";
  const std::wstring flag = L"\U0001F1F8\U0001F1EA";
  unittest::ExpectEqual(size_t, Util::GetClusterEnd(heart + L"x", 0), heart.size());
  unittest::ExpectEqual(size_t, Util::GetClusterEnd(biker + L"x", 0), biker.size());
  unittest::ExpectEqual(size_t, Util::GetClusterEnd(keycap + L"x", 0), keycap.size());
  unittest::ExpectEqual(size_t, Util::GetClusterEnd(flag + flag, 0), flag.size());
  unittest::ExpectEqual(size_t, Util::GetClusterEnd(L"ab", 0), (size_t)1);
  unittest::ExpectEqual(size_t, Util::GetClusterEnd(L"ab", 2), (size_t)2);

  // trimming never leaves a base character without its variation selector
  const int heartWidth = Util::WStringWidth(heart);
  unittest::ExpectEqual(std::wstring, Util::TrimWString(L"Bob" + heart, 3 + heartWidth), L"Bob" + heart);
  unittest::ExpectEqual(std::wstring, Util::TrimWString(L"Bob" + heart, 3), L"Bob");
  unittest::ExpectEqual(std::wstring, Util::TrimWString(biker + L" Bike", 1), L"");

  // trim/pad always returns the requested width
  unittest::ExpectEqual(std::wstring, Util::TrimPadWString(L"Bob", 5), L"Bob  ");
  unittest::ExpectEqual(std::wstring, Util::TrimPadWString(L"Bob" + heart, 3), L"Bob");
  unittest::ExpectEqual(int, Util::WStringWidth(Util::TrimPadWString(biker + L" Bike Group", 6)), 6);
  unittest::ExpectEqual(int, Util::WStringWidth(Util::TrimPadWString(L"\u4F60\u597D", 3)), 3);
}

int main()
{
  int rv = 0;

  setlocale(LC_ALL, "");

  try
  {
    TestGetQuotePrefix();
    TestWordWrap();
    TestWordWrapQuoted();
    TestWordWrapPos();

    // wide character width depends on a utf-8 capable locale being present
    if (wcwidth(L'你') == 2)
    {
      TestClusters();
    }
    else
    {
      std::cout << "skipping wide character tests, no utf-8 locale" << std::endl;
    }
  }
  catch (const std::exception& ex)
  {
    std::cout << "exception: " << ex.what() << std::endl;
    rv = 1;
  }

  return rv;
}
