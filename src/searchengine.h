// searchengine.h
//
// Copyright (c) 2020-2021 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <xapian.h>

class SearchEngine
{
public:
  explicit SearchEngine(const std::string& p_DbPath);
  virtual ~SearchEngine();

  void Index(const std::string& p_DocId, const int64_t p_Time, const std::vector<std::string>& p_Strs);
  void Remove(const std::string& p_DocId);
  void Commit();

  std::vector<std::string> Search(const std::string& p_QueryStr, const unsigned p_Offset,
                                  const unsigned p_Max, bool& p_HasMore);
  std::vector<std::string> List();
  bool Exists(const std::string& p_DocId);

  static std::string GetXapianVersion();

private:
  std::string m_DbPath;
  std::unique_ptr<Xapian::Database> m_Database;
  std::unique_ptr<Xapian::WritableDatabase> m_WritableDatabase;
  std::mutex m_DatabaseMutex;
  std::mutex m_WritableDatabaseMutex;
  const Xapian::valueno m_DateSlot = 1;
};
