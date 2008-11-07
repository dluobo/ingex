/*
 * $Id: mxf_index_table.c,v 1.3 2008/11/07 14:12:59 philipn Exp $
 *
 * MXF index table
 *
 * Copyright (C) 2006  Philip de Nier <philipn@users.sourceforge.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
 
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <mxf/mxf.h>


static void add_delta_entry(MXFIndexTableSegment* segment, MXFDeltaEntry* entry)
{
    if (segment->deltaEntryArray == NULL)
    {
        segment->deltaEntryArray = entry;
    }
    else
    {
        uint32_t deltaEntryArrayLen = 0;
        MXFDeltaEntry* lastEntry = segment->deltaEntryArray;
        while (lastEntry->next != NULL)
        {
            deltaEntryArrayLen++;
            lastEntry = lastEntry->next;
        }
        assert(8 + deltaEntryArrayLen * 14 <= 0xffff);
        lastEntry->next = entry;
    }
}

static void add_index_entry(MXFIndexTableSegment* segment, MXFIndexEntry* entry)
{
    if (segment->indexEntryArray == NULL)
    {
        segment->indexEntryArray = entry;
    }
    else
    {
        uint32_t indexEntryArrayLen = 0;
        MXFIndexEntry* lastEntry = segment->indexEntryArray;
        while (lastEntry->next != NULL)
        {
            indexEntryArrayLen++;
            lastEntry = lastEntry->next;
        }
        assert(8 + indexEntryArrayLen * (11 + segment->sliceCount * 4 + segment->posTableCount * 8) <= 0xffff);
        lastEntry->next = entry;
    }
}

static void free_index_entry(MXFIndexEntry** entry)
{
    if (*entry == NULL)
    {
        return;
    }
    
    SAFE_FREE(&(*entry)->sliceOffset);
    SAFE_FREE(&(*entry)->posTable);
    // free of list is done in mxf_free_index_table_segment
    (*entry)->next = NULL;
    SAFE_FREE(entry);
}

static int create_index_entry(MXFIndexTableSegment* segment, MXFIndexEntry** entry)
{
    MXFIndexEntry* newEntry = NULL;
    
    CHK_MALLOC_ORET(newEntry, MXFIndexEntry);
    memset(newEntry, 0, sizeof(MXFIndexEntry));
    
    if (segment->sliceCount > 0)
    {
        CHK_MALLOC_ARRAY_OFAIL(newEntry->sliceOffset, uint32_t, segment->sliceCount);
        memset(newEntry->sliceOffset, 0, sizeof(uint32_t) * segment->sliceCount);
    }
    if (segment->posTableCount > 0)
    {
        CHK_MALLOC_ARRAY_OFAIL(newEntry->posTable, mxfRational, segment->posTableCount);
        memset(newEntry->posTable, 0, sizeof(mxfRational) * segment->posTableCount);
    }
    
    add_index_entry(segment, newEntry);
    *entry = newEntry;
    return 1;
   
fail:
    free_index_entry(&newEntry);
    return 0;
}

static void free_delta_entry(MXFDeltaEntry** entry)
{
    if (*entry == NULL)
    {
        return;
    }
    
    // free of list is done in mxf_free_index_table_segment
    (*entry)->next = NULL;
    SAFE_FREE(entry);
}

static int create_delta_entry(MXFIndexTableSegment* segment, MXFDeltaEntry** entry)
{
    MXFDeltaEntry* newEntry;
    
    CHK_MALLOC_ORET(newEntry, MXFDeltaEntry);
    memset(newEntry, 0, sizeof(MXFDeltaEntry));
    
    add_delta_entry(segment, newEntry);
    *entry = newEntry;
    return 1;   
}



int mxf_is_index_table_segment(const mxfKey* key)
{
    return mxf_equals_key(key, &g_IndexTableSegment_key);
}

int mxf_create_index_table_segment(MXFIndexTableSegment** segment)
{
    MXFIndexTableSegment* newSegment;
    
    CHK_MALLOC_ORET(newSegment, MXFIndexTableSegment);
    memset(newSegment, 0, sizeof(MXFIndexTableSegment));
    
    *segment = newSegment;
    return 1;   
}

void mxf_free_index_table_segment(MXFIndexTableSegment** segment)
{
    MXFIndexEntry* indexEntry;
    MXFIndexEntry* tmpNextIndexEntry;
    MXFDeltaEntry* deltaEntry;
    MXFDeltaEntry* tmpNextDeltaEntry;

    if (*segment == NULL)
    {
        return;
    }

    indexEntry = (*segment)->indexEntryArray;
    deltaEntry = (*segment)->deltaEntryArray;
    
    // free list using loop instead of recursive call in free_index_entry
    while (indexEntry != NULL)
    {
        tmpNextIndexEntry = indexEntry->next;
        free_index_entry(&indexEntry);
        indexEntry = tmpNextIndexEntry;
    }
    
    // free list using loop instead of recursive call in free_delta_entry
    while (deltaEntry != NULL)
    {
        tmpNextDeltaEntry = deltaEntry->next;
        free_delta_entry(&deltaEntry);
        deltaEntry = tmpNextDeltaEntry;
    }
    
    SAFE_FREE(segment);
}

int mxf_add_delta_entry(MXFIndexTableSegment* segment, int8_t posTableIndex,
    uint8_t slice, uint32_t elementData)
{
    MXFDeltaEntry* newEntry;
    
    CHK_ORET(create_delta_entry(segment, &newEntry));
    newEntry->posTableIndex = posTableIndex;
    newEntry->slice = slice;
    newEntry->elementData = elementData;

    return 1;
}

int mxf_add_index_entry(MXFIndexTableSegment* segment, int8_t temporalOffset,
    int8_t keyFrameOffset, uint8_t flags, uint64_t streamOffset, 
    uint32_t* sliceOffset, mxfRational* posTable)
{
    MXFIndexEntry* newEntry;
    
    CHK_ORET(create_index_entry(segment, &newEntry));
    newEntry->temporalOffset = temporalOffset;
    newEntry->keyFrameOffset = keyFrameOffset;
    newEntry->flags = flags;
    newEntry->streamOffset = streamOffset;
    if (segment->sliceCount > 0)
    {
        memcpy(newEntry->sliceOffset, sliceOffset, sizeof(uint32_t) * segment->sliceCount);
    }
    if (segment->posTableCount > 0)
    {
        memcpy(newEntry->posTable, posTable, sizeof(mxfRational) * segment->posTableCount);
    }

    return 1;
}

int mxf_write_index_table_segment(MXFFile* mxfFile, const MXFIndexTableSegment* segment)
{
    uint64_t segmentLen = 80;
    uint32_t deltaEntryArrayLen = 0;
    uint32_t indexEntryArrayLen = 0;
    if (segment->deltaEntryArray != NULL)
    {
        MXFDeltaEntry* entry = segment->deltaEntryArray;
        segmentLen += 12;
        while (entry != NULL)
        {
            segmentLen += 6;
            deltaEntryArrayLen++;
            entry = entry->next;
        }
    }
    if (segment->indexEntryArray != NULL)
    {
        MXFIndexEntry* entry = segment->indexEntryArray;
        segmentLen += 22; /* includes PosTableCount and SliceCount */
        while (entry != NULL)
        {
            segmentLen += 11 + segment->sliceCount * 4 + segment->posTableCount * 8;
            indexEntryArrayLen++;
            entry = entry->next;
        }
    }
    
    CHK_ORET(mxf_write_kl(mxfFile, &g_IndexTableSegment_key, segmentLen));
    
    CHK_ORET(mxf_write_local_tl(mxfFile, 0x3c0a, mxfUUID_extlen));
    CHK_ORET(mxf_write_uuid(mxfFile, &segment->instanceUID));
    CHK_ORET(mxf_write_local_tl(mxfFile, 0x3f0b, 8));
    CHK_ORET(mxf_write_int32(mxfFile, segment->indexEditRate.numerator));
    CHK_ORET(mxf_write_int32(mxfFile, segment->indexEditRate.denominator));
    CHK_ORET(mxf_write_local_tl(mxfFile, 0x3f0c, 8));
    CHK_ORET(mxf_write_int64(mxfFile, segment->indexStartPosition));
    CHK_ORET(mxf_write_local_tl(mxfFile, 0x3f0d, 8));
    CHK_ORET(mxf_write_int64(mxfFile, segment->indexDuration));
    CHK_ORET(mxf_write_local_tl(mxfFile, 0x3f05, 4));
    CHK_ORET(mxf_write_uint32(mxfFile, segment->editUnitByteCount));
    CHK_ORET(mxf_write_local_tl(mxfFile, 0x3f06, 4));
    CHK_ORET(mxf_write_uint32(mxfFile, segment->indexSID));
    CHK_ORET(mxf_write_local_tl(mxfFile, 0x3f07, 4));
    CHK_ORET(mxf_write_uint32(mxfFile, segment->bodySID));
    if (segment->indexEntryArray != NULL)
    {
        CHK_ORET(mxf_write_local_tl(mxfFile, 0x3f08, 1));
        CHK_ORET(mxf_write_uint8(mxfFile, segment->sliceCount));
        CHK_ORET(mxf_write_local_tl(mxfFile, 0x3f0e, 1));
        CHK_ORET(mxf_write_uint8(mxfFile, segment->posTableCount));
    }

    if (segment->deltaEntryArray != NULL)
    {
        MXFDeltaEntry* entry = segment->deltaEntryArray;
        CHK_ORET(mxf_write_local_tl(mxfFile, 0x3f09, (uint16_t)(8 + deltaEntryArrayLen * 6)));
        CHK_ORET(mxf_write_uint32(mxfFile, deltaEntryArrayLen));
        CHK_ORET(mxf_write_uint32(mxfFile, 6));
        while (entry != NULL)
        {
            CHK_ORET(mxf_write_int8(mxfFile, entry->posTableIndex));
            CHK_ORET(mxf_write_uint8(mxfFile, entry->slice));
            CHK_ORET(mxf_write_uint32(mxfFile, entry->elementData));
            entry = entry->next;
        }
    }
    if (segment->indexEntryArray != NULL)
    {
        MXFIndexEntry* entry = segment->indexEntryArray;
        uint32_t i;
        CHK_ORET(mxf_write_local_tl(mxfFile, 0x3f0a, (uint16_t)(8 + indexEntryArrayLen * (11 + segment->sliceCount * 4 + segment->posTableCount * 8))));
        CHK_ORET(mxf_write_uint32(mxfFile, indexEntryArrayLen));
        CHK_ORET(mxf_write_uint32(mxfFile, 11 + segment->sliceCount * 4 + segment->posTableCount * 8));
        while (entry != NULL)
        {
            CHK_ORET(mxf_write_uint8(mxfFile, entry->temporalOffset));
            CHK_ORET(mxf_write_uint8(mxfFile, entry->keyFrameOffset));
            CHK_ORET(mxf_write_uint8(mxfFile, entry->flags));
            CHK_ORET(mxf_write_uint64(mxfFile, entry->streamOffset));
            for (i = 0; i < segment->sliceCount; i++)
            {
                CHK_ORET(mxf_write_uint32(mxfFile, entry->sliceOffset[i]));
            }
            for (i = 0; i < segment->posTableCount; i++)
            {
                CHK_ORET(mxf_write_int32(mxfFile, entry->posTable[i].numerator));
                CHK_ORET(mxf_write_int32(mxfFile, entry->posTable[i].denominator));
            }
            entry = entry->next;
        }
    }
    
    return 1;
}

int mxf_read_index_table_segment(MXFFile* mxfFile, uint64_t segmentLen, MXFIndexTableSegment** segment)
{
    MXFIndexTableSegment* newSegment = NULL;
    mxfLocalTag localTag;
    uint16_t localLen;
    uint64_t totalLen;
    uint32_t deltaEntryArrayLen;
    uint32_t deltaEntryLen;
    int8_t posTableIndex;
    uint8_t slice;
    uint32_t elementData;    
    uint32_t indexEntryArrayLen;
    uint32_t indexEntryLen;
    uint8_t temporalOffset;
    uint8_t keyFrameOffset;
    uint8_t flags;
    uint64_t streamOffset;
    uint32_t* sliceOffset = NULL;
    mxfRational* posTable = NULL; 
    uint8_t i;
    
    CHK_ORET(mxf_create_index_table_segment(&newSegment));
    
    totalLen = 0;
    while (totalLen < segmentLen)
    {
        CHK_OFAIL(mxf_read_local_tl(mxfFile, &localTag, &localLen));
        totalLen += mxfLocalTag_extlen + 2;
        
        switch (localTag)
        {
            case 0x3c0a:
                CHK_OFAIL(localLen == mxfUUID_extlen);
                CHK_OFAIL(mxf_read_uuid(mxfFile, &newSegment->instanceUID));
                break;
            case 0x3f0b:
                CHK_OFAIL(localLen == 8);
                CHK_OFAIL(mxf_read_int32(mxfFile, &newSegment->indexEditRate.numerator));
                CHK_OFAIL(mxf_read_int32(mxfFile, &newSegment->indexEditRate.denominator));
                break;
            case 0x3f0c:
                CHK_OFAIL(localLen == 8);
                CHK_OFAIL(mxf_read_int64(mxfFile, &newSegment->indexStartPosition));
                break;
            case 0x3f0d:
                CHK_OFAIL(localLen == 8);
                CHK_OFAIL(mxf_read_int64(mxfFile, &newSegment->indexDuration));
                break;
            case 0x3f05:
                CHK_OFAIL(localLen == 4);
                CHK_OFAIL(mxf_read_uint32(mxfFile, &newSegment->editUnitByteCount));
                break;
            case 0x3f06:
                CHK_OFAIL(localLen == 4);
                CHK_OFAIL(mxf_read_uint32(mxfFile, &newSegment->indexSID));
                break;
            case 0x3f07:
                CHK_OFAIL(localLen == 4);
                CHK_OFAIL(mxf_read_uint32(mxfFile, &newSegment->bodySID));
                break;
            case 0x3f08:
                CHK_OFAIL(localLen == 1);
                CHK_OFAIL(mxf_read_uint8(mxfFile, &newSegment->sliceCount));
                break;
            case 0x3f0e:
                CHK_OFAIL(localLen == 1);
                CHK_OFAIL(mxf_read_uint8(mxfFile, &newSegment->posTableCount));
                break;
            case 0x3f09:
                CHK_OFAIL(mxf_read_uint32(mxfFile, &deltaEntryArrayLen));
                CHK_OFAIL(mxf_read_uint32(mxfFile, &deltaEntryLen));
                CHK_OFAIL(deltaEntryLen == 6);
                CHK_OFAIL(localLen == 8 + deltaEntryArrayLen * 6);
                for (; deltaEntryArrayLen > 0; deltaEntryArrayLen--)
                {
                    CHK_OFAIL(mxf_read_int8(mxfFile, &posTableIndex));
                    CHK_OFAIL(mxf_read_uint8(mxfFile, &slice));
                    CHK_OFAIL(mxf_read_uint32(mxfFile, &elementData));
                    CHK_OFAIL(mxf_add_delta_entry(newSegment, posTableIndex, slice, elementData));
                }
                break;
            case 0x3f0a:
                if (newSegment->sliceCount > 0)
                {
                    CHK_MALLOC_ARRAY_OFAIL(sliceOffset, uint32_t, newSegment->sliceCount);
                }
                if (newSegment->posTableCount > 0)
                {
                    CHK_MALLOC_ARRAY_OFAIL(posTable, mxfRational, newSegment->posTableCount);
                }
                CHK_OFAIL(mxf_read_uint32(mxfFile, &indexEntryArrayLen));
                CHK_OFAIL(mxf_read_uint32(mxfFile, &indexEntryLen));
                CHK_OFAIL(indexEntryLen == (uint32_t)11 + newSegment->sliceCount * 4 + newSegment->posTableCount * 8);
                CHK_OFAIL(localLen == 8 + indexEntryArrayLen * (11 + newSegment->sliceCount * 4 + 
                    newSegment->posTableCount * 8));
                for (; indexEntryArrayLen > 0; indexEntryArrayLen--)
                {
                    CHK_OFAIL(mxf_read_uint8(mxfFile, &temporalOffset));
                    CHK_OFAIL(mxf_read_uint8(mxfFile, &keyFrameOffset));
                    CHK_OFAIL(mxf_read_uint8(mxfFile, &flags));
                    CHK_OFAIL(mxf_read_uint64(mxfFile, &streamOffset));
                    for (i = 0; i < newSegment->sliceCount; i++)
                    {
                        CHK_OFAIL(mxf_read_uint32(mxfFile, &sliceOffset[i]));
                    }
                    for (i = 0; i < newSegment->posTableCount; i++)
                    {
                        CHK_OFAIL(mxf_read_int32(mxfFile, &posTable[i].numerator));
                        CHK_OFAIL(mxf_read_int32(mxfFile, &posTable[i].denominator));
                    }
                    CHK_OFAIL(mxf_add_index_entry(newSegment, temporalOffset, keyFrameOffset, flags,
                        streamOffset, sliceOffset, posTable));
                }
                break;
            default:
                mxf_log_warn("Unknown local item (%u) in index table segment", localTag);
                CHK_OFAIL(mxf_skip(mxfFile, localLen));
        }

        totalLen += localLen;
        
    }
    CHK_ORET(totalLen == segmentLen);
    
    *segment = newSegment;
    return 1;
    
fail:
    SAFE_FREE(&sliceOffset);
    SAFE_FREE(&posTable);
    mxf_free_index_table_segment(&newSegment);
    return 0;
}

int mxf_write_index_table_segment_header(MXFFile* mxfFile, const MXFIndexTableSegment* segment, 
    uint32_t numDeltaEntries, uint32_t numIndexEntries)
{
    uint64_t segmentLen = 80;
    if (numDeltaEntries > 0)
    {
        segmentLen += 12 + numDeltaEntries * 6;
    }
    if (numIndexEntries > 0)
    {
        segmentLen += 22 /* includes PosTableCount and SliceCount */ + 
            numIndexEntries * (11 + segment->sliceCount * 4 + segment->posTableCount * 8);
    }
    
    CHK_ORET(mxf_write_kl(mxfFile, &g_IndexTableSegment_key, segmentLen));
    
    CHK_ORET(mxf_write_local_tl(mxfFile, 0x3c0a, mxfUUID_extlen));
    CHK_ORET(mxf_write_uuid(mxfFile, &segment->instanceUID));
    CHK_ORET(mxf_write_local_tl(mxfFile, 0x3f0b, 8));
    CHK_ORET(mxf_write_int32(mxfFile, segment->indexEditRate.numerator));
    CHK_ORET(mxf_write_int32(mxfFile, segment->indexEditRate.denominator));
    CHK_ORET(mxf_write_local_tl(mxfFile, 0x3f0c, 8));
    CHK_ORET(mxf_write_int64(mxfFile, segment->indexStartPosition));
    CHK_ORET(mxf_write_local_tl(mxfFile, 0x3f0d, 8));
    CHK_ORET(mxf_write_int64(mxfFile, segment->indexDuration));
    CHK_ORET(mxf_write_local_tl(mxfFile, 0x3f05, 4));
    CHK_ORET(mxf_write_uint32(mxfFile, segment->editUnitByteCount));
    CHK_ORET(mxf_write_local_tl(mxfFile, 0x3f06, 4));
    CHK_ORET(mxf_write_uint32(mxfFile, segment->indexSID));
    CHK_ORET(mxf_write_local_tl(mxfFile, 0x3f07, 4));
    CHK_ORET(mxf_write_uint32(mxfFile, segment->bodySID));
    if (numIndexEntries > 0)
    {
        CHK_ORET(mxf_write_local_tl(mxfFile, 0x3f08, 1));
        CHK_ORET(mxf_write_uint8(mxfFile, segment->sliceCount));
        CHK_ORET(mxf_write_local_tl(mxfFile, 0x3f0e, 1));
        CHK_ORET(mxf_write_uint8(mxfFile, segment->posTableCount));
    }

    
    return 1;
}

int mxf_write_delta_entry_array_header(MXFFile* mxfFile, uint32_t numDeltaEntries)
{
    CHK_ORET(mxf_write_local_tl(mxfFile, 0x3f09, (uint16_t)(8 + numDeltaEntries * 6)));
    CHK_ORET(mxf_write_uint32(mxfFile, numDeltaEntries));
    CHK_ORET(mxf_write_uint32(mxfFile, 6));

    return 1;
}

int mxf_write_delta_entry(MXFFile* mxfFile, MXFDeltaEntry* entry)
{
    CHK_ORET(mxf_write_int8(mxfFile, entry->posTableIndex));
    CHK_ORET(mxf_write_uint8(mxfFile, entry->slice));
    CHK_ORET(mxf_write_uint32(mxfFile, entry->elementData));

    return 1;
}

int mxf_write_index_entry_array_header(MXFFile* mxfFile, uint8_t sliceCount, uint8_t posTableCount, 
    uint32_t numIndexEntries)
{
    CHK_ORET(mxf_write_local_tl(mxfFile, 0x3f0a, (uint16_t)(8 + numIndexEntries * (11 + sliceCount * 4 + posTableCount * 8))));
    CHK_ORET(mxf_write_uint32(mxfFile, numIndexEntries));
    CHK_ORET(mxf_write_uint32(mxfFile, 11 + sliceCount * 4 + posTableCount * 8));

    return 1;
}

int mxf_write_index_entry(MXFFile* mxfFile, uint8_t sliceCount, uint8_t posTableCount, 
    MXFIndexEntry* entry)
{
    uint32_t i;
    
    CHK_ORET(mxf_write_uint8(mxfFile, entry->temporalOffset));
    CHK_ORET(mxf_write_uint8(mxfFile, entry->keyFrameOffset));
    CHK_ORET(mxf_write_uint8(mxfFile, entry->flags));
    CHK_ORET(mxf_write_uint64(mxfFile, entry->streamOffset));
    for (i = 0; i < sliceCount; i++)
    {
        CHK_ORET(mxf_write_uint32(mxfFile, entry->sliceOffset[i]));
    }
    for (i = 0; i < posTableCount; i++)
    {
        CHK_ORET(mxf_write_int32(mxfFile, entry->posTable[i].numerator));
        CHK_ORET(mxf_write_int32(mxfFile, entry->posTable[i].denominator));
    }
    
    return 1;
}








