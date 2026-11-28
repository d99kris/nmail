// ui.cpp
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "ui.h"

#include <algorithm>
#include <climits>
#include <cstdint>

#include <sys/ioctl.h>
#include <unistd.h>

#include "apputil.h"
#include "contact.h"
#include "extutil.h"
#include "fileutil.h"
#include "loghelp.h"
#include "mailutil.h"
#include "maphelp.h"
#include "offlinequeue.h"
#include "sethelp.h"
#include "sleepdetect.h"
#include "status.h"
#include "uiaddresslistview.h"
#include "uicolorconfig.h"
#include "uicomposeview.h"
#include "uiconfig.h"
#include "uidialogview.h"
#include "uiexternal.h"
#include "uifilelistview.h"
#include "uifolderlistview.h"
#include "uihelpview.h"
#include "uikeyconfig.h"
#include "uikeyinput.h"
#include "uimessagelistview.h"
#include "uimessageview.h"
#include "uimodel.h"
#include "uipartlistview.h"
#include "uipickerview.h"
#include "uitopview.h"

bool Ui::s_Running = false;

Ui::Ui(const std::string& p_Inbox, const std::string& p_Address, const std::string& p_Name,
       uint32_t p_PrefetchLevel, bool p_PrefetchAllHeaders)
  : m_Name(p_Name)
{
  // dialog view is created in init
  auto dialogMessageHandler = [this](const std::string& p_DialogMessage, bool p_Warn)
  {
    m_DialogView->SetDialogMessage(p_DialogMessage, p_Warn);
  };

  m_Model.reset(new UiModel(p_Inbox, p_Address, Contact(p_Address, p_Name).ToString(), p_PrefetchLevel,
                            p_PrefetchAllHeaders, std::bind(&Ui::AsyncUiRequest, this, std::placeholders::_1),
                            dialogMessageHandler));
  SetCurrentFolder(p_Inbox);
  Init();
  InitWindows();
}

Ui::~Ui()
{
  CleanupWindows();
  Cleanup();
}

void Ui::Init()
{
  UiConfig::Init();

  m_TerminalTitle = UiConfig::GetStr("terminal_title");

  if (!m_TerminalTitle.empty())
  {
    printf("\033]0;%s\007", m_TerminalTitle.c_str());
  }

  UiKeyInput::EnableMetaSendsEsc();

  AppUtil::InitLocale();
  initscr();
  noecho();
  cbreak();
  keypad(stdscr, TRUE);
  curs_set(0);
  timeout(0);

  UiKeyConfig::Init(true);

  m_ComposeLineWrap = UiConfig::GetNum("compose_line_wrap");
  m_HelpEnabled = UiConfig::GetBool("help_enabled");
  UiPickerView::Init();
  m_Plaintext = UiConfig::GetBool("plain_text");
  m_MarkdownView = UiConfig::GetBool("markdown_view");
  m_MarkdownViewLinks = m_MarkdownView && UiConfig::GetBool("markdown_view_links");
  ExtUtil::SetHtmlToTextLinks(m_MarkdownViewLinks);

  m_KeyTerminalResize = UiKeyConfig::GetKey("key_terminal_resize");

  m_ShowProgress = UiConfig::GetNum("show_progress");
  m_Model->SetNewMsgBell(UiConfig::GetBool("new_msg_bell"));
  m_QuitWithoutConfirm = UiConfig::GetBool("quit_without_confirm");

  LOG_IF_NONZERO(pipe(m_Pipe));

  UiColorConfig::Init();

  m_AttrsRegularText = UiColorConfig::GetAttrs("color_regular_text");

  MailUtil::SetLocalizedSubjectPrefixes(UiConfig::GetStr("localized_subject_prefixes"));

  m_Model->SetShowProgress(m_ShowProgress);

  m_TopView.reset(new UiTopView(this));
  m_HelpView.reset(new UiHelpView(this));
  m_DialogView.reset(new UiDialogView(this));
  m_External.reset(new UiExternal(this));
  m_AddressListView.reset(new UiAddressListView(this));
  m_FileListView.reset(new UiFileListView(this));
  m_PartListView.reset(new UiPartListView(this));
  m_FolderListView.reset(new UiFolderListView(this));
  m_ComposeView.reset(new UiComposeView(this));
  m_MessageView.reset(new UiMessageView(this));
  m_MessageListView.reset(new UiMessageListView(this));

  SetRunning(true);

  m_SleepDetect.reset(new SleepDetect(std::bind(&Ui::OnWakeUp, this), 10));
}

void Ui::Cleanup()
{
  m_SleepDetect.reset();

  UiConfig::SetBool("plain_text", m_Plaintext);
  m_ComposeView->Cleanup();
  m_MessageView->Cleanup();
  m_MessageListView->Cleanup();
  UiConfig::Cleanup();

  UiKeyConfig::Cleanup();

  close(m_Pipe[0]);
  close(m_Pipe[1]);
  wclear(stdscr);
  endwin();

  UiKeyInput::RestoreMetaSendsEsc();

  if (!m_TerminalTitle.empty())
  {
    printf("\033]0;%s\007", "");
  }

  fflush(stdout);
}

void Ui::InitWindows()
{
  getmaxyx(stdscr, m_ScreenHeight, m_ScreenWidth);
  m_ScreenWidth = std::max(m_ScreenWidth, 40);
  m_ScreenHeight = std::max(m_ScreenHeight, 8);

  m_MaxViewLineLength = m_ScreenWidth;
  m_MaxComposeLineLength = (m_ComposeLineWrap == LineWrapHardWrap) ? std::min(m_ScreenWidth, 72) : m_ScreenWidth;
  wbkgd(stdscr, m_AttrsRegularText | ' ');
  // erase without flushing, the following draws then only update changed cells
  werase(stdscr);
  wnoutrefresh(stdscr);
  const int topHeight = 1;
  m_TopWin = newwin(topHeight, m_ScreenWidth, 0, 0);
  wbkgd(m_TopWin, m_AttrsRegularText | ' ');
  leaveok(m_TopWin, true);

  int helpHeight = 0;
  if (m_HelpEnabled)
  {
    helpHeight = 2;
    m_HelpWin = newwin(2, m_ScreenWidth, m_ScreenHeight - helpHeight, 0);
    wbkgd(m_HelpWin, m_AttrsRegularText | ' ');
    leaveok(m_HelpWin, true);
  }

  const int dialogHeight = 1;
  m_DialogWin = newwin(1, m_ScreenWidth, m_ScreenHeight - helpHeight - dialogHeight, 0);
  wbkgd(m_DialogWin, m_AttrsRegularText | ' ');
  leaveok(m_DialogWin, true);

  bool listPad = true;
  if (listPad)
  {
    m_MainWinHeight = m_ScreenHeight - topHeight - helpHeight - 2;
    m_MainWin = newwin(m_MainWinHeight, m_ScreenWidth, topHeight + 1, 0);
  }
  else
  {
    m_MainWinHeight = m_ScreenHeight - topHeight - helpHeight;
    m_MainWin = newwin(m_MainWinHeight, m_ScreenWidth, topHeight, 0);
  }

  wbkgd(m_MainWin, m_AttrsRegularText | ' ');
  leaveok(m_MainWin, true);
}

void Ui::CleanupWindows()
{
  delwin(m_TopWin);
  m_TopWin = NULL;
  delwin(m_MainWin);
  m_MainWin = NULL;
  delwin(m_DialogWin);
  m_DialogWin = NULL;
  if (m_HelpWin != NULL)
  {
    delwin(m_HelpWin);
    m_HelpWin = NULL;
  }
}

void Ui::TerminalControlPause()
{
  m_TermLines = LINES;
  m_TermCols = COLS;
  UiKeyInput::RestoreMetaSendsEsc();
  fflush(stdout);
  endwin();
}

void Ui::TerminalControlResume()
{
  refresh();
  UiKeyInput::EnableMetaSendsEsc();
  fflush(stdout);
  wint_t key = 0;
  while (UiKeyInput::GetWch(&key) != ERR)
  {
    // Discard any remaining input
  }

  // check if terminal was resized while controlled by external program
  struct winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0)
  {
    if (ws.ws_row != m_TermLines || ws.ws_col != m_TermCols)
    {
      resizeterm(ws.ws_row, ws.ws_col);
      CleanupWindows();
      InitWindows();
      DrawAll();
    }
  }
}

void Ui::RefreshWin(WINDOW* p_Win)
{
  // stage update, and flush unless part of a full redraw
  wnoutrefresh(p_Win);
  if (!m_DrawAllActive)
  {
    doupdate();
  }
}

void Ui::DrawAll()
{
  const bool wasDrawAllActive = m_DrawAllActive;
  m_DrawAllActive = true;

  UiScreenView* screenView = GetScreenView(m_State);
  if (screenView != nullptr)
  {
    m_TopView->Draw();
    if (!screenView->IsDrawnLast())
    {
      screenView->Draw();
    }

    m_HelpView->Draw();
    screenView->DrawDialog();
    if (screenView->IsDrawnLast())
    {
      screenView->Draw();
    }
  }
  else
  {
    werase(m_MainWin);
    mvwprintw(m_MainWin, 0, 0, "Unimplemented state %d", m_State);
    RefreshWin(m_MainWin);
  }

  // flush all staged updates in one go, to avoid partially drawn frames
  m_DrawAllActive = wasDrawAllActive;
  if (!m_DrawAllActive)
  {
    doupdate();
  }

  LOG_LATENCY_END(LatencyKeyPress);
}

void Ui::AsyncUiRequest(char p_UiRequest)
{
  LOG_IF_NOT_EQUAL(write(m_Pipe[1], &p_UiRequest, 1), 1);
}

void Ui::PerformUiRequest(char p_UiRequest)
{
  m_MessageListView->SyncCursor();

  if (p_UiRequest & UiRequestBell)
  {
    beep();
  }

  if (p_UiRequest & UiRequestDrawAll)
  {
    DrawAll();
  }

  if (p_UiRequest & UiRequestDrawError)
  {
    bool hasResult = false;
    SmtpManager::Result result;
    {
      UiModel::Lock model(m_Model.get());
      if (!model->m_SmtpErrorResults.empty())
      {
        result = model->m_SmtpErrorResults.front();
        model->m_SmtpErrorResults.pop_front();
        hasResult = true;
      }
    }

    if (hasResult)
    {
      SmtpResultHandlerError(result);
    }
  }

  if (p_UiRequest & UiRequestHandleConnected)
  {
    HandleConnected();
  }

  if (p_UiRequest & UiRequestSpecialFolders)
  {
    m_Model->HandleSpecialFolders();
  }
}

void Ui::SmtpResultHandlerError(const SmtpManager::Result& p_Result)
{
  bool saveDraft = false;
  SmtpManager::Action smtpAction = p_Result.m_Action;
  std::string draftMessage;
  const std::string draftsFolder = m_Model->GetDraftsFolder();

  if (!draftsFolder.empty())
  {
    if (smtpAction.m_IsSendCreatedMessage)
    {
      m_DialogView->SetDialogMessage("Failed sending queued message, uploading draft");
      saveDraft = true;
      draftMessage = smtpAction.m_CreatedMsg;
    }
    else
    {
      const std::string errStr = Smtp::GetErrorMessage(p_Result.m_SmtpStatus);
      const std::string errMsg = (!errStr.empty())
        ? "Send failed (" + errStr + ")."
        : "Send failed.";
      const std::string msg =
        (smtpAction.m_ComposeDraftUid != 0)
        ? errMsg + " Overwrite draft (y) or queue send (n)?"
        : errMsg + " Save draft (y) or queue send (n)?";
      if (m_DialogView->PromptYesNo(msg))
      {
        saveDraft = true;
        smtpAction.m_IsSendMessage = false;
        smtpAction.m_IsCreateMessage = true;

        SmtpManager::Result smtpResult = m_SmtpManager->SyncAction(smtpAction);
        if (smtpResult.m_SmtpStatus == SmtpStatusOk)
        {
          draftMessage = smtpResult.m_Message;
        }
        else
        {
          m_DialogView->SetDialogMessage("Message creation failed", true /* p_Warn */);
          return;
        }
      }
    }
  }

  if (saveDraft)
  {
    ImapManager::Action imapAction;
    imapAction.m_UploadDraft = true;
    imapAction.m_Folder = draftsFolder;
    imapAction.m_Msg = draftMessage;
    m_Model->GetImapManager()->AsyncAction(imapAction);

    if (smtpAction.m_ComposeDraftUid != 0)
    {
      const std::string trashFolder = m_Model->GetTrashFolder();
      m_Model->MoveMessages(std::set<uint32_t>({ smtpAction.m_ComposeDraftUid }), draftsFolder, trashFolder);
      m_MessageListView->SyncCursor();
      m_Model->SetHasRequestedUids(trashFolder, false);
    }

    m_Model->SetHasRequestedUids(draftsFolder, false);
  }
  else
  {
    if (!smtpAction.m_IsSendCreatedMessage)
    {
      smtpAction.m_IsSendMessage = false;
      smtpAction.m_IsCreateMessage = true;
      SmtpManager::Result smtpResult = m_SmtpManager->SyncAction(smtpAction);
      if (smtpResult.m_SmtpStatus == SmtpStatusOk)
      {
        draftMessage = smtpResult.m_Message;
      }
      else
      {
        m_DialogView->SetDialogMessage("Message creation failed", true /* p_Warn */);
      }
    }

    if (!draftMessage.empty())
    {
      OfflineQueue::PushOutboxMessage(draftMessage);
      m_DialogView->SetDialogMessage("Message queued for sending");
    }
  }

  AsyncUiRequest(UiRequestDrawAll);
}

void Ui::Run()
{
  DrawAll();
  int64_t uiIdleTime = 0;
  LOG_INFO("entering ui loop");
  AppUtil::InitUiSignalHandlers();
  raw();

  while (s_Running && !AppUtil::IsTerminateRequested())
  {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    FD_SET(m_Pipe[0], &fds);
    int maxfd = std::max(STDIN_FILENO, m_Pipe[0]);
    struct timeval tv = { 1, 0 }; // uiIdleTime logic below is dependent on timeout value
    int rv = select(maxfd + 1, &fds, NULL, NULL, &tv);

    if (rv == 0)
    {
      if (++uiIdleTime >= 600) // ui idle refresh every 10 minutes
      {
        PerformUiRequest(UiRequestDrawAll);
        uiIdleTime = 0;
      }

      continue;
    }

    uiIdleTime = 0;

    if (FD_ISSET(STDIN_FILENO, &fds))
    {
      wint_t key = 0;
      UiKeyInput::GetWch(&key);

      LOG_LATENCY_START(LatencyKeyPress);

      m_MessageListView->SyncCursor();

      if (static_cast<int>(key) == m_KeyTerminalResize)
      {
        CleanupWindows();
        InitWindows();
        DrawAll();
        continue;
      }

      UiScreenView* screenView = GetScreenView(m_State);
      if (screenView != nullptr)
      {
        screenView->KeyHandler(key);
      }

      continue;
    }

    if (FD_ISSET(m_Pipe[0], &fds))
    {
      int len = 0;
      ioctl(m_Pipe[0], FIONREAD, &len);
      if (len > 0)
      {
        len = std::min(len, 256);
        std::vector<char> buf(len);
        LOG_IF_NOT_EQUAL(read(m_Pipe[0], &buf[0], len), len);
        char uiRequest = UiRequestNone;
        for (int i = 0; i < len; ++i)
        {
          uiRequest |= buf[i];
        }

        PerformUiRequest(uiRequest);
      }
    }

  }

  if (AppUtil::IsTerminateRequested())
  {
    const int sig = AppUtil::GetTerminateSignal();
    LOG_WARNING("termination requested by signal %d (%s)", sig, AppUtil::GetSigName(sig).c_str());
    SetRunning(false);

    m_ComposeView->OnTerminate();
  }

  noraw();
  AppUtil::CleanupUiSignalHandlers();
  LOG_INFO("exiting ui loop");

  return;
}

void Ui::SetState(Ui::State p_State)
{
  if ((p_State == StateAddressList) || (p_State == StateFromAddressList) || (p_State == StateFileList))
  {
    // going to address or file list
    m_LastMessageState = m_State;
    m_State = p_State;
  }
  else if ((m_State != StateAddressList) && (m_State != StateFromAddressList) && (m_State != StateFileList))
  {
    // normal state transition
    m_LastState = m_State;
    m_State = p_State;
  }
  else
  {
    // exiting address or file list
    m_State = p_State;
    return;
  }

  UiScreenView* screenView = GetScreenView(m_State);
  if (screenView != nullptr)
  {
    screenView->OnEnter();
  }
}

UiScreenView* Ui::GetScreenView(State p_State)
{
  switch (p_State)
  {
    case StateViewMessageList:
      return m_MessageListView.get();

    case StateViewMessage:
      return m_MessageView.get();

    case StateGotoFolder:
    case StateMoveToFolder:
      return m_FolderListView.get();

    case StateComposeMessage:
    case StateComposeCopyMessage:
    case StateReplyAllMessage:
    case StateReplySenderMessage:
    case StateForwardMessage:
    case StateForwardAttachedMessage:
      return m_ComposeView.get();

    case StateAddressList:
    case StateFromAddressList:
      return m_AddressListView.get();

    case StateFileList:
      return m_FileListView.get();

    case StateViewPartList:
      return m_PartListView.get();

    default:
      return nullptr;
  }
}

void Ui::SetImapManager(std::shared_ptr<ImapManager> p_ImapManager)
{
  m_Model->SetImapManager(p_ImapManager);
  if (m_Model->GetImapManager())
  {
    m_Model->GetImapManager()->SetCurrentFolder(m_CurrentFolder);
  }
}

void Ui::SetSmtpManager(std::shared_ptr<SmtpManager> p_SmtpManager)
{
  m_SmtpManager = p_SmtpManager;
}

void Ui::ResetImapManager()
{
  m_Model->ResetImapManager();
}

void Ui::ResetSmtpManager()
{
  m_SmtpManager.reset();
}

void Ui::SetTrashFolder(const std::string& p_TrashFolder)
{
  m_Model->SetTrashFolder(p_TrashFolder);
}

void Ui::SetSpamFolder(const std::string& p_SpamFolder)
{
  m_Model->SetSpamFolder(p_SpamFolder);
}

void Ui::SetDraftsFolder(const std::string& p_DraftsFolder)
{
  m_Model->SetDraftsFolder(p_DraftsFolder);
}

void Ui::SetSentFolder(const std::string& p_SentFolder)
{
  m_Model->SetSentFolder(p_SentFolder);
}

void Ui::SetClientStoreSent(bool p_ClientStoreSent)
{
  m_Model->SetClientStoreSent(p_ClientStoreSent);
}

void Ui::SetSpecialFoldersHandler(const std::function<void(const std::map<std::string, std::string>&)>& p_Handler)
{
  m_Model->SetSpecialFoldersHandler(p_Handler);
}

void Ui::ResponseHandler(const ImapManager::Request& p_Request, const ImapManager::Response& p_Response)
{
  m_Model->ResponseHandler(p_Request, p_Response);
}

void Ui::ResultHandler(const ImapManager::Action& p_Action, const ImapManager::Result& p_Result)
{
  m_Model->ResultHandler(p_Action, p_Result);
}

void Ui::SmtpResultHandler(const SmtpManager::Result& p_Result)
{
  m_Model->SmtpResultHandler(p_Result);
}

void Ui::StatusHandler(const StatusUpdate& p_StatusUpdate)
{
  m_Model->StatusHandler(p_StatusUpdate);
}

void Ui::SearchHandler(const ImapManager::SearchQuery& p_SearchQuery, const ImapManager::SearchResult& p_SearchResult)
{
  m_Model->SearchHandler(p_SearchQuery, p_SearchResult);
}

bool Ui::IsConnected()
{
  UiModel::Lock model(m_Model.get());
  return model->m_Status.IsSet(Status::FlagConnected);
}

void Ui::SetLastStateOrMessageList()
{
  bool isHeaderUidsEmpty = false;
  if (m_MessageListSearch)
  {
    isHeaderUidsEmpty = false;
  }
  else
  {
    UiModel::Lock model(m_Model.get());
    isHeaderUidsEmpty = model->GetHeaderUids(m_CurrentFolder).empty();
  }

  if (isHeaderUidsEmpty)
  {
    SetState(StateViewMessageList);
  }
  else
  {
    SetState(m_LastState);
  }

  m_MessageListView->UpdateIndexFromUid();
}

void Ui::Quit()
{
  if (m_QuitWithoutConfirm || m_DialogView->PromptYesNo("Quit nmail (y/n)?"))
  {
    {
      UiModel::Lock model(m_Model.get());
      StatusUpdate statusUpdate;
      statusUpdate.SetFlags = model->m_Status.IsSet(Status::FlagConnected) ? Status::FlagDisconnecting
                                                                           : Status::FlagExiting;
      model->m_Status.Update(statusUpdate);
    }

    SetRunning(false);
    LOG_DEBUG("stop thread");
  }
}

void Ui::StartSync()
{
  bool isSyncIdle = false;
  {
    UiModel::Lock model(m_Model.get());
    isSyncIdle = (model->m_SyncState == SyncStateIdle);
  }

  if (!isSyncIdle)
  {
    m_DialogView->SetDialogMessage("Sync already in progress");
    return;
  }

  if (!IsConnected())
  {
    m_DialogView->SetDialogMessage("Cannot sync while offline");
    return;
  }

  if (AppUtil::GetReadOnly())
  {
    m_DialogView->SetDialogMessage("Cannot sync in read-only mode");
    return;
  }

  LOG_DEBUG("sync started (manual)");
  {
    UiModel::Lock model(m_Model.get());
    model->m_SyncState = SyncStateStarted;
    model->m_HasPrefetchRequestedFolders = true;
  }

  ImapManager::Request request;
  request.m_PrefetchLevel = PrefetchLevelFullSync;
  request.m_GetFolders = true;
  LOG_DEBUG("prefetch req folders");
  m_Model->GetImapManager()->PrefetchRequest(request);
}

bool Ui::IsRunning()
{
  return s_Running;
}

void Ui::SetRunning(bool p_Running)
{
  s_Running = p_Running;
}

void Ui::HandleConnected()
{
  if (IsConnected() && !AppUtil::GetReadOnly())
  {
    const std::string draftsFolder = m_Model->GetDraftsFolder();
    if (!draftsFolder.empty())
    {
      std::vector<std::string> draftMsgs = OfflineQueue::PopDraftMessages();
      for (const auto& draftMsg : draftMsgs)
      {
        m_DialogView->SetDialogMessage("Uploading queued draft messages");

        ImapManager::Action imapAction;
        imapAction.m_UploadDraft = true;
        imapAction.m_Folder = draftsFolder;
        imapAction.m_Msg = draftMsg;
        m_Model->GetImapManager()->AsyncAction(imapAction);

        m_Model->SetHasRequestedUids(draftsFolder, false);
      }
    }
    else if (OfflineQueue::HasDraftMessages())
    {
      // keep queued drafts until a drafts folder is configured
      m_DialogView->SetDialogMessage("Drafts folder not configured, queued draft kept", true /* p_Warn */);
    }

    std::vector<std::string> outboxMsgs = OfflineQueue::PopOutboxMessages();
    for (const auto& outboxMsg : outboxMsgs)
    {
      m_DialogView->SetDialogMessage("Sending queued messages");

      Header header;
      header.SetHeaderData(outboxMsg, "", 0); // no server time for local message

      SmtpManager::Action smtpAction;
      smtpAction.m_CreatedMsg = outboxMsg;
      smtpAction.m_From = header.GetFrom();
      smtpAction.m_To = header.GetTo();
      smtpAction.m_Cc = header.GetCc();
      smtpAction.m_Bcc = header.GetBcc();
      smtpAction.m_IsSendCreatedMessage = true;
      smtpAction.m_FormatFlowed = (m_ComposeLineWrap == LineWrapFormatFlowed);

      m_SmtpManager->AsyncAction(smtpAction);
    }
  }
}

void Ui::OnWakeUp()
{
  LOG_DEBUG_FUNC(STR());

  // Dummy request to trigger exit of idle
  ImapManager::Request request;
  LOG_DEBUG("async req none");
  m_Model->GetImapManager()->AsyncRequest(request);
}

void Ui::SetCurrentFolder(const std::string& p_Folder)
{
  m_CurrentFolder = p_Folder;
  if (m_Model->GetImapManager() && !m_CurrentFolder.empty())
  {
    m_Model->GetImapManager()->SetCurrentFolder(m_CurrentFolder);
  }
}
