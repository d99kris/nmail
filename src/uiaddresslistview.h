// uiaddresslistview.h
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <string>
#include <vector>

#include "uipickerview.h"

class UiAddressListView : public UiPickerView
{
public:
  explicit UiAddressListView(Ui* p_Ui);

  virtual void Draw();
  virtual void KeyHandler(int p_Key);
  virtual void OnEnter();
  virtual void DrawDialog();

private:
  std::wstring GetAddressListEntry(const std::string& p_Address, int p_Width, size_t& p_DimPos);

private:
  std::vector<std::string> m_Addresses;

  int m_AddressListCurrentIndex = 0;
  std::string m_AddressListCurrentAddress;

  int m_AddressListFilterPos = 0;
  std::wstring m_AddressListFilterStr;
};
