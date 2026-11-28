// uiaddresslistview.cpp
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "uiaddresslistview.h"

#include <algorithm>

#include "addressbook.h"
#include "strutil.h"
#include "uicomposeview.h"

UiAddressListView::UiAddressListView(Ui* p_Ui)
  : UiPickerView(p_Ui)
{
}

void UiAddressListView::Draw()
{
  werase(m_MainWin);

  static std::wstring lastAddressListFilterStr = m_AddressListFilterStr;
  if (m_AddressListFilterStr != lastAddressListFilterStr)
  {
    std::string filter = StrUtil::ToString(m_AddressListFilterStr);
    m_Addresses = (m_State == Ui::StateAddressList) ? AddressBook::Get(filter)
                                                    : AddressBook::GetFrom(filter);
    lastAddressListFilterStr = m_AddressListFilterStr;
  }

  const int width = GetPickerWidth();
  int count = m_Addresses.size();
  m_AddressListCurrentAddress.clear();
  if (count > 0)
  {
    m_AddressListCurrentIndex = StrUtil::Bound(0, m_AddressListCurrentIndex, (int)m_Addresses.size() - 1);

    int itemsMax = m_MainWinHeight - 1;
    int idxOffs = StrUtil::Bound(0, (int)(m_AddressListCurrentIndex - ((itemsMax - 1) / 2)),
                                 std::max(0, (int)m_Addresses.size() - (int)itemsMax));
    int idxMax = idxOffs + std::min(itemsMax, (int)m_Addresses.size());

    for (int i = idxOffs; i < idxMax; ++i)
    {
      const std::string& address = *std::next(m_Addresses.begin(), i);

      if (i == m_AddressListCurrentIndex)
      {
        wattron(m_MainWin, m_AttrsHighlightedText);
        m_AddressListCurrentAddress = address;
      }

      size_t dimPos = std::wstring::npos;
      std::wstring waddress = GetAddressListEntry(address, width, dimPos);
      const bool isCurrent = (i == m_AddressListCurrentIndex);
      if (isCurrent && (m_PickerWidth != 0))
      {
        waddress = StrUtil::TrimPadWString(waddress, width);
      }

      if (isCurrent || (dimPos >= waddress.size()))
      {
        mvwaddnwstr(m_MainWin, i - idxOffs, 2, waddress.c_str(), waddress.size());
      }
      else
      {
        mvwaddnwstr(m_MainWin, i - idxOffs, 2, waddress.c_str(), dimPos);
        wattron(m_MainWin, m_AttrsAddressText);
        waddnwstr(m_MainWin, waddress.c_str() + dimPos, waddress.size() - dimPos);
        wattroff(m_MainWin, m_AttrsAddressText);
      }

      if (i == m_AddressListCurrentIndex)
      {
        wattroff(m_MainWin, m_AttrsHighlightedText);
      }
    }
  }

  RefreshWin(m_MainWin);
}

std::wstring UiAddressListView::GetAddressListEntry(const std::string& p_Address, int p_Width, size_t& p_DimPos)
{
  // classic layout shows address as stored
  p_DimPos = std::wstring::npos;
  if (m_PickerWidth == 0) return StrUtil::TrimEllipsisWString(StrUtil::ToWString(p_Address), p_Width);

  // name followed by dimmed address, or address only if no name or name is address
  std::wstring entry = StrUtil::ToWString(p_Address);
  const size_t ltPos = p_Address.rfind('<');
  if ((ltPos != std::string::npos) && (p_Address.back() == '>'))
  {
    std::string name = StrUtil::Trim(p_Address.substr(0, ltPos));
    if ((name.size() >= 2) && (name.front() == '"') && (name.back() == '"'))
    {
      name = name.substr(1, name.size() - 2);
      StrUtil::ReplaceString(name, "\\\"", "\"");
    }

    const std::string addr = p_Address.substr(ltPos + 1, p_Address.size() - ltPos - 2);
    if (name.empty() || (StrUtil::ToLower(name) == StrUtil::ToLower(addr)))
    {
      entry = StrUtil::ToWString(addr);
    }
    else
    {
      const std::wstring wname = StrUtil::ToWString(name) + L" ";
      entry = wname + L"<" + StrUtil::ToWString(addr) + L">";
      p_DimPos = wname.size();
    }
  }

  return StrUtil::TrimEllipsisWString(entry, p_Width);
}

void UiAddressListView::KeyHandler(int p_Key)
{
  if (p_Key == m_KeyCancel)
  {
    SetState(m_LastMessageState);
  }
  else if ((p_Key == m_KeyReturn) || (p_Key == m_KeyEnter) ||
           ((p_Key == m_KeyRight) && (m_AddressListFilterPos == (int)m_AddressListFilterStr.size())))
  {
    if (m_AddressListCurrentAddress.empty())
    {
      // no address shown, leave the field as is
    }
    else if (m_State == Ui::StateAddressList)
    {
      m_ComposeView->AddAddress(m_AddressListCurrentAddress);
    }
    else if (m_State == Ui::StateFromAddressList)
    {
      m_ComposeView->SetAddress(m_AddressListCurrentAddress);
    }
    SetState(m_LastMessageState);
  }
  else if (HandleListKey(p_Key, m_AddressListCurrentIndex))
  {
    // none
  }
  else if (HandleLineKey(p_Key, m_AddressListFilterStr, m_AddressListFilterPos))
  {
    // none
  }
  else if (HandleTextKey(p_Key, m_AddressListFilterStr, m_AddressListFilterPos))
  {
    // none
  }

  DrawAll();
}

void UiAddressListView::OnEnter()
{
  curs_set(1);
  m_AddressListFilterPos = 0;
  m_AddressListFilterStr.clear();
  m_Addresses = (m_State == Ui::StateAddressList) ? AddressBook::Get("") : AddressBook::GetFrom("");
  m_AddressListCurrentIndex = 0;
  m_AddressListCurrentAddress = "";
}

void UiAddressListView::DrawDialog()
{
  DrawSearchDialog(m_AddressListFilterStr, m_AddressListFilterPos);
}
