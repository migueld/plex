/*
 *      Copyright (C) 2005-2008 Team XBMC
 *      http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */
 
#include "stdafx.h"
#include "DVDPlayerSubtitle.h"
#include "DVDCodecs/Overlay/DVDOverlay.h"
#include "DVDCodecs/Overlay/DVDOverlaySpu.h"
#include "DVDCodecs/Overlay/DVDOverlayText.h"
#include "DVDCodecs/Overlay/DVDOverlayCodec.h"
#include "DVDClock.h"
#include "DVDInputStreams/DVDFactoryInputStream.h"
#include "DVDInputStreams/DVDInputStream.h"
#include "DVDInputStreams/DVDInputStreamNavigator.h"
#include "DVDSubtitles/DVDSubtitleParser.h"
#include "DVDCodecs/DVDCodecs.h"
#include "DVDCodecs/DVDFactoryCodec.h"
#include "DVDDemuxers/DVDDemuxUtils.h"

using namespace std;

CDVDPlayerSubtitle::CDVDPlayerSubtitle(CDVDOverlayContainer* pOverlayContainer)
{
  m_pOverlayContainer = pOverlayContainer;
  
  m_pSubtitleFileParser = NULL;
  m_pSubtitleStream = NULL;
  m_pOverlayCodec = NULL;
  m_lastPts = 0.0;
}

CDVDPlayerSubtitle::~CDVDPlayerSubtitle()
{
  CloseStream(false);
}

  
void CDVDPlayerSubtitle::Flush()
{
  SendMessage(new CDVDMsg(CDVDMsg::GENERAL_FLUSH));
}

void CDVDPlayerSubtitle::SendMessage(CDVDMsg* pMsg)
{
  if (pMsg->IsType(CDVDMsg::DEMUXER_PACKET))
  {
    CDVDMsgDemuxerPacket* pMsgDemuxerPacket = (CDVDMsgDemuxerPacket*)pMsg;
    DemuxPacket* pPacket = pMsgDemuxerPacket->GetPacket();

    if (m_pOverlayCodec)
    {
      double pts = pPacket->dts != DVD_NOPTS_VALUE ? pPacket->dts : pPacket->pts;
      double duration = pPacket->duration;
      int result = m_pOverlayCodec->Decode(pPacket->pData, pPacket->iSize, pts, duration);

      if(result == OC_OVERLAY)
      {
        CDVDOverlay* overlay;
        while((overlay = m_pOverlayCodec->GetOverlay()) != NULL)
        {
          overlay->iGroupId = pPacket->iGroupId;

          if(pts == DVD_NOPTS_VALUE)
          {
            if(overlay->iPTSStartTime == 0 && overlay->iPTSStopTime == 0)
              CLog::Log(LOGWARNING, "%s - unable to find timestamp for overlay", __FUNCTION__);
          }
          else
          {
            // we assume pts is better than what
            // decoder gives us, only take duration
            // from decoder if available
            overlay->iPTSStopTime -= overlay->iPTSStartTime;
            overlay->iPTSStartTime = pts;
            if(overlay->iPTSStopTime == 0.0)
              overlay->iPTSStopTime = duration;
            overlay->iPTSStopTime += overlay->iPTSStartTime;
          }

          m_pOverlayContainer->Add(overlay);
          overlay->Release();
        }
      }
    } 
    else if (m_streaminfo.codec == CODEC_ID_DVD_SUBTITLE)
    {
      CSPUInfo* pSPUInfo = m_dvdspus.AddData(pPacket->pData, pPacket->iSize, pPacket->pts);
      if (pSPUInfo)
      {
        CLog::Log(LOGDEBUG, "CDVDPlayer::ProcessSubData: Got complete SPU packet");
        pSPUInfo->iGroupId = pPacket->iGroupId;
        m_pOverlayContainer->Add(pSPUInfo);
        pSPUInfo->Release();
      }
    }

  }
  else if( pMsg->IsType(CDVDMsg::SUBTITLE_CLUTCHANGE) )
  {
    CDVDMsgSubtitleClutChange* pData = (CDVDMsgSubtitleClutChange*)pMsg;
    for (int i = 0; i < 16; i++)
    {
      BYTE* color = m_dvdspus.m_clut[i];
      BYTE* t = (BYTE*)pData->m_data[i];

      color[0] = t[2]; // Y
      color[1] = t[1]; // Cr
      color[2] = t[0]; // Cb
    }
    m_dvdspus.m_bHasClut = true;
  }
  else if( pMsg->IsType(CDVDMsg::GENERAL_FLUSH) )
  {
    m_dvdspus.Reset();
    if (m_pSubtitleFileParser) 
      m_pSubtitleFileParser->Reset();

    if (m_pOverlayCodec)
      m_pOverlayCodec->Flush();
    
    m_lastPts = 0.0;
  }

  pMsg->Release();
}

bool CDVDPlayerSubtitle::OpenStream(CDVDStreamInfo &hints, string &filename)
{
  CloseStream(false);
  m_streaminfo = hints;
  m_lastPts = 0.0;

  // okey check if this is a filesubtitle
  if(filename.size() && filename != "dvd" )
  {
    m_pSubtitleFileParser = CDVDFactorySubtitle::CreateParser(filename);
    if (!m_pSubtitleFileParser)
    {
      CLog::Log(LOGERROR, "%s - Unable to create subtitle parser", __FUNCTION__);
      CloseStream(false);
      return false;
    }

    if (!m_pSubtitleFileParser->Open(hints))
    {
      CLog::Log(LOGERROR, "%s - Unable to init subtitle parser", __FUNCTION__);
      CloseStream(false);
      return false;
    }
    return true;
  }

  // dvd's use special subtitle decoder
  if(hints.codec == CODEC_ID_DVD_SUBTITLE && filename == "dvd")
    return true;

  m_pOverlayCodec = CDVDFactoryCodec::CreateOverlayCodec(hints);
  if(m_pOverlayCodec)
    return true;

  CLog::Log(LOGERROR, "%s - Unable to init overlay codec", __FUNCTION__);
  return false;
}

void CDVDPlayerSubtitle::CloseStream(bool flush)
{
  if(m_pSubtitleStream)
    SAFE_DELETE(m_pSubtitleStream);
  if(m_pSubtitleFileParser)
    SAFE_DELETE(m_pSubtitleFileParser);
  if(m_pOverlayCodec)
    SAFE_DELETE(m_pOverlayCodec);

  m_dvdspus.FlushCurrentPacket();

  if(flush)
    m_pOverlayContainer->Clear();
}

void CDVDPlayerSubtitle::Process(double pts)
{
  if(pts == DVD_NOPTS_VALUE)
    return;
  if(!AcceptsData())
    return;

  // Clear the container if we're moving backwards in time.
  if (pts < m_lastPts)
    m_pOverlayContainer->Clear();
  
  if (m_pSubtitleFileParser)
  {
    CDVDOverlay* pOverlay = m_pSubtitleFileParser->Parse(pts);
    if (pOverlay)
      m_pOverlayContainer->Add(pOverlay);
  }
  
  m_lastPts = pts;

#if 0
  printf("===================================================\n");
  printf("Asked for %d, have %d subtitles\n", (int)(pts/1000), m_pOverlayContainer->GetOverlays()->size());
  for (int i=0; i<m_pOverlayContainer->GetOverlays()->size(); i++)
  {
    CDVDOverlayText* text = (CDVDOverlayText* )m_pOverlayContainer->GetOverlays()->at(i);
    printf(" Overlay [%d - %d]:\n", (int)(text->iPTSStartTime/1000), (int)(text->iPTSStopTime/1000));
    text->Dump();
  }
  printf("---------------------------------------------------\n");
#endif
}

bool CDVDPlayerSubtitle::AcceptsData()
{
  return m_pOverlayContainer->GetSize() < 5;
}

bool CDVDPlayerSubtitle::GetCurrentSubtitle(CStdString& strSubtitle, double pts)
{
  strSubtitle = "";
  
  Process(pts); // TODO: move to separate thread?

  m_pOverlayContainer->Lock();
  VecOverlays* pOverlays = m_pOverlayContainer->GetOverlays();
  if (pOverlays)
  {
    for(vector<CDVDOverlay*>::iterator it = pOverlays->begin();it != pOverlays->end();it++)
    {
      CDVDOverlay* pOverlay = *it;

      if (pOverlay->IsOverlayType(DVDOVERLAY_TYPE_TEXT) 
      && (pOverlay->iPTSStartTime <= pts)
      && (pOverlay->iPTSStopTime >= pts || pOverlay->iPTSStopTime == 0LL))
      {
        CDVDOverlayText::CElement* e = ((CDVDOverlayText*)pOverlay)->m_pHead;
        while (e)
        {
          if (e->IsElementType(CDVDOverlayText::ELEMENT_TYPE_TEXT))
          {
            CDVDOverlayText::CElementText* t = (CDVDOverlayText::CElementText*)e;
            strSubtitle += t->m_text;
            strSubtitle += "\n";
          }
          e = e->pNext;
        }
      }
    }
  }
  m_pOverlayContainer->Unlock();
  strSubtitle.TrimRight('\n');
  return !strSubtitle.IsEmpty();
}
