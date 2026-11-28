// uicolorconfig.cpp
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "uicolorconfig.h"

#include <ncurses.h>

#include "apputil.h"
#include "colorutil.h"
#include "config.h"
#include "loghelp.h"
#include "uiconfig.h"

std::map<std::string, int> UiColorConfig::m_Attrs;

void UiColorConfig::Init()
{
  // attributes used when colors are disabled or not supported
  m_Attrs =
  {
    { "color_address_text", A_NORMAL },
    { "color_bold_text", A_NORMAL },
    { "color_code_text", A_NORMAL },
    { "color_dialog", A_REVERSE },
    { "color_header_name", A_NORMAL },
    { "color_help_desc", A_NORMAL },
    { "color_help_keys", A_REVERSE },
    { "color_hidden_file", A_NORMAL },
    { "color_highlighted_link", A_REVERSE },
    { "color_highlighted_text", A_REVERSE },
    { "color_italic_text", A_NORMAL },
    { "color_link_text", A_UNDERLINE },
    { "color_quoted_text", A_NORMAL },
    { "color_regular_text", A_NORMAL },
    { "color_selected_highlighted", A_NORMAL },
    { "color_selected_item", A_NORMAL },
    { "color_signature_text", A_NORMAL },
    { "color_strike_text", A_NORMAL },
    { "color_top_bar", A_REVERSE },
  };

  bool colorsEnabled = UiConfig::GetBool("colors_enabled");
  if (colorsEnabled && !has_colors())
  {
    LOG_WARNING("terminal does not support colors");
    colorsEnabled = false;
  }

  if (colorsEnabled)
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

    m_Attrs["color_regular_text"] = ColorUtil::GetColorAttrs(regularFg, regularBg);
    m_Attrs["color_dialog"] = getAttrs("color_dialog");
    m_Attrs["color_help_desc"] = getAttrs("color_help_desc");
    m_Attrs["color_help_keys"] = getAttrs("color_help_keys");
    m_Attrs["color_highlighted_text"] = getAttrs("color_highlighted_text");
    m_Attrs["color_quoted_text"] = getAttrs("color_quoted_text");
    m_Attrs["color_bold_text"] = getAttrs("color_bold_text");
    m_Attrs["color_header_name"] = getAttrs("color_header_name");
    m_Attrs["color_code_text"] = getAttrs("color_code_text");
    m_Attrs["color_italic_text"] = getAttrs("color_italic_text");
    m_Attrs["color_strike_text"] = getAttrs("color_strike_text");
    m_Attrs["color_signature_text"] = getAttrs("color_signature_text");
    m_Attrs["color_link_text"] = getAttrs("color_link_text");
    m_Attrs["color_highlighted_link"] = getAttrs("color_highlighted_link");
    m_Attrs["color_hidden_file"] = getAttrs("color_hidden_file");
    m_Attrs["color_address_text"] = getAttrs("color_address_text");
    m_Attrs["color_top_bar"] = getAttrs("color_top_bar");
    m_Attrs["color_selected_item"] = getAttrs("color_selected_item");

    if (!(m_Attrs["color_highlighted_text"] & A_COLOR))
    {
      // attributes only (e.g. reverse), apply on top of selected item
      m_Attrs["color_selected_highlighted"] = m_Attrs["color_selected_item"] | m_Attrs["color_highlighted_text"];
    }
    else
    {
      m_Attrs["color_selected_highlighted"] =
        getColorAttrs(colorsConfig.Get("color_selected_item_fg"), colorsConfig.Get("color_highlighted_text_bg")) |
        ColorUtil::GetAttrs(colorsConfig.Get("color_selected_item_attr")) |
        ColorUtil::GetAttrs(colorsConfig.Get("color_highlighted_text_attr"));
    }

    colorsConfig.Save();
  }
}

int UiColorConfig::GetAttrs(const std::string& p_Item)
{
  return m_Attrs.at(p_Item);
}
