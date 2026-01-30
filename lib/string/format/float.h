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

#ifndef M_FLOAT_H

#define M_FLOAT_H

#include "../../askl.h"

/** @defgroup float util::float */

/** Read or write a big endian floating point value */
#define FLOAT_BIG    0x08
/** Read or write a little endian floating point value */
#define FLOAT_LITTLE 0x10

/* -------------------------------------------------------------------------- */

ASKL_API float float_read_single(const char *buffer, int flags);

/**
 * @ingroup float
 * @fn float float_read_single(const char *buffer, int flags)
 * @param buffer the buffer to read the float from.
 * @param flags apply a specific treatment to the data read.
 * @return NAN if an error occurs, the float read otherwise.
 *
 * This function reads a binary single precision float from a buffer, using
 * the IEEE754 standard. The flags FLOAT_BIG or FLOAT_LITTLE indicate the
 * endianness of the binary data to the function.
 *
 * The function returns a float in the system endianness.
 *
 */

/* -------------------------------------------------------------------------- */

ASKL_API double float_read_double(const char *buffer, int flags);

/**
 * @ingroup float
 * @fn double float_read_double(const char *buffer, int flags)
 * @param buffer the buffer to read the float from.
 * @param flags apply a specific treatment to the data read.
 * @return NAN if an error occurs, the double read otherwise.
 *
 * This function reads a binary double precision float from a buffer, using
 * the IEEE754 standard. The flags FLOAT_BIG or FLOAT_LITTLE indicate the
 * endianness of the binary data to the function.
 *
 * The function returns a double in the system endianness.
 *
 */

/* -------------------------------------------------------------------------- */

ASKL_API int float_write_single(char *buffer, const void *f, int flags);

/**
 * @ingroup float
 * @fn int float_write_single(const char *buffer, float f, int flags)
 * @param buffer the buffer write the float to.
 * @param f the float to write.
 * @param flags apply a specific treatment to the data written.
 * @return -1 if an error occurs, 0 otherwise.
 *
 * This function writes a binary single precision float to the given buffer,
 * using the IEEE754 standard. The flags FLOAT_BIG or FLOAT_LITTLE indicate
 * the endianness with which the data should be written.
 *
 */

/* -------------------------------------------------------------------------- */

ASKL_API int float_write_double(char *buffer, const void *f, int flags);

/**
 * @ingroup float
 * @fn int float_write_double(const char *buffer, double f, int flags)
 * @param buffer the buffer write the double to.
 * @param f the double to write.
 * @param flags apply a specific treatment to the data written.
 * @return -1 if an error occurs, 0 otherwise.
 *
 * This function writes a binary double precision float to the given buffer,
 * using the IEEE754 standard. The flags FLOAT_BIG or FLOAT_LITTLE indicate
 * the endianness with which the data should be written.
 *
 */

/* -------------------------------------------------------------------------- */

#endif
