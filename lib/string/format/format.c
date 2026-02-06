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

#include "format.h"

/* -------------------------------------------------------------------------- */

ASKL_API String *string_vfmt(String *str, const char *fmt, va_list args)
{
    int ret = 0;
    va_list copy;

    /* sanity check */
    if (! fmt) {
        debug("string_vfmt(): bad parameters.\n");
        return NULL;
    }

    if (str && str->internal.flags & _STRING_READ_ONLY) {
        debug("string_vfmt(): illegal write attempt.\n");
        return NULL;
    }

    /* XXX va_lists are mangled in GNU C and cannot be reused */
    va_copy(copy, args);

    if ( (ret = m_vsnprintf(NULL, 0, fmt, args)) <= 0) {
        debug("string_vfmt(): wrong format or vsnprintf() error.\n");
        va_end(copy);
        return NULL;
    }

    if (! str) {
        if (! (str = string_alloc(NULL, ret)) ) {
            debug("string_vfmt(): allocation failure.\n");
            va_end(copy);
            return NULL;
        }
    } else if (string_extend(str, ret) == -1) {
        debug("string_vfmt(): resize failure.\n");
        va_end(copy);
        return NULL;
    }

    /* the length returned by vsnprintf does not include the trailing \0 */
    str->len = m_vsnprintf(str->data, ret + 1, fmt, copy);

    va_end(copy);

    return str;
}

/* -------------------------------------------------------------------------- */

ASKL_API String *string_fmt(String *str, const char *fmt, ...)
{
    /** @brief overwrite a string with the given format or allocate it */

    va_list args;

    va_start(args, fmt);
    str = string_vfmt(str, fmt, args);
    va_end(args);

    return str;
}

/* -------------------------------------------------------------------------- */

ASKL_API String *string_catfmt(String *str, const char *fmt, ...)
{
    int ret = 0;
    size_t off = 0;

    va_list args, copy;

    va_start(args, fmt);

    /* sanity check */
    if (! fmt) {
        debug("string_catfmt(): bad parameters.\n");
        va_end(args);
        return NULL;
    }

    if (str && str->internal.flags & _STRING_READ_ONLY) {
        debug("string_catfmt(): illegal write attempt.\n");
        va_end(args);
        return NULL;
    }

    /* XXX va_lists are mangled in GNU C and cannot be reused */
    va_copy(copy, args);

    if ( (ret = m_vsnprintf(NULL, 0, fmt, args)) <= 0) {
        debug("string_catfmt(): wrong format or vsnprintf() error.\n");
        va_end(args); va_end(copy);
        return NULL;
    }

    /* discard the mangled list */
    va_end(args);

    if (! str) {
        if (! (str = string_alloc(NULL, ret)) ) {
            debug("string_catfmt(): allocation failure.\n");
            va_end(copy);
            return NULL;
        }
    } else {
        if (string_extend(str, (off = str->len) + ret) == -1) {
            debug("string_catfmt(): resize failure.\n");
            va_end(copy);
            return NULL;
        } else str->len += ret;
    }

    /* the length returned by vsnprintf does not include the trailing \0 */
    m_vsnprintf(str->data + off, ret + 1, fmt, copy);

    va_end(copy);

    return str;
}

/* -------------------------------------------------------------------------- */

ASKL_API int string_peek_fmt(String *string, const char *fmt, ...)
{
    /** @brief copy data from the string according to the given format string */

    int ret = 0;
    va_list args;

    if (! string || ! string->data || ! fmt) return -1;

    va_start(args, fmt);

    if (! (ret = m_vsnscanf(string->data, string->len, fmt, args)) ) {
        debug("string_peek_fmt(): wrong format or vsnscanf() error.\n");
        return -1;
    }

    va_end(args);

    return 0;
}

/* -------------------------------------------------------------------------- */

ASKL_API int string_fetch_fmt(String *string, const char *fmt, ...)
{
    /** @brief move data from the string according to the given format string */

    int ret = 0;
    va_list args;

    if (! string || ! string->data || ! fmt) return -1;

    va_start(args, fmt);

    if (! (ret = m_vsnscanf(string->data, string->len, fmt, args)) ) {
        debug("string_fetch_fmt(): wrong format or vsnscanf() error.\n");
        return -1;
    }

    string_cut(string, 0, ret, NULL);

    va_end(args);

    return 0;
}

/* -------------------------------------------------------------------------- */
