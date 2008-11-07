#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <mxf/mxf.h>



int test_read(const char* filename)
{
    MXFFile* mxfFile = NULL;
    MXFFilePartitions partitions;
    MXFPartition* headerPartition = NULL;
    mxfKey key;
    uint8_t llen;
    uint64_t len;
    int i;
    int k;
    MXFIndexTableSegment* indexSegment = NULL;
    MXFDeltaEntry* deltaEntry;
    MXFIndexEntry* indexEntry;

    
    if (!mxf_disk_file_open_read(filename, &mxfFile))
    {
        mxf_log_error("Failed to open '%s'" LOG_LOC_FORMAT, filename, LOG_LOC_PARAMS);
        return 0;
    }

    mxf_initialise_file_partitions(&partitions);
    
    /* read header pp */
    CHK_OFAIL(mxf_read_header_pp_kl(mxfFile, &key, &llen, &len));
    CHK_OFAIL(mxf_read_partition(mxfFile, &key, &headerPartition));
    CHK_OFAIL(mxf_append_partition(&partitions, headerPartition));
    
    
    /* TEST */

    /* read index table segment */
    
    CHK_OFAIL(mxf_read_next_nonfiller_kl(mxfFile, &key, &llen, &len));
    CHK_OFAIL(mxf_is_index_table_segment(&key));
    CHK_OFAIL(mxf_read_index_table_segment(mxfFile, len, &indexSegment));
    CHK_OFAIL(indexSegment->indexEditRate.numerator == 25 && indexSegment->indexEditRate.denominator == 1); 
    CHK_OFAIL(indexSegment->indexStartPosition == 0);
    CHK_OFAIL(indexSegment->indexDuration == 0x64);
    CHK_OFAIL(indexSegment->editUnitByteCount == 0x100);
    CHK_OFAIL(indexSegment->indexSID == 1);
    CHK_OFAIL(indexSegment->bodySID == 2);
    CHK_OFAIL(indexSegment->sliceCount == 0);
    CHK_OFAIL(indexSegment->posTableCount == 0);
    CHK_OFAIL(indexSegment->deltaEntryArray != 0);
    CHK_OFAIL(indexSegment->indexEntryArray == 0);
    deltaEntry = indexSegment->deltaEntryArray;
    for (i = 0; i < 4; i++)
    {
        CHK_OFAIL(deltaEntry != 0);
        
        CHK_OFAIL(deltaEntry->posTableIndex == i);
        CHK_OFAIL(deltaEntry->slice == i);
        CHK_OFAIL((int)deltaEntry->elementData == i);
        
        deltaEntry = deltaEntry->next;
    }

    
    /* read index table segment */
     
    CHK_OFAIL(mxf_read_next_nonfiller_kl(mxfFile, &key, &llen, &len));
    CHK_OFAIL(mxf_is_index_table_segment(&key));
    CHK_OFAIL(mxf_read_index_table_segment(mxfFile, len, &indexSegment));
    CHK_OFAIL(indexSegment->indexEditRate.numerator == 25 && indexSegment->indexEditRate.denominator == 1); 
    CHK_OFAIL(indexSegment->indexStartPosition == 0);
    CHK_OFAIL(indexSegment->indexDuration == 0x0a);
    CHK_OFAIL(indexSegment->editUnitByteCount == 0);
    CHK_OFAIL(indexSegment->indexSID == 1);
    CHK_OFAIL(indexSegment->bodySID == 2);
    CHK_OFAIL(indexSegment->sliceCount == 2);
    CHK_OFAIL(indexSegment->posTableCount == 2);
    CHK_OFAIL(indexSegment->deltaEntryArray == 0);
    CHK_OFAIL(indexSegment->indexEntryArray != 0);
    indexEntry = indexSegment->indexEntryArray;
    for (i = 0; i < indexSegment->indexDuration; i++)
    {
        CHK_OFAIL(indexEntry != 0);
        
        CHK_OFAIL(indexEntry->temporalOffset == i);
        CHK_OFAIL(indexEntry->keyFrameOffset == i);
        CHK_OFAIL(indexEntry->flags == i);
        CHK_OFAIL((int)indexEntry->streamOffset == i);
        
        for (k = 0; k < indexSegment->sliceCount; k++)
        {
            CHK_OFAIL((int)indexEntry->sliceOffset[k] == i);
        }
        
        for (k = 0; k < indexSegment->posTableCount; k++)
        {
            CHK_OFAIL(indexEntry->posTable[k].numerator == i);
            CHK_OFAIL(indexEntry->posTable[k].denominator == i + 1);
        }
        indexEntry = indexEntry->next;
    }
    
    
    
    mxf_free_index_table_segment(&indexSegment);
    mxf_file_close(&mxfFile);
    mxf_clear_file_partitions(&partitions);
    return 1;
    
fail:
    mxf_free_index_table_segment(&indexSegment);
    mxf_file_close(&mxfFile);
    mxf_clear_file_partitions(&partitions);
    return 0;
}

int test_create_and_write(const char* filename)
{
    MXFFile* mxfFile = NULL;
    MXFFilePartitions partitions;
    MXFPartition* headerPartition = NULL;
    MXFIndexTableSegment* indexSegment = NULL;
    uint32_t sliceOffset[2];
    mxfRational posTable[2];
    int i;
    int k;

    
    if (!mxf_disk_file_open_new(filename, &mxfFile))
    {
        mxf_log_error("Failed to create '%s'" LOG_LOC_FORMAT, filename, LOG_LOC_PARAMS);
        return 0;
    }

    mxf_initialise_file_partitions(&partitions);
    
    /* TEST */
    
    /* write the header pp */    
    CHK_OFAIL(mxf_append_new_partition(&partitions, &headerPartition));
    headerPartition->key = MXF_PP_K(ClosedComplete, Header);
    headerPartition->kagSize = 256;
    CHK_OFAIL(mxf_write_partition(mxfFile, headerPartition));

    /* write index table segment */    
    CHK_OFAIL(mxf_mark_index_start(mxfFile, headerPartition));

    CHK_OFAIL(mxf_create_index_table_segment(&indexSegment)); 
    mxf_generate_uuid(&indexSegment->instanceUID);
    const mxfRational editRate = {25, 1};
    indexSegment->indexEditRate = editRate;
    indexSegment->indexDuration = 0x64;
    indexSegment->editUnitByteCount = 0x100;
    indexSegment->indexSID = 1;
    indexSegment->bodySID = 2;
    for (i = 0; i < 4; i++)
    {
        CHK_OFAIL(mxf_add_delta_entry(indexSegment, i, i, i));
    }
    CHK_OFAIL(mxf_write_index_table_segment(mxfFile, indexSegment));
    
    CHK_OFAIL(mxf_fill_to_kag(mxfFile, headerPartition));

    CHK_OFAIL(mxf_mark_index_end(mxfFile, headerPartition));

    
    /* write index table segment */  
    CHK_OFAIL(mxf_mark_index_start(mxfFile, headerPartition));

    CHK_OFAIL(mxf_create_index_table_segment(&indexSegment)); 
    mxf_generate_uuid(&indexSegment->instanceUID);
    indexSegment->indexEditRate = editRate;
    indexSegment->indexDuration = 0x0a;
    indexSegment->editUnitByteCount = 0;
    indexSegment->indexSID = 1;
    indexSegment->bodySID = 2;
    indexSegment->sliceCount = 2;
    indexSegment->posTableCount = 2;
    for (i = 0; i < indexSegment->indexDuration; i++)
    {
        for (k = 0; k < indexSegment->sliceCount; k++)
        {
            sliceOffset[k] = i;
        }
        for (k = 0; k < indexSegment->posTableCount; k++)
        {
            posTable[k].numerator = i;
            posTable[k].denominator = i + 1;
        }
        CHK_OFAIL(mxf_add_index_entry(indexSegment, i, i, i, i, sliceOffset, posTable));
    }
    CHK_OFAIL(mxf_write_index_table_segment(mxfFile, indexSegment));
    
    CHK_OFAIL(mxf_fill_to_kag(mxfFile, headerPartition));

    CHK_OFAIL(mxf_mark_index_end(mxfFile, headerPartition));

    

    /* update the partitions */
    CHK_OFAIL(mxf_update_partitions(mxfFile, &partitions));    
    
    
    mxf_file_close(&mxfFile);
    mxf_clear_file_partitions(&partitions);
    mxf_free_index_table_segment(&indexSegment);
    return 1;
    
fail:
    mxf_file_close(&mxfFile);
    mxf_clear_file_partitions(&partitions);
    mxf_free_index_table_segment(&indexSegment);
    return 0;
}


void usage(const char* cmd)
{
    fprintf(stderr, "Usage: %s filename\n", cmd);
}

int main(int argc, const char* argv[])
{
    if (argc != 2)
    {
        usage(argv[0]);
        return 1;
    }

    if (!test_create_and_write(argv[1]))
    {
        return 1;
    }
    
    if (!test_read(argv[1]))
    {
        return 1;
    }

    return 0;
}

