// uicomposeview.h
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <condition_variable>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "uiscreenview.h"

class UiComposeView : public UiScreenView
{
public:
  explicit UiComposeView(Ui* p_Ui);

  virtual void Draw();
  virtual void KeyHandler(int p_Key);
  virtual void OnEnter();
  virtual bool IsDrawnLast();
  void OnTerminate();
  void Cleanup();

  void AddAttachmentPath(const std::string& p_Path);
  void AddAddress(const std::string& p_Address);
  void SetAddress(const std::string& p_Address);
  std::string GetDefaultFrom();

private:
  bool ComposedMessageIsValid(bool p_ForSend);
  void SendComposedMessage();
  void UploadDraftMessage();
  int GetComposeMessageColumn();
  void SetComposeMessagePos(int p_WrapLine, int p_Column);
  void ComposeMessagePrevLine();
  void ComposeMessageNextLine();
  void SetComposeMailto(const std::string& p_Url);
  std::wstring GetComposeStr(int p_HeaderField);
  void SetComposeStr(int p_HeaderField, const std::wstring& p_Str);
  std::wstring GetComposeBodyForSend();
  int GetCurrentHeaderField();
  std::string MakeHtmlPart(const std::string& p_Text);
  std::string MakeHtmlPartCustomSig(const std::string& p_Text);
  void StartComposeBackup();
  void StopComposeBackup();
  void ComposeBackupProcess();
  std::wstring GetSignatureStr(bool p_NoPrefix = false);
  bool HandleComposeKey(int p_Key);
  void FilePickerOrStateFileList();

private:
  int m_TabSize = 8;
  bool m_RewrapQuotedLines = true;
  bool m_RespectFormatFlowed = true;
  int m_ComposeLineWrap = Ui::LineWrapNone;
  uint32_t m_ComposeBackupInterval = 0;
  bool m_CancelWithoutConfirm = false;
  bool m_PostponeWithoutConfirm = false;
  bool m_SendWithoutConfirm = false;
  bool m_Signature = false;
  bool m_BottomReply = false;
  bool m_MarkdownHtmlCompose = false;
  bool m_UnwrapQuotedLines = false;
  bool m_ShowRichHeader = false;

  std::map<uint32_t, std::wstring> m_ComposeHeaderStr;
  int m_ComposeHeaderLine = 0;
  int m_ComposeHeaderPos = 0;
  bool m_IsComposeHeader = true;
  std::string m_ComposeHeaderRef;
  std::string m_ComposeTempDirectory;
  std::wstring m_ComposeMessageStr;
  int m_ComposeMessagePos = 0;
  std::vector<std::wstring> m_ComposeMessageLines;
  std::vector<int> m_ComposeMessageLineOffsets;
  int m_ComposeMessageWrapLine = 0;
  int m_ComposeMessageWrapPos = 0;
  int m_ComposeMessageOffsetY = 0;
  uint32_t m_ComposeDraftUid = 0;
  std::string m_ComposeQuotedStart;
  std::thread m_ComposeBackupThread;
  bool m_ComposeBackupRunning = false;
  bool m_ComposeBackupKeep = false;
  std::condition_variable m_ComposeBackupCond;
  std::mutex m_ComposeBackupMutex;
};
