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

#ifndef ASKL_SCANF_H

#define ASKL_SCANF_H

#include "scanf.h"
#include "float.h"

/** @defgroup scanf util::scanf */

/* -------------------------------------------------------------------------- */

ASKL_API int m_vsnscanf(
    const char *buffer,
    size_t bufsize,
    const char *fmt0,
    va_list ap
);

/**
 * @ingroup scanf
 * @fn int m_vsnscanf(const char *buffer, size_t bufsize, const char *fmt0, va_list ap)
 * @param buffer the buffer to read data from
 * @param bufsize size of the buffer
 * @param fmt0 format
 * @param ap variable argument list
 *
 * This modified scanf function is based upon the original
 * scanf of the OpenBSD libc.
 *
 * The first thing which was modified is the return value of
 * scanf. This function returns the number of bytes which were
 * read, not the number of fields filled.
 *
 * The scanf behaviour has slightly changed too; now, if an
 * input or match failure occurs, it returns -1. Some fields
 * may have however be altered before failure.
 *
 * If an input failure occurs, the function returns -1 and
 * EAGAIN. If a match failure happens, it will return -1 and
 * set errno to ENOMEM.
 *
 * But most of the changes are introduced by a couple flags,
 * b and t.
 *
 * These new flags allow to read binary values from a buffer.
 *
 * To read binary data, it is required to add the 'b' flag to the
 * format string. Using the 'b' flag alone implies using the
 * system endianness.
 *
 * However, the binary data endianness may be enforced using
 * the bB (big endian) or bb (little endian) combinations.
 * If the enforced endianness matches the system endianness,
 * nothing special will be done, but otherwise the data will
 * be swapped.
 *
 * e.g: you want to read a big endian integer, on a
 * little endian machine:
 *
 * snscanf(buf, sizeof(buf), "%bBi", & i);
 * original raw data in 'buf' (big endian seen by little endian):
 * 0xEFBEADDE
 * data stored in 'i' (little endian):
 * 0xDEADBEEF
 *
 * It is also possible to force the binary data size, using the
 * classical field width syntax, or the Concrete Server variable
 * length extension '$'.[1]
 *
 * This said, forcing the type width only works 'as expected'
 * with *complete* binary types of the forced length. It will
 * indeed return the n lower or higher bytes of a wider type
 * depending on the system endianness (swapped if another endianness
 * was enforced).
 * So, if you want to truncate a wider data type, you should use the
 * t/T (or tL/tH) flags to reliably get its lower/higher bytes.
 *
 * NB: The binary data type is still mandatory (dioux).
 *
 * So, it is correct to use the formats :
 * %3bBti to read the 24 lower bits of a big endian integer
 * %3bBi reads a 24 bits big endian integer just fine
 * %4bbTlli returns the higher 32 bits of a little endian long long
 * %bi reads an integer of system width and endianness
 * but the next are not valid or dangerous formats :
 * %3bBt invalid (missing type)
 * %2bi (it will return the lower/higher two bytes of data depending
 *      on the system endianness, and the data are assumed to use
 *      the system endianness -- it may be perfectly fine, if that
 *      was the intented purpose)
 *
 *
 * [1] About the variable field width extension
 *
 * It works just like '*' in the printf family, except it uses the
 * '$' flag ('*' has another meaning with scanf, see the manual for
 * further details), and that it works for strings, too.
 *
 * So, '$' define the field width, but unlike the printf '*' it
 * does not imply padding. In this implementation, it is designed
 * to limit the number of bytes which can be stored in the output
 * variable (useful to avoid a buffer overflow with %s).
 *
 * E.g:
 *
 * snscanf(buffer, sizeof(buffer), "%$s", sizeof(outbuf), outbuf);
 * will read a string from 'buffer' until a space is encountered, or
 * sizeof(outbuf) - 1 bytes were written in 'outbuf', whichever occurs
 * first. If the field width is reached, a trailing NUL is appended
 * and the function returns -1, ENOMEM.
 *
 */

/* -------------------------------------------------------------------------- */

ASKL_API int m_snscanf(
    const char *buffer,
    size_t size,
    const char *fmt,
    ...
);

/**
 * @ingroup scanf
 * @fn int m_vsnscanf(const char *buffer, size_t bufsize, const char *fmt0, va_list ap)
 * @param buffer the buffer to read data from
 * @param size size of the buffer
 * @param fmt format
 * @param ... variable argument list
 *
 * @see m_vsnscanf
 *
 */

/* -------------------------------------------------------------------------- */

#endif
