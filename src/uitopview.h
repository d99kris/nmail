// uitopview.h
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <string>

#include "uiviewbase.h"

class UiTopView : public UiViewBase
{
public:
  explicit UiTopView(Ui* p_Ui);

  void Draw();

private:
  std::string GetStatusStr();
  std::string GetStateStr();
  std::string GetCountStateStr(UiModel::Lock& p_Model);
  std::string GetFilterStateStr(UiModel::Lock& p_Model);

private:
  bool m_TopBarShowVersion = false;
  bool m_TopBarShowMessageCount = false;
};
