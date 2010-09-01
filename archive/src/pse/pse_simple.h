/*
 * $Id: pse_simple.h,v 1.2 2010/09/01 16:05:22 philipn Exp $
 *
 * Simple PSE analysis (currently does no analysis and always passes)
 *
 * Copyright (C) 2007 BBC Research, Stuart Cunningham <stuart_hc@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef pse_simple_h
#define pse_simple_h

#include "pse.h"

class PSE_Simple : public PSE_Analyse
{
public:
    PSE_Simple();
    ~PSE_Simple();

    virtual bool init(void);
    virtual bool fini(void);
    virtual bool open(void);
    virtual bool close(void);

    virtual bool is_init(void);
    virtual bool is_open(void);

    virtual bool analyse_frame(const uint8_t *video_frame);
    virtual bool get_remaining_results(std::vector<PSEResult> &p_results);

private:
    std::vector<PSEResult> results;
    uint64_t FrameCounter, ResultsCounter;
    bool initialised;
    bool opened;
};

#endif // pse_simple_h
