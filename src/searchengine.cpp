// searchengine.cpp
//
// Copyright (c) 2020 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "searchengine.h"

#include "loghelp.h"

SearchEngine::SearchEngine(const std::string& p_DbPath)
  : m_DbPath(p_DbPath)
{
  m_WritableDatabase.reset(new Xapian::WritableDatabase(m_DbPath, Xapian::DB_CREATE_OR_OPEN));
  m_Database.reset(new Xapian::Database(m_DbPath, Xapian::DB_CREATE_OR_OPEN));
}

SearchEngine::~SearchEngine()
{
}

void SearchEngine::Index(const std::string& p_DocId, const int64_t p_Time, const std::vector<std::string>& p_Strs)
{
  Xapian::TermGenerator termGenerator;
  termGenerator.set_stemmer(Xapian::Stem("none")); // @todo: add natural language detection

  Xapian::Document doc;
  termGenerator.set_document(doc);

  for (size_t i = 0; i < p_Strs.size(); ++i)
  {
    if (i > 0)
    {
      termGenerator.increase_termpos();
    }

    termGenerator.index_text(p_Strs.at(i));
  }

  doc.set_data(p_DocId);
  doc.add_boolean_term(p_DocId);
  doc.add_value(m_DateSlot, Xapian::sortable_serialise((double)p_Time));

  std::lock_guard<std::mutex> writableDatabaseLock(m_WritableDatabaseMutex);
  m_WritableDatabase->replace_document(p_DocId, doc);
}

void SearchEngine::Remove(const std::string& p_DocId)
{
  std::lock_guard<std::mutex> writableDatabaseLock(m_WritableDatabaseMutex);
  m_WritableDatabase->delete_document(p_DocId);
}

void SearchEngine::Commit()
{
  std::lock_guard<std::mutex> writableDatabaseLock(m_WritableDatabaseMutex);
  m_WritableDatabase->commit();
}

std::vector<std::string> SearchEngine::Search(const std::string& p_QueryStr, const unsigned p_Offset, const unsigned p_Max, bool& p_HasMore)
{
  std::vector<std::string> docIds;

  try
  {
    Xapian::QueryParser queryParser;
    queryParser.set_stemmer(Xapian::Stem("none")); // @todo: add natural language detection
    queryParser.set_default_op(Xapian::Query::op::OP_AND);

    Xapian::Query query = queryParser.parse_query(p_QueryStr);

    std::lock_guard<std::mutex> DatabaseLock(m_DatabaseMutex);
    m_Database->reopen();
    Xapian::Enquire enquire(*m_Database);
    enquire.set_query(query);
    enquire.set_sort_by_value(m_DateSlot, true /* reverse */);

    p_HasMore = false;
    size_t cnt = 0;
    Xapian::MSet mset = enquire.get_mset(p_Offset, p_Max + 1);
    for (Xapian::MSetIterator it = mset.begin(); it != mset.end(); ++it, ++cnt)
    {
      if (cnt >= p_Max)
      {
        p_HasMore = true;
        break;
      }

      Xapian::Document doc = m_Database->get_document(*it);
      docIds.push_back(doc.get_data());
    }
  }
  catch (const Xapian::QueryParserError& queryParserError)
  {
    const std::string& msg = queryParserError.get_msg();
    LOG_WARNING("query parser error \"%s\"", msg.c_str());
  }

  return docIds;
}

std::vector<std::string> SearchEngine::List()
{
  std::lock_guard<std::mutex> DatabaseLock(m_DatabaseMutex);
  m_Database->reopen();
  std::vector<std::string> docIds;
  for (Xapian::PostingIterator it = m_Database->postlist_begin("");
       it != m_Database->postlist_end(""); ++it)
  {
    Xapian::Document doc = m_Database->get_document(*it);
    docIds.push_back(doc.get_data());
  }

  return docIds;
}

bool SearchEngine::Exists(const std::string& p_DocId)
{
  std::lock_guard<std::mutex> DatabaseLock(m_DatabaseMutex);
  m_Database->reopen();
  return (m_Database->postlist_begin(p_DocId) != m_Database->postlist_end(p_DocId));
}

std::string SearchEngine::GetXapianVersion()
{
  return std::string(XAPIAN_VERSION);
}
