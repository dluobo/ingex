/*
 * $Id: make_osd_symbol.c,v 1.3 2009/01/29 07:10:26 stuart_hc Exp $
 *
 *
 *
 * Copyright (C) 2008-2009 British Broadcasting Corporation, All Rights Reserved
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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/*
   0. create image in gimp
        the YUV overlay requires an even number for image width and height
        choose greyscale
   1. export the bitmap image from gimp as a C file
        deselect all options at export (eg. no guint8*)
   2. set the width, height and name
   3. copy the bitmpa string from the .c file to rawSymbol
   4. compile - gcc make_osd_symbol.c
   5. execute = ./a.out > dump
   6. copy dump to osd_symbols.c
   7. add declaration in osd_symbol.h

*/


static const int width = 42;
static const int height = 42;
static const char* name = "FieldSymbol";
static const unsigned char* rawSymbol =
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\352\352\352\312\312\312\266\266\266\266\266\266\266\266\266"
  "\227\227\227fffGGGGGGfff\227\227\227\266\266\266\266\266\266\266\266\266"
  "\312\312\312\352\352\352\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\352\352\352\253\253\253fff333\23\23\23\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\23\23\23""333fff\253\253"
  "\253\352\352\352\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\352\352\352\253\253\253fff333\23\23\23\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\23\23\23""333fff\253\253\253\352\352\352\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\352\352\352\253\253\253RRR\23"
  "\23\23\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\23\23\23RRR\253\253\253\352\352\352\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\352\352"
  "\352\253\253\253RRR\23\23\23\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\23\23\23RRR\253\253"
  "\253\352\352\352\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\352\352\352\227\227\227333\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0""333\227\227\227\352\352\352\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\312"
  "\312\312fff\23\23\23\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\23\23\23fff\312\312\312\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\266\266\266GGG\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0GG"
  "G\266\266\266\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\352\352\352\227\227\227333\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0""333\227\227\227\352\352"
  "\352\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\266\266\266GGG\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0GGG\266\266\266\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\266\266\266GGG\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0GG"
  "G\266\266\266\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\312\312\312fff\23\23\23\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\23\23\23fff\312\312\312\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\266\266\266GGG\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0GGG\266\266\266\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\266\266\266GGG\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0GGG\266"
  "\266\266\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\352\352\352\253\253\253RRR\23\23\23\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\23\23\23RRR\253\253\253\352"
  "\352\352\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\312\312\312fff\23"
  "\23\23\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\23\23\23fff\312\312\312\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\352\352\352\227\227\227333\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0""333\227\227\227\352"
  "\352\352\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\352\352\352\253\253\253fff3"
  "33\23\23\23\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\23"
  "\23\23""333fff\253\253\253\352\352\352\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\352\352\352\253\253\253"
  "fff333\23\23\23\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\23\23\23""333fff\253\253\253\352\352"
  "\352\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\352\352\352\312\312\312\227\227\227fffGGGGGGGGG333\23\23"
  "\23\0\0\0\0\0\0\23\23\23""333GGGGGGGGGfff\227\227\227\312\312\312\352\352"
  "\352\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\352\352"
  "\352\312\312\312\266\266\266\266\266\266\312\312\312\352\352\352\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376"
  "";


int main()
{
    int i, j;
    size_t size = width * height * 3;

    printf("BYTE g_osd%sBitMap[] = \n", name);
    printf("{\n");
    for (i = 0; i < size / (width * 3); i++)
    {
        printf("    ");
        for (j = 0; j < width; j++)
        {
            if (rawSymbol[i * width * 3 + 3 * j] >= 0xfe)
            {
                printf("0x%02x,", 0x00);
            }
            else
            {
                printf("0x%02x,", 0xff - rawSymbol[i * width * 3 + 3 * j]);
            }
        }
        printf("\n");
    }
    for (i = 0; i < size / (width * 3); i++)
    {
        printf("    ");
        for (j = 0; j < width; j++)
        {
            if (j + 1 >= width && i + 1 >= size / (width * 3))
            {
                printf("0x%02x", 0x00);
            }
            else
            {
                printf("0x%02x,", 0x00);
            }
        }
        printf("\n");
    }
    printf("};\n");
    printf("\n");
    printf("overlay g_osd%s = \n", name);
    printf("{\n");
    printf("    %d,\n", width);
    printf("    %d,\n", height);
    printf("    -1,\n");
    printf("    -1,\n");
    printf("    g_osd%sBitMap,\n", name);
    printf("    0\n");
    printf("};\n");
    printf("\n");
}

