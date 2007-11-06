/*
 * $Id: test_mxf_essence.c,v 1.1 2007/11/06 10:07:23 stuart_hc Exp $
 *
 * Utility to test the essence extraction code.
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
 
 
#include <stdio.h>
#include <stdlib.h>

#include <mxf_essence.h>


void usage(const char* cmd)
{
    fprintf(stderr, "%s <mxf filename> [<out raw>]\n", cmd);
}

int main(int argv, const char* argc[])
{
    FILE* input = NULL;
    FILE* output = NULL;
    unsigned char buffer[4096];
    const size_t bufferSize = 4096;
    size_t numRead;
    size_t totalRead;
    
    if (argv < 2)
    {
        usage(argc[0]);
        exit(1);
    }
    
    if ((input = fopen(argc[1], "rb")) == NULL)
    {
        fprintf(stderr, "Failed to open input file %s\n", argc[1]);
        exit(1);
    }
    
    if (argv == 3 && (output = fopen(argc[2], "wb")) == NULL)
    {
        fprintf(stderr, "Failed to open output file %s\n", argc[2]);
        exit(1);
    }
    printf("Writing essence data to '%s'\n", argc[2]);
    
    mxfe_EssenceType type;
    if (!mxfe_get_essence_type(input, &type))
    {
        fprintf(stderr, "Failed to get essence data type\n");
    }
    else
    {
        const char* suffix;
        if (!mxfe_get_essence_suffix(type, &suffix))
        {
            fprintf(stderr, "Failed to get essence data suffix\n");
        }
        else
        {
            printf("MXF essence type = '%s'\n", suffix);
            
            uint64_t offset;
            uint64_t len;
            if (!mxfe_get_essence_element_info(input, &offset, &len))
            {
                fprintf(stderr, "Failed to get essence element offset and len\n");
            }
            else
            {
                printf("Essence element offset = %lld and length = %lld\n", offset, len);
            }
            if (output != NULL)
            {
                if (fseek(input, offset, SEEK_SET) != 0)
                {
                    fprintf(stderr, "Failed to seek to start of essence element offset\n");
                }
                else
                {
                    totalRead = 0;
                    while (totalRead < len)
                    {
                        if (len - totalRead > bufferSize)
                        {
                            numRead = bufferSize;
                        }
                        else
                        {
                            numRead = len - totalRead;
                        }
                        if (fread(buffer, 1, numRead, input) != numRead)
                        {
                            fprintf(stderr, "Failed to read essence data\n");
                            break;
                        }
                        if (fwrite(buffer, 1, numRead, output) != numRead)
                        {
                            fprintf(stderr, "Failed to write essence data\n");
                            break;
                        }
                        totalRead += numRead;
                    }
                }
            }
        }
    }
    
    fclose(input);
    if (output != NULL)
    {
        fclose(output);
    }
    
    return 0;
}
