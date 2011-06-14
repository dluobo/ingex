/*
 * Copyright (C) 2011  British Broadcasting Corporation.
 * All Rights Reserved.
 *
 * Author: Tom Heritage
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


// Functions for accessing MXF files from disk from a Linux machine. It
// is particularly advantageous to use these optimised functions when performance
// is critical e.g. when the MXF files are stored on Network Attached Storage.


// This is needed for using posix_fadvise
#define _XOPEN_SOURCE 600

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <inttypes.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <mxf/mxf.h>
#include "mxf_linux_disk_file.h"



struct MXFLinuxDiskFile
{
    MXFFile *mxfFile;
};

struct MXFFileSysData
{
    MXFLinuxDiskFile mxfLinuxDiskFile;

    int fileId;
    int isSeekable;
    int haveTestedIsSeekable;
    int contentPackageSize;
};



static void linux_disk_file_close(MXFFileSysData *sysData)
{
    if (sysData->fileId != -1)
    {
        close(sysData->fileId);
        sysData->fileId = -1;
    }
}

static uint32_t linux_disk_file_write(MXFFileSysData *sysData, const uint8_t *data, uint32_t count)
{
    return write(sysData->fileId, data, count);
}

static int linux_disk_file_putchar(MXFFileSysData *sysData, int c)
{
    uint8_t cbyte = (uint8_t)c;
    if (write(sysData->fileId, &cbyte, 1) != 1)
    {
        return EOF;
    }

    return c;
}

static int linux_disk_file_seek(MXFFileSysData *sysData, int64_t offset, int whence)
{
    off_t return_value;
    return_value = lseek(sysData->fileId, offset, whence);

    if (return_value >= 0)
    {
        // Using posix_fadvise tells the kernel which data from the file we're going to need.
        // This should prevent the kernel readahead functions from reading into the page file
        // lots of data that we know will not actually be needed. This is most often observed
        // when shuttling through MXF video content.
        // Don't worry about advising the kernel that we'll need data beyond the end of the file
        posix_fadvise(sysData->fileId, return_value, sysData->contentPackageSize, POSIX_FADV_WILLNEED);
    }

    return (return_value != -1);
}

static int64_t linux_disk_file_tell(MXFFileSysData *sysData)
{
    return lseek(sysData->fileId, 0, SEEK_CUR);
}

static uint32_t linux_disk_file_read(MXFFileSysData *sysData, uint8_t *data, uint32_t count)
{
    return read(sysData->fileId, data, count);
}

static int linux_disk_file_getchar(MXFFileSysData *sysData)
{
    uint8_t c;
    if (read(sysData->fileId, &c, 1) != 1)
    {
        return EOF;
    }

    return c;
}

static void free_linux_disk_file(MXFFileSysData *sysData)
{
    if (sysData == NULL)
    {
        return;
    }

    free(sysData);
}

static int linux_disk_file_is_seekable(MXFFileSysData *sysData)
{
    if (sysData == NULL)
    {
        return 0;
    }

    if (!sysData->haveTestedIsSeekable)
    {
        sysData->isSeekable = (lseek(sysData->fileId, 0, SEEK_CUR) != -1);
        sysData->haveTestedIsSeekable = 1;
    }

    return sysData->isSeekable;
}

static int64_t linux_disk_file_size(MXFFileSysData *sysData)
{
    struct stat64 statBuf;

    if (sysData == NULL)
    {
        return -1;
    }

    if (fstat64(sysData->fileId, &statBuf) != 0)
    {
        return -1;
    }

    return statBuf.st_size;
}

static int linux_disk_file_eof(MXFFileSysData *sysData)
{
    // There isn't a UNIX eof function
    // This implementation will be subtelty different to how feof operates
    // because feof will only return 1 if an operation has tried to reach *beyond*
    // the end of the file, whereas this implementation will return 1 if the file pointer
    // is *at* the end of the file. This is because there doesn't appear to be an EOF flag that
    // is set by the Unix file access functions which records when an attempt has been made to reach
    // beyond the end of a file.
    int64_t current_file_size = linux_disk_file_size(sysData);
    if (current_file_size == -1)
    {
        // An error has occurred...
        return -1;
    }
    else if (current_file_size == linux_disk_file_tell(sysData))
    {
        // At end-of-file
        return 1;
    }
    else
    {
        // Not at end-of-file
        return 0;
    }
}



int mldf_open_read(const char *filename, MXFLinuxDiskFile **mxfLinuxDiskFile)
{
    MXFFile *newMXFFile = NULL;
    MXFFileSysData *newDiskFile = NULL;

    CHK_MALLOC_ORET(newMXFFile, MXFFile);
    memset(newMXFFile, 0, sizeof(MXFFile));
    CHK_MALLOC_OFAIL(newDiskFile, MXFFileSysData);
    memset(newDiskFile, 0, sizeof(MXFFileSysData));

    if ((newDiskFile->fileId = open(filename, O_RDONLY | O_LARGEFILE )) == -1)
    {
        goto fail;
    }

    newMXFFile->close = linux_disk_file_close;
    newMXFFile->read = linux_disk_file_read;
    newMXFFile->write = linux_disk_file_write;
    newMXFFile->get_char = linux_disk_file_getchar;
    newMXFFile->put_char = linux_disk_file_putchar;
    newMXFFile->eof = linux_disk_file_eof;
    newMXFFile->seek = linux_disk_file_seek;
    newMXFFile->tell = linux_disk_file_tell;
    newMXFFile->is_seekable = linux_disk_file_is_seekable;
    newMXFFile->size = linux_disk_file_size;
    newMXFFile->sysData = newDiskFile;
    newMXFFile->free_sys_data = free_linux_disk_file;
    newMXFFile->sysData->mxfLinuxDiskFile.mxfFile = newMXFFile;
    newMXFFile->sysData->contentPackageSize = 870000; //Set default value.

    *mxfLinuxDiskFile = &newMXFFile->sysData->mxfLinuxDiskFile;
    return 1;

fail:
    SAFE_FREE(&newMXFFile);
    SAFE_FREE(&newDiskFile);
    return 0;
}

MXFFile* mldf_get_file(MXFLinuxDiskFile *mxfLinuxDiskFile)
{
    return mxfLinuxDiskFile->mxfFile;
}

int mldf_set_content_package_size(int packageSize, MXFLinuxDiskFile *mxfLinuxDiskFile)
{
    if (packageSize < 0)
    {
       return 0;
    }

    mxfLinuxDiskFile->mxfFile->sysData->contentPackageSize = packageSize;
    return 1;
}

