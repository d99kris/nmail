// uiviewbase.cpp
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "uiviewbase.h"

#include "strutil.h"
#include "textutil.h"
#include "textwrap.h"
#include "uicolorconfig.h"
#include "uidialogview.h"
#include "uikeyconfig.h"
#include "uimodel.h"

UiViewBase::UiViewBase(Ui* p_Ui)
  : m_Ui(p_Ui)
  , m_Model(p_Ui->m_Model.get())
  , m_CurrentFolderUid(p_Ui->m_CurrentFolderUid)
  , m_CurrentFolder(p_Ui->m_CurrentFolder)
  , m_PreviousFolder(p_Ui->m_PreviousFolder)
  , m_State(p_Ui->m_State)
  , m_LastState(p_Ui->m_LastState)
  , m_LastMessageState(p_Ui->m_LastMessageState)
  , m_MessageListSearch(p_Ui->m_MessageListSearch)
  , m_IsAutoMove(p_Ui->m_IsAutoMove)
  , m_DialogWin(p_Ui->m_DialogWin)
  , m_HelpWin(p_Ui->m_HelpWin)
  , m_MainWin(p_Ui->m_MainWin)
  , m_ScreenWidth(p_Ui->m_ScreenWidth)
  , m_MainWinHeight(p_Ui->m_MainWinHeight)
  , m_HelpView(p_Ui->m_HelpView)
  , m_External(p_Ui->m_External)
  , m_SmtpManager(p_Ui->m_SmtpManager)
  , m_Name(p_Ui->m_Name)
  , m_Plaintext(p_Ui->m_Plaintext)
  , m_CurrentMessageProcessFlowed(p_Ui->m_CurrentMessageProcessFlowed)
  , m_ComposeMailto(p_Ui->m_ComposeMailto)
  , m_CurrentMarkdownHtmlCompose(p_Ui->m_CurrentMarkdownHtmlCompose)
  , m_MaxComposeLineLength(p_Ui->m_MaxComposeLineLength)
  , m_ComposeView(p_Ui->m_ComposeView)
  , m_MessageListCurrentIndex(p_Ui->m_MessageListCurrentIndex)
  , m_MessageFindMatchLine(p_Ui->m_MessageFindMatchLine)
  , m_MessageFindQuery(p_Ui->m_MessageFindQuery)
  , m_MessageViewLineOffset(p_Ui->m_MessageViewLineOffset)
  , m_MessageViewToggledSeen(p_Ui->m_MessageViewToggledSeen)
  , m_MaxViewLineLength(p_Ui->m_MaxViewLineLength)
  , m_MessageView(p_Ui->m_MessageView)
  , m_MessageListView(p_Ui->m_MessageListView)
  , m_TopWin(p_Ui->m_TopWin)
  , m_MessageListSearchQuery(p_Ui->m_MessageListSearchQuery)
  , m_MessageListCurrentUid(p_Ui->m_MessageListCurrentUid)
  , m_DialogView(p_Ui->m_DialogView)
{
  InitKeys();
  InitAttrs();
}

UiViewBase::~UiViewBase()
{
}

void UiViewBase::RefreshWin(WINDOW* p_Win)
{
  m_Ui->RefreshWin(p_Win);
}

void UiViewBase::TerminalControlPause()
{
  m_Ui->TerminalControlPause();
}

void UiViewBase::TerminalControlResume()
{
  m_Ui->TerminalControlResume();
}

void UiViewBase::DrawAll()
{
  m_Ui->DrawAll();
}

void UiViewBase::SetState(Ui::State p_State)
{
  m_Ui->SetState(p_State);
}

void UiViewBase::SetDialogMessage(const std::string& p_DialogMessage, bool p_Warn /*= false */)
{
  m_DialogView->SetDialogMessage(p_DialogMessage, p_Warn);
}

bool UiViewBase::PromptString(const std::string& p_Prompt, const std::string& p_Action, std::string& p_Entry)
{
  return m_DialogView->PromptString(p_Prompt, p_Action, p_Entry);
}

void UiViewBase::Quit()
{
  m_Ui->Quit();
}

void UiViewBase::SetCurrentFolder(const std::string& p_Folder)
{
  m_Ui->SetCurrentFolder(p_Folder);
}

void UiViewBase::SetLastStateOrMessageList()
{
  m_Ui->SetLastStateOrMessageList();
}

void UiViewBase::AsyncUiRequest(char p_UiRequest)
{
  m_Ui->AsyncUiRequest(p_UiRequest);
}

bool UiViewBase::IsConnected()
{
  return m_Ui->IsConnected();
}

bool UiViewBase::PromptYesNo(const std::string& p_Prompt)
{
  return m_DialogView->PromptYesNo(p_Prompt);
}

void UiViewBase::StartSync()
{
  m_Ui->StartSync();
}

bool UiViewBase::IsValidTextKey(int p_Key)
{
  return ((p_Key >= 0x20) || (p_Key == 0xA));
}

bool UiViewBase::HandleListKey(int p_Key, int& p_Index)
{
  if (p_Key == m_KeyUp)
  {
    --p_Index;
  }
  else if (p_Key == m_KeyDown)
  {
    ++p_Index;
  }
  else if (p_Key == m_KeyPrevPage)
  {
    p_Index = p_Index - m_MainWinHeight;
  }
  else if (p_Key == m_KeyNextPage)
  {
    p_Index = p_Index + m_MainWinHeight;
  }
  else if (p_Key == m_KeyHome)
  {
    p_Index = 0;
  }
  else if (p_Key == m_KeyEnd)
  {
    p_Index = std::numeric_limits<int>::max() - 1;
  }
  else
  {
    return false;
  }

  return true;
}

bool UiViewBase::HandleLineKey(int p_Key, std::wstring& p_Str, int& p_Pos)
{
  // move and delete by character cluster, to not split emoji sequences or combining marks
  p_Pos = StrUtil::Bound(0, p_Pos, (int)p_Str.size());
  if (p_Key == m_KeyLeft)
  {
    if (p_Pos > 0)
    {
      p_Pos = (int)TextWrap::GetClusterStart(p_Str, p_Pos - 1);
    }
  }
  else if (p_Key == m_KeyRight)
  {
    p_Pos = (int)TextWrap::GetClusterEnd(p_Str, p_Pos);
  }
  else if ((p_Key == m_KeyBackspace) || (p_Key == m_KeyBackspaceAlt))
  {
    if (p_Pos > 0)
    {
      const int startPos = (int)TextWrap::GetClusterStart(p_Str, p_Pos - 1);
      p_Str.erase(startPos, p_Pos - startPos);
      p_Pos = startPos;
    }
  }
  else if ((p_Key == m_KeyDeleteChar) || (p_Key == m_KeyDeleteCharAfterCursor))
  {
    if (p_Pos < (int)p_Str.size())
    {
      const int endPos = (int)TextWrap::GetClusterEnd(p_Str, p_Pos);
      p_Str.erase(p_Pos, endPos - p_Pos);
    }
  }
  else if (p_Key == m_KeyDeleteLineAfterCursor)
  {
    TextUtil::DeleteToNextMatch(p_Str, p_Pos, 0, L"\n");
  }
  else if (p_Key == m_KeyDeleteLineBeforeCursor)
  {
    TextUtil::DeleteToPrevMatch(p_Str, p_Pos, -1, L"\n");
  }
  else if (p_Key == m_KeyBeginLine)
  {
    TextUtil::JumpToPrevMatch(p_Str, p_Pos, -1, L"\n");
  }
  else if (p_Key == m_KeyEndLine)
  {
    TextUtil::JumpToNextMatch(p_Str, p_Pos, 0, L"\n");
  }
  else if (p_Key == m_KeyBackwardWord)
  {
    TextUtil::JumpToPrevMatch(p_Str, p_Pos, -2, L" \n");
  }
  else if (p_Key == m_KeyForwardWord)
  {
    TextUtil::JumpToNextMatch(p_Str, p_Pos, 1, L" \n");
  }
  else if (p_Key == m_KeyBackwardKillWord)
  {
    TextUtil::DeleteToPrevMatch(p_Str, p_Pos, -1, L" \n");
  }
  else if (p_Key == m_KeyKillWord)
  {
    TextUtil::DeleteToNextMatch(p_Str, p_Pos, 0, L" \n");
  }
  else
  {
    return false;
  }

  return true;
}

bool UiViewBase::HandleTextKey(int p_Key, std::wstring& p_Str, int& p_Pos)
{
  if (IsValidTextKey(p_Key))
  {
    p_Str.insert(p_Pos++, 1, p_Key);
  }
  else
  {
    return false;
  }

  return true;
}

bool UiViewBase::HandleDocKey(int p_Key, std::wstring& p_Str, int& p_Pos)
{
  if (p_Key == m_KeyHome)
  {
    p_Pos = 0;
  }
  else if (p_Key == m_KeyEnd)
  {
    p_Pos = p_Str.size();
  }
  else
  {
    return false;
  }

  return true;
}

void UiViewBase::InitKeys()
{
  m_KeyPrevMsg = UiKeyConfig::GetKey("key_prev_msg");
  m_KeyNextMsg = UiKeyConfig::GetKey("key_next_msg");
  m_KeyReplyAll = UiKeyConfig::GetKey("key_reply_all");
  m_KeyReplySender = UiKeyConfig::GetKey("key_reply_sender");
  m_KeyForward = UiKeyConfig::GetKey("key_forward");
  m_KeyForwardAttached = UiKeyConfig::GetKey("key_forward_attached");
  m_KeyDelete = UiKeyConfig::GetKey("key_delete");
  m_KeyCompose = UiKeyConfig::GetKey("key_compose");
  m_KeyComposeCopy = UiKeyConfig::GetKey("key_compose_copy");
  m_KeyToggleUnread = UiKeyConfig::GetKey("key_toggle_unread");
  m_KeyMove = UiKeyConfig::GetKey("key_move");
  m_KeyAutoMove = UiKeyConfig::GetKey("key_auto_move");
  m_KeyRefresh = UiKeyConfig::GetKey("key_refresh");
  m_KeyQuit = UiKeyConfig::GetKey("key_quit");
  m_KeyToggleTextHtml = UiKeyConfig::GetKey("key_toggle_text_html");
  m_KeyCancel = UiKeyConfig::GetKey("key_cancel");
  m_KeySend = UiKeyConfig::GetKey("key_send");
  m_KeyDeleteCharAfterCursor = UiKeyConfig::GetKey("key_delete_char_after_cursor");
  m_KeyDeleteLineAfterCursor = UiKeyConfig::GetKey("key_delete_line_after_cursor");
  m_KeyDeleteLineBeforeCursor = UiKeyConfig::GetKey("key_delete_line_before_cursor");
  m_KeyOpen = UiKeyConfig::GetKey("key_open");
  m_KeyBack = UiKeyConfig::GetKey("key_back");
  m_KeyGotoFolder = UiKeyConfig::GetKey("key_goto_folder");
  m_KeyGotoInbox = UiKeyConfig::GetKey("key_goto_inbox");
  m_KeyToSelect = UiKeyConfig::GetKey("key_to_select");
  m_KeySaveFile = UiKeyConfig::GetKey("key_save_file");
  m_KeySaveAllFiles = UiKeyConfig::GetKey("key_save_all_files");
  m_KeySaveMarkdown = UiKeyConfig::GetKey("key_save_markdown");
  m_KeyExtEditor = UiKeyConfig::GetKey("key_ext_editor");
  m_KeyExtPager = UiKeyConfig::GetKey("key_ext_pager");
  m_KeyPostpone = UiKeyConfig::GetKey("key_postpone");
  m_KeyOtherCmdHelp = UiKeyConfig::GetKey("key_othercmd_help");
  m_KeyExport = UiKeyConfig::GetKey("key_export");
  m_KeyImport = UiKeyConfig::GetKey("key_import");
  m_KeyRichHeader = UiKeyConfig::GetKey("key_rich_header");
  m_KeyExtHtmlViewer = UiKeyConfig::GetKey("key_ext_html_viewer");
  m_KeyExtHtmlPreview = UiKeyConfig::GetKey("key_ext_html_preview");
  m_KeyExtMsgViewer = UiKeyConfig::GetKey("key_ext_msg_viewer");
  m_KeySearch = UiKeyConfig::GetKey("key_search");
  m_KeyFind = UiKeyConfig::GetKey("key_find");
  m_KeyFindNext = UiKeyConfig::GetKey("key_find_next");
  m_KeySync = UiKeyConfig::GetKey("key_sync");
  m_KeyToggleMarkdownCompose = UiKeyConfig::GetKey("key_toggle_markdown_compose");
  m_KeyNextLink = UiKeyConfig::GetKey("key_next_link");
  m_KeyPrevLink = UiKeyConfig::GetKey("key_prev_link");
  m_KeyOpenLink = UiKeyConfig::GetKey("key_open_link");
  m_KeyBackwardWord = UiKeyConfig::GetKey("key_backward_word");
  m_KeyForwardWord = UiKeyConfig::GetKey("key_forward_word");
  m_KeyBackwardKillWord = UiKeyConfig::GetKey("key_backward_kill_word");
  m_KeyKillWord = UiKeyConfig::GetKey("key_kill_word");
  m_KeyBeginLine = UiKeyConfig::GetKey("key_begin_line");
  m_KeyEndLine = UiKeyConfig::GetKey("key_end_line");
  m_KeyPrevPage = UiKeyConfig::GetKey("key_prev_page");
  m_KeyNextPage = UiKeyConfig::GetKey("key_next_page");
  m_KeyPrevPageCompose = UiKeyConfig::GetKey("key_prev_page_compose");
  m_KeyNextPageCompose = UiKeyConfig::GetKey("key_next_page_compose");
  m_KeyFilterSortReset = UiKeyConfig::GetKey("key_filter_sort_reset");
  m_KeyFilterShowUnread = UiKeyConfig::GetKey("key_filter_show_unread");
  m_KeyFilterShowHasAttachments = UiKeyConfig::GetKey("key_filter_show_has_attachments");
  m_KeyFilterShowCurrentDate = UiKeyConfig::GetKey("key_filter_show_current_date");
  m_KeyFilterShowCurrentName = UiKeyConfig::GetKey("key_filter_show_current_name");
  m_KeyFilterShowCurrentSubject = UiKeyConfig::GetKey("key_filter_show_current_subject");
  m_KeySortUnread = UiKeyConfig::GetKey("key_sort_unread");
  m_KeySortHasAttachments = UiKeyConfig::GetKey("key_sort_has_attachments");
  m_KeySortDate = UiKeyConfig::GetKey("key_sort_date");
  m_KeySortName = UiKeyConfig::GetKey("key_sort_name");
  m_KeySortSubject = UiKeyConfig::GetKey("key_sort_subject");
  m_KeyJumpTo = UiKeyConfig::GetKey("key_jump_to");
  m_KeyToggleFullHeader = UiKeyConfig::GetKey("key_toggle_full_header");
  m_KeyToggleCompactHeader = UiKeyConfig::GetKey("key_toggle_compact_header");
  m_KeySelectItem = UiKeyConfig::GetKey("key_select_item");
  m_KeySelectAll = UiKeyConfig::GetKey("key_select_all");
  m_KeySearchShowFolder = UiKeyConfig::GetKey("key_search_show_folder");
  m_KeySearchCurrentSubject = UiKeyConfig::GetKey("key_search_current_subject");
  m_KeySearchCurrentName = UiKeyConfig::GetKey("key_search_current_name");
  m_KeySpell = UiKeyConfig::GetKey("key_spell");
  m_KeySearchServer = UiKeyConfig::GetKey("key_search_server");

  m_KeyReturn = UiKeyConfig::GetKey("key_return");
  m_KeyEnter = UiKeyConfig::GetKey("key_enter");
  m_KeyLeft = UiKeyConfig::GetKey("key_left");
  m_KeyRight = UiKeyConfig::GetKey("key_right");

  m_KeyDown = UiKeyConfig::GetKey("key_down");
  m_KeyUp = UiKeyConfig::GetKey("key_up");
  m_KeyEnd = UiKeyConfig::GetKey("key_end");
  m_KeyHome = UiKeyConfig::GetKey("key_home");
  m_KeyBackspace = UiKeyConfig::GetKey("key_backspace");
  m_KeyBackspaceAlt = UiKeyConfig::GetKey("key_backspace_alt");
  m_KeyDeleteChar = UiKeyConfig::GetKey("key_delete_char");
  m_KeySpace = UiKeyConfig::GetKey("key_space");
  m_KeyTab = UiKeyConfig::GetKey("key_tab");
  m_KeyTerminalResize = UiKeyConfig::GetKey("key_terminal_resize");
}

void UiViewBase::InitAttrs()
{
  m_AttrsRegularText = UiColorConfig::GetAttrs("color_regular_text");
  m_AttrsDialog = UiColorConfig::GetAttrs("color_dialog");
  m_AttrsHelpDesc = UiColorConfig::GetAttrs("color_help_desc");
  m_AttrsHelpKeys = UiColorConfig::GetAttrs("color_help_keys");
  m_AttrsHighlightedText = UiColorConfig::GetAttrs("color_highlighted_text");
  m_AttrsQuotedText = UiColorConfig::GetAttrs("color_quoted_text");
  m_AttrsBoldText = UiColorConfig::GetAttrs("color_bold_text");
  m_AttrsHeaderName = UiColorConfig::GetAttrs("color_header_name");
  m_AttrsCodeText = UiColorConfig::GetAttrs("color_code_text");
  m_AttrsItalicText = UiColorConfig::GetAttrs("color_italic_text");
  m_AttrsStrikeText = UiColorConfig::GetAttrs("color_strike_text");
  m_AttrsSignatureText = UiColorConfig::GetAttrs("color_signature_text");
  m_AttrsLinkText = UiColorConfig::GetAttrs("color_link_text");
  m_AttrsHighlightedLink = UiColorConfig::GetAttrs("color_highlighted_link");
  m_AttrsHiddenFile = UiColorConfig::GetAttrs("color_hidden_file");
  m_AttrsAddressText = UiColorConfig::GetAttrs("color_address_text");
  m_AttrsTopBar = UiColorConfig::GetAttrs("color_top_bar");
  m_AttrsSelectedItem = UiColorConfig::GetAttrs("color_selected_item");
  m_AttrsSelectedHighlighted = UiColorConfig::GetAttrs("color_selected_highlighted");
}
