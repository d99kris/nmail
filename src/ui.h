// ui.h
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <csignal>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include <ncurses.h>

#include "fileutil.h"
#include "imapmanager.h"
#include "smtpmanager.h"
#include "textstyle.h"

class SleepDetect;
class UiAddressListView;
class UiComposeView;
class UiDialogView;
class UiExternal;
class UiFileListView;
class UiFolderListView;
class UiHelpView;
class UiMessageListView;
class UiMessageView;
class UiModel;
class UiPartListView;
class UiScreenView;
class UiTopView;

class Ui
{
public:
  enum State
  {
    StateViewMessageList = 0,
    StateViewMessage = 1,
    StateGotoFolder = 2,
    StateMoveToFolder = 3,
    StateComposeMessage = 4,
    StateComposeCopyMessage = 5,
    StateReplyAllMessage = 6,
    StateReplySenderMessage = 7,
    StateForwardMessage = 8,
    StateForwardAttachedMessage = 9,
    StateAddressList = 10,
    StateFileList = 11,
    StateViewPartList = 12,
    StateFromAddressList = 13,
  };

  enum UiRequest
  {
    UiRequestNone = 0,
    UiRequestDrawAll = (1 << 0),
    UiRequestDrawError = (1 << 1),
    UiRequestHandleConnected = (1 << 2),
    UiRequestSpecialFolders = (1 << 3),
    UiRequestBell = (1 << 4),
  };

  enum PrefetchLevel
  {
    PrefetchLevelNone = 0,
    PrefetchLevelCurrentMessage = 1,
    PrefetchLevelCurrentView = 2,
    PrefetchLevelFullSync = 3,
  };

  enum HeaderField
  {
    HeaderAll = -1,
    HeaderFrom = 0,
    HeaderTo,
    HeaderCc,
    HeaderBcc,
    HeaderAtt,
    HeaderSub,
  };

  enum SortFilter
  {
    SortDefault = 0,
    SortUnseenAsc,
    SortUnseenDesc,
    SortUnseenOnly,
    SortAttchAsc,
    SortAttchDesc,
    SortAttchOnly,
    SortDateAsc,
    SortDateDesc,
    SortCurrDateOnly,
    SortNameAsc,
    SortNameDesc,
    SortCurrNameOnly,
    SortSubjAsc,
    SortSubjDesc,
    SortCurrSubjOnly,
  };

  enum LineWrap
  {
    LineWrapNone = 0,
    LineWrapFormatFlowed = 1,
    LineWrapHardWrap = 2,
  };

  enum SyncState
  {
    SyncStateIdle = 0,
    SyncStateStarted = 1,
    SyncStateInProgress = 2,
  };

  Ui(const std::string& p_Inbox, const std::string& p_Address, const std::string& p_Name,
     uint32_t p_PrefetchLevel, bool p_PrefetchAllHeaders);
  virtual ~Ui();

  void SetImapManager(std::shared_ptr<ImapManager> p_ImapManager);
  void SetSmtpManager(std::shared_ptr<SmtpManager> p_SmtpManager);
  void SetTrashFolder(const std::string& p_TrashFolder);
  void SetSpamFolder(const std::string& p_SpamFolder);
  void SetDraftsFolder(const std::string& p_DraftsFolder);
  void SetSentFolder(const std::string& p_SentFolder);
  void SetClientStoreSent(bool p_ClientStoreSent);
  void SetSpecialFoldersHandler(const std::function<void(const std::map<std::string, std::string>&)>& p_Handler);
  void ResetImapManager();
  void ResetSmtpManager();

  void Run();

  void ResponseHandler(const ImapManager::Request& p_Request, const ImapManager::Response& p_Response);
  void ResultHandler(const ImapManager::Action& p_Action, const ImapManager::Result& p_Result);
  void SmtpResultHandlerError(const SmtpManager::Result& p_Result);
  void SmtpResultHandler(const SmtpManager::Result& p_Result);
  void StatusHandler(const StatusUpdate& p_StatusUpdate);
  void SearchHandler(const ImapManager::SearchQuery& p_SearchQuery,
                     const ImapManager::SearchResult& p_SearchResult);

public:
  static bool IsRunning();
  static void SetRunning(bool p_Running);

private:
  void Init();
  void Cleanup();

  void InitWindows();
  void CleanupWindows();
  void TerminalControlPause();
  void TerminalControlResume();

  void RefreshWin(WINDOW* p_Win);
  void DrawAll();

  void AsyncUiRequest(char p_UiRequest);
  void PerformUiRequest(char p_UiRequest);


  void SetState(State p_State);
  UiScreenView* GetScreenView(State p_State);
  bool IsConnected();

  void SetLastStateOrMessageList();
  void Quit();
  void StartSync();
  void HandleConnected();

  void OnWakeUp();
  void SetCurrentFolder(const std::string& p_Folder);

private:
  std::unique_ptr<UiModel> m_Model;
  std::shared_ptr<SmtpManager> m_SmtpManager;

  std::string m_Name;

  std::string m_CurrentFolder = "INBOX";
  std::string m_PreviousFolder;





  WINDOW* m_TopWin = NULL;
  WINDOW* m_MainWin = NULL;
  WINDOW* m_DialogWin = NULL;
  WINDOW* m_HelpWin = NULL;

  int m_ScreenWidth = 0;
  int m_ScreenHeight = 0;
  int m_TermLines = 0;
  int m_TermCols = 0;
  int m_MainWinHeight = 0;

  std::map<std::string, int32_t> m_MessageListCurrentIndex;
  std::map<std::string, int32_t> m_MessageListCurrentUid;

  int m_MessageViewLineOffset = 0;
  bool m_Plaintext = false;
  bool m_MarkdownView = false;
  bool m_MarkdownViewLinks = false;
  bool m_CurrentMarkdownHtmlCompose = false;
  bool m_IsAutoMove = false;

  int m_ComposeLineWrap = LineWrapNone;
  bool m_HelpEnabled = true;
  bool m_DrawAllActive = false;

  int m_KeyTerminalResize = 0;

  int m_ShowProgress = 1;
  bool m_QuitWithoutConfirm = true;

  std::string m_TerminalTitle;

  int m_AttrsRegularText = A_NORMAL;

  std::string m_ComposeMailto;


  State m_State = StateViewMessageList;
  State m_LastState = StateViewMessageList;
  State m_LastMessageState = StateComposeMessage;

  std::unique_ptr<UiTopView> m_TopView;
  std::unique_ptr<UiHelpView> m_HelpView;
  std::unique_ptr<UiDialogView> m_DialogView;
  std::unique_ptr<UiExternal> m_External;
  std::unique_ptr<UiAddressListView> m_AddressListView;
  std::unique_ptr<UiFileListView> m_FileListView;
  std::unique_ptr<UiPartListView> m_PartListView;
  std::unique_ptr<UiFolderListView> m_FolderListView;
  std::unique_ptr<UiComposeView> m_ComposeView;
  std::unique_ptr<UiMessageView> m_MessageView;
  std::unique_ptr<UiMessageListView> m_MessageListView;

  bool m_MessageViewToggledSeen = false;


  int m_MaxViewLineLength = 0;
  int m_MaxComposeLineLength = 0;

  int m_Pipe[2] = { -1, -1 };

  bool m_MessageListSearch = false;
  std::string m_MessageListSearchQuery;

  std::pair<std::string, int32_t> m_CurrentFolderUid = std::make_pair("", -1);


  int m_MessageFindMatchLine = -1;
  std::string m_MessageFindQuery;
  bool m_CurrentMessageProcessFlowed = false;



  std::unique_ptr<SleepDetect> m_SleepDetect;


private:
  static bool s_Running;

  friend class UiViewBase;
};
