// ui.h
//
// Copyright (c) 2019-2021 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <csignal>
#include <string>
#include <vector>

#include <ncursesw/ncurses.h>

#include "config.h"
#include "imapmanager.h"
#include "smtpmanager.h"

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
    StateReplyMessage = 5,
    StateForwardMessage = 6,
    StateAddressList = 7,
    StateFileList = 8,
    StateViewPartList = 9,
  };

  enum UiRequest
  {
    UiRequestNone = 0,
    UiRequestDrawAll = (1 << 0),
    UiRequestDrawError = (1 << 1),
    UiRequestHandleConnected = (1 << 2),
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
    HeaderTo = 0,
    HeaderCc,
    HeaderBcc,
    HeaderAtt,
    HeaderSub,
  };

  Ui(const std::string& p_Inbox, const std::string& p_Address, uint32_t p_PrefetchLevel);
  virtual ~Ui();

  void SetImapManager(std::shared_ptr<ImapManager> p_ImapManager);
  void SetSmtpManager(std::shared_ptr<SmtpManager> p_SmtpManager);
  void SetTrashFolder(const std::string& p_TrashFolder);
  void SetDraftsFolder(const std::string& p_DraftsFolder);
  void SetSentFolder(const std::string& p_SentFolder);
  void SetClientStoreSent(bool p_ClientStoreSent);
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
  static void SetRunning(bool p_Running);

private:
  void Init();
  void Cleanup();

  void InitWindows();
  void CleanupWindows();

  void DrawAll();
  void DrawTop();
  void DrawDialog();
  void DrawSearchDialog();
  void DrawDefaultDialog();
  void DrawHelp();
  void DrawHelpText(const std::vector<std::vector<std::string>>& p_HelpText);
  void DrawFolderList();
  void DrawAddressList();
  void DrawFileList();
  void DrawMessageList();
  void DrawMessageListSearch();
  void DrawMessage();
  void DrawComposeMessage();
  void DrawPartList();

  void AsyncUiRequest(char p_UiRequest);
  void PerformUiRequest(char p_UiRequest);
  void SetDialogMessage(const std::string& p_DialogMessage, bool p_Warn = false);

  void ViewFolderListKeyHandler(int p_Key);
  void ViewAddressListKeyHandler(int p_Key);
  void ViewFileListKeyHandler(int p_Key);
  void ViewMessageListKeyHandler(int p_Key);
  void ViewMessageKeyHandler(int p_Key);
  void ComposeMessageKeyHandler(int p_Key);
  void ViewPartListKeyHandler(int p_Key);

  void SetState(State p_State);
  bool IsConnected();

  std::string GetKeyDisplay(int p_Key);
  std::string GetStatusStr();
  std::string GetStateStr();
  bool IsValidTextKey(int p_Key);

  void SendComposedMessage();
  void UploadDraftMessage();
  bool DeleteMessage();
  void MoveMessage(uint32_t p_Uid, const std::string& p_From, const std::string& p_To);
  void DeleteMessage(uint32_t p_Uid, const std::string& p_Folder);
  void ToggleSeen();
  void MarkSeen();
  void UpdateUidFromIndex(bool p_UserTriggered);
  void UpdateIndexFromUid();
  void AddUidDate(const std::string& p_Folder, const std::map<uint32_t, Header>& p_UidHeaders);
  void RemoveUidDate(const std::string& p_Folder, const std::set<uint32_t>& p_Uids);
  void ComposeMessagePrevLine();
  void ComposeMessageNextLine();
  int ReadKeyBlocking();
  bool PromptYesNo(const std::string& p_Prompt);
  bool PromptString(const std::string& p_Prompt, const std::string& p_Action,
                    std::string& p_Entry);
  bool CurrentMessageBodyAvailable();
  void InvalidateUiCache(const std::string& p_Folder);
  void ExtEditor(std::wstring& p_ComposeMessageStr, int& p_ComposeMessagePos);
  void ExtPager();
  int ExtPartsViewer(const std::string& p_Path);
  void ExtHtmlViewer();
  int ExtHtmlViewer(const std::string& p_Path);
  void ExtMsgViewer();
  void ExtMsgViewer(const std::string& p_Path);
  void SetLastStateOrMessageList();
  void ExportMessage();
  void ImportMessage();
  void SearchMessage();
  void MessageFind();
  void MessageFindNext();
  void Quit();
  std::wstring GetComposeStr(int p_HeaderField);
  void SetComposeStr(int p_HeaderField, const std::wstring& p_Str);
  int GetCurrentHeaderField();
  void StartSync();
  std::string MakeHtmlPart(const std::string& p_Text);
  void HandleConnected();
  void StartComposeBackup();
  void StopComposeBackup();
  void ComposeBackupProcess();
  void UpdateFilteredMsgDateUids();
  std::map<std::string, uint32_t>& GetMsgDateUids(const std::string& p_Folder);
  void FilterDisable();
  void ToggleFilterShowUnread();
  void ToggleFilterShowAttachments();

private:
  std::shared_ptr<ImapManager> m_ImapManager;
  std::shared_ptr<SmtpManager> m_SmtpManager;
  std::string m_TrashFolder;
  std::string m_DraftsFolder;
  std::string m_SentFolder;
  bool m_ClientStoreSent = false;

  std::string m_Inbox;
  std::string m_Address;
  uint32_t m_PrefetchLevel = 0;

  std::string m_CurrentFolder = "INBOX";
  std::string m_PreviousFolder;

  std::mutex m_Mutex;
  Status m_Status;
  std::set<std::string> m_Folders;
  std::map<std::string, std::set<uint32_t>> m_Uids;
  std::map<std::string, std::map<uint32_t, Header>> m_Headers;
  std::map<std::string, std::map<uint32_t, uint32_t>> m_Flags;
  std::map<std::string, std::map<uint32_t, Body>> m_Bodys;
  std::map<std::string, std::map<std::string, uint32_t>> m_MsgDateUids;
  std::map<std::string, std::map<uint32_t, std::string>> m_MsgUidDates;
  std::map<std::string, std::set<uint32_t>> m_NewUids;

  bool m_HasRequestedFolders = false;
  bool m_HasPrefetchRequestedFolders = false;
  std::map<std::string, bool> m_HasRequestedUids;
  std::map<std::string, bool> m_HasPrefetchRequestedUids;
  std::map<std::string, std::set<uint32_t>> m_PrefetchedHeaders;
  std::map<std::string, std::set<uint32_t>> m_RequestedHeaders;

  std::map<std::string, std::set<uint32_t>> m_PrefetchedFlags;
  std::map<std::string, std::set<uint32_t>> m_RequestedFlags;

  std::map<std::string, std::set<uint32_t>> m_PrefetchedBodys;
  std::map<std::string, std::set<uint32_t>> m_RequestedBodys;

  std::vector<std::string> m_Addresses;

  std::string m_CurrentDir;
  std::set<Fileinfo, FileinfoCompare> m_Files;

  WINDOW* m_TopWin = NULL;
  WINDOW* m_MainWin = NULL;
  WINDOW* m_DialogWin = NULL;
  WINDOW* m_HelpWin = NULL;

  int m_ScreenWidth = 0;
  int m_ScreenHeight = 0;
  int m_MainWinHeight = 0;

  std::string m_DialogMessage;
  std::chrono::time_point<std::chrono::system_clock> m_DialogMessageTime;

  std::map<std::string, int32_t> m_MessageListCurrentIndex;
  std::map<std::string, int32_t> m_MessageListCurrentUid;
  std::map<std::string, bool> m_MessageListUidSet;

  int m_AddressListCurrentIndex = 0;
  std::string m_AddressListCurrentAddress;

  int m_FileListCurrentIndex = 0;
  Fileinfo m_FileListCurrentFile;

  int m_PartListCurrentIndex = 0;
  Part m_PartListCurrentPart;

  int m_MessageViewLineOffset = 0;
  bool m_PersistFileSelectionDir = true;
  bool m_PersistFindQuery = false;
  bool m_PersistFolderFilter = true;
  bool m_PersistSearchQuery = false;
  bool m_Plaintext = true;
  bool m_MarkdownHtmlCompose = false;
  bool m_CurrentMarkdownHtmlCompose = false;

  int m_FolderListCurrentIndex = 0;
  std::string m_FolderListCurrentFolder;

  int m_PersistedFolderListCurrentIndex = 0;
  std::string m_PersistedFolderListCurrentFolder;

  Config m_Config;

  bool m_ComposeHardwrap = false;
  bool m_HelpEnabled = true;

  int m_KeyPrevMsg = 0;
  int m_KeyNextMsg = 0;
  int m_KeyReply = 0;
  int m_KeyForward = 0;
  int m_KeyDelete = 0;
  int m_KeyCompose = 0;
  int m_KeyToggleUnread = 0;
  int m_KeyMove = 0;
  int m_KeyRefresh = 0;
  int m_KeyQuit = 0;
  int m_KeyToggleTextHtml = 0;
  int m_KeyCancel = 0;
  int m_KeySend = 0;
  int m_KeyDeleteLine = 0;
  int m_KeyOpen = 0;
  int m_KeyBack = 0;
  int m_KeyGotoFolder = 0;
  int m_KeyToSelect = 0;
  int m_KeySaveFile = 0;
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
  int m_KeyBackwardWord = 0;
  int m_KeyForwardWord = 0;
  int m_KeyBackwardKillWord = 0;
  int m_KeyKillWord = 0;
  int m_KeyPrevPage = 0;
  int m_KeyNextPage = 0;
  int m_KeyFilterShowUnread = 0;
  int m_KeyFilterShowHasAttachments = 0;

  bool m_ShowProgress = false;
  bool m_NewMsgBell = false;
  bool m_QuitWithoutConfirm = true;
  bool m_SendWithoutConfirm = false;
  bool m_CancelWithoutConfirm = false;
  bool m_PostponeWithoutConfirm = false;
  bool m_DeleteWithoutConfirm = false;
  bool m_ShowEmbeddedImages = true;
  bool m_ShowRichHeader = false;
  bool m_ColorsEnabled = false;
  int m_ColorMessageQuoted = -1;

  std::string m_AttachmentIndicator;
  bool m_BottomReply = false;

  int m_FolderListFilterPos = 0;
  std::wstring m_FolderListFilterStr;

  int m_PersistedFolderListFilterPos = 0;
  std::wstring m_PersistedFolderListFilterStr;

  int m_AddressListFilterPos = 0;
  std::wstring m_AddressListFilterStr;

  int m_FileListFilterPos = 0;
  std::wstring m_FileListFilterStr;

  int m_FilenameEntryStringPos = 0;
  std::wstring m_FilenameEntryString;

  std::map<uint32_t, std::wstring> m_ComposeHeaderStr;
  int m_ComposeHeaderLine = 0;
  int m_ComposeHeaderPos = 0;
  bool m_IsComposeHeader = true;

  std::string m_ComposeHeaderRef;
  std::string m_ComposeTempDirectory;

  std::wstring m_ComposeMessageStr;
  int m_ComposeMessagePos = 0;
  std::vector<std::wstring> m_ComposeMessageLines;
  int m_ComposeMessageWrapLine = 0;
  int m_ComposeMessageWrapPos = 0;
  int m_ComposeMessageOffsetY = 0;
  uint32_t m_ComposeDraftUid = 0;

  std::deque<SmtpManager::Result> m_SmtpErrorResults;
  std::mutex m_SmtpErrorMutex;

  State m_State = StateViewMessageList;
  State m_LastState = StateViewMessageList;
  State m_LastMessageState = StateComposeMessage;

  int m_HelpViewMessagesListOffset = 0;
  int m_HelpViewMessageOffset = 0;

  bool m_MessageViewToggledSeen = false;

  std::string m_CurrentMessageViewText;

  int m_MaxViewLineLength = 0;
  int m_MaxComposeLineLength = 0;

  int m_Pipe[2] = { -1, -1 };

  std::mutex m_SearchMutex;
  bool m_MessageListSearch = false;
  std::string m_MessageListSearchQuery;
  size_t m_MessageListSearchOffset = 0;
  size_t m_MessageListSearchMax = 0;
  bool m_MessageListSearchHasMore = false;
  std::vector<Header> m_MessageListSearchResultHeaders;
  std::vector<std::pair<std::string, uint32_t>> m_MessageListSearchResultFolderUids;

  std::pair<std::string, int32_t> m_CurrentFolderUid = std::make_pair("", -1);

  uint32_t m_ComposeBackupInterval = 0;
  std::thread m_ComposeBackupThread;
  bool m_ComposeBackupRunning = false;
  std::condition_variable m_ComposeBackupCond;
  std::mutex m_ComposeBackupMutex;

  int m_MessageFindMatchLine = -1;
  int m_MessageFindMatchPos = 0;
  std::string m_MessageFindQuery;

  bool m_FilterEnabled = false;
  bool m_FilterShowUnread = false;
  bool m_FilterShowAttachments = false;
  std::map<std::string, std::map<std::string, uint32_t>> m_FilteredMsgDateUids;

private:
  static bool s_Running;
};
