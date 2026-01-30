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

#ifndef ASKL_CODECS_H

#define ASKL_CODECS_H

#include "../askl.h"
#include "../askl_string.h"

#define RFC1738_ESCAPE_RESERVED  0x1
#define RFC1738_ESCAPE_UNESCAPED 0x2

#ifdef HAS_ZLIB
#include <zlib.h>
#endif

/* -------------------------------------------------------------------------- */

ASKL_API String *string_b58s(const char *s, size_t size);

/**
 * @ingroup string
 * @fn m_string *string_b58s(const char *s, size_t size)
 * @param s the string to be processed
 * @param size the length of the string
 * @return NULL if an error occurred, a pointer to the base58 string otherwise.
 *
 * This function returns a base58 encoded copy of the given string, using
 * Satoshi Nakamoto's alphabet.
 *
 */

/* -------------------------------------------------------------------------- */

ASKL_API String *string_b58(const String *s);

/**
 * @ingroup string
 * @fn m_string *string_b58(const m_string *s)
 * @param s the string to be processed
 * @return NULL if an error occurred, a pointer to the base58 string otherwise.
 *
 * This function is simply a wrapper around @ref string_b58s(), please see
 * the documentation of @ref string_b58s().
 *
 */

/* -------------------------------------------------------------------------- */

ASKL_API String *string_deb58s(const char *s, size_t size);

/**
 * @ingroup string
 * @fn m_string *string_deb58s(const char *s, size_t size)
 * @param s the string to be processed
 * @param size the length of the string
 * @return NULL if an error occurred, a pointer to the decoded string otherwise.
 *
 * This function decodes a base58 encoded string to plain text.
 *
 */

/* -------------------------------------------------------------------------- */

ASKL_API String *string_deb58(const String *s);

/**
 * @ingroup string
 * @fn m_string *string_deb58(const m_string *s)
 * @param s the string to be processed
 * @return NULL if an error occurred, a pointer to the decoded string otherwise.
 *
 * This function is simply a wrapper around @ref string_deb58s(), please see
 * the documentation of @ref string_deb58s().
 *
 */

/* -------------------------------------------------------------------------- */

ASKL_API String *string_b64s(const char *s, size_t size, size_t linesize);

/**
 * @ingroup string
 * @fn m_string *string_b64s(const char *s, size_t size, size_t linesize)
 * @param s the string to be processed
 * @param size the length of the string
 * @param linesize the maximal length of a base64 line
 * @return NULL if an error occurred, a pointer to the base64 string otherwise.
 *
 * This function returns a base64 encoded copy of the given string, with
 * @b linesize characters per line.
 *
 * If @b linesize is set to 0, the base64 string will be written "as-is",
 * without additional CRLF.
 *
 * If a @b linesize is provided, it should be a multiple of 4 and not greater
 * than 72, or the function will return NULL.
 *
 */

/* -------------------------------------------------------------------------- */

ASKL_API String *string_b64(const String *s, size_t linesize);

/**
 * @ingroup string
 * @fn m_string *string_b64(const m_string *s, size_t linesize)
 * @param s the string to be processed
 * @param linesize the maximal length of a base64 line
 * @return NULL if an error occurred, a pointer to the base64 string otherwise.
 *
 * This function is simply a wrapper around @ref string_b64s(), please see
 * the documentation of @ref string_b64s().
 *
 */

/* -------------------------------------------------------------------------- */

ASKL_API String *string_deb64s(const char *s, size_t size);

/**
 * @ingroup string
 * @fn m_string *string_deb64s(const char *s, size_t size)
 * @param s the string to be processed
 * @param size the length of the string
 * @return NULL if an error occurred, a pointer to the decoded string otherwise.
 *
 * This function decodes a base64 encoded string to plain text, handling
 * eventual embedded CRLFs.
 *
 */

/* -------------------------------------------------------------------------- */

ASKL_API String *string_deb64(const String *s);

/**
 * @ingroup string
 * @fn m_string *string_deb64(const m_string *s)
 * @param s the string to be processed
 * @return NULL if an error occurred, a pointer to the decoded string otherwise.
 *
 * This function is simply a wrapper around @ref string_deb64s(), please see
 * the documentation of @ref string_deb64s().
 *
 */

/* -------------------------------------------------------------------------- */

ASKL_API char *string_rawurlencode(const char *url, size_t len, int flags);

/* -------------------------------------------------------------------------- */

ASKL_API int string_urlencode(String *url, int flags);

/* -------------------------------------------------------------------------- */
#ifdef HAS_ZLIB
/* -------------------------------------------------------------------------- */

ASKL_API String *string_deflate(String *s);

/**
 * @ingroup string
 * @fn m_string *string_compress(m_string *s)
 * @param s the string to be compressed
 * @return NULL if an error occurred, a pointer to compressed string otherwise.
 *
 * This function returns a compressed copy of a given string.
 *
 */

/* -------------------------------------------------------------------------- */

ASKL_API String *string_inflate(String *s, size_t original_size);

/**
 * @ingroup string
 * @fn m_string *string_uncompress(m_string *s, size_t original_size)
 * @param s the string to be uncompressed
 * @param original_size the size of the original uncompressed data
 * @return NULL if an error occurred, pointer to uncompressed string otherwise.
 *
 * This function returns an uncompressed copy of a given compressed string.
 *
 */

/* -------------------------------------------------------------------------- */
#endif
/* -------------------------------------------------------------------------- */

#endif
