/*
 * $Id: Rational.h,v 1.1 2010/09/06 13:48:24 john_f Exp $
 *
 * Rational data type
 *
 * Copyright (C) 2010 British Broadcasting Corporation
 * All rights reserved
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
 
#ifndef common_Rational_h
#define common_Rational_h

namespace Ingex
{

struct Rational
{
    Rational() : numerator(0), denominator(0) {}
    Rational(int num, int den) : numerator(num), denominator(den) {}
    int numerator;
    int denominator;
};

inline bool operator == (const Rational& l, const Rational& r)
{
    return l.numerator == r.numerator && l.denominator == r.denominator;
}

inline bool operator != (const Rational& l, const Rational& r)
{
    return l.numerator != r.numerator || l.denominator != r.denominator;
}

const Rational RATIONAL_0_0 (0, 0);
const Rational RATIONAL_4_3 (4, 3);
const Rational RATIONAL_16_9 (16, 9);

}; // namespace


#endif //ifndef common_Rational_h

