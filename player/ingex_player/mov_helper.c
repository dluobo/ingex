/*
 * $Id: mov_helper.c,v 1.1 2010/07/21 16:29:34 john_f Exp $
 *
 * Copyright (C) 2010  British Broadcasting Corporation.
 * All Rights Reserved.
 *
 * Author: Philip de Nier
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "mov_helper.h"
#include "logging.h"



#define MOV_CHECK(cmd) if (!(cmd)) { return 0; }

#if defined(NDEBUG)
#define MOV_ASSERT(cond) MOV_CHECK(cond)
#else
#define MOV_ASSERT(cond) assert(cond)
#endif

#define MOV_CHECKF(cmd) if (!(cmd)) { result = -1; goto end; }


#define MKTAG(cs) ((((uint32_t)cs[0])<<24)|(((uint32_t)cs[1])<<16)|(((uint32_t)cs[2])<<8)|(((uint32_t)cs[3])))


typedef struct
{
    uint32_t type;
    uint64_t rem_size;
    int depth;
} MOVAtom;

#define MAX_MOV_DEPTH       16
#define ATOM_DEPTH          atoms[0].depth
#define FIRST_ATOM          atoms[0]
#define CUR_ATOM            atoms[atoms[0].depth]



#if 0
static void print_type(uint32_t type)
{
    printf("%c%c%c%c\n", (type >> 24) & 0xff, (type >> 16) & 0xff, (type >> 8) & 0xff, type & 0xff);
}
#endif

static void dec_atom_size(MOVAtom *atoms, uint64_t size)
{
    /* handle case where data is read for the first atom's size and type */
    if (ATOM_DEPTH == 0 && FIRST_ATOM.rem_size == 0)
        return;

    /* decrement atom's size through the sequence of atoms back to the root */
    int i;
    for (i = ATOM_DEPTH; i >= 0; i--)
        atoms[i].rem_size -= size;
}

static int skip_bytes(FILE *file, MOVAtom *atoms, uint64_t size)
{
    MOV_CHECK(size <= CUR_ATOM.rem_size);
    if (fseek(file, size, SEEK_CUR) != 0)
        return 0;

    dec_atom_size(atoms, size);
    return 1;
}

static int push_mov_atom(MOVAtom *atoms, uint32_t type, uint64_t rem_size)
{
    MOV_ASSERT(atoms->depth < MAX_MOV_DEPTH);

    /* depth increases if it isn't the special case of the first atom being pushed */
    if (ATOM_DEPTH != 0 || FIRST_ATOM.rem_size != 0)
        ATOM_DEPTH++;

    CUR_ATOM.type = type;
    CUR_ATOM.rem_size = rem_size;

    return 1;
}

static int pop_mov_atom(FILE *file, MOVAtom *atoms)
{
    if (CUR_ATOM.rem_size > 0)
        MOV_CHECK(skip_bytes(file, atoms, CUR_ATOM.rem_size));

    if (ATOM_DEPTH != 0)
        ATOM_DEPTH--;

    return 1;
}

static int read_uint64_be(FILE *file, MOVAtom *atoms, uint64_t *value)
{
    unsigned char bytes[8];
    if (fread(bytes, 8, 1, file) != 1)
        return 0;

    *value = (((uint64_t)bytes[0]) << 56) |
             (((uint64_t)bytes[1]) << 48) |
             (((uint64_t)bytes[2]) << 40) |
             (((uint64_t)bytes[3]) << 32) |
             (((uint64_t)bytes[4]) << 24) |
             (((uint64_t)bytes[5]) << 16) |
             (((uint64_t)bytes[6]) << 8) |
               (uint64_t)bytes[7];

    dec_atom_size(atoms, 8);
    return 1;
}

static int read_int64_be(FILE *file, MOVAtom *atoms, int64_t *value)
{
    uint64_t uvalue;
    MOV_CHECK(read_uint64_be(file, atoms, &uvalue));

    *value = (int64_t)uvalue;
    return 1;
}

static int read_uint32_be(FILE *file, MOVAtom *atoms, uint32_t *value)
{
    unsigned char bytes[4];
    if (fread(bytes, 4, 1, file) != 1)
        return 0;

    *value = (((uint32_t)bytes[0]) << 24) |
             (((uint32_t)bytes[1]) << 16) |
             (((uint32_t)bytes[2]) << 8) |
               (uint32_t)bytes[3];

    dec_atom_size(atoms, 4);
    return 1;
}

static int read_int32_be(FILE *file, MOVAtom *atoms, int32_t *value)
{
    uint32_t uvalue;
    MOV_CHECK(read_uint32_be(file, atoms, &uvalue));

    *value = (int32_t)uvalue;
    return 1;
}

static int read_mov_atom_start(FILE *file, MOVAtom *atoms)
{
    uint32_t type;
    uint32_t size_32;
    uint64_t size_64;
    uint64_t rem_size;

    MOV_CHECK(read_uint32_be(file, atoms, &size_32));
    MOV_CHECK(size_32 == 1 || size_32 >= 8);
    MOV_CHECK(read_uint32_be(file, atoms, &type));
    if (size_32 == 1) {
        /* size == 1 indicates an extended 64-bit size follows */
        MOV_CHECK(read_uint64_be(file, atoms, &size_64));
        rem_size = size_64 - 16;
    } else {
        rem_size = size_32 - 8;
    }

    MOV_CHECK(push_mov_atom(atoms, type, rem_size));
    return 1;
}

static int read_timecode_data(FILE *file, int64_t offset, uint32_t *start_timecode)
{
    unsigned char bytes[4];

    if (fseek(file, offset, SEEK_SET) != 0)
        return 0;

    if (fread(bytes, 4, 1, file) != 1)
        return 0;

    *start_timecode = (((uint32_t)bytes[0]) << 24) |
                      (((uint32_t)bytes[1]) << 16) |
                      (((uint32_t)bytes[2]) << 8) |
                        (uint32_t)bytes[3];

    return 1;
}



int mvh_read_start_timecode(const char *filename, uint32_t *start_timecode)
{
    MOVAtom atoms[MAX_MOV_DEPTH];
    int result = -1;
    uint32_t component_type, component_sub_type;
    int is_timecode_track;
    uint32_t num_entries;
    int32_t timecode_offset_32;
    int64_t timecode_offset;

    memset(atoms, 0, sizeof(atoms));

    FILE *file = fopen(filename, "rb");
    if (!file) {
        ml_log_error("Failed to open file '%s': %s\n", filename, strerror(errno));
        return -2;
    }


    /* find the moov type */
    while (1) {
        MOV_CHECKF(read_mov_atom_start(file, atoms));

        if (CUR_ATOM.type == MKTAG("moov"))
            break;

        /* (possibly incomplete) selection of atoms we could expect to find at the top level */
        MOV_CHECKF(CUR_ATOM.type == MKTAG("ftyp") ||
                   CUR_ATOM.type == MKTAG("mdat") ||
                   CUR_ATOM.type == MKTAG("free") ||
                   CUR_ATOM.type == MKTAG("skip") ||
                   CUR_ATOM.type == MKTAG("wide") ||
                   CUR_ATOM.type == MKTAG("pnot"));

        MOV_CHECKF(pop_mov_atom(file, atoms));
    }


    /* find the first timecode track: trak -> mdia -> hdlr (component type = mhlr, component sub type = tmcd)
       then get the timecode data offset from the same track: trak -> mdia -> minf -> stbl -> stco/co64 atom
       then read the timecode at the given file offset */

    while (CUR_ATOM.rem_size > 0) {
        MOV_CHECKF(read_mov_atom_start(file, atoms));
        if (CUR_ATOM.type == MKTAG("trak")) {
            is_timecode_track = 0;
            while (CUR_ATOM.rem_size > 0) {
                MOV_CHECKF(read_mov_atom_start(file, atoms));
                if (CUR_ATOM.type == MKTAG("mdia")) {
                    while (CUR_ATOM.rem_size > 0) {
                        MOV_CHECKF(read_mov_atom_start(file, atoms));
                        if (CUR_ATOM.type == MKTAG("hdlr")) {
                            MOV_CHECKF(skip_bytes(file, atoms, 4));
                            MOV_CHECKF(read_uint32_be(file, atoms, &component_type));
                            MOV_CHECKF(read_uint32_be(file, atoms, &component_sub_type));
                            if (component_type == MKTAG("mhlr") && component_sub_type == MKTAG("tmcd"))
                                is_timecode_track = 1; /* this is a timecode track and minf arrives later */
                        } else if (is_timecode_track && CUR_ATOM.type == MKTAG("minf")) {
                            while (CUR_ATOM.rem_size > 0) {
                                MOV_CHECKF(read_mov_atom_start(file, atoms));
                                if (CUR_ATOM.type == MKTAG("stbl")) {
                                    while (CUR_ATOM.rem_size > 0) {
                                        MOV_CHECKF(read_mov_atom_start(file, atoms));
                                        if (CUR_ATOM.type == MKTAG("stco") || CUR_ATOM.type == MKTAG("co64")) {
                                            MOV_CHECKF(skip_bytes(file, atoms, 4));
                                            MOV_CHECKF(read_uint32_be(file, atoms, &num_entries));
                                            if (num_entries == 1) {
                                                if (CUR_ATOM.type == MKTAG("stco")) {
                                                    MOV_CHECKF(read_int32_be(file, atoms, &timecode_offset_32));
                                                    timecode_offset = timecode_offset_32;
                                                } else {
                                                    MOV_CHECKF(read_int64_be(file, atoms, &timecode_offset));
                                                }
                                                MOV_CHECKF(read_timecode_data(file, timecode_offset, start_timecode));

                                                /* success */
                                                result = 0;
                                                goto end;
                                            }
                                        }
                                        MOV_CHECKF(pop_mov_atom(file, atoms));
                                    }
                                }
                                MOV_CHECKF(pop_mov_atom(file, atoms));
                            }
                        }
                        MOV_CHECKF(pop_mov_atom(file, atoms));
                    }
                }
                MOV_CHECKF(pop_mov_atom(file, atoms));
            }
        }
        MOV_CHECKF(pop_mov_atom(file, atoms));
    }

end:
    fclose(file);
    return result;
}

