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
#include <sstream>

#include <sys/ioctl.h>
#include <unistd.h>

#include "addressbook.h"
#include "apputil.h"
#include "colorutil.h"
#include "extutil.h"
#include "fileutil.h"
#include "loghelp.h"
#include "mailutil.h"
#include "maphelp.h"
#include "offlinequeue.h"
#include "sethelp.h"
#include "sleepdetect.h"
#include "status.h"
#include "strutil.h"
#include "textutil.h"
#include "textwrap.h"
#include "uikeyconfig.h"
#include "uikeyinput.h"

bool Ui::s_Running = false;

Ui::Ui(const std::string& p_Inbox, const std::string& p_Address, const std::string& p_Name,
       uint32_t p_PrefetchLevel, bool p_PrefetchAllHeaders)
  : m_Inbox(p_Inbox)
  , m_Address(p_Address)
  , m_Name(p_Name)
  , m_PrefetchLevel(p_PrefetchLevel)
  , m_PrefetchAllHeaders(p_PrefetchAllHeaders)
{
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
  const std::map<std::string, std::string> defaultConfig =
  {
    { "compose_line_wrap", "0" },
    { "respect_format_flowed", "1" },
    { "rewrap_quoted_lines", "1" },
    { "help_enabled", "1" },
    { "picker_width", "58" },
    { "persist_file_selection_dir", "1" },
    { "persist_find_query", "0" },
    { "persist_folder_filter", "1" },
    { "persist_search_query", "0" },
    { "plain_text", "0" },
    { "show_progress", "1" },
    { "new_msg_bell", "1" },
    { "quit_without_confirm", "1" },
    { "send_without_confirm", "0" },
    { "cancel_without_confirm", "0" },
    { "postpone_without_confirm", "0" },
    { "delete_without_confirm", "0" },
    { "show_embedded_images", "1" },
    { "show_rich_header", "0" },
    { "markdown_html_compose", "0" },
    { "markdown_view", "1" },
    { "markdown_view_links", "1" },
    { "plain_view_links", "1" },
    { "align_header_labels", "1" },
    { "colors_enabled", "1" },
    { "attachment_indicator", "\xF0\x9F\x93\x8E" },
    { "bottom_reply", "0" },
    { "compose_backup_interval_sec", "10" },
    { "persist_sortfilter", "1" },
    { "persist_selection_on_sortfilter_change", "1" },
    { "unread_indicator", "N" },
    { "invalid_input_notify", "1" },
    { "full_header_include_local", "0" },
    { "show_compact_header", "1" },
    { "tab_size", "8" },
    { "search_show_folder", "1" },
    { "localized_subject_prefixes", "" },
    { "signature", "0" },
    { "terminal_title", "" },
    { "top_bar_show_version", "0" },
    { "unwrap_quoted_lines", "1" },
    { "automove_trash_allow", "1" },
    { "top_bar_show_message_count", "0" },
  };
  const std::map<std::string, std::string> renamedConfig =
  {
    { "compose_backup_interval", "compose_backup_interval_sec" },
  };
  const std::string configPath(AppUtil::GetApplicationDir() + std::string("ui.conf"));
  m_Config = Config(configPath, defaultConfig, false, renamedConfig);
  m_Config.LogParams();

  m_TerminalTitle = m_Config.Get("terminal_title");

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

  m_ComposeLineWrap = StrUtil::ToInteger(m_Config.Get("compose_line_wrap"));
  m_RespectFormatFlowed = m_Config.Get("respect_format_flowed") == "1";
  m_RewrapQuotedLines = m_Config.Get("rewrap_quoted_lines") == "1";
  m_HelpEnabled = m_Config.Get("help_enabled") == "1";
  {
    // fraction (0.618 or 61.8%), columns (2 or more), or 0 for classic layout
    std::string pickerWidthStr = StrUtil::Trim(m_Config.Get("picker_width"));
    const bool isPercent = !pickerWidthStr.empty() && (pickerWidthStr.back() == '%');
    if (isPercent)
    {
      pickerWidthStr.pop_back();
    }

    std::istringstream iss(pickerWidthStr);
    iss.imbue(std::locale::classic());
    double pickerWidth = -1;
    iss >> pickerWidth;
    bool isValid = !iss.fail() && iss.eof() && (pickerWidth >= 0);
    if (isValid && isPercent)
    {
      isValid = (pickerWidth > 0) && (pickerWidth <= 100);
      pickerWidth /= 100;
    }
    else if (isValid && (pickerWidth > 1))
    {
      isValid = (pickerWidth >= 2) && (pickerWidth == (int)pickerWidth);
    }

    if (isValid)
    {
      m_PickerWidth = pickerWidth;
    }
    else
    {
      LOG_WARNING("invalid picker_width \"%s\"", m_Config.Get("picker_width").c_str());
    }
  }
  m_PersistFileSelectionDir = m_Config.Get("persist_file_selection_dir") == "1";
  m_PersistFindQuery = m_Config.Get("persist_find_query") == "1";
  m_PersistFolderFilter = m_Config.Get("persist_folder_filter") == "1";
  m_PersistSearchQuery = m_Config.Get("persist_search_query") == "1";
  m_Plaintext = m_Config.Get("plain_text") == "1";
  m_MarkdownHtmlCompose = m_Config.Get("markdown_html_compose") == "1";
  m_MarkdownView = m_Config.Get("markdown_view") == "1";
  m_MarkdownViewLinks = m_MarkdownView && (m_Config.Get("markdown_view_links") == "1");
  ExtUtil::SetHtmlToTextLinks(m_MarkdownViewLinks);
  m_PlainViewLinks = m_Config.Get("plain_view_links") == "1";
  m_AlignHeaderLabels = m_Config.Get("align_header_labels") == "1";
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

  m_ShowProgress = StrUtil::ToInteger(m_Config.Get("show_progress"));
  m_NewMsgBell = m_Config.Get("new_msg_bell") == "1";
  m_QuitWithoutConfirm = m_Config.Get("quit_without_confirm") == "1";
  m_SendWithoutConfirm = m_Config.Get("send_without_confirm") == "1";
  m_CancelWithoutConfirm = m_Config.Get("cancel_without_confirm") == "1";
  m_PostponeWithoutConfirm = m_Config.Get("postpone_without_confirm") == "1";
  m_DeleteWithoutConfirm = m_Config.Get("delete_without_confirm") == "1";
  m_ShowEmbeddedImages = m_Config.Get("show_embedded_images") == "1";
  m_ShowRichHeader = m_Config.Get("show_rich_header") == "1";

  m_ColorsEnabled = m_Config.Get("colors_enabled") == "1";

  LOG_IF_NONZERO(pipe(m_Pipe));

  if (m_ColorsEnabled && !has_colors())
  {
    LOG_WARNING("terminal does not support colors");
    m_ColorsEnabled = false;
  }

  if (m_ColorsEnabled)
  {
    start_color();
    assume_default_colors(-1, -1);

    const std::string defaultAddressFg = (COLORS > 8) ? "gray" : "";
    const std::string defaultQuotedFg = (COLORS > 8) ? "gray" : "";
    const std::string defaultHiddenFg = (COLORS > 8) ? "gray" : "";
    const std::string defaultSelectedFg = (COLORS > 8) ? "gray" : "";
    const std::string defaultStrikeFg = (COLORS > 8) ? "gray" : "";
    const std::map<std::string, std::string> defaultColorsConfig =
    {
      { "color_address_text_attr", "" },
      { "color_address_text_bg", "" },
      { "color_address_text_fg", defaultAddressFg },
      { "color_bold_text_attr", "" },
      { "color_bold_text_bg", "" },
      { "color_bold_text_fg", "" },
      { "color_code_text_attr", "" },
      { "color_code_text_bg", "" },
      { "color_code_text_fg", "" },
      { "color_dialog_attr", "" },
      { "color_dialog_bg", "reverse" },
      { "color_dialog_fg", "reverse" },
      { "color_header_name_attr", "" },
      { "color_header_name_bg", "" },
      { "color_header_name_fg", "" },
      { "color_help_desc_attr", "" },
      { "color_help_desc_bg", "" },
      { "color_help_desc_fg", "" },
      { "color_help_keys_attr", "" },
      { "color_help_keys_bg", "reverse" },
      { "color_help_keys_fg", "reverse" },
      { "color_hidden_file_attr", "" },
      { "color_hidden_file_bg", "" },
      { "color_hidden_file_fg", defaultHiddenFg },
      { "color_highlighted_link_attr", "" },
      { "color_highlighted_link_bg", "reverse" },
      { "color_highlighted_link_fg", "reverse" },
      { "color_highlighted_text_attr", "" },
      { "color_highlighted_text_bg", "reverse" },
      { "color_highlighted_text_fg", "reverse" },
      { "color_italic_text_attr", "" },
      { "color_italic_text_bg", "" },
      { "color_italic_text_fg", "" },
      { "color_link_text_attr", "underline" },
      { "color_link_text_bg", "" },
      { "color_link_text_fg", "" },
      { "color_quoted_text_attr", "" },
      { "color_quoted_text_bg", "" },
      { "color_quoted_text_fg", defaultQuotedFg },
      { "color_regular_text_bg", "" },
      { "color_regular_text_fg", "" },
      { "color_selected_item_attr", "" },
      { "color_selected_item_bg", "" },
      { "color_selected_item_fg", defaultSelectedFg },
      { "color_signature_text_attr", "" },
      { "color_signature_text_bg", "" },
      { "color_signature_text_fg", "" },
      { "color_strike_text_attr", "" },
      { "color_strike_text_bg", "" },
      { "color_strike_text_fg", defaultStrikeFg },
      { "color_top_bar_attr", "" },
      { "color_top_bar_bg", "reverse" },
      { "color_top_bar_fg", "reverse" },
    };
    const std::string colorsConfigPath(AppUtil::GetApplicationDir() + std::string("colors.conf"));
    Config colorsConfig = Config(colorsConfigPath, defaultColorsConfig);
    colorsConfig.LogParams();

    // regular text is applied as window background, as ncurses does not output
    // color pair 0 set by assume_default_colors(), unset fg/bg inherit from it
    const std::string regularFg = colorsConfig.Get("color_regular_text_fg");
    const std::string regularBg = colorsConfig.Get("color_regular_text_bg");
    auto getColorAttrs = [&](const std::string& p_Fg, const std::string& p_Bg)
    {
      return ColorUtil::GetColorAttrs(p_Fg.empty() ? regularFg : p_Fg, p_Bg.empty() ? regularBg : p_Bg);
    };

    auto getAttrs = [&](const std::string& p_Item)
    {
      return getColorAttrs(colorsConfig.Get(p_Item + "_fg"), colorsConfig.Get(p_Item + "_bg")) |
             ColorUtil::GetAttrs(colorsConfig.Get(p_Item + "_attr"));
    };

    m_AttrsRegularText = ColorUtil::GetColorAttrs(regularFg, regularBg);
    m_AttrsDialog = getAttrs("color_dialog");
    m_AttrsHelpDesc = getAttrs("color_help_desc");
    m_AttrsHelpKeys = getAttrs("color_help_keys");
    m_AttrsHighlightedText = getAttrs("color_highlighted_text");
    m_AttrsQuotedText = getAttrs("color_quoted_text");
    m_AttrsBoldText = getAttrs("color_bold_text");
    m_AttrsHeaderName = getAttrs("color_header_name");
    m_AttrsCodeText = getAttrs("color_code_text");
    m_AttrsItalicText = getAttrs("color_italic_text");
    m_AttrsStrikeText = getAttrs("color_strike_text");
    m_AttrsSignatureText = getAttrs("color_signature_text");
    m_AttrsLinkText = getAttrs("color_link_text");
    m_AttrsHighlightedLink = getAttrs("color_highlighted_link");
    m_AttrsHiddenFile = getAttrs("color_hidden_file");
    m_AttrsAddressText = getAttrs("color_address_text");
    m_AttrsTopBar = getAttrs("color_top_bar");
    m_AttrsSelectedItem = getAttrs("color_selected_item");

    if (!(m_AttrsHighlightedText & A_COLOR))
    {
      // attributes only (e.g. reverse), apply on top of selected item
      m_AttrsSelectedHighlighted = m_AttrsSelectedItem | m_AttrsHighlightedText;
    }
    else
    {
      m_AttrsSelectedHighlighted =
        getColorAttrs(colorsConfig.Get("color_selected_item_fg"), colorsConfig.Get("color_highlighted_text_bg")) |
        ColorUtil::GetAttrs(colorsConfig.Get("color_selected_item_attr")) |
        ColorUtil::GetAttrs(colorsConfig.Get("color_highlighted_text_attr"));
    }

    colorsConfig.Save();
  }

  m_AttachmentIndicator = m_Config.Get("attachment_indicator");
  m_BottomReply = m_Config.Get("bottom_reply") == "1";
  m_PersistSortFilter = m_Config.Get("persist_sortfilter") == "1";
  m_PersistSelectionOnSortFilterChange = m_Config.Get("persist_selection_on_sortfilter_change") == "1";
  m_UnreadIndicator = m_Config.Get("unread_indicator");
  m_InvalidInputNotify = m_Config.Get("invalid_input_notify") == "1";
  m_KeyToggleFullHeader = UiKeyConfig::GetKey("key_toggle_full_header");
  m_FullHeaderIncludeLocal = m_Config.Get("full_header_include_local") == "1";
  m_KeyToggleCompactHeader = UiKeyConfig::GetKey("key_toggle_compact_header");
  m_ShowCompactHeader = m_Config.Get("show_compact_header") == "1";
  m_TabSize = StrUtil::Bound(1, (int)StrUtil::ToInteger(m_Config.Get("tab_size")), 80);
  m_KeySelectItem = UiKeyConfig::GetKey("key_select_item");
  m_KeySelectAll = UiKeyConfig::GetKey("key_select_all");
  m_SearchShowFolder = m_Config.Get("search_show_folder") == "1";
  MailUtil::SetLocalizedSubjectPrefixes(m_Config.Get("localized_subject_prefixes"));
  m_Signature = m_Config.Get("signature") == "1";
  m_TopBarShowVersion = m_Config.Get("top_bar_show_version") == "1";
  m_UnwrapQuotedLines = m_Config.Get("unwrap_quoted_lines") == "1";
  m_AutomoveTrashAllow = m_Config.Get("automove_trash_allow") == "1";
  m_TopBarShowMessageCount = m_Config.Get("top_bar_show_message_count") == "1";

  try
  {
    m_ComposeBackupInterval = std::stoll(m_Config.Get("compose_backup_interval_sec"));
  }
  catch (...)
  {
  }

  m_Status.SetShowProgress(m_ShowProgress);

  SetRunning(true);

  m_SleepDetect.reset(new SleepDetect(std::bind(&Ui::OnWakeUp, this), 10));
}

void Ui::Cleanup()
{
  m_SleepDetect.reset();

  m_Config.Set("plain_text", m_Plaintext ? "1" : "0");
  m_Config.Set("show_rich_header", m_ShowRichHeader ? "1" : "0");
  m_Config.Set("show_compact_header", m_ShowCompactHeader ? "1" : "0");
  m_Config.Set("search_show_folder", m_SearchShowFolder ? "1" : "0");
  m_Config.Save();

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

  switch (m_State)
  {
    case StateViewMessageList:
      DrawTop();
      if (m_MessageListSearch)
      {
        DrawMessageListSearch();
      }
      else
      {
        DrawMessageList();
      }
      DrawHelp();
      DrawDialog();
      break;

    case StateViewMessage:
      DrawTop();
      DrawMessage();
      DrawHelp();
      DrawDialog();
      break;

    case StateGotoFolder:
    case StateMoveToFolder:
      DrawTop();
      DrawFolderList();
      DrawHelp();
      DrawDialog();
      break;

    case StateComposeMessage:
    case StateComposeCopyMessage:
    case StateReplyAllMessage:
    case StateReplySenderMessage:
    case StateForwardMessage:
    case StateForwardAttachedMessage:
      DrawTop();
      DrawHelp();
      DrawDialog();
      DrawComposeMessage();
      break;

    case StateAddressList:
    case StateFromAddressList:
      DrawTop();
      DrawAddressList();
      DrawHelp();
      DrawDialog();
      break;

    case StateFileList:
      DrawTop();
      DrawFileList();
      DrawHelp();
      DrawDialog();
      break;

    case StateViewPartList:
      DrawTop();
      DrawPartList();
      DrawHelp();
      DrawDialog();
      break;

    default:
      werase(m_MainWin);
      mvwprintw(m_MainWin, 0, 0, "Unimplemented state %d", m_State);
      RefreshWin(m_MainWin);
      break;
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
  if (p_UiRequest & UiRequestDrawAll)
  {
    DrawAll();
  }

  if (p_UiRequest & UiRequestDrawError)
  {
    std::unique_lock<std::mutex> lock(m_SmtpErrorMutex);
    if (!m_SmtpErrorResults.empty())
    {
      const SmtpManager::Result result = m_SmtpErrorResults.front();
      m_SmtpErrorResults.pop_front();
      lock.unlock();

      SmtpResultHandlerError(result);
    }
  }

  if (p_UiRequest & UiRequestHandleConnected)
  {
    HandleConnected();
  }

  if (p_UiRequest & UiRequestSpecialFolders)
  {
    HandleSpecialFolders();
  }
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

      if (static_cast<int>(key) == m_KeyTerminalResize)
      {
        CleanupWindows();
        InitWindows();
        DrawAll();
        continue;
      }

      switch (m_State)
      {
        case StateViewMessageList:
          ViewMessageListKeyHandler(key);
          break;

        case StateViewMessage:
          ViewMessageKeyHandler(key);
          break;

        case StateGotoFolder:
        case StateMoveToFolder:
          ViewFolderListKeyHandler(key);
          break;

        case StateComposeMessage:
        case StateComposeCopyMessage:
        case StateReplyAllMessage:
        case StateReplySenderMessage:
        case StateForwardMessage:
        case StateForwardAttachedMessage:
          ComposeMessageKeyHandler(key);
          break;

        case StateAddressList:
        case StateFromAddressList:
          ViewAddressListKeyHandler(key);
          break;

        case StateFileList:
          ViewFileListKeyHandler(key);
          break;

        case StateViewPartList:
          ViewPartListKeyHandler(key);
          break;

        default:
          break;
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

    if (m_ComposeBackupRunning)
    {
      // keep compose backup, for draft recovery at next start
      m_ComposeBackupKeep = true;
      StopComposeBackup();
    }
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

  if (m_State == StateGotoFolder)
  {
    curs_set(1);
    m_FolderListFilterPos = 0;
    m_FolderListFilterStr.clear();
    m_FolderListCurrentFolder = m_CurrentFolder;
    m_FolderListCurrentIndex = INT_MAX;
  }
  else if (m_State == StateMoveToFolder)
  {
    curs_set(1);
    if (m_IsAutoMove)
    {
      AutoMoveSelectFolder();
    }
    else if (m_PersistFolderFilter)
    {
      m_FolderListFilterPos = m_PersistedFolderListFilterPos;
      m_FolderListFilterStr = m_PersistedFolderListFilterStr;
      if (!m_PersistedFolderListCurrentFolder.empty())
      {
        m_FolderListCurrentFolder = m_PersistedFolderListCurrentFolder;
        m_FolderListCurrentIndex = m_PersistedFolderListCurrentIndex;
      }
      else
      {
        m_FolderListCurrentFolder = m_CurrentFolder;
        m_FolderListCurrentIndex = INT_MAX;
      }
    }
    else
    {
      m_FolderListFilterPos = 0;
      m_FolderListFilterStr.clear();
      m_FolderListCurrentFolder = m_CurrentFolder;
      m_FolderListCurrentIndex = INT_MAX;
    }
  }
  else if (m_State == StateViewMessageList)
  {
    curs_set(0);
    m_HelpViewMessagesListOffset = 0;
    {
      std::lock_guard<std::mutex> lock(m_Mutex);
      m_MessageViewToggledSeen = false;
    }
  }
  else if (m_State == StateViewMessage)
  {
    curs_set(0);
    m_MessageViewLineOffset = 0;
    m_HelpViewMessageOffset = 0;
    m_MessageFindMatchLine = -1;
    m_MessageViewLinkIndex = -1;
    m_MessageViewLinkPastEnd = false;
  }
  else if ((m_State == StateComposeMessage) || (m_State == StateComposeCopyMessage))
  {
    curs_set(1);
    SetComposeStr(HeaderAll, L"");
    SetComposeStr(HeaderFrom, StrUtil::ToWString(GetDefaultFrom()));
    StartComposeBackup();
    m_ComposeHeaderLine = m_ShowRichHeader ? 1 : 0;
    m_ComposeHeaderPos = 0;
    m_ComposeHeaderRef.clear();
    m_ComposeMessageStr.clear();
    m_ComposeMessagePos = 0;
    m_IsComposeHeader = true;
    m_ComposeDraftUid = 0;
    m_ComposeMessageOffsetY = 0;
    m_ComposeTempDirectory.clear();
    m_CurrentMarkdownHtmlCompose = m_MarkdownHtmlCompose;
    m_ComposeQuotedStart.clear();

    std::lock_guard<std::mutex> lock(m_Mutex);
    const std::string& folder = m_CurrentFolderUid.first;
    const int uid = m_CurrentFolderUid.second;

    if (m_ComposeMailto.empty() && ((folder == m_DraftsFolder) || (m_State == StateComposeCopyMessage)))
    {
      std::map<uint32_t, Header>& headers = m_Headers[folder];
      std::map<uint32_t, Body>& bodys = m_Bodys[folder];

      std::map<uint32_t, Header>::iterator hit = headers.find(uid);
      std::map<uint32_t, Body>::iterator bit = bodys.find(uid);
      if ((hit != headers.end()) && (bit != bodys.end()))
      {
        m_ComposeDraftUid = (m_State != StateComposeCopyMessage) ? uid : 0;

        Header& header = hit->second;
        Body& body = bit->second;

        std::string bodyText = body.GetTextPlain();
        if ((folder != m_DraftsFolder) && !bodyText.empty() && bodyText[bodyText.size() - 1] == '\n')
        {
          bodyText = bodyText.substr(0, bodyText.size() - 1);
        }

        m_ComposeMessageStr = StrUtil::ToWString(bodyText);
        StrUtil::StripCR(m_ComposeMessageStr);

        SetComposeStr(HeaderFrom, StrUtil::ToWString(header.GetFrom()));
        SetComposeStr(HeaderTo, StrUtil::ToWString(header.GetTo()));
        SetComposeStr(HeaderCc, StrUtil::ToWString(header.GetCc()));
        SetComposeStr(HeaderBcc, StrUtil::ToWString(header.GetBcc()));
        SetComposeStr(HeaderAtt, L"");
        SetComposeStr(HeaderSub, StrUtil::ToWString(header.GetSubject()));

        if (!GetComposeStr(HeaderBcc).empty())
        {
          m_ShowRichHeader = true;
        }

        int idx = 0;
        std::string tmppath = FileUtil::GetTempDirectory();
        const std::map<ssize_t, std::string>& partDatas = body.GetPartDatas();
        for (auto& part : body.GetPartInfos())
        {
          if (!part.second.m_Filename.empty())
          {
            std::string tmpfiledir = tmppath + "/" + std::to_string(idx++) + "/";
            FileUtil::MkDir(tmpfiledir);
            std::string tmpfilepath = tmpfiledir + part.second.m_Filename;

            FileUtil::WriteFile(tmpfilepath, partDatas.at(part.first));
            tmpfilepath = FileUtil::EscapePath(tmpfilepath);
            if (GetComposeStr(HeaderAtt).empty())
            {
              SetComposeStr(HeaderAtt, GetComposeStr(HeaderAtt) + StrUtil::ToWString(tmpfilepath));
            }
            else
            {
              SetComposeStr(HeaderAtt, GetComposeStr(HeaderAtt) + L", " + StrUtil::ToWString(tmpfilepath));
            }
          }
        }

        m_ComposeHeaderRef = header.GetMessageId();
        m_ComposeTempDirectory = tmppath;
      }
    }
    else
    {
      m_ComposeMessageStr = GetSignatureStr();
    }

    if (!m_ComposeMailto.empty())
    {
      SetComposeMailto(m_ComposeMailto);
      m_ComposeMailto.clear();
    }
  }
  else if ((m_State == StateReplyAllMessage) || (m_State == StateReplySenderMessage))
  {
    curs_set(1);
    SetComposeStr(HeaderAll, L"");
    SetComposeStr(HeaderFrom, StrUtil::ToWString(GetDefaultFrom()));
    StartComposeBackup();
    m_ComposeHeaderLine = m_ShowRichHeader ? 5 : 3;
    m_ComposeHeaderPos = 0;
    m_ComposeMessageStr.clear();
    m_ComposeMessagePos = 0;
    m_ComposeMessageOffsetY = 0;
    m_ComposeTempDirectory.clear();
    m_CurrentMarkdownHtmlCompose = m_MarkdownHtmlCompose;
    m_ComposeQuotedStart.clear();

    std::lock_guard<std::mutex> lock(m_Mutex);
    const std::string& folder = m_CurrentFolderUid.first;
    const int uid = m_CurrentFolderUid.second;

    std::map<uint32_t, Header>& headers = m_Headers[folder];
    std::map<uint32_t, Body>& bodys = m_Bodys[folder];

    std::map<uint32_t, Header>::iterator hit = headers.find(uid);
    std::map<uint32_t, Body>::iterator bit = bodys.find(uid);
    if ((hit != headers.end()) && (bit != bodys.end()))
    {
      Header& header = hit->second;
      Body& body = bit->second;

      std::string bodyText = GetBodyText(body);
      if (!bodyText.empty() && bodyText[bodyText.size() - 1] == '\n')
      {
        bodyText = bodyText.substr(0, bodyText.size() - 1);
      }

      std::string indentBodyText = TextUtil::AddIndent(bodyText, "> ");
      const int quotedMaxLen = 78;

      if (m_UnwrapQuotedLines)
      {
        indentBodyText = TextUtil::UnwrapQuotedLines(indentBodyText, quotedMaxLen);
      }

      std::string indentBody;
      if (m_ComposeLineWrap == LineWrapFormatFlowed)
      {
        indentBody = indentBodyText;
      }
      else
      {
        const bool processFlowed = m_RespectFormatFlowed && m_Plaintext && body.IsFormatFlowed();
        const bool outputFlowed = false;
        const bool quoteWrap = m_RewrapQuotedLines;
        const int expandTabSize = m_TabSize; // enabled
        std::vector<std::wstring> indentBodyLines =
          TextUtil::WordWrap(StrUtil::ToWString(indentBodyText), quotedMaxLen, processFlowed,
                             outputFlowed, quoteWrap, expandTabSize);
        indentBody = StrUtil::ToString(StrUtil::Join(indentBodyLines));
      }

      m_ComposeQuotedStart = "On " + header.GetDateTime() + " " + header.GetFrom() + " wrote:\n\n";
      if (!m_BottomReply)
      {
        m_ComposeMessageStr = GetSignatureStr() +
          StrUtil::ToWString("\n\n" + m_ComposeQuotedStart + indentBody);
        StrUtil::StripCR(m_ComposeMessageStr);
      }
      else
      {
        m_ComposeMessageStr =
          StrUtil::ToWString(m_ComposeQuotedStart + indentBody + "\n\n\n");
        StrUtil::StripCR(m_ComposeMessageStr);
        m_ComposeMessagePos = (int)m_ComposeMessageStr.size() - 1;
        int lineCount = StrUtil::Split(StrUtil::ToString(m_ComposeMessageStr), '\n').size();
        m_ComposeMessageOffsetY = std::max(lineCount - (m_MainWinHeight / 2), 0);
        m_ComposeMessageStr += GetSignatureStr();
      }

      {
        if (folder == m_SentFolder)
        {
          SetComposeStr(HeaderTo, StrUtil::ToWString(header.GetTo()));
        }
        else
        {
          if (!header.GetReplyTo().empty())
          {
            SetComposeStr(HeaderTo, StrUtil::ToWString(header.GetReplyTo()));
          }
          else
          {
            SetComposeStr(HeaderTo, StrUtil::ToWString(header.GetFrom()));
          }
        }

        if (m_State == StateReplyAllMessage)
        {
          std::vector<std::string> tos = MailUtil::SplitAddrs(header.GetTo());
          std::vector<std::string> ccs = MailUtil::SplitAddrs(header.GetCc());

          if (folder != m_SentFolder)
          {
            ccs.insert(ccs.end(), tos.begin(), tos.end());
          }

          std::string selfAddress = m_SmtpManager->GetAddress();
          for (auto it = ccs.begin(); it != ccs.end(); /* incremented in loop */)
          {
            it = ((it->find(selfAddress) == std::string::npos) &&
                  (it->find(header.GetFrom()) == std::string::npos)) ? std::next(it) : ccs.erase(it);
          }

          SetComposeStr(HeaderCc, StrUtil::ToWString(StrUtil::Join(ccs, ", ")));
        }
      }

      SetComposeStr(HeaderBcc, L"");
      SetComposeStr(HeaderAtt, L"");
      SetComposeStr(HeaderSub, StrUtil::ToWString(MailUtil::MakeReplySubject(header.GetSubject())));

      m_ComposeHeaderRef = header.GetMessageId();
    }

    m_IsComposeHeader = false;
  }
  else if (m_State == StateForwardMessage)
  {
    curs_set(1);
    SetComposeStr(HeaderAll, L"");
    SetComposeStr(HeaderFrom, StrUtil::ToWString(GetDefaultFrom()));
    StartComposeBackup();
    m_ComposeHeaderLine = m_ShowRichHeader ? 1 : 0;
    m_ComposeHeaderPos = 0;
    m_ComposeMessageStr.clear();
    m_ComposeMessagePos = 0;
    m_ComposeMessageOffsetY = 0;
    m_ComposeTempDirectory.clear();
    m_CurrentMarkdownHtmlCompose = m_MarkdownHtmlCompose;
    m_ComposeQuotedStart.clear();

    std::lock_guard<std::mutex> lock(m_Mutex);
    const std::string& folder = m_CurrentFolderUid.first;
    const int uid = m_CurrentFolderUid.second;

    std::map<uint32_t, Header>& headers = m_Headers[folder];
    std::map<uint32_t, Body>& bodys = m_Bodys[folder];

    std::map<uint32_t, Header>::iterator hit = headers.find(uid);
    std::map<uint32_t, Body>::iterator bit = bodys.find(uid);
    if ((hit != headers.end()) && (bit != bodys.end()))
    {
      Header& header = hit->second;
      Body& body = bit->second;

      int idx = 0;
      std::string tmppath = FileUtil::GetTempDirectory();
      const std::map<ssize_t, std::string>& partDatas = body.GetPartDatas();
      for (auto& part : body.GetPartInfos())
      {
        if (!part.second.m_Filename.empty())
        {
          std::string tmpfiledir = tmppath + "/" + std::to_string(idx++) + "/";
          FileUtil::MkDir(tmpfiledir);
          std::string tmpfilepath = tmpfiledir + part.second.m_Filename;

          FileUtil::WriteFile(tmpfilepath, partDatas.at(part.first));
          tmpfilepath = FileUtil::EscapePath(tmpfilepath);
          if (GetComposeStr(HeaderAtt).empty())
          {
            SetComposeStr(HeaderAtt,
                          GetComposeStr(HeaderAtt) + StrUtil::ToWString(tmpfilepath));
          }
          else
          {
            SetComposeStr(HeaderAtt,
                          GetComposeStr(HeaderAtt) + L", " + StrUtil::ToWString(tmpfilepath));
          }
        }
      }

      m_ComposeQuotedStart = "\n\n---------- Forwarded message ---------\n";
      m_ComposeMessageStr =
        GetSignatureStr() +
        StrUtil::ToWString(m_ComposeQuotedStart +
                           "From: " + header.GetFrom() + "\n"
                           "Date: " + header.GetDateTime() + "\n"
                           "Subject: " + header.GetSubject() + "\n"
                           "To: " + header.GetTo() + "\n");
      if (!header.GetReplyTo().empty())
      {
        m_ComposeMessageStr +=
          StrUtil::ToWString("Reply-To: " + header.GetReplyTo() + "\n");
      }

      if (!header.GetCc().empty())
      {
        m_ComposeMessageStr +=
          StrUtil::ToWString("Cc: " + header.GetCc() + "\n");
      }

      std::string bodyText = GetBodyText(body);
      if (!bodyText.empty() && bodyText[bodyText.size() - 1] == '\n')
      {
        bodyText = bodyText.substr(0, bodyText.size() - 1);
      }

      m_ComposeMessageStr += StrUtil::ToWString("\n" + bodyText);
      StrUtil::StripCR(m_ComposeMessageStr);

      SetComposeStr(HeaderSub, StrUtil::ToWString(MailUtil::MakeForwardSubject(header.GetSubject())));

      m_ComposeHeaderRef = header.GetMessageId();
      m_ComposeTempDirectory = tmppath;
    }

    m_IsComposeHeader = true;
  }
  else if (m_State == StateForwardAttachedMessage)
  {
    curs_set(1);
    SetComposeStr(HeaderAll, L"");
    SetComposeStr(HeaderFrom, StrUtil::ToWString(GetDefaultFrom()));
    StartComposeBackup();
    m_ComposeHeaderLine = m_ShowRichHeader ? 1 : 0;
    m_ComposeHeaderPos = 0;
    m_ComposeMessageStr.clear();
    m_ComposeMessagePos = 0;
    m_ComposeMessageOffsetY = 0;
    m_ComposeTempDirectory.clear();
    m_CurrentMarkdownHtmlCompose = m_MarkdownHtmlCompose;
    m_ComposeQuotedStart.clear();

    std::lock_guard<std::mutex> lock(m_Mutex);
    const std::string& folder = m_CurrentFolderUid.first;
    const int uid = m_CurrentFolderUid.second;

    std::map<uint32_t, Header>& headers = m_Headers[folder];
    std::map<uint32_t, Body>& bodys = m_Bodys[folder];

    std::map<uint32_t, Header>::iterator hit = headers.find(uid);
    std::map<uint32_t, Body>::iterator bit = bodys.find(uid);
    if ((hit != headers.end()) && (bit != bodys.end()))
    {
      Header& header = hit->second;
      Body& body = bit->second;

      std::string tmppath = FileUtil::GetTempDirectory();
      std::string filename = header.GetSubject();
      StrUtil::RemoveNonAlphaNumSpace(filename);
      StrUtil::ReplaceString(filename, " ", "_");
      std::string filepath = tmppath + "/" + filename + ".eml";
      LOG_INFO("write to %s size %d", filepath.c_str(), body.GetData().size());
      FileUtil::WriteFile(filepath, body.GetData());

      SetComposeStr(HeaderAtt,
                    GetComposeStr(HeaderAtt) + StrUtil::ToWString(filepath));

      SetComposeStr(HeaderSub, StrUtil::ToWString(MailUtil::MakeForwardSubject(header.GetSubject())));

      m_ComposeHeaderRef = header.GetMessageId();
      m_ComposeTempDirectory = tmppath;
    }

    m_IsComposeHeader = true;
  }
  else if ((m_State == StateAddressList) || (m_State == StateFromAddressList))
  {
    curs_set(1);
    m_AddressListFilterPos = 0;
    m_AddressListFilterStr.clear();
    m_Addresses = (m_State == StateAddressList) ? AddressBook::Get("") : AddressBook::GetFrom("");
    m_AddressListCurrentIndex = 0;
    m_AddressListCurrentAddress = "";
  }
  else if (m_State == StateFileList)
  {
    curs_set(1);
    m_FileListFilterPos = 0;
    m_FileListFilterStr.clear();
    if (m_CurrentDir.empty() || !m_PersistFileSelectionDir)
    {
      m_CurrentDir = FileUtil::GetCurrentWorkingDir();
    }

    m_Files = FileUtil::ListPaths(m_CurrentDir);
    m_FileListCurrentIndex = 0;
    m_FileListCurrentFile.m_Name = "";
  }
  else if (m_State == StateViewPartList)
  {
    curs_set(0);
    m_PartListCurrentIndex = 0;
  }
}

void Ui::SetImapManager(std::shared_ptr<ImapManager> p_ImapManager)
{
  m_ImapManager = p_ImapManager;
  if (m_ImapManager)
  {
    m_ImapManager->SetCurrentFolder(m_CurrentFolder);
  }
}

void Ui::SetSmtpManager(std::shared_ptr<SmtpManager> p_SmtpManager)
{
  m_SmtpManager = p_SmtpManager;
}

void Ui::ResetImapManager()
{
  m_ImapManager.reset();
}

void Ui::ResetSmtpManager()
{
  m_SmtpManager.reset();
}

void Ui::SetTrashFolder(const std::string& p_TrashFolder)
{
  m_TrashFolder = p_TrashFolder;
}

void Ui::SetSpamFolder(const std::string& p_SpamFolder)
{
  m_SpamFolder = p_SpamFolder;
}

void Ui::SetDraftsFolder(const std::string& p_DraftsFolder)
{
  m_DraftsFolder = p_DraftsFolder;
}

void Ui::SetSentFolder(const std::string& p_SentFolder)
{
  m_SentFolder = p_SentFolder;
}

void Ui::SetClientStoreSent(bool p_ClientStoreSent)
{
  m_ClientStoreSent = p_ClientStoreSent;
}

void Ui::SetSpecialFoldersHandler(const std::function<void(const std::map<std::string, std::string>&)>& p_Handler)
{
  m_SpecialFoldersHandler = p_Handler;
}

bool Ui::IsConnected()
{
  std::lock_guard<std::mutex> lock(m_Mutex);
  return m_Status.IsSet(Status::FlagConnected);
}

bool Ui::IsValidTextKey(int p_Key)
{
  return ((p_Key >= 0x20) || (p_Key == 0xA));
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
    std::lock_guard<std::mutex> lock(m_Mutex);
    isHeaderUidsEmpty = GetHeaderUids(m_CurrentFolder).empty();
  }

  if (isHeaderUidsEmpty)
  {
    SetState(StateViewMessageList);
  }
  else
  {
    SetState(m_LastState);
  }

  UpdateIndexFromUid();
}

void Ui::Quit()
{
  if (m_QuitWithoutConfirm || Ui::PromptYesNo("Quit nmail (y/n)?"))
  {
    StatusUpdate statusUpdate;
    statusUpdate.SetFlags = m_Status.IsSet(Status::FlagConnected) ? Status::FlagDisconnecting
                                                                  : Status::FlagExiting;
    m_Status.Update(statusUpdate);
    SetRunning(false);
    LOG_DEBUG("stop thread");
  }
}

void Ui::StartSync()
{
  if (m_SyncState != SyncStateIdle)
  {
    SetDialogMessage("Sync already in progress");
    return;
  }

  if (!IsConnected())
  {
    SetDialogMessage("Cannot sync while offline");
    return;
  }

  if (AppUtil::GetReadOnly())
  {
    SetDialogMessage("Cannot sync in read-only mode");
    return;
  }

  LOG_DEBUG("sync started (manual)");
  m_SyncState = SyncStateStarted;

  ImapManager::Request request;
  request.m_PrefetchLevel = PrefetchLevelFullSync;
  request.m_GetFolders = true;
  LOG_DEBUG("prefetch req folders");
  m_HasPrefetchRequestedFolders = true;
  m_ImapManager->PrefetchRequest(request);
}

void Ui::SetRunning(bool p_Running)
{
  s_Running = p_Running;
}

void Ui::HandleConnected()
{
  if (IsConnected() && !AppUtil::GetReadOnly())
  {
    if (!m_DraftsFolder.empty())
    {
      std::vector<std::string> draftMsgs = OfflineQueue::PopDraftMessages();
      for (const auto& draftMsg : draftMsgs)
      {
        SetDialogMessage("Uploading queued draft messages");

        ImapManager::Action imapAction;
        imapAction.m_UploadDraft = true;
        imapAction.m_Folder = m_DraftsFolder;
        imapAction.m_Msg = draftMsg;
        m_ImapManager->AsyncAction(imapAction);

        m_HasRequestedUids[m_DraftsFolder] = false;
      }
    }
    else if (OfflineQueue::HasDraftMessages())
    {
      // keep queued drafts until a drafts folder is configured
      SetDialogMessage("Drafts folder not configured, queued draft kept", true /* p_Warn */);
    }

    std::vector<std::string> outboxMsgs = OfflineQueue::PopOutboxMessages();
    for (const auto& outboxMsg : outboxMsgs)
    {
      SetDialogMessage("Sending queued messages");

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

bool Ui::HandleListKey(int p_Key, int& p_Index)
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

bool Ui::HandleLineKey(int p_Key, std::wstring& p_Str, int& p_Pos)
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

bool Ui::HandleTextKey(int p_Key, std::wstring& p_Str, int& p_Pos)
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

bool Ui::HandleDocKey(int p_Key, std::wstring& p_Str, int& p_Pos)
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

void Ui::OnWakeUp()
{
  LOG_DEBUG_FUNC(STR());

  // Dummy request to trigger exit of idle
  ImapManager::Request request;
  LOG_DEBUG("async req none");
  m_ImapManager->AsyncRequest(request);
}

void Ui::SetCurrentFolder(const std::string& p_Folder)
{
  m_CurrentFolder = p_Folder;
  if (m_ImapManager && !m_CurrentFolder.empty())
  {
    m_ImapManager->SetCurrentFolder(m_CurrentFolder);
  }
}
