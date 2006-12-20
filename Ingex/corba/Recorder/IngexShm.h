/*
 * $Id: IngexShm.h,v 1.1 2006/12/20 12:28:26 john_f Exp $
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
        return frame;
    }

    enum TcEnum { LTC, VITC };

    void TcMode(TcEnum mode) { mTcMode = mode; }

    // In funtions below, you could check (card < mCards) first.

    int32_t Timecode(int card, int frame)
    {
        uint8_t * p_tc = mRing[card] + mpControl->elementsize * (frame % mpControl->ringlen)
            + mpControl->tc_offset - (mTcMode == LTC ? 4 : 0);
        return * (int32_t *) p_tc;
    }
    int32_t CurrentTimecode(int card)
    {
        return Timecode(card, LastFrame(card));
    }

    uint8_t * pVideo422(int card, int frame)
    {
        return mRing[card] + mpControl->elementsize * (frame % mpControl->ringlen);
    }
    int sizeVideo422() { return 720*576*2; }

    uint8_t * pVideo420(int card, int frame)
    {
        return mRing[card] + mpControl->elementsize * (frame % mpControl->ringlen)
            + mpControl->audio12_offset + mpControl->audio_size;
    }
    int sizeVideo420() { return 720*576*3/2; }

    int32_t * pAudio12(int card, int frame)
    {
        return (int32_t *)
            (mRing[card] + mpControl->elementsize * (frame % mpControl->ringlen)
            + mpControl->audio12_offset);
    }

    int32_t * pAudio34(int card, int frame)
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
    uint8_t * mRing[MAX_CARDS];
    NexusControl * mpControl;
    TcEnum mTcMode;
// static instance pointer
    static IngexShm * mInstance;
};

#endif //#ifndef IngexShm_h

