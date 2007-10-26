/*
 * $Id: IngexShm.h,v 1.2 2007/10/26 16:01:22 john_f Exp $
 *
 * Interface for reading audio/video data from shared memory.
 *
 * Copyright (C) 2006  British Broadcasting Corporation.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef IngexShm_h
#define IngexShm_h

//#include <ace/Log_Msg.h>
#include "integer_types.h"

#include "nexus_control.h"

/**
C++ API for the shared memory used to pass video/audio
data into the recorder.
*/
class IngexShm
{
public:
  // Singelton accessor
    static IngexShm * Instance()
    {
        if (mInstance == 0)  // is it the first call?
        {  
            mInstance = new IngexShm; // create sole instance
        }
        return mInstance; // address of sole instance
    }
  // Singleton destroy
    static void Destroy() { delete mInstance; mInstance = 0; }

    ~IngexShm(void);
    int Init();
    unsigned int Cards() { return mCards; }
    int RingLength() { if (mpControl) return mpControl->ringlen; else return 0; }
    int LastFrame(unsigned int card_i)
    {
        int frame = 0;
        if(card_i < mCards)
        {
#ifndef _MSC_VER
            PTHREAD_MUTEX_LOCK(&mpControl->card[card_i].m_lastframe)
#endif
            frame = mpControl->card[card_i].lastframe;
#ifndef _MSC_VER
            PTHREAD_MUTEX_UNLOCK(&mpControl->card[card_i].m_lastframe)
#endif
        }
		if (frame < 0)
		{
			frame = 0;
		}
        return frame;
    }

    enum TcEnum { LTC, VITC };

    void TcMode(TcEnum mode) { mTcMode = mode; }

    // In funtions below, you could check (card < mCards) first.

    int32_t Timecode(unsigned int card, unsigned int frame)
    {
		//ACE_DEBUG((LM_DEBUG, ACE_TEXT("Timecode(%d, %d)\n"), card, frame));
        uint8_t * p_tc = mRing[card] + mpControl->elementsize * (frame % mpControl->ringlen)
            + (mTcMode == LTC ? mpControl->ltc_offset : mpControl->tc_offset);
        return * (int32_t *) p_tc;
    }
    int32_t CurrentTimecode(unsigned int card)
    {
        return Timecode(card, LastFrame(card));
    }

    bool SignalPresent(unsigned int card)
    {
        // Extend this when SHM carries status info
        return true;
    }

    uint8_t * pVideo422(int card, int frame)
    {
        return mRing[card] + mpControl->elementsize * (frame % mpControl->ringlen);
    }
    int sizeVideo422() { return mpControl->width*mpControl->height*2; }

    uint8_t * pVideo420(unsigned int card, unsigned int frame)
    {
        return mRing[card] + mpControl->elementsize * (frame % mpControl->ringlen)
            + mpControl->sec_video_offset;
    }
    unsigned int sizeVideo420() { return  mpControl->width*mpControl->height*3/2; }

    int32_t * pAudio12(unsigned int card, unsigned int frame)
    {
        return (int32_t *)
            (mRing[card] + mpControl->elementsize * (frame % mpControl->ringlen)
            + mpControl->audio12_offset);
    }

    int32_t * pAudio34(unsigned int card, unsigned int frame)
    {
        return  (int32_t *)
            (mRing[card] + mpControl->elementsize * (frame % mpControl->ringlen)
            + mpControl->audio34_offset);
    }



protected:
    // Protect constructors to force use of Instance()
    IngexShm();
    IngexShm(const IngexShm &);
    IngexShm & operator= (const IngexShm &);

private:
    unsigned int mCards;
    uint8_t * mRing[MAX_CHANNELS];
    NexusControl * mpControl;
    TcEnum mTcMode;
// static instance pointer
    static IngexShm * mInstance;
};

#endif //#ifndef IngexShm_h

