/*
 * $Id: mxf_essence.c,v 1.1 2007/11/06 10:07:23 stuart_hc Exp $
 *
 * Functions to support extraction of raw essence data out of MXF files.
 *
 * Copyright (C) 2006  Philip de Nier <philipn@users.sourceforge.net>
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
 
 
#include <assert.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>

#include <mxf_essence.h>


/* keep this in sync with mxfe_EssenceType */
static const struct 
{
    mxfe_EssenceType type;
    const char* suffix;
} essenceTypeSuffixes[] = 
{
    {MXFE_DV, "dv"},
    {MXFE_PCM, "pcm"},
    {MXFE_AVIDMJPEG, "mjpeg"}
};

typedef struct 
{
    uint8_t octet0;
    uint8_t octet1;
    uint8_t octet2;
    uint8_t octet3;
    uint8_t octet4;
    uint8_t octet5;
    uint8_t octet6;
    uint8_t octet7;
    uint8_t octet8;
    uint8_t octet9;
    uint8_t octet10;
    uint8_t octet11;
    uint8_t octet12;
    uint8_t octet13;
    uint8_t octet14;
    uint8_t octet15;
} Key;
 
typedef Key UL;


static const Key g_ClosedCompleteHeaderPP_key = 
    {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x05, 0x01, 0x01, 0x0d, 0x01, 0x02, 0x01, 0x01, 0x02, 0x04, 0x00};

    
/* octet7 is the registry version (should be 0x02, but is sometimes set to 0x01) */
static const UL g_Base_OPAtom_ul = 
    {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0xFF, 0x0d, 0x01, 0x02, 0x01, 0x10, 0xFF, 0x00, 0x00};

    
/* octect14 should be either 0x02 (BWF Clip Wrapped) or 0x04 (AES3 Clip Wrapped) */ 
static const UL g_Base_AES3BWFEssenceContainer_ul = 
    {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01, 0x0d, 0x01, 0x03, 0x01, 0x02, 0x06, 0xFF, 0x00};

/* private Avid label */
static const UL g_AvidMJPEGEssenceContainer_ul = 
    {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01, 0x0e, 0x04, 0x03, 0x01, 0x02, 0x01, 0x00, 0x00};

/* octet14 indicates the DV type and octet15 value 0x02 indicates clip wrapping mode */ 
static const UL g_Base_DVEssenceContainer_ul = 
    {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01, 0x0d, 0x01, 0x03, 0x01, 0x02, 0x02, 0xFF, 0xFF};

    
static const Key g_Base_GCEssenceElement_key = 
    {0x06, 0x0e, 0x2b, 0x34, 0x01, 0x02, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    

static int is_cc_header_partition(const Key* key)
{
    return memcmp(&g_ClosedCompleteHeaderPP_key, key, sizeof(Key)) == 0;
}


static int is_op_atom(const UL* label)
{
    return memcmp(&g_Base_OPAtom_ul, label, 7) == 0 &&
        (label->octet7 == 0x02 || label->octet7 == 0x01 /* invalid value */) &&
        memcmp(&g_Base_OPAtom_ul.octet8, &label->octet8, 5) == 0;
}


static int is_pcm_essence(const UL* label)
{
    return memcmp(&g_Base_AES3BWFEssenceContainer_ul, label, 14) == 0 &&
        (label->octet14 == 0x02 || label->octet14 == 0x04);
}

static int is_dv_essence(const UL* label)
{
    return memcmp(&g_Base_DVEssenceContainer_ul, label, 14) == 0 && 
        label->octet15 == 0x02;
}

static int is_avidmjpeg_essence(const UL* label)
{
    return memcmp(&g_AvidMJPEGEssenceContainer_ul, label, 16) == 0;
}


static int is_gc_essence_element(const Key* key)
{
    return memcmp(&g_Base_GCEssenceElement_key, key, 7) == 0;
}



static int read_uint32(FILE* file, uint32_t* value)
{
    unsigned char buffer[4];
    if (fread(buffer, 1, 4, file) != 4)
    {
        return 0;
    }
    
    *value = (buffer[0]<<24) | (buffer[1]<<16) | (buffer[2]<<8) | (buffer[3]);
    
    return 1;
}

static void print_key(Key *key)
{
    int i;
    printf("K = ");
    for (i = 0; i < 16; i++)
    {
        printf("%02x ", ((uint8_t*)key)[i]);
    }
    printf("\n");
}

static int read_k(FILE *file, Key *key)
{
    if (fread((uint8_t*)key, 16, 1, file) != 1) 
    {
        return 0;
    }
    
    return 1;
}

static int read_kl(FILE *file, Key *key, uint8_t* llen, uint64_t *len)
{
    int i;
    int c;
    uint64_t length = 0;
    uint64_t lenLength = 0;

    if (!read_k(file, key))
    {
        return 0;
    }
    
    if ((c = fgetc(file)) == EOF) 
    {
        return 0;
    }

    if (c < 0x80)
    {
        length = c;
        lenLength = 1;
    }
    else 
    {
        lenLength = (c & 0x7f) + 1;
        for (i = 0; i < lenLength - 1; i++) 
        {
            if ((c = fgetc(file)) == EOF) 
            {
                return 0;
            }
            length = length << 8;
            length = length | c;
        }
    }
    
    *len = length;
    *llen = lenLength;
    return 1;
}

static int read_label(FILE* file, UL* label)
{
    return read_k(file, label);
}

static int read_batch_header(FILE* file, uint32_t* len, uint32_t* eleLen)
{
    if (!read_uint32(file, len) || !read_uint32(file, eleLen))
    {
        return 0;
    }
    
    return 1;
}





int mxfe_get_essence_type(FILE* file, mxfe_EssenceType* type)
{
    Key key;
    uint8_t llen;
    uint64_t len;
    UL opLabel;
    int isAAFKLV = 0;
    uint32_t batchLen;
    uint32_t batchEleLen;
    uint32_t i;
    UL ecLabel;

    
    /* make sure we are at the start of the file */    
    if (fseek(file, 0, SEEK_SET) != 0)
    {
        perror("fseek");
        fprintf(stderr, "Failed to seek to start of file\n");
        return 0;
    }

    /* check closed and complete header partition key */
    if (!read_kl(file, &key, &llen, &len) || !is_cc_header_partition(&key))
    {
        return 0;
    }
    
    /* skip uninteresting partition pack metadata */    
    if (fseek(file, 64, SEEK_CUR) != 0)
    {
        perror("fseek");
        fprintf(stderr, "Failed to skip partition elements before OP label\n");
        return 0;
    }
    
    /* check UL for operational pattern Atom */
    if (!read_label(file, &opLabel) || !is_op_atom(&opLabel))
    {
        return 0;
    }

    
    /* check essence container labels */
    
    if (!read_batch_header(file, &batchLen, &batchEleLen))
    {
        fprintf(stderr, "Failed to read essence container labels batch header\n");
        return 0;
    }
    if (batchLen != 1 || batchEleLen != 16)
    {
        fprintf(stderr, "Expecting single essence container label\n");
        return 0;
    }
    if (!read_label(file, &ecLabel))
    {
        fprintf(stderr, "Failed to read essence container label\n");
        return 0;
    }
    
    if (is_pcm_essence(&ecLabel))
    {
        *type = MXFE_PCM;
        return 1;
    }
    else if (is_dv_essence(&ecLabel))
    {
        *type =  MXFE_DV;
        return 1;
    }
    else if (is_avidmjpeg_essence(&ecLabel))
    {
        *type =  MXFE_AVIDMJPEG;
        return 1;
    }

    /* unknown essence type */
    return 0;
}

int mxfe_get_essence_element_info(FILE* file, uint64_t* offset, uint64_t* len)
{
    Key key;
    uint8_t tllen;
    uint64_t tlen;
    int64_t toffset;

    /* make sure we are at the start of the file */    
    if (fseek(file, 0, SEEK_SET) != 0)
    {
        perror("fseek");
        fprintf(stderr, "Failed to seek to start of file\n");
        return 0;
    }

    /* position file at the essence data */    
    while (1)
    {
        if (!read_kl(file, &key, &tllen, &tlen))
        {
            fprintf(stderr, "Failed to read KL\n");
            return 0;
        }
        
        if (is_gc_essence_element(&key))
        {
            if ((toffset = ftell(file)) < 0)
            {
                perror("ftell");
                fprintf(stderr, "Failed to tell file position\n");
                return 0;
            }
            break;
        }
        /* skip value */
        else
        {
            if (fseek(file, tlen, SEEK_CUR) != 0)
            {
                perror("fseek");
                fprintf(stderr, "Failed to skip value\n");
                return 0;
            }
        }
    }

    
    *len = tlen;
    *offset = (uint64_t)toffset;
    return 1;
}

int mxfe_get_essence_suffix(mxfe_EssenceType type, const char** suffix)
{
    assert(type < sizeof(essenceTypeSuffixes) / sizeof(struct {mxfe_EssenceType type; const char* suffix;}));
    
    *suffix = essenceTypeSuffixes[type].suffix;
    
    return 1;
}


