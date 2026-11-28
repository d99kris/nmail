// uiexternal.cpp
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "ui.h"

#include <cstdint>

#include "extutil.h"
#include "fileutil.h"
#include "loghelp.h"
#include "sysutil.h"

void Ui::ExtEditor(const std::string& p_EditorCmd, std::wstring& p_ComposeMessageStr, int& p_ComposeMessagePos)
{
  TerminalControlPause();
  const std::string& tempPath = FileUtil::GetTempFilename(".txt");
  FileUtil::WriteWFile(tempPath, p_ComposeMessageStr);
  const std::string& cmd = ExtUtil::MakeCmd(p_EditorCmd, tempPath);
  LOG_DEBUG("launching external editor: %s", cmd.c_str());
  int rv = SysUtil::System(cmd);
  if (rv == 0)
  {
    LOG_DEBUG("external editor exited successfully");
    p_ComposeMessageStr = FileUtil::ReadWFile(tempPath);
    p_ComposeMessagePos = 0;
  }
  else
  {
    LOG_WARNING("external editor exited with %d", rv);
    SysUtil::DetectCommandNotPresent(cmd);
  }

  FileUtil::DeleteFile(tempPath);
  TerminalControlResume();
}

void Ui::ExtPager()
{
  TerminalControlPause();
  const std::string& tempPath = FileUtil::GetTempFilename(".txt");
  FileUtil::WriteFile(tempPath, m_CurrentMessageViewText);
  const std::string& pager = ExtUtil::GetPagerCmd();
  const std::string& cmd = ExtUtil::MakeCmd(pager, tempPath);
  LOG_DEBUG("launching external pager: %s", cmd.c_str());
  int rv = SysUtil::System(cmd);
  if (rv == 0)
  {
    LOG_DEBUG("external pager exited successfully");
  }
  else
  {
    LOG_WARNING("external pager exited with %d", rv);
    SysUtil::DetectCommandNotPresent(cmd);
  }

  FileUtil::DeleteFile(tempPath);
  TerminalControlResume();
}

int Ui::ExtPartsViewer(const std::string& p_Path)
{
  const bool isDefaultPartsViewerCmd = ExtUtil::IsDefaultPartsViewerCmd();
  if (!isDefaultPartsViewerCmd)
  {
    TerminalControlPause();
  }

  const std::string& viewer = ExtUtil::GetPartsViewerCmd();

  const std::string& cmd = ExtUtil::MakeCmd(viewer, p_Path);
  LOG_DEBUG("launching external viewer: %s", cmd.c_str());
  int rv = SysUtil::System(cmd);
  if (rv == 0)
  {
    LOG_DEBUG("external viewer exited successfully");
  }
  else
  {
    LOG_WARNING("external viewer exited with %d", rv);
    SysUtil::DetectCommandNotPresent(cmd);
  }

  if (!isDefaultPartsViewerCmd)
  {
    TerminalControlResume();
  }

  return rv;
}

void Ui::ExtHtmlViewer()
{
  static const std::string& tempPath = FileUtil::GetTempDir() + std::string("htmlview/tmp.html");
  FileUtil::DeleteFile(tempPath);

  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    const std::string& folder = m_CurrentFolderUid.first;
    const int uid = m_CurrentFolderUid.second;
    std::map<uint32_t, Body>& bodys = m_Bodys[folder];
    std::map<uint32_t, Body>::iterator bodyIt = bodys.find(uid);
    if (bodyIt != bodys.end())
    {
      Body& body = bodyIt->second;
      const std::string& html = body.GetHtml(); // falls back to text/plain if no html
      FileUtil::WriteFile(tempPath, html);
    }
  }

  if (FileUtil::Exists(tempPath))
  {
    int rv = ExtHtmlViewer(tempPath);
    if (rv == 0)
    {
      MarkSeen();
    }
  }
  else
  {
    SetDialogMessage("View html failed (message not available)", true /* p_Warn */);
  }
}

int Ui::ExtHtmlViewer(const std::string& p_Path)
{
  const bool isDefaultHtmlViewerCmd = ExtUtil::IsHtmlViewerSystemOpenCmd();
  if (!isDefaultHtmlViewerCmd)
  {
    TerminalControlPause();
  }

  const std::string& viewer = ExtUtil::GetHtmlViewerCmd();
  const std::string& cmd = ExtUtil::MakeCmd(viewer, p_Path);
  LOG_DEBUG("launching html viewer: %s", cmd.c_str());
  int rv = SysUtil::System(cmd);
  if (rv == 0)
  {
    LOG_DEBUG("html viewer exited successfully");
  }
  else
  {
    LOG_WARNING("html viewer exited with %d", rv);
    SysUtil::DetectCommandNotPresent(cmd);
  }

  if (!isDefaultHtmlViewerCmd)
  {
    TerminalControlResume();
  }

  return rv;
}

int Ui::ExtHtmlPreview(const std::string& p_Path)
{
  const bool isDefaultHtmlPreviewCmd = ExtUtil::IsDefaultHtmlPreviewCmd();
  if (!isDefaultHtmlPreviewCmd)
  {
    TerminalControlPause();
  }

  const std::string& viewer = ExtUtil::GetHtmlPreviewCmd();
  const std::string& cmd = ExtUtil::MakeCmd(viewer, p_Path);
  LOG_DEBUG("launching html viewer: %s", cmd.c_str());
  int rv = SysUtil::System(cmd);
  if (rv == 0)
  {
    LOG_DEBUG("html viewer exited successfully");
  }
  else
  {
    LOG_WARNING("html viewer exited with %d", rv);
    SysUtil::DetectCommandNotPresent(cmd);
  }

  if (!isDefaultHtmlPreviewCmd)
  {
    TerminalControlResume();
  }

  return rv;
}

void Ui::ExtMsgViewer()
{
  static const std::string& tempPath = FileUtil::GetTempDir() + std::string("msgview/tmp.eml");
  FileUtil::DeleteFile(tempPath);

  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    const std::string& folder = m_CurrentFolderUid.first;
    const int uid = m_CurrentFolderUid.second;
    std::map<uint32_t, Body>& bodys = m_Bodys[folder];
    std::map<uint32_t, Body>::iterator bodyIt = bodys.find(uid);
    if (bodyIt != bodys.end())
    {
      Body& body = bodyIt->second;
      const std::string& data = body.GetData(); // falls back to text/plain if no html
      FileUtil::WriteFile(tempPath, data);
    }
  }

  if (FileUtil::Exists(tempPath))
  {
    int rv = ExtMsgViewer(tempPath);
    if (rv == 0)
    {
      MarkSeen();
    }
  }
  else
  {
    SetDialogMessage("View message failed (message not available)", true /* p_Warn */);
  }
}

int Ui::ExtMsgViewer(const std::string& p_Path)
{
  const bool isDefaultMsgViewerCmd = ExtUtil::IsDefaultMsgViewerCmd();
  if (!isDefaultMsgViewerCmd)
  {
    TerminalControlPause();
  }

  const std::string& viewer = ExtUtil::GetMsgViewerCmd();
  const std::string& cmd = ExtUtil::MakeCmd(viewer, p_Path);
  LOG_DEBUG("launching message viewer: %s", cmd.c_str());
  int rv = SysUtil::System(cmd);
  if (rv == 0)
  {
    LOG_DEBUG("message viewer exited successfully");
  }
  else
  {
    LOG_WARNING("message viewer exited with %d", rv);
    SysUtil::DetectCommandNotPresent(cmd);
  }

  if (!isDefaultMsgViewerCmd)
  {
    TerminalControlResume();
  }

  return rv;
}
