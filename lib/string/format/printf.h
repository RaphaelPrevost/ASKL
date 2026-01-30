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

#ifndef ASKL_PRINTF_H

#define ASKL_PRINTF_H

#include "../../askl.h"
#include "../../askl_string.h"
#include "float.h"

#define FLOATING_POINT

/** @defgroup printf util::printf */

/* -------------------------------------------------------------------------- */

ASKL_API int m_vsnprintf(
    char *buffer,
    size_t bufsize,
    const char *fmt0,
    va_list ap
);

/**
 * @ingroup printf
 * @fn int m_vsnprintf(char *buffer, size_t bufsize, const char *fmt0, va_list ap)
 * @param buffer the buffer to output data into
 * @param size size of the buffer
 * @param fmt0 format
 * @param ap variable argument list
 * @return the size in bytes of the resulting formatted string
 *
 * This modified printf function is based upon the original
 * printf of the OpenBSD libc.
 *
 * The printf behaviour has been slightly modified to suits
 * the needs of this software. Now, passing a NULL output
 * buffer is allowed; in this case, @ref m_vsnprintf()
 * will simply return the number of bytes which would have
 * been written. Some extensions to write binary data have
 * also been added to the original formatting convention.
 *
 * Additionnally, the %url flag allows to print an urlencoded
 * string.
 *
 * The number of bytes written does not include the terminal NUL.
 *
 * To write binary data, it is required to add the 'b' flag to the
 * format string. Using the 'b' flag alone implies using the
 * system endianness.
 *
 * However, the binary data endianness may be enforced using
 * the bB (big endian) or bb (little endian) combinations.
 * If the enforced endianness matches the system endianness,
 * nothing special will be done, but otherwise the data will
 * be swapped.
 *
 * e.g: you want to write a big endian integer, on a
 * little endian machine:
 *
 * snprintf(buf, sizeof(buf), "%bBi", i);
 * raw data written in 'buf' (big endian seen by little endian):
 * 0xEFBEADDE
 * data originally stored in 'i' (little endian):
 * 0xDEADBEEF
 *
 * It is also possible to force the binary data size, using the
 * classical field width syntax '*'.
 *
 * This said, forcing the type width only works 'as expected'
 * with *complete* binary types of the forced length. It will
 * indeed write the n lower or higher bytes of a wider type
 * depending on the system endianness (swapped if another endianness
 * was enforced).
 * So, if you want to truncate a wider data type, you should use the
 * t/T (or tL/tH) flags to reliably write its lower/higher bytes.
 *
 * NB: The binary data type is still mandatory (dioux).
 *
 * So, it is correct to use the formats :
 * %3bBti to write the 24 lower bits of a big endian integer
 * %3bBi writes a 24 bits big endian integer just fine
 * %4bbTlli writes the higher 32 bits of a little endian long long
 * %bi writes an integer of system width and endianness
 * but the next are not valid or dangerous formats :
 * %3bBt invalid (missing type)
 * %2bi (it will write the lower/higher two bytes of data depending
 *      on the system endianness, and the data are assumed to use
 *      the system endianness -- it may be perfectly fine, if that
 *      was the intented purpose)
 *
 */

/* -------------------------------------------------------------------------- */

#ifdef _ENABLE_DB
public int m_vsnprintf_db(void *con, char *buffer, size_t bufsize,
                          const char *fmt0, va_list ap);
/**
 * @ingroup printf
 * @fn int m_vsnprintf_db(void *con, char *buffer, size_t bufsize,
 *                        const char *fmt0, va_list ap)
 * @param buffer the buffer to output data into
 * @param size size of the buffer
 * @param fmt0 format
 * @param ap variable argument list
 * @return the size in bytes of the resulting formatted string
 *
 * This modified printf() automatically quote and escape string literals
 * in order to generate secure SQL queries.
 *
 */
#endif

/* -------------------------------------------------------------------------- */

ASKL_API int m_snprintf(char *buffer, size_t size, const char *fmt, ...);

/**
 * @ingroup printf
 * @fn int m_snprintf(char *buffer, size_t size, const char *fmt, ...)
 * @param buffer the buffer to output data into
 * @param size size of the buffer
 * @param fmt format
 * @param ... variable argument list
 *
 * @see m_vsnprintf
 *
 */
 
/* -------------------------------------------------------------------------- */

ASKL_API int m_snprintf_db(void *con, char *buffer, size_t size,
                         const char *fmt, ...);

/**
 * @ingroup printf
 * @fn int m_snprintf(char *buffer, size_t size, const char *fmt, ...)
 * @param buffer the buffer to output data into
 * @param size size of the buffer
 * @param fmt format
 * @param ... variable argument list
 *
 * @see m_vsnprintf_db
 *
 */

/* -------------------------------------------------------------------------- */

#endif
