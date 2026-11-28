// uipartlistview.cpp
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "uipartlistview.h"

#include <algorithm>
#include <cstdint>

#include "fileutil.h"
#include "loghelp.h"
#include "strutil.h"
#include "uiconfig.h"
#include "uiexternal.h"

UiPartListView::UiPartListView(Ui* p_Ui)
  : UiScreenView(p_Ui)
{
  m_ShowEmbeddedImages = UiConfig::GetBool("show_embedded_images");
}

void UiPartListView::Draw()
{
  werase(m_MainWin);

  UiModel::Lock model(m_Model);
  const std::string& folder = m_CurrentFolderUid.first;
  const int uid = m_CurrentFolderUid.second;
  std::map<uint32_t, Body>& bodys = model->m_Bodys[folder];
  std::map<uint32_t, Body>::iterator bodyIt = bodys.find(uid);
  if (bodyIt != bodys.end())
  {
    Body& body = bodyIt->second;
    const std::map<ssize_t, PartInfo>& parts = body.GetPartInfos();

    int count = parts.size();
    if (count > 0)
    {
      m_PartListCurrentIndex = StrUtil::Bound(0, m_PartListCurrentIndex, (int)parts.size() - 1);

      int itemsMax = m_MainWinHeight - 1;
      int idxOffs = StrUtil::Bound(0, (int)(m_PartListCurrentIndex - ((itemsMax - 1) / 2)),
                                   std::max(0, (int)parts.size() - (int)itemsMax));
      int idxMax = idxOffs + std::min(itemsMax, (int)parts.size());

      for (int i = idxOffs; i < idxMax; ++i)
      {
        auto it = std::next(parts.begin(), i);
        const PartInfo& part = it->second;

        if (i == m_PartListCurrentIndex)
        {
          wattron(m_MainWin, m_AttrsHighlightedText);
          m_PartListCurrentPartInfo = part;
        }

        std::string leftPad = "    ";
        std::string sizeStr = std::to_string(part.m_Size) + " bytes";
        std::string sizeStrPadded = StrUtil::TrimPadString(sizeStr, 17) + " ";
        std::string mimeTypePadded = StrUtil::TrimPadString(part.m_MimeType, 29) + " ";
        std::wstring wline = StrUtil::ToWString(leftPad + sizeStrPadded + mimeTypePadded);
        std::wstring wfilename = StrUtil::ToWString(part.m_Filename);
        int filenameMaxLen = m_ScreenWidth - (int)wline.size();
        std::wstring wfilenamePadded = StrUtil::TrimPadWString(wfilename, filenameMaxLen - 1) + L" ";
        wline = wline + wfilenamePadded;

        mvwaddnwstr(m_MainWin, i - idxOffs, 0, wline.c_str(),
                    std::min((int)wline.size(), m_ScreenWidth));

        if (i == m_PartListCurrentIndex)
        {
          wattroff(m_MainWin, m_AttrsHighlightedText);
        }
      }
    }
  }

  RefreshWin(m_MainWin);
}

void UiPartListView::KeyHandler(int p_Key)
{
  if (p_Key == m_KeyQuit)
  {
    Quit();
  }
  else if ((p_Key == m_KeyBackspace) || (p_Key == m_KeyBackspaceAlt) || (p_Key == m_KeyBack) || (p_Key == m_KeyLeft))
  {
    const std::string& attachmentsTempDir = FileUtil::GetAttachmentsTempDir();
    LOG_DEBUG("deleting %s", attachmentsTempDir.c_str());
    FileUtil::CleanupAttachmentsTempDir();
    SetState(Ui::StateViewMessage);
  }
  else if ((p_Key == m_KeyReturn) || (p_Key == m_KeyEnter) || (p_Key == m_KeyOpen) || (p_Key == m_KeyRight) ||
           (p_Key == m_KeyExtHtmlViewer))
  {
    std::string ext;
    std::string err;
    std::string fileName;
    bool isUnamedTextHtml = false;
    if (!m_PartListCurrentPartInfo.m_Filename.empty())
    {
      ext = FileUtil::GetFileExt(m_PartListCurrentPartInfo.m_Filename);
      fileName = m_PartListCurrentPartInfo.m_Filename;
      if (ext.empty())
      {
        LOG_DEBUG("cannot determine file extension for %s", m_PartListCurrentPartInfo.m_Filename.c_str());
      }
    }
    else
    {
      ext = FileUtil::ExtensionForMimeType(m_PartListCurrentPartInfo.m_MimeType);
      fileName = std::to_string(m_PartListCurrentIndex) + ext;
      isUnamedTextHtml = (m_PartListCurrentPartInfo.m_MimeType == "text/html");
      if (ext.empty())
      {
        LOG_DEBUG("no file extension for MIME type %s", m_PartListCurrentPartInfo.m_MimeType.c_str());
      }
    }

    std::string tempFilePath;
    std::string partData;

    {
      UiModel::Lock model(m_Model);
      const std::string& folder = m_CurrentFolderUid.first;
      const int uid = m_CurrentFolderUid.second;
      std::map<uint32_t, Body>& bodys = model->m_Bodys[folder];
      std::map<uint32_t, Body>::iterator bodyIt = bodys.find(uid);
      if (bodyIt != bodys.end())
      {
        Body& body = bodyIt->second;
        const std::map<ssize_t, PartInfo>& parts = body.GetPartInfos();
        const std::map<ssize_t, std::string>& partDatas = body.GetPartDatas();
        partData = partDatas.at(m_PartListCurrentIndex);

        if (m_ShowEmbeddedImages && isUnamedTextHtml)
        {
          for (auto& part : parts)
          {
            if (!part.second.m_ContentId.empty())
            {
              const std::string& tempPartFilePath = FileUtil::GetAttachmentsTempDir() + part.second.m_ContentId;
              LOG_DEBUG("writing \"%s\"", tempPartFilePath.c_str());
              FileUtil::WriteFile(tempPartFilePath, partDatas.at(part.first));
            }
          }
        }
      }

      if (m_ShowEmbeddedImages && isUnamedTextHtml)
      {
        tempFilePath = FileUtil::GetAttachmentsTempDir() + fileName;
        StrUtil::ReplaceString(partData, "src=cid:", "src=file://" + FileUtil::GetAttachmentsTempDir());
        StrUtil::ReplaceString(partData, "src=\"cid:", "src=\"file://" + FileUtil::GetAttachmentsTempDir());
        LOG_DEBUG("writing \"%s\"", tempFilePath.c_str());
        FileUtil::WriteFile(tempFilePath, partData);
      }
      else
      {
        tempFilePath = FileUtil::GetAttachmentsTempDir() + fileName;
        LOG_DEBUG("writing \"%s\"", tempFilePath.c_str());
        FileUtil::WriteFile(tempFilePath, partData);
      }
    }

    SetDialogMessage("Waiting for external viewer to exit");
    DrawDialog();
    int rv = 0;
    if (p_Key == m_KeyExtHtmlViewer)
    {
      LOG_DEBUG("opening \"%s\" in external html viewer", tempFilePath.c_str());
      rv = m_External->ExtHtmlViewer(tempFilePath);
    }
    else
    {
      LOG_DEBUG("opening \"%s\" in external viewer", tempFilePath.c_str());
      rv = m_External->ExtPartsViewer(tempFilePath);
    }

    if (rv != 0)
    {
      SetDialogMessage("External viewer error code " + std::to_string(rv), true /* p_Warn */);
    }
    else
    {
      LOG_DEBUG("external viewer exited successfully");
      SetDialogMessage("");
    }
  }
  else if (p_Key == m_KeySaveFile)
  {
    std::string filename = FileUtil::GetDownloadsDir() + m_PartListCurrentPartInfo.m_Filename;
    if (PromptString("Save Filename: ", "Save", filename))
    {
      if (!filename.empty())
      {
        filename = FileUtil::ExpandPath(filename);

        std::string partData;
        {
          UiModel::Lock model(m_Model);
          const std::string& folder = m_CurrentFolderUid.first;
          const int uid = m_CurrentFolderUid.second;
          std::map<uint32_t, Body>& bodys = model->m_Bodys[folder];
          std::map<uint32_t, Body>::iterator bodyIt = bodys.find(uid);
          if (bodyIt != bodys.end())
          {
            Body& body = bodyIt->second;
            const std::map<ssize_t, std::string>& partDatas = body.GetPartDatas();
            partData = partDatas.at(m_PartListCurrentIndex);
          }
        }

        FileUtil::WriteFile(filename, partData);
        SetDialogMessage("File saved");
      }
      else
      {
        SetDialogMessage("Save cancelled (empty filename)");
      }
    }
    else
    {
      SetDialogMessage("Save cancelled");
    }
  }
  else if (p_Key == m_KeySaveAllFiles)
  {
    std::string directory = FileUtil::GetDownloadsDir();
    if (PromptString("Save Directory: ", "Save", directory))
    {
      directory = FileUtil::ExpandPath(directory);
      directory = directory.empty() ? "." : directory;

      if (FileUtil::IsDir(directory))
      {
        bool savedOne = false;

        {
          UiModel::Lock model(m_Model);
          const std::string& folder = m_CurrentFolderUid.first;
          const int uid = m_CurrentFolderUid.second;
          std::map<uint32_t, Body>& bodys = model->m_Bodys[folder];
          std::map<uint32_t, Body>::iterator bodyIt = bodys.find(uid);
          if (bodyIt != bodys.end())
          {
            Body& body = bodyIt->second;
            const std::map<ssize_t, std::string>& partDatas = body.GetPartDatas();
            for (auto& part : body.GetPartInfos())
            {
              if (!part.second.m_Filename.empty())
              {
                std::string filename = FileUtil::GetNonExistentPath(directory + "/" + part.second.m_Filename);
                if (!filename.empty())
                {
                  savedOne = true;
                  std::string partData = partDatas.at(part.first);
                  FileUtil::WriteFile(filename, partData);
                }
                else
                {
                  LOG_WARNING("failed generating a unique non-existent path for \"%s\"",
                              part.second.m_Filename.c_str());
                }
              }
            }
          }
        }

        if (savedOne)
        {
          SetDialogMessage("All files saved");
        }
        else
        {
          SetDialogMessage("Save cancelled (no named parts)");
        }
      }
      else
      {
        SetDialogMessage("Save cancelled (invalid dir)");
      }
    }
    else
    {
      SetDialogMessage("Save all cancelled");
    }
  }
  else if (p_Key == m_KeyGotoInbox)
  {
    if (m_MessageListSearch)
    {
      m_MessageListSearch = false;
      m_PreviousFolder = "";
    }

    SetCurrentFolder(m_Model->GetInbox());
    SetState(Ui::StateViewMessageList);
  }
  else if (HandleListKey(p_Key, m_PartListCurrentIndex))
  {
    // none
  }
  else if (m_InvalidInputNotify)
  {
    SetDialogMessage("Invalid input (" + StrUtil::ToHexString(p_Key) + ")");
  }

  DrawAll();
}

void UiPartListView::OnEnter()
{
  curs_set(0);
  m_PartListCurrentIndex = 0;
}
