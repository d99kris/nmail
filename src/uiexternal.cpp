// uiexternal.cpp
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "uiexternal.h"

#include <cstdint>

#include "extutil.h"
#include "fileutil.h"
#include "loghelp.h"
#include "strutil.h"
#include "sysutil.h"

UiExternal::UiExternal(Ui* p_Ui)
  : UiViewBase(p_Ui)
{
}

void UiExternal::ExtEditor(const std::string& p_EditorCmd, std::wstring& p_ComposeMessageStr, int& p_ComposeMessagePos)
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

void UiExternal::ExtPager(const std::string& p_Text)
{
  TerminalControlPause();
  const std::string& tempPath = FileUtil::GetTempFilename(".txt");
  FileUtil::WriteFile(tempPath, p_Text);
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

int UiExternal::ExtPartsViewer(const std::string& p_Path)
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

int UiExternal::ExtHtmlViewer(const std::string& p_Path)
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

int UiExternal::ExtHtmlPreview(const std::string& p_Path)
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

int UiExternal::ExtMsgViewer(const std::string& p_Path)
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

int UiExternal::ExtLinkOpen(const std::string& p_Url)
{
  const bool isDefaultLinkOpenCmd = ExtUtil::IsDefaultLinkOpenCmd();
  if (!isDefaultLinkOpenCmd)
  {
    TerminalControlPause();
  }

  // single quoted, as url is from message
  const std::string& cmd = ExtUtil::MakeCmd(ExtUtil::GetLinkOpenCmd(), p_Url);
  LOG_DEBUG("launching link open: %s", cmd.c_str());
  int rv = SysUtil::System(cmd);
  if (rv == 0)
  {
    LOG_DEBUG("link open exited successfully");
  }
  else
  {
    LOG_WARNING("link open exited with %d", rv);
    SysUtil::DetectCommandNotPresent(cmd);
  }

  if (!isDefaultLinkOpenCmd)
  {
    TerminalControlResume();
  }

  return rv;
}

std::vector<std::string> UiExternal::ExtFilePicker(const std::string& p_FilePickerCmd)
{
  std::vector<std::string> files;
  TerminalControlPause();

  std::string outPath = FileUtil::GetTempFilename(".txt");
  std::string command = p_FilePickerCmd + " > " + outPath;
  if (SysUtil::System(command) == 0)
  {
    std::string filesStr = FileUtil::ReadFile(outPath);
    if (!filesStr.empty())
    {
      files = StrUtil::Split(filesStr, '\n');
    }
  }
  else
  {
    LOG_WARNING("external command failed: %s", command.c_str());
  }

  FileUtil::DeleteFile(outPath);
  TerminalControlResume();
  return files;
}
