// test003.cpp - string folding, filename sanitizing and command argument handling
//
// Copyright (c) 2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include <clocale>
#include <iostream>
#include <string>
#include <vector>

#include "extutil.h"
#include "fileutil.h"
#include "strutil.h"

#include "unittest.h"

static void TestToFold()
{
  // ascii is lowercased
  unittest::ExpectEqual(std::string, StrUtil::ToFold("Hello World 123"), "hello world 123");
  unittest::ExpectEqual(std::string, StrUtil::ToFold(""), "");

  // accented latin letters are folded to their lowercase base letter(s)
  unittest::ExpectEqual(std::string, StrUtil::ToFold("Ärende"), "arende");
  unittest::ExpectEqual(std::string, StrUtil::ToFold("Café"), "cafe");
  unittest::ExpectEqual(std::string, StrUtil::ToFold("ÅÄÖ åäö"), "aao aao");
  unittest::ExpectEqual(std::string, StrUtil::ToFold("Straße"), "strasse");
  unittest::ExpectEqual(std::string, StrUtil::ToFold("Æble Œuvre"), "aeble oeuvre");
  unittest::ExpectEqual(std::string, StrUtil::ToFold("Łódź"), "lodz");

  // the two math symbols in the folded range are left as-is
  unittest::ExpectEqual(std::string, StrUtil::ToFold("2×3÷1"), "2×3÷1");

  // other scripts are not base folded
  unittest::ExpectEqual(std::string, StrUtil::ToFold("你好"), "你好");
}

static void TestToFoldPositions()
{
  // positions refer to the source char of each folded char, also when folding changes length
  std::vector<size_t> positions;
  unittest::ExpectEqual(std::wstring, StrUtil::ToFold(L"aßB", &positions), L"assb");
  const std::vector<size_t> expected = { 0, 1, 1, 2 };
  unittest::ExpectTrue(positions == expected);

  std::vector<size_t> noPositions;
  unittest::ExpectEqual(std::wstring, StrUtil::ToFold(L"", &noPositions), L"");
  unittest::ExpectTrue(noPositions.empty());
}

static void TestSanitizeFilename()
{
  // regular names are left as-is
  unittest::ExpectEqual(std::string, FileUtil::SanitizeFilename("report 2026.pdf"), "report 2026.pdf");
  unittest::ExpectEqual(std::string, FileUtil::SanitizeFilename("Übung.txt"), "Übung.txt");
  unittest::ExpectEqual(std::string, FileUtil::SanitizeFilename("$(touch x).txt"), "$(touch x).txt");

  // only the last path component is kept
  unittest::ExpectEqual(std::string, FileUtil::SanitizeFilename("../../x.txt"), "x.txt");
  unittest::ExpectEqual(std::string, FileUtil::SanitizeFilename("/etc/passwd"), "passwd");
  unittest::ExpectEqual(std::string, FileUtil::SanitizeFilename("C:\\dir\\f.txt"), "f.txt");

  // control chars are removed
  unittest::ExpectEqual(std::string, FileUtil::SanitizeFilename("a\x01" "b\n.txt"), "ab.txt");

  // names not usable are emptied
  unittest::ExpectEqual(std::string, FileUtil::SanitizeFilename(""), "");
  unittest::ExpectEqual(std::string, FileUtil::SanitizeFilename("."), "");
  unittest::ExpectEqual(std::string, FileUtil::SanitizeFilename(".."), "");
  unittest::ExpectEqual(std::string, FileUtil::SanitizeFilename("dir/"), "");
  unittest::ExpectEqual(std::string, FileUtil::SanitizeFilename("../.."), "");
}

static void TestMakeCmd()
{
  unittest::ExpectEqual(std::string, ExtUtil::EscapeSingleQuote("plain"), "plain");
  unittest::ExpectEqual(std::string, ExtUtil::EscapeSingleQuote("it's"), "it'\\''s");

  // argument is added last when the command has no placeholder
  unittest::ExpectEqual(std::string, ExtUtil::MakeCmd("open", "/tmp/a b.txt"), "open '/tmp/a b.txt'");

  // placeholder is replaced, whether bare or quoted in the command
  unittest::ExpectEqual(std::string, ExtUtil::MakeCmd("open '%1'", "/tmp/a.txt"), "open '/tmp/a.txt'");
  unittest::ExpectEqual(std::string, ExtUtil::MakeCmd("open \"%1\"", "/tmp/a.txt"), "open '/tmp/a.txt'");
  unittest::ExpectEqual(std::string, ExtUtil::MakeCmd("cat %1 | less", "/tmp/a.txt"), "cat '/tmp/a.txt' | less");
  unittest::ExpectEqual(std::string, ExtUtil::MakeCmd("xdg-open '%1' >/dev/null 2>&1", "/tmp/a.txt"),
                        "xdg-open '/tmp/a.txt' >/dev/null 2>&1");

  // argument is always passed single-quoted, with any single quote escaped
  unittest::ExpectEqual(std::string, ExtUtil::MakeCmd("open", "$(touch x) `y` \"z\""),
                        "open '$(touch x) `y` \"z\"'");
  unittest::ExpectEqual(std::string, ExtUtil::MakeCmd("open \"%1\"", "it's"), "open 'it'\\''s'");

  // placeholder text in the argument is not expanded
  unittest::ExpectEqual(std::string, ExtUtil::MakeCmd("open", "a%1b"), "open 'a%1b'");
}

int main()
{
  int rv = 0;

  setlocale(LC_ALL, "");

  try
  {
    TestToFold();
    TestToFoldPositions();
    TestSanitizeFilename();
    TestMakeCmd();
  }
  catch (const std::exception& ex)
  {
    std::cout << "exception: " << ex.what() << std::endl;
    rv = 1;
  }

  return rv;
}
