/*******************************************************************************
 *  ASKL.                                                                      *
 *  Copyright (c) 2026 Raphael Prevost <raph@el.bzh>                           *
 *                                                                             *
 *  This software is a computer program whose purpose is to provide a          *
 *  framework for developing and prototyping network services.                 *
 *                                                                             *
 *  This software is governed by the CeCILL  license under French law and      *
 *  abiding by the rules of distribution of free software.  You can  use,      *
 *  modify and/ or redistribute the software under the terms of the CeCILL     *
 *  license as circulated by CEA, CNRS and INRIA at the following URL          *
 *  "http://www.cecill.info".                                                  *
 *                                                                             *
 *  As a counterpart to the access to the source code and  rights to copy,     *
 *  modify and redistribute granted by the license, users are provided only    *
 *  with a limited warranty  and the software's author,  the holder of the     *
 *  economic rights,  and the successive licensors  have only  limited         *
 *  liability.                                                                 *
 *                                                                             *
 *  In this respect, the user's attention is drawn to the risks associated     *
 *  with loading,  using,  modifying and/or developing or reproducing the      *
 *  software by the user in light of its specific status of free software,     *
 *  that may mean  that it is complicated to manipulate,  and  that  also      *
 *  therefore means  that it is reserved for developers  and  experienced      *
 *  professionals having in-depth computer knowledge. Users are therefore      *
 *  encouraged to load and test the software's suitability as regards their    *
 *  requirements in conditions enabling the security of their systems and/or   *
 *  data to be ensured and,  more generally, to use and operate it in the      *
 *  same conditions as regards security.                                       *
 *                                                                             *
 *  The fact that you are presently reading this means that you have had       *
 *  knowledge of the CeCILL license and that you accept its terms.             *
 *                                                                             *
 ******************************************************************************/

#include "float.h"

typedef struct _float {
    int _exponent;
    unsigned long _fraction;
    unsigned long _extended;
    unsigned char _type;
    unsigned char _sign;
} _float;

#define _TYPE_SINGLE 0x01
#define _TYPE_DOUBLE 0x02

/* -------------------------------------------------------------------------- */

static int _single_to_fp(const unsigned char bin[4], _float *fp, int f)
{
    int test_endian = 1;
    int sys_endian = 0; /* 0 == big endian, little endian otherwise */
    unsigned char b1, b2, b3, b4;

    if (! bin || ! fp) return -1;

    b1 = b2 = b3 = b4 = 0;

    /* check the system endianness */
    sys_endian = (*(char *) & test_endian == 1) ? 1 : 0;

    /* use the system endianness if nothing was precised */
    if ( (~f & FLOAT_BIG) && (~f & FLOAT_LITTLE) )
        f |= (sys_endian) ? FLOAT_LITTLE : FLOAT_BIG;

    if (f & FLOAT_BIG) {
        b1 = bin[0]; b2 = bin[1]; b3 = bin[2]; b4 = bin[3];
    } else if (f & FLOAT_LITTLE) {
        b1 = bin[3]; b2 = bin[2]; b3 = bin[1]; b4 = bin[0];
    }

    fp->_type = _TYPE_SINGLE;
    fp->_sign = ( b1 & 0x80 ) >> 7;
    fp->_exponent = ( ( b1 & 0x7f ) << 1 ) + ( ( b2 & 0x80 ) >> 7 );
    fp->_fraction = ( ( b2 & 0x7f ) << 16 ) + ( b3 << 8 ) + b4;

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _double_to_fp(const unsigned char bin[8], _float *fp, int f)
{
    int test_endian = 1;
    int sys_endian = 0; /* 0 == big endian, little endian otherwise */
    unsigned char b1, b2, b3, b4, b5, b6, b7, b8;

    if (! bin || ! fp) return -1;

    b1 = b2 = b3 = b4 = b5 = b6 = b7 = b8 = 0;

    /* check the system endianness */
    sys_endian = (*(char *) & test_endian == 1) ? 1 : 0;

    /* use the system endianness if nothing was precised */
    if ( (~f & FLOAT_BIG) && (~f & FLOAT_LITTLE) )
        f |= (sys_endian) ? FLOAT_LITTLE : FLOAT_BIG;

    if (f & FLOAT_BIG) {
        b1 = bin[0]; b2 = bin[1]; b3 = bin[2]; b4 = bin[3];
        b5 = bin[4]; b6 = bin[5]; b7 = bin[6]; b8 = bin[7];
    } else if (f & FLOAT_LITTLE) {
        b1 = bin[7]; b2 = bin[6]; b3 = bin[5]; b4 = bin[4];
        b5 = bin[3]; b6 = bin[2]; b7 = bin[1]; b8 = bin[0];
    }

    fp->_type = _TYPE_DOUBLE;
    fp->_sign = ( b1 & 0x80 ) >> 7;
    fp->_exponent = ( ( b1 & 0x7f ) << 4 ) + ( ( b2 & 0xf0 ) >> 4 );
    fp->_fraction = ( ( b2 & 0x0f ) << 16 ) + ( b3 << 8 ) + b4;
    fp->_extended = ( b5 << 24 ) + ( b6 << 16 ) + ( b7 << 8 ) + b8;

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _fp_to_single(_float *fp, unsigned char bin[4], int f)
{
    int test_endian = 1;
    int sys_endian = 0; /* 0 == big endian, little endian otherwise */
    unsigned char *b1, *b2, *b3, *b4;

    if (! bin || ! fp || fp->_type != _TYPE_SINGLE) return -1;

    b1 = b2 = b3 = b4 = NULL;

    /* check the system endianness */
    sys_endian = (*(char *) & test_endian == 1) ? 1 : 0;

    /* use the system endianness if nothing was precised */
    if ( (~f & FLOAT_BIG) && (~f & FLOAT_LITTLE) )
        f |= (sys_endian) ? FLOAT_LITTLE : FLOAT_BIG;

    if (f & FLOAT_BIG) {
        b1 = & bin[0]; b2 = & bin[1]; b3 = & bin[2]; b4 = & bin[3];
    } else if (f & FLOAT_LITTLE) {
        b1 = & bin[3]; b2 = & bin[2]; b3 = & bin[1]; b4 = & bin[0];
    }

    *b1 = (unsigned char) (fp->_sign + ( ( fp->_exponent & 0xfe ) >> 1 ) );
    *b2 = (unsigned char) (( ( fp->_exponent & 0x00000001 ) << 7 ) +
          ( ( fp->_fraction & 0x007f0000 ) >> 16));
    *b3 = (unsigned char) ( ( fp->_fraction & 0x0000ff00 ) >> 8);
    *b4 = (unsigned char) ( ( fp->_fraction & 0x000000ff ) );

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _fp_to_double(_float *fp, unsigned char bin[8], int f)
{
    int test_endian = 1;
    int sys_endian = 0; /* 0 == big endian, little endian otherwise */
    unsigned char *b1, *b2, *b3, *b4, *b5, *b6, *b7, *b8;

    if (! bin || ! fp || fp->_type != _TYPE_DOUBLE) return -1;

    /* check the system endianness */
    sys_endian = (*(char *) & test_endian == 1) ? 1 : 0;

    b1 = b2 = b3 = b4 = b5 = b6 = b7 = b8 = NULL;

    /* use the system endianness if nothing was precised */
    if ( (~f & FLOAT_BIG) && (~f & FLOAT_LITTLE) )
        f |= (sys_endian) ? FLOAT_LITTLE : FLOAT_BIG;

    if (f & FLOAT_BIG) {
        b1 = & bin[0]; b2 = & bin[1]; b3 = & bin[2]; b4 = & bin[3];
        b5 = & bin[4]; b6 = & bin[5]; b7 = & bin[6]; b8 = & bin[7];
    } else if (f & FLOAT_LITTLE) {
        b1 = & bin[7]; b2 = & bin[6]; b3 = & bin[5]; b4 = & bin[4];
        b5 = & bin[3]; b6 = & bin[2]; b7 = & bin[1]; b8 = & bin[0];
    }

    *b1 = (unsigned char) (fp->_sign + ( ( fp->_exponent & 0x7f0 ) >> 4 ) );
    *b2 = (unsigned char) ( ( ( fp->_exponent & 0x0000000f ) << 4 ) +
          ( ( fp->_fraction & 0x000f0000 ) >> 16 ) );
    *b3 = (unsigned char) ( ( fp->_fraction & 0x0000ff00 ) >> 8 );
    *b4 = (unsigned char) ( ( fp->_fraction & 0x000000ff ) );
    *b5 = (unsigned char) ( ( fp->_extended & 0xff000000 ) >> 24 );
    *b6 = (unsigned char) ( ( fp->_extended & 0x00ff0000 ) >> 16 );
    *b7 = (unsigned char) ( ( fp->_extended & 0x0000ff00 ) >> 8 );
    *b8 = (unsigned char) ( ( fp->_extended & 0x000000ff ) );

    return 0;
}

/* -------------------------------------------------------------------------- */

ASKL_API float float_read_single(const char *buffer, int flags)
{
    _float cookie;
    float ret = 0;

    memset(& cookie, 0, sizeof(cookie));

    /* read accordingly to the flags */
    if (_single_to_fp((unsigned char *) buffer, & cookie, flags) == -1)
        return (float) NAN;

    /* write with system defaults */
    if (_fp_to_single(& cookie, (unsigned char *) & ret, 0x0) == -1)
        return (float) NAN;

    return ret;
}

/* -------------------------------------------------------------------------- */

ASKL_API double float_read_double(const char *buffer, int flags)
{
    _float cookie;
    double ret = 0;

    memset(& cookie, 0, sizeof(cookie));

    /* read accordingly to the flags */
    if (_double_to_fp((unsigned char *) buffer, & cookie, flags) == -1)
        return NAN;

    /* write with system defaults */
    if (_fp_to_double(& cookie, (unsigned char *) & ret, 0x0) == -1)
        return NAN;

    return ret;
}

/* -------------------------------------------------------------------------- */

ASKL_API int float_write_single(char *buffer, const void *f, int flags)
{
    _float cookie;

    memset(& cookie, 0, sizeof(cookie));

    /* read with system defaults */
    if (_single_to_fp((unsigned char *) f, & cookie, 0x0) == -1)
        return -1;

    /* write accordingly to the flags */
    if (_fp_to_single(& cookie, (unsigned char *) buffer, flags) == -1)
        return -1;

    return 0;
}

/* -------------------------------------------------------------------------- */

ASKL_API int float_write_double(char *buffer, const void *f, int flags)
{
    _float cookie;

    memset(& cookie, 0, sizeof(cookie));

    /* read with system defaults */
    if (_double_to_fp((unsigned char *) f, & cookie, 0x0) == -1)
        return -1;

    /* write accordingly to the flags */
    if (_fp_to_double(& cookie, (unsigned char *) buffer, flags) == -1)
        return -1;

    return 0;
}

/* -------------------------------------------------------------------------- */
