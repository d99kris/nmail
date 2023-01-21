// ui.h
//
// Copyright (c) 2019-2023 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <csignal>
#include <string>
#include <vector>

#include <ncurses.h>

#include "config.h"
#include "imapmanager.h"
#include "smtpmanager.h"

class SleepDetect;

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

  Ui(const std::string& p_Inbox, const std::string& p_Address, const std::string& p_Name,
     uint32_t p_PrefetchLevel, bool p_PrefetchAllHeaders);
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
  std::string GetFilterStateStr();
  bool IsValidTextKey(int p_Key);

  bool ComposedMessageIsValid(bool p_ForSend);
  void SendComposedMessage();
  void UploadDraftMessage();
  bool DeleteMessage();
  void MoveSelectedMessages(const std::string& p_To);
  void MoveMessages(const std::set<uint32_t>& p_Uids, const std::string& p_From,
                    const std::string& p_To);
  void DeleteSelectedMessages();
  void DeleteMessages(const std::set<uint32_t>& p_Uids, const std::string& p_Folder);
  void ToggleSeen();
  void SetSeen(const std::string& p_Folder, const std::set<uint32_t>& p_Uids, bool p_Seen);
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
  bool CurrentMessageBodyHeaderAvailable();
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
  void SearchMessageBasedOnCurrent(bool p_Subject);
  void SearchMessage(const std::string& p_Query = std::string());
  void MessageFind();
  void MessageFindNext();
  void Quit();
  std::wstring GetComposeStr(int p_HeaderField);
  void SetComposeStr(int p_HeaderField, const std::wstring& p_Str);
  std::wstring GetComposeBodyForSend();
  int GetCurrentHeaderField();
  void StartSync();
  std::string MakeHtmlPart(const std::string& p_Text);
  std::string MakeHtmlPartCustomSig(const std::string& p_Text);
  void HandleConnected();
  void StartComposeBackup();
  void StopComposeBackup();
  void ComposeBackupProcess();

  std::map<std::string, uint32_t>& GetDisplayUids(const std::string& p_Folder);
  std::set<uint32_t>& GetHeaderUids(const std::string& p_Folder);
  std::string GetDisplayUidsKey(const std::string& p_Folder, uint32_t p_Uid, SortFilter p_SortFilter);
  void UpdateDisplayUids(const std::string& p_Folder,
                         const std::set<uint32_t>& p_RemovedUids = std::set<uint32_t>(),
                         const std::set<uint32_t>& p_AddedUids = std::set<uint32_t>(),
                         bool p_FilterUpdated = false);
  void SortFilterPreUpdate();
  void SortFilterUpdated(bool p_FilterUpdated);
  void DisableSortFilter();
  void ToggleFilter(SortFilter p_SortFilter);
  void ToggleSort(SortFilter p_SortFirst, SortFilter p_SortSecond);
  const std::vector<std::wstring>& GetCachedWordWrapLines(const std::string& p_Folder, uint32_t p_Uid);
  void ClearSelection();
  void ToggleSelected();
  void ToggleSelectAll();
  int GetSelectedCount();

  std::string GetBodyText(Body& p_Body);
  void FilePickerOrStateFileList();
  void AddAttachmentPath(const std::string& p_Path);
  void AddAddress(const std::string& p_Address);
  void SetAddress(const std::string& p_Address);
  std::string GetDefaultFrom();
  std::wstring GetSignatureStr(bool p_NoPrefix = false);

  inline bool HandleListKey(int p_Key, int& p_Index);
  inline bool HandleLineKey(int p_Key, std::wstring& p_Str, int& p_Pos);
  inline bool HandleTextKey(int p_Key, std::wstring& p_Str, int& p_Pos);
  inline bool HandleDocKey(int p_Key, std::wstring& p_Str, int& p_Pos);
  inline bool HandleComposeKey(int p_Key);

  void OnWakeUp();

private:
  std::shared_ptr<ImapManager> m_ImapManager;
  std::shared_ptr<SmtpManager> m_SmtpManager;
  std::string m_TrashFolder;
  std::string m_DraftsFolder;
  std::string m_SentFolder;
  bool m_ClientStoreSent = false;

  std::string m_Inbox;
  std::string m_Address;
  std::string m_Name;
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
  std::map<std::string, SortFilter> m_SortFilter;
  std::map<std::string, std::set<uint32_t>> m_HeaderUids;
  std::map<std::string, std::map<SortFilter, std::map<std::string, uint32_t>>> m_DisplayUids;
  std::map<std::string, std::map<SortFilter, uint64_t>> m_DisplayUidsVersion;
  std::map<std::string, uint64_t> m_HeaderUidsVersion;

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
  PartInfo m_PartListCurrentPartInfo;

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

  int m_ComposeLineWrap = LineWrapNone;
  bool m_RespectFormatFlowed = true;
  bool m_RewrapQuotedLines = true;
  bool m_HelpEnabled = true;

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
  int m_KeyBeginLine = 0;
  int m_KeyEndLine = 0;
  int m_KeyPrevPage = 0;
  int m_KeyNextPage = 0;
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
  int m_KeySelectItem = 0;
  int m_KeySelectAll = 0;
  int m_KeySearchShowFolder = 0;
  int m_KeySearchCurrentSubject = 0;
  int m_KeySearchCurrentName = 0;

  int m_ShowProgress = 1;
  bool m_NewMsgBell = false;
  bool m_QuitWithoutConfirm = true;
  bool m_SendWithoutConfirm = false;
  bool m_CancelWithoutConfirm = false;
  bool m_PostponeWithoutConfirm = false;
  bool m_DeleteWithoutConfirm = false;
  bool m_ShowEmbeddedImages = true;
  bool m_ShowRichHeader = false;
  bool m_ColorsEnabled = false;
  bool m_ShowFullHeader = false;
  bool m_SearchShowFolder = false;
  bool m_Signature = false;

  std::string m_TerminalTitle;

  int m_AttrsDialog = A_REVERSE;
  int m_AttrsHelpDesc = A_NORMAL;
  int m_AttrsHelpKeys = A_REVERSE;
  int m_AttrsHighlightedText = A_REVERSE;
  int m_AttrsQuotedText = A_NORMAL;
  int m_AttrsTopBar = A_REVERSE;
  int m_AttrsSelectedItem = A_NORMAL;
  int m_AttrsSelectedHighlighted = A_NORMAL;

  std::string m_AttachmentIndicator;
  bool m_BottomReply = false;
  bool m_PersistSortFilter = true;
  bool m_PersistSelectionOnSortFilterChange = true;
  std::string m_UnreadIndicator;
  bool m_InvalidInputNotify = true;
  bool m_FullHeaderIncludeLocal = false;

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
  int m_HelpViewMessagesListSize = 0;
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
  bool m_PrefetchAllHeaders = true;
  bool m_CurrentMessageProcessFlowed = false;
  int m_MessageViewHeaderLineCount = 0;

  std::string m_FilterCustomStr;
  int m_TabSize = 8;

  std::map<std::string, std::set<uint32_t>> m_SelectedUids;
  bool m_AllSelected = false;

  std::unique_ptr<SleepDetect> m_SleepDetect;

private:
  static bool s_Running;
};
