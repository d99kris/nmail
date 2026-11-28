// uiviewbase.h
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <memory>
#include <mutex>
#include <string>

#include <ncurses.h>

#include "ui.h"
#include "uimodel.h"

class UiComposeView;
class UiDialogView;
class UiExternal;
class UiHelpView;
class UiMessageListView;
class UiMessageView;

class UiViewBase
{
public:
  explicit UiViewBase(Ui* p_Ui);
  virtual ~UiViewBase();

protected:
  // forwarded to ui
  void RefreshWin(WINDOW* p_Win);
  void TerminalControlPause();
  void TerminalControlResume();
  void DrawAll();
  void SetState(Ui::State p_State);
  void SetDialogMessage(const std::string& p_DialogMessage, bool p_Warn = false);
  bool PromptString(const std::string& p_Prompt, const std::string& p_Action, std::string& p_Entry);
  void Quit();
  void SetCurrentFolder(const std::string& p_Folder);
  void SetLastStateOrMessageList();
  void AsyncUiRequest(char p_UiRequest);
  bool IsConnected();
  bool PromptYesNo(const std::string& p_Prompt);
  void StartSync();

  // key edit helpers
  bool IsValidTextKey(int p_Key);
  bool HandleListKey(int p_Key, int& p_Index);
  bool HandleLineKey(int p_Key, std::wstring& p_Str, int& p_Pos);
  bool HandleTextKey(int p_Key, std::wstring& p_Str, int& p_Pos);
  bool HandleDocKey(int p_Key, std::wstring& p_Str, int& p_Pos);

private:
  void InitKeys();
  void InitAttrs();

protected:
  Ui* m_Ui = nullptr;
  UiModel* m_Model = nullptr;

  // owned by ui
  std::pair<std::string, int32_t>& m_CurrentFolderUid;
  std::string& m_CurrentFolder;
  std::string& m_PreviousFolder;
  Ui::State& m_State;
  Ui::State& m_LastState;
  Ui::State& m_LastMessageState;
  bool& m_MessageListSearch;
  bool& m_IsAutoMove;
  WINDOW*& m_DialogWin;
  WINDOW*& m_HelpWin;
  WINDOW*& m_MainWin;
  int& m_ScreenWidth;
  int& m_MainWinHeight;
  std::unique_ptr<UiHelpView>& m_HelpView;
  std::unique_ptr<UiExternal>& m_External;
  std::shared_ptr<SmtpManager>& m_SmtpManager;
  std::string& m_Name;
  bool& m_Plaintext;
  bool& m_CurrentMessageProcessFlowed;
  std::string& m_ComposeMailto;
  bool& m_CurrentMarkdownHtmlCompose;
  int& m_MaxComposeLineLength;
  std::unique_ptr<UiComposeView>& m_ComposeView;
  std::map<std::string, int32_t>& m_MessageListCurrentIndex;
  int& m_MessageFindMatchLine;
  std::string& m_MessageFindQuery;
  int& m_MessageViewLineOffset;
  bool& m_MessageViewToggledSeen;
  int& m_MaxViewLineLength;
  std::unique_ptr<UiMessageView>& m_MessageView;
  std::unique_ptr<UiMessageListView>& m_MessageListView;
  WINDOW*& m_TopWin;
  std::string& m_MessageListSearchQuery;
  std::map<std::string, int32_t>& m_MessageListCurrentUid;
  std::unique_ptr<UiDialogView>& m_DialogView;

  // same as in ui, until all views are extracted
  int m_KeyPrevMsg = 0;
  int m_KeyNextMsg = 0;
  int m_KeyReplyAll = 0;
  int m_KeyReplySender = 0;
  int m_KeyForward = 0;
  int m_KeyForwardAttached = 0;
  int m_KeyDelete = 0;
  int m_KeyCompose = 0;
  int m_KeyComposeCopy = 0;
  int m_KeyToggleUnread = 0;
  int m_KeyMove = 0;
  int m_KeyAutoMove = 0;
  int m_KeyRefresh = 0;
  int m_KeyQuit = 0;
  int m_KeyToggleTextHtml = 0;
  int m_KeyCancel = 0;
  int m_KeySend = 0;
  int m_KeyDeleteCharAfterCursor = 0;
  int m_KeyDeleteLineAfterCursor = 0;
  int m_KeyDeleteLineBeforeCursor = 0;
  int m_KeyOpen = 0;
  int m_KeyBack = 0;
  int m_KeyGotoFolder = 0;
  int m_KeyGotoInbox = 0;
  int m_KeyToSelect = 0;
  int m_KeySaveFile = 0;
  int m_KeySaveAllFiles = 0;
  int m_KeySaveMarkdown = 0;
  int m_KeyExtEditor = 0;
  int m_KeyExtPager = 0;
  int m_KeyPostpone = 0;
  int m_KeyOtherCmdHelp = 0;
  int m_KeyExport = 0;
  int m_KeyImport = 0;
  int m_KeyRichHeader = 0;
  int m_KeyExtHtmlViewer = 0;
  int m_KeyExtHtmlPreview = 0;
  int m_KeyExtMsgViewer = 0;
  int m_KeySearch = 0;
  int m_KeyFind = 0;
  int m_KeyFindNext = 0;
  int m_KeySync = 0;
  int m_KeyToggleMarkdownCompose = 0;
  int m_KeyNextLink = 0;
  int m_KeyPrevLink = 0;
  int m_KeyOpenLink = 0;
  int m_KeyBackwardWord = 0;
  int m_KeyForwardWord = 0;
  int m_KeyBackwardKillWord = 0;
  int m_KeyKillWord = 0;
  int m_KeyBeginLine = 0;
  int m_KeyEndLine = 0;
  int m_KeyPrevPage = 0;
  int m_KeyNextPage = 0;
  int m_KeyPrevPageCompose = 0;
  int m_KeyNextPageCompose = 0;
  int m_KeyFilterSortReset = 0;
  int m_KeyFilterShowUnread = 0;
  int m_KeyFilterShowHasAttachments = 0;
  int m_KeyFilterShowCurrentDate = 0;
  int m_KeyFilterShowCurrentName = 0;
  int m_KeyFilterShowCurrentSubject = 0;
  int m_KeySortUnread = 0;
  int m_KeySortHasAttachments = 0;
  int m_KeySortDate = 0;
  int m_KeySortName = 0;
  int m_KeySortSubject = 0;
  int m_KeyJumpTo = 0;
  int m_KeyToggleFullHeader = 0;
  int m_KeyToggleCompactHeader = 0;
  int m_KeySelectItem = 0;
  int m_KeySelectAll = 0;
  int m_KeySearchShowFolder = 0;
  int m_KeySearchCurrentSubject = 0;
  int m_KeySearchCurrentName = 0;
  int m_KeySpell = 0;
  int m_KeySearchServer = 0;

  int m_KeyReturn = 0;
  int m_KeyEnter = 0;
  int m_KeyLeft = 0;
  int m_KeyRight = 0;

  int m_KeyDown = 0;
  int m_KeyUp = 0;
  int m_KeyEnd = 0;
  int m_KeyHome = 0;
  int m_KeyBackspace = 0;
  int m_KeyBackspaceAlt = 0;
  int m_KeyDeleteChar = 0;
  int m_KeySpace = 0;
  int m_KeyTab = 0;
  int m_KeyTerminalResize = 0;

  int m_AttrsRegularText = A_NORMAL;
  int m_AttrsDialog = A_NORMAL;
  int m_AttrsHelpDesc = A_NORMAL;
  int m_AttrsHelpKeys = A_NORMAL;
  int m_AttrsHighlightedText = A_NORMAL;
  int m_AttrsQuotedText = A_NORMAL;
  int m_AttrsHiddenFile = A_NORMAL;
  int m_AttrsAddressText = A_NORMAL;
  int m_AttrsTopBar = A_NORMAL;
  int m_AttrsSelectedItem = A_NORMAL;
  int m_AttrsSelectedHighlighted = A_NORMAL;
  int m_AttrsBoldText = A_NORMAL;
  int m_AttrsCodeText = A_NORMAL;
  int m_AttrsHeaderName = A_NORMAL;
  int m_AttrsItalicText = A_NORMAL;
  int m_AttrsStrikeText = A_NORMAL;
  int m_AttrsSignatureText = A_NORMAL;
  int m_AttrsLinkText = A_NORMAL;
  int m_AttrsHighlightedLink = A_NORMAL;
};
