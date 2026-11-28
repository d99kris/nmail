// uiscreenview.cpp
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "uiscreenview.h"

#include "uiconfig.h"
#include "uidialogview.h"

UiScreenView::UiScreenView(Ui* p_Ui)
  : UiViewBase(p_Ui)
{
  m_InvalidInputNotify = UiConfig::GetBool("invalid_input_notify");
}

void UiScreenView::DrawDialog()
{
  m_DialogView->Draw();
}

bool UiScreenView::IsDrawnLast()
{
  return false;
}
