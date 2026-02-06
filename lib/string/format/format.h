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

#ifndef ASKL_FORMAT_H

#define ASKL_FORMAT_H

#include "../../askl.h"
#include "../../askl_string.h"
#include "scanf.h"
#include "printf.h"

/* -------------------------------------------------------------------------- */

ASKL_API String *string_vfmt(String *str, const char *fmt, va_list args);

/**
 * @ingroup string
 * @fn String *string_vfmt(String *str, const char *fmt, ...)
 * @param str the string to be overwritten, may be NULL.
 * @param fmt printf(3) compatible format.
 * @param args arguments matching the format.
 * @return NULL if an error occurred, a pointer to the @b str string otherwise.
 *
 * This function overwrites the given string (or a newly allocated one if the
 * @b str parameter was NULL) with a formatted output.
 *
 * @ref string_fmt() uses its own printf() implementation, which provides
 * various extensions to support writing binary data. Please see the
 * @ref m_vsnprintf() documentation for more details.
 *
 * Since this function allocates a string and returns it if the @b str
 * parameter is NULL, it is recommended to always check its return value to
 * avoid memory leaks.
 *
 * If an error occurs, the original string will be left unchanged and the
 * function will return NULL.
 *
 */

/* -------------------------------------------------------------------------- */

ASKL_API String *string_fmt(String *str, const char *fmt, ...);

/**
 * @ingroup string
 * @fn String *string_fmt(String *str, const char *fmt, ...)
 * @param str the string to be overwritten, may be NULL.
 * @param fmt the printf(3) compatible format.
 * @param ... the arguments matching the format.
 * @return NULL if an error occurred, a pointer to the @b str string otherwise.
 *
 * This function is a wrapper around @ref string_vfmt().
 *
 * @see string_vfmt
 *
 */

/* -------------------------------------------------------------------------- */

ASKL_API String *string_catfmt(String *str, const char *fmt, ...);

/* -------------------------------------------------------------------------- */

ASKL_API int string_peek_fmt(String *string, const char *fmt, ...);

/* -------------------------------------------------------------------------- */

ASKL_API int string_fetch_fmt(String *string, const char *fmt, ...);

/* -------------------------------------------------------------------------- */

#endif
