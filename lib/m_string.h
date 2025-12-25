/*******************************************************************************
 *  ASKL.                                                                      *
 *  Copyright (c) 2025 Raphael Prevost <raph@el.bzh>                           *
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

#ifndef M_STRING_H

#define M_STRING_H

#include "askl.h"
#include "util/m_util_vfprintf.h"
#include "util/m_util_vfscanf.h"

#ifdef HAS_PCRE
#include <pcre.h>
#endif

#ifdef HAS_ZLIB
#include <zlib.h>
#endif

#ifdef HAS_ICONV
#include <iconv.h>
#endif

/** @defgroup buffer ASKL::buffer */
#if 0
typedef struct ASKL_Buffer {
    char *data;
    struct ASKL_Buffer *tokens;
    struct ASKL_Buffer *parent;
    uint32_t len;
    uint32_t count;
    union {
        uint64_t metadata;
        #ifdef __ASKL_INTERNALS__
        struct {
            uint32_t capacity;
            uint16_t tokens_capacity;
            uint16_t flags;
        } buffer;
        #endif
    } internal;
} ASKL_Buffer;
#endif

typedef struct m_search_string {
    /* private */
    size_t _len;
    size_t _shift;
    uint8_t _lut[UCHAR_MAX + 1];
} m_search_string;
#if 0
/* private string flags */
#define _BUFFER_COMMON_FLAGS 0x000f
#define _BUFFER_CUSTOM_FLAGS 0xf000
#define _BUFFER_FIXED_LENGTH 0x0001 /* disable string resizing */
#define _BUFFER_READ_ONLY    0x0002 /* disable string writing */
#define _BUFFER_IMMUTABLE    0x0003 /* disable writing and resizing */
#define _BUFFER_DISABLE_FREE 0x0004 /* disable free() on string content */
#define _BUFFER_ENCAPSULATED 0x0005 /* disable all dynamic allocation */
#define _BUFFER_STATIC_ALLOC 0x0008 /* static, stack allocated string */
#define _BUFFER_EXTENSION    0x000f /* mask extension flags */
#define _BUFFER_VALIDATED    0x0010 /* this string has been validated */
#define _BUFFER_HAS_ERROR    0x0020 /* this string contains errors */
#define IS_ERROR(x) ((x)->_flags & _BUFFER_HAS_ERROR)
#define _BUFFER_PARTIAL_DATA 0x0040 /* this string is used for buffering */
#define IS_BUFFERING(x) ((x)->_flags & _STRING_PARTIAL_DATA)
/*                           0x0080    reserved */
#ifdef _ENABLE_HTTP
#define _BUFFER_HTTP_REQUEST 0x0100 /* HTTP request */
#define IS_HTTP(x) ((x)->_flags & _BUFFER_HTTP_REQUEST)
#endif
#define _BUFFER_STREAMING 0x0200 /* used by the server for streaming */
#define IS_STREAMING(x) ((x)->_flags & _BUFFER_STREAMING)
#define _BUFFER_LARGE 0x0400 /* large request */
#define IS_LARGE(x) ((x)->_flags & _BUFFER_LARGE)
#ifdef _ENABLE_HTTP
#define _BUFFER_HTTP_CHUNKED 0x0800 /* HTTP 1.1 Chunked encoding */
#define IS_CHUNK(x) ((x)->_flags & _BUFFER_HTTP_CHUNKED)
#endif
#endif

/* legacy */
typedef struct m_string {
    /* private */
    size_t _len;
    uint16_t parts;
    uint16_t _flags;
    char *_data;
    struct m_string *token;
    struct m_string *parent;
    size_t _alloc;
    uint16_t _parts_alloc;
} m_string;
#define _STRING_FIXED_LENGTH 0x0001 /* disable string resizing */
#define _STRING_READ_ONLY    0x0002 /* disable string writing */
#define _STRING_IMMUTABLE    0x0003 /* disable writing and resizing */
#define _STRING_DISABLE_FREE 0x0004 /* disable free() on string content */
#define _STRING_ENCAPSULATED 0x0005 /* disable all dynamic allocation */
#define _STRING_STATIC_ALLOC 0x0008 /* static, stack allocated string */
#define _STRING_EXTENSION    0x000f /* mask extension flags */
#define _STRING_VALIDATED    0x0010 /* this string has been validated */
#define _STRING_HAS_ERROR    0x0020 /* this string contains errors */
#define IS_ERROR(x) ((x)->_flags & _STRING_HAS_ERROR)
#define _STRING_BUFFERING    0x0040 /* this string is used for buffering */
#define IS_BUFFER(x) ((x)->_flags & _STRING_BUFFERING)
/*                           0x0080    reserved */
#ifdef _ENABLE_HTTP
#define _STRING_HTTP_REQUEST 0x0100 /* HTTP request */
#define IS_HTTP(x) ((x)->_flags & _STRING_HTTP_REQUEST)
#endif
#define _STRING_PARTIAL_DATA 0x0200 /* used by the server for streaming */
#define IS_FRAGMENT(x) ((x)->_flags & _STRING_PARTIAL_DATA)
#define _STRING_LARGE_BUFFER 0x0400 /* large request */
#define IS_LARGE(x) ((x)->_flags & _STRING_LARGE_BUFFER)
#ifdef _ENABLE_HTTP
#define _STRING_HTTP_CHUNKED 0x0800 /* HTTP 1.1 Chunked encoding */
#define IS_CHUNK(x) ((x)->_flags & _STRING_HTTP_CHUNKED)
#endif
/* /legacy */

#ifdef _ENABLE_JSON
typedef struct m_json_parser {
    void *context;
    struct {
        const char *current;
        size_t len;
    } key;
    union {
        struct {
            uint8_t type;
            uint8_t neg;
            uint8_t rad;
            uint8_t exp;
        } current;
        uint32_t data;
    } primitive;
    int strict;
    int parent;
    int (CALLBACK *init)(int, struct m_json_parser *);
    int (CALLBACK *data)(m_string *, struct m_json_parser *);
    int (CALLBACK *exit)(int, struct m_json_parser *);
} m_json_parser;

#define JSON_OBJECT         0x1000
#define JSON_ARRAY          0x2000
#define JSON_STRING         0x4000
#define JSON_PRIMITIVE      0x8000
#define JSON_TYPE           0xF000

#define JSON_PRIMITIVE_NUMBER  0x1
#define JSON_PRIMITIVE_NULL    0x2
#define JSON_PRIMITIVE_BOOL    0x4
#define JSON_PRIMITIVE_TRUE   0x10
#define _JSON_PRIMITIVE_HEX   0x20

#define IS_OBJECT(x) ((x)->_flags & JSON_OBJECT)
#define IS_ARRAY(x) ((x)->_flags & JSON_ARRAY)
#define IS_STRING(x) ((x)->_flags & JSON_STRING)
#define IS_PRIMITIVE(x) ((x)->_flags & JSON_PRIMITIVE)
#define IS_TYPE(x, type) ((x)->_flags & (type))

#define JSON_QUIRKS         0
#define JSON_STRICT         1

#endif

#define STRING_STATIC_INITIALIZER(s, l) \
{ (l), 0, \
  _STRING_IMMUTABLE | _STRING_ENCAPSULATED | _STRING_STATIC_ALLOC, \
  (char *) (s), NULL, NULL, 0, 0 \
}

/**
 * @ingroup string
 * @struct m_string
 *
 * This structure is designed to hold arbitrary sequences of bytes. Its fields
 * should never be directly accessed, at the exception of the @ref parent,
 * @ref parts and @ref token fields, which are considered public members.
 *
 * @ref parts holds the number of token in which the string has been sliced,
 * with @ref string_split or any other function.
 *
 * @ref token is an array of m_string structures, which are set to point
 * to particular subsections of the current string. Tokens can be resized or
 * altered with the various functions defined in this API, without restrictions.
 * However, some functions performing destructive transformations on their
 * input may destroy all tokens associated to a string; in this case, this
 * behaviour will be specified in the function documentation.
 *
 * @ref parent is a pointer to the parent string of a token (which may be
 * a token itself). You can test if a string is a token or not by checking
 * the parent field value; if it is NULL, then the string is not a token.
 *
 * Even if these members are public, the macros @ref TOKEN and @ref PARTS have
 * been defined to use them more conveniently.
 *
 * You can clean up the tokens of a string at any time by calling
 * @ref string_free_token().
 *
 * All other fields are private, using or altering their values is likely
 * to result in unexpected behaviours.
 *
 * @b private @ref _flags is a bitfield holding several internal flags, like
 * write protection, or resizing protection.
 *
 * @b private @ref _len is the actual size, in bytes, of the data stored inside
 * the m_string structure.
 *
 * @b private @ref _alloc is the actual size of the inner data buffer of the
 * m_string structure. It should only be used in the resizing functions.
 *
 * @b private @ref _data is the internal buffer where data are stored.
 *
 */

/** Macro for accessing a token inside a m_string. No bound check, be careful */
#define TOKEN(s, i) (& (s)->token[(i)])
#define SUBTOKEN(s, i, j) (& (s)->token[(i)].token[(j)])
/** Macro for accessing the number of token held inside a m_string */
#define PARTS(s)    ((s)->parts)

#define EMPTY(s)    (! (s)->_len || ((s)->_len == 1 && ! *(s)->_data))

#define FIRST_TOKEN(s)   (& (s)->token[0])
#define LAST_TOKEN(s)    (& (s)->token[(s)->parts - 1])

/** Macro to READ-ONLY access the inner data buffer of a m_string */
#define DATA(s) ((const char *) (s)->_data)
/** Macro to get the size of a string. Consider using @ref string_len instead */
#define SIZE(s) ((s)->_len)

/** Macro to READ-ONLY access the last byte of a string */
#define STRING_END(s) ((const char *) (s)->_data + (s)->_len)
/** Macro to get the available length in a string */
#define STRING_AVL(s) ((s)->_alloc - (s)->_len)

/** Macro to read the last character of a NUL-terminated string */
#define LAST_CHAR(s) ((s)->_data[(s)->_len - 1])

/** Macro to READ-ONLY access the inner data buffer of a token. No bound check */
#define TOKEN_DATA(s, i) ((const char *) (s)->token[(i)]._data)
/** Macro to get the size of a token. Consider using @ref string_len instead */
#define TOKEN_SIZE(s, i) ((s)->token[(i)]._len)
/** Macro to READ-ONLY access the last byte of a token */
#define TOKEN_END(s, i)  ((const char *) (s)->token[(i)]._data + \
                                         (s)->token[(i)]._len)

/** Similar macros to get the size of a subtoken. */
#define SUBTOKEN_SIZE(s, i, j) ((s)->token[(i)].token[(j)]._len)

#ifdef _ENABLE_HTTP
/** urlencode options */
#define RFC1738_ESCAPE_RESERVED  0x1
#define RFC1738_ESCAPE_UNESCAPED 0x2
#endif

/* -------------------------------------------------------------------------- */

public int string_api_setup(void);

/* -------------------------------------------------------------------------- */

public m_string *string_prealloc(const char *string, size_t len, size_t total);

/**
 * @ingroup string
 * @fn m_string *string_prealloc(const char *string, size_t len, size_t total)
 * @param string the data with which the new string will be initialized.
 * @param len the size of the initialization string.
 * @param total the total amount of space in bytes that should be allocated.
 * @return a pointer to a new m_string, or NULL.
 *
 * This function allocates a new m_string structure, and its internal buffer
 * if the @b total parameter is valid (non zero).
 *
 * If a pointer to a @b string was given, up to @b len bytes will be copied
 * from the given string to the freshly allocated buffer.
 *
 * The @b total parameter should be greater than or equal to @b len.
 *
 * This function is useful to reserve space upfront when it is known the
 * string will grow later, avoiding going through the system allocator
 * repeatedly.
 *
 * A m_string structure should be destroyed with @ref string_free() after use.
 *
 */

/* -------------------------------------------------------------------------- */

public m_string *string_alloc(const char *string, size_t len);

/**
 * @ingroup string
 * @fn m_string *string_alloc(const char *string, size_t len)
 * @param string the data with which the new string will be initialized.
 * @param len the size of the buffer to be allocated.
 * @return a pointer to a new m_string, or NULL.
 *
 * This function allocates a new m_string structure, and its internal buffer if
 * the @b len parameter is valid (non zero).
 *
 * If a pointer to a @b string was given, up to @b len bytes will be copied
 * from the given string to the freshly allocated buffer.
 *
 * So, it is perfectly legal to do:
 * my_string = string_alloc(NULL, 256); to allocate a 256 bytes wide buffer
 * or:
 * my_string = string_alloc(NULL, 0) to allocate an empty m_string structure.
 *
 * However, if a pointer to some data is provided, the @b len parameter must
 * be consistent, or the function will fail.
 *
 * A m_string structure should be destroyed with @ref string_free() after use.
 *
 */

/* -------------------------------------------------------------------------- */

public m_string *string_encaps(const char *string, size_t len);

/**
 * @ingroup string
 * @fn m_string *string_encaps(const char *string, size_t len)
 * @param string the buffer to encapsulate in a m_string structure
 * @param len the length of the buffer
 * @return NULL if an error occurred, a pointer to a new m_string otherwise
 *
 * This functions wraps an existing static or dynamically allocated buffer
 * in a new read-only, fixed length m_string structure.
 *
 * This "static" m_string structure can be used with all string functions
 * accepting read-only m_strings, and should be deleted after use with
 * the @ref string_free() function. This will not free the initial buffer,
 * though. Handling of the initial buffer is left to the programmer.
 *
 */

/* -------------------------------------------------------------------------- */

public m_string *string_vfmt(m_string *str, const char *fmt, va_list args);

/**
 * @ingroup string
 * @fn m_string *string_vfmt(m_string *str, const char *fmt, ...)
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

public m_string *string_fmt(m_string *str, const char *fmt, ...);

/**
 * @ingroup string
 * @fn m_string *string_fmt(m_string *str, const char *fmt, ...)
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

public m_string *string_catfmt(m_string *str, const char *fmt, ...);

/* -------------------------------------------------------------------------- */

public m_string *string_from_uint32(uint32_t u32);

/**
 * @ingroup string
 * @fn m_string *string_from_uint32(uint32_t u32)
 * @param u32 the integer to be converted into a binary string
 * @return NULL if the conversion failed, a pointer to a new string otherwise
 *
 * This function creates a new m_string from a 32 bits integer. The resulting
 * binary string is not swapped.
 *
 * You may consider using string_fmt() binary extensions for finer control
 * on binary output.
 *
 * @see string_fmt
 *
 */

 /* -------------------------------------------------------------------------- */

public int string_to_uint32s(const char *string, uint32_t *out);

/**
 * @ingroup string
 * @fn int string_to_uint32s(const char *string, uint32_t *out)
 * @param string the binary string to convert to a 32 bits integer
 * @param out a pointer to a 32 bits integer for storage
 * @return -1 if an error occurred, 0 otherwise
 *
 * This function simply converts a binary string into a 32 bits integer,
 * without swapping.
 *
 * If an error occurs, the function will return -1.
 *
 */
 
/* -------------------------------------------------------------------------- */

public int string_to_uint32(const m_string *string, uint32_t *out);

/**
 * @ingroup string
 * @fn int string_to_uint32(const m_string *string, uint32_t *out)
 * @param string the binary string to convert to a 32 bits integer
 * @param out a pointer to a 32 bits integer for storage
 * @return -1 if an error occurred, 0 otherwise
 *
 * This function simply converts a binary string into a 32 bits integer,
 * without swapping.
 *
 * If an error occurs, the function will return -1.
 *
 */

/* -------------------------------------------------------------------------- */

public m_string *string_from_uint64(uint64_t u64);

/**
 * @ingroup string
 * @fn m_string *string_from_uint64(uint64_t u64)
 * @param u64 the integer to be converted into a binary string
 * @return NULL if the conversion failed, a pointer to a new string otherwise
 *
 * This function creates a new m_string from a 64 bits integer. The resulting
 * binary string is not swapped.
 *
 * You should consider using string_fmt() binary extensions for more
 * control on the binary output.
 *
 * @see string_fmt
 *
 */

/* -------------------------------------------------------------------------- */

public int string_to_uint64s(const char *string, uint64_t *out);

/**
 * @ingroup string
 * @fn int string_to_uint64s(const char *string, uint64_t *out)
 * @param string the binary string to convert to a 64 bits integer
 * @param out a pointer to a 64 bits integer for storage
 * @return -1 if an error occurred, 0 otherwise
 *
 * This function simply converts a binary string into a 64 bits integer,
 * without swapping.
 *
 * If an error occurs, the function will return -1.
 *
 */

/* -------------------------------------------------------------------------- */

public int string_to_uint64(const m_string *string, uint64_t *out);

/**
 * @ingroup string
 * @fn int string_to_uint64(const m_string *string, uint64_t *out)
 * @param string the binary string to convert to a 64 bits integer
 * @param out a pointer to a 64 bits integer for storage
 * @return -1 if an error occurred, 0 otherwise
 *
 * This function simply converts a binary string into a 64 bits integer,
 * without swapping.
 *
 * If an error occurs, the function will return -1.
 *
 */

/* -------------------------------------------------------------------------- */

public uint8_t string_peek_uint8(const m_string *string);

/* -------------------------------------------------------------------------- */

public uint8_t string_fetch_uint8(m_string *string);

/* -------------------------------------------------------------------------- */

public uint16_t string_peek_uint16(const m_string *string);

/* -------------------------------------------------------------------------- */

public uint16_t string_fetch_uint16(m_string *string);

/* -------------------------------------------------------------------------- */

public uint32_t string_peek_uint32(const m_string *string);

/* -------------------------------------------------------------------------- */

public uint32_t string_fetch_uint32(m_string *string);

/* -------------------------------------------------------------------------- */

public uint64_t string_peek_uint64(const m_string *string);

/* -------------------------------------------------------------------------- */

public uint64_t string_fetch_uint64(m_string *string);

/* -------------------------------------------------------------------------- */

public int string_peek_fmt(m_string *string, const char *fmt, ...);

/* -------------------------------------------------------------------------- */

public int string_fetch_fmt(m_string *string, const char *fmt, ...);

/* -------------------------------------------------------------------------- */

public int string_peek_buffer(m_string *string, char *out, size_t len);

/* -------------------------------------------------------------------------- */

public int string_fetch_buffer(m_string *string, char *out, size_t len);

/* -------------------------------------------------------------------------- */

public m_string *string_peek(m_string *string, size_t len);

/* -------------------------------------------------------------------------- */

public m_string *string_fetch(m_string *string, size_t len);

/* -------------------------------------------------------------------------- */

public void string_flush(m_string *string);

/* -------------------------------------------------------------------------- */

public int string_wchar(m_string *string);

/**
 * @ingroup string
 * @fn int string_wchar(m_string *string)
 * @param string the string to be converted.
 * @return -1 if an error occurred, 0 otherwise.
 *
 * This function converts the data stored in the internal buffer of the given
 * @b string to wide characters from multibyte characters.
 *
 * If the conversion is successful, the inner buffer is replaced by its
 * multibyte equivalent.
 *
 * If the function fails to convert the data, it will returns -1 and leave the
 * buffer unchanged.
 *
 */

/* -------------------------------------------------------------------------- */

public int string_mbyte(m_string *string);

/**
 * @ingroup string
 * @fn int string_mbyte(m_string *string)
 * @param string the string to be converted.
 * @return -1 if an error occurred, 0 otherwise.
 *
 * This function converts the data stored in the internal buffer of the given
 * @b string to multibyte characters from wide characters.
 *
 * If the conversion is successful, the inner buffer is replaced by its
 * wide character equivalent.
 *
 * If the function fails to convert the data, it will returns -1 and leave the
 * buffer unchanged.
 *
 */

/* -------------------------------------------------------------------------- */
#ifdef HAS_ICONV
/* -------------------------------------------------------------------------- */

public size_t string_convs(const char *src, size_t srclen, const char *src_enc,
                           char *dst, size_t dstlen, const char *dst_enc);

/**
 * @ingroup string
 * @fn size_t string_convs(const char *src, size_t srclen, const char *src_enc,
 *                         char *dst, size_t dstlen, const char *dst_enc)
 * @param src the string to be converted.
 * @param srclen the length of the string to be converted.
 * @param src_enc the encoding of the string to be converted.
 * @param dst the output buffer.
 * @param dstlen length of the output buffer.
 * @param dst_enc encoding to use for the conversion.
 * @return -1 if an error occurred, 0 otherwise.
 *
 * This function uses the Iconv library to convert the encoding of the given
 * string. If the output is NULL, the function will return the length the
 * output buffer should have to fit the converted string.
 *
 */

/* -------------------------------------------------------------------------- */

public int string_conv(m_string *s, const char *src_enc, const char *dst_enc);

/**
 * @ingroup string
 * @fn int string_conv(m_string *s, const char *src_enc, const char *dst_enc)
 * @param s the string to be converted.
 * @param src_enc the encoding of the original string
 * @param dst_enc the encoding to use for the conversion
 * @return -1 if an error occurred, 0 otherwise.
 *
 * This function uses the Iconv library to convert the encoding of the given
 * string.
 *
 */

/* -------------------------------------------------------------------------- */
#endif
/* -------------------------------------------------------------------------- */

public int string_swap(m_string *string);

/**
 * @ingroup string
 * @fn int string_swap(m_string *string)
 * @param string the string to be swapped
 * @return -1 if an error occurred, 0 otherwise.
 *
 * This function reverses the endianness of a binary string, without
 * consideration for the current system endianness.
 *
 */

/* -------------------------------------------------------------------------- */

public m_string *string_dup(const m_string *string);

/**
 * @ingroup string
 * @fn m_string *string_dup(const m_string *string)
 * @param string the string to be duplicated.
 * @return a pointer to a new string, or NULL.
 *
 * This function simply makes an exact copy of the m_string given in parameter,
 * and returns it.
 *
 * If the source string has subtokens, they are copied along with the data.
 *
 * If for some reason copying the string was not possible, the function
 * will return NULL.
 *
 */

/* -------------------------------------------------------------------------- */

public m_string *string_dupextend(const m_string *string, size_t extra);

/**
 * @ingroup string
 * @fn m_string *string_dup(const m_string *string)
 * @param string the string to be duplicated.
 * @param extra additional space to allocate.
 * @return a pointer to a new string, or NULL.
 *
 * This function simply makes an exact copy of the m_string given in parameter,
 * and returns it. The new string will be extended to be @b extra bytes longer
 * than the original.
 *
 * If the source string has subtokens, they are copied along with the data.
 *
 * If for some reason copying the string was not possible, the function
 * will return NULL.
 *
 */

/* -------------------------------------------------------------------------- */

public m_string *string_free(m_string *string);

/**
 * @ingroup string
 * @fn m_string *string_free(m_string *string)
 * @param string the string to be destroyed.
 * @return always NULL.
 *
 * This function will properly clean up and destroy a m_string structure,
 * including its tokens if there are any.
 *
 * This function always returns NULL, so it can be used to clean a pointer:
 * str = string_free(str);
 *
 */

/* -------------------------------------------------------------------------- */

public size_t string_len(const m_string *string);

/**
 * @ingroup string
 * @fn size_t string_len(const m_string *string)
 * @param string the string the size has to be read.
 * @return the size of the string, or (size_t) -1.
 *
 * This function simply returns the size of the given string. It may be
 * preferable to the SIZE, CLEN or WLEN macros since it performs a NULL check.
 *
 * If the size can not be read, the function returns (size_t) -1.
 *
 */

/* -------------------------------------------------------------------------- */

public size_t string_spc(const m_string *string);

/**
 * @ingroup string
 * @fn size_t string_spc(const m_string *string)
 * @param string the string the buffer space has to be read.
 * @return the allocation size of the string, or (size_t) -1.
 *
 * This function simply returns the allocation space consumed by the buffer
 * of the given string.
 *
 * If the buffer space can not be read, the function returns (size_t) -1.
 *
 */

/* -------------------------------------------------------------------------- */

public size_t string_avl(const m_string *string);

/* -------------------------------------------------------------------------- */

public int string_dim(m_string *string, size_t size);

/**
 * @ingroup string
 * @fn int string_dim(m_string *string, size_t size)
 * @param string the string to resize.
 * @param size the new size to give to the string.
 * @return -1 if the string could not be resized, 0 otherwise.
 *
 * This function forces the resizing of a string. The buffer will be truncated
 * or extended to the given @b size, without care for the inner data.
 *
 * If the buffer can not be resized (wrong size, not enough memory or if the
 * string is marked as not resizable), the string is left untouched and the
 * function returns -1.
 *
 * If the buffer size matches the given @b size, no changes are done to the
 * string and the function returns 0.
 *
 * If this function is called on a token, it will extend or shrink the
 * main string and alter the token size accordingly.
 *
 * Subtokens are kept and updated after resizing.
 *
 */

/* -------------------------------------------------------------------------- */

public int string_extend(m_string *string, size_t size);

/**
 * @ingroup string
 * @fn int string_extend(m_string *string, size_t size)
 * @param string the string to be extended.
 * @param size the new size of the string.
 * @return -1 if an error occurs (see @ref string_dim), 0 otherwise.
 *
 * This function is simply a wrapper around @ref string_dim(), which ensures
 * the buffer will only be resized if @b size is greater than its current size.
 *
 */
 

/* -------------------------------------------------------------------------- */

public int string_shrink(m_string *string, size_t size);

/**
 * @ingroup string
 * @fn int string_shrink(m_string *string, size_t size)
 * @param string the string to be shrunk.
 * @param size the new size of the string.
 * @return -1 if an error occurs (see @ref string_dim), 0 otherwise.
 *
 * This function is simply a wrapper around @ref string_dim(), which ensures
 * the buffer will only be resized if @b size is less than its current size.
 *
 */

/* -------------------------------------------------------------------------- */

public m_string *string_movs(m_string *to, off_t o, const char *from, size_t l);

/**
 * @ingroup string
 * @fn m_string *string_movs(m_string *to, off_t o, const char *from, size_t l)
 * @param to the destination string.
 * @param o an offset within the destination string bounds
 * @param from the source string.
 * @param l the length of the C source string.
 * @return NULL if an error occurs, or a pointer to the destination string.
 *
 * This function performs low level manipulations on m_strings.
 *
 * It will move @b len bytes from the source string to the offset @b o
 * of the destination string given in parameter. The destination
 * string is automatically resized.
 *
 * It handles aliasing (same string as both source and destination),
 * performs bound checking and size adjustments.
 *
 * If the given destination string is NULL, it will be allocated, so the
 * return value of this function should always be checked.
 *
 * @ref string_movs() may fail for multiple reasons:
 * - the given source or its size are not correct
 * - resizing the destination string is not possible or allowed
 * - writing to the destination string is not allowed
 * - the destination string is NULL and can not be allocated
 * - the given offset is out of bound
 * However, in all these case, the strings will be left unchanged and the
 * function will returns NULL.
 *
 * If the manipulation is successful, the function will discard the destination
 * string's tokens and return a pointer to the updated destination string.
 *
 * Most of the classic string manipulation functions below use
 * @ref string_movs() internally, and therefore will discard the destination
 * string's token unless expressly stated in the API documentation.
 *
 */

/* -------------------------------------------------------------------------- */

public int string_suppr(m_string *string, off_t o, size_t l);

/**
 * @ingroup string
 * @fn int string_suppr(m_string *string, off_t o, size_t l)
 * @param string the target string.
 * @param o beginning of the substring to suppress.
 * @param l length of the substring to suppress.
 * @return -1 if an error occurred, 0 otherwise.
 *
 * This function suppress a substring delimited by its offset in the target
 * string and its length from the target string, using @ref string_movs() as
 * backend. This function will keep and properly reposition the destination
 * string's tokens.
 *
 */

/* -------------------------------------------------------------------------- */

public m_string *string_cats(m_string *to, const char *from, size_t len);

/**
 * @ingroup string
 * @fn m_string *string_cats(m_string *to, const char *from, size_t len)
 * @param to the destination string.
 * @param from the source C string.
 * @param len the source C string length.
 * @return NULL if an error occurred, 0 otherwise.
 *
 * This function concatenates the source string to the destination string,
 * using @ref string_movs() as backend.
 *
 */

/* -------------------------------------------------------------------------- */

public m_string *string_pres(m_string *to, const char *from, size_t len);

/**
 * @ingroup string
 * @fn m_string *string_pres(m_string *to, const char *from, size_t len)
 * @param to the destination string.
 * @param from the source C string.
 * @param len the source C string length.
 * @return NULL if an error occurred, 0 otherwise.
 *
 * This function prepends the source string to the destination string,
 * using @ref string_movs() as backend.
 *
 */

/* -------------------------------------------------------------------------- */

public m_string *string_cpys(m_string *to, const char *from, size_t len);

/**
 * @ingroup string
 * @fn m_string *string_cpys(m_string *to, const char *from, size_t len)
 * @param to the destination string.
 * @param from the source C string.
 * @param len the source C string length.
 * @return NULL if an error occurred, 0 otherwise.
 *
 * This function copies up to @b len bytes from the source string to the
 * destination string, using @ref string_movs() as backend.
 *
 */

/* -------------------------------------------------------------------------- */

public int string_cmps(const m_string *a, const char *b, size_t len);

/**
 * @ingroup string
 * @fn m_string *string_cmps(const m_string *a, const char *b, size_t len)
 * @param a the destination string.
 * @param b the source C string.
 * @param len the source C string length.
 * @return memcmp() return value, or 2 if an error occurs
 *
 * This function compares up to @b len bytes of from the string A with the
 * string B.
 *
 */

/* -------------------------------------------------------------------------- */

public m_string *string_mov(m_string *to, off_t off,
                            const m_string *from, size_t len);

/**
 * @ingroup string
 * @fn m_string *string_mov(m_string *to, off_t off, const m_string *from,
                            size_t len                                    )
 * @param to the destination string.
 * @param off an offset within the destination string bounds.
 * @param from the source string.
 * @param len the length of the data from the source string to use.
 * @return NULL if an error occurred, 0 otherwise.
 *
 * This function is simply a wrapper around @ref string_movs(), please see
 * the documentation of @ref string_movs().
 *
 */

/* -------------------------------------------------------------------------- */

public m_string *string_cat(m_string *to, const m_string *from);

/**
 * @ingroup string
 * @fn m_string *string_cat(m_string *to, const m_string *from)
 * @param to the destination string.
 * @param from the source string.
 * @return NULL if an error occurred, 0 otherwise.
 *
 * This function is simply a wrapper around @ref string_cats(), please see
 * the documentation of @ref string_cats().
 *
 */

/* -------------------------------------------------------------------------- */

public m_string *string_pre(m_string *to, const m_string *from);

/**
 * @ingroup string
 * @fn m_string *string_pre(m_string *to, const m_string *from)
 * @param to the destination string.
 * @param from the source string.
 * @return NULL if an error occurred, 0 otherwise.
 *
 * This function is simply a wrapper around @ref string_pres(), please see
 * the documentation of @ref string_pres().
 *
 */

/* -------------------------------------------------------------------------- */

public m_string *string_cpy(m_string *to, const m_string *from);

/**
 * @ingroup string
 * @fn m_string *string_cpy(m_string *to, const m_string *from)
 * @param to the destination string.
 * @param from the source string.
 * @return NULL if an error occurred, 0 otherwise.
 *
 * This function is simply a wrapper around @ref string_cpys(), please see
 * the documentation of @ref string_cpys().
 *
 */

/* -------------------------------------------------------------------------- */

public int string_cmp(const m_string *a, const m_string *b);

/**
 * @ingroup string
 * @fn string_cmp(const m_string *a, const m_string *b)
 * @param a the destination string.
 * @param b the source string.
 * @return NULL if an error occurred, 0 otherwise.
 *
 * This function is simply a wrapper around @ref string_cmps(), please see
 * the documentation of @ref string_cmps().
 *
 */

/* -------------------------------------------------------------------------- */

public int string_upper(m_string *string);

/**
 * @ingroup string
 * @fn int string_upper(m_string *string)
 * @param string
 * @return -1 if an error occurred, 0 otherwise
 *
 * This function simply converts the internal buffer of the given string to
 * upper case.
 *
 * Since the conversion is done in place, no resizing is done and thus
 * existing tokens are preserved.
 *
 * If an error occurs, the function returns -1.
 *
 */

/* -------------------------------------------------------------------------- */

public int string_lower(m_string *string);

/**
 * @ingroup string
 * @fn int string_upper(m_string *string)
 * @param string
 * @return -1 if an error occurred, 0 otherwise
 *
 * This function simply converts the internal buffer of the given string to
 * lower case.
 *
 * Since the conversion is done in place, no resizing is done and thus
 * existing tokens are preserved.
 *
 * If an error occurs, the function returns -1.
 *
 */

/* -------------------------------------------------------------------------- */

public int string_compile_searchstring(m_search_string *c,
                                       const char *s, size_t len);

/* -------------------------------------------------------------------------- */

public off_t string_compile_find(const char *s, size_t len, size_t o,
                                 const char *sub, m_search_string *c);

/* -------------------------------------------------------------------------- */

public off_t string_sfinds(const char *str, size_t slen, size_t o,
                           const char *sub, size_t len            );

/* -------------------------------------------------------------------------- */

public off_t string_finds(const m_string *str, size_t o, const char *sub,
                          size_t len                                     );

/**
 * @ingroup string
 * @fn off_t string_finds(const m_string *str, off_t o, const char *sub,
                          size_t len                                    )
 * @param str the "haystack".
 * @param o an offset within the bounds of the "haystack".
 * @param sub the "needle".
 * @param len the size of the needle.
 * @return -1 if an error occurs, or the position of the needle in the haystack.
 *
 * This functions searches for a substring starting from the offset @b o
 * in the given string @b str.
 *
 * @warning The "needle" can not be longer than 255 bytes.
 *
 * If the substring is not found or an error occurs, this function will
 * return -1. Otherwise, the position of the first byte of the substring in
 * the string is returned.
 *
 */

/* -------------------------------------------------------------------------- */

public off_t string_find(const m_string *string, size_t o, const m_string *sub);

/**
 * @ingroup string
 * @fn off_t string_find(const m_string *string, size_t o, const m_string *sub)
 * @param string the "haystack".
 * @param size_t o an offset within the bounds of the "haystack".
 * @param sub the "needle".
 * @return -1 if an error occurs, or the position of the needle in the haystack.
 *
 * This function is simply a wrapper around @ref string_finds(), please see
 * the documentation of @ref string_finds().
 *
 */

/* -------------------------------------------------------------------------- */

public off_t string_pos(const m_string *string, const char *sub);

/**
 * @ingroup string
 * @fn off_t string_pos(const m_string *string, const char *sub)
 * @param string the "haystack".
 * @param sub the "needle".
 * @return -1 if an error occurs, or the position of the needle in the haystack.
 *
 * This function is simply a wrapper around @ref string_finds(), please see
 * the documentation of @ref string_finds().
 *
 */

/* -------------------------------------------------------------------------- */

public int string_splits(m_string *string, const char *pattern, size_t len);

/**
 * @ingroup string
 * @fn string_splits(m_string *string, const char *pattern, size_t len)
 * @param string the string to be split.
 * @param pattern the token delimiter.
 * @param len the size of the delimiter.
 * @return -1 if an error occurred, 0 otherwise.
 *
 * This function use @ref string_finds() to split the given @b string, using
 * a delimiter passed in parameter.
 *
 * The parts of a split string are stored in the @ref token field of the
 * m_string structure, and their number in the @ref parts field.
 * They can be accessed more conveniently using the @ref PARTS and
 * @ref TOKEN macros.
 *
 * @note It is important to understand that tokens are *not* standalone
 * strings, they merely point to parts of interest within the string which
 * hosts them, the @b parent string.
 * However, tokens can be transparently used as perfectly valid,
 * resizeable and writeable m_string with the great majority of the
 * functions of the @ref string module.
 *
 * @warning several string manipulation functions destroy the embedded
 * tokens of a string to avoid corruptions.
 *
 * You can clean up tokens at any time by calling @ref string_free_token().
 *
 */
 
/* -------------------------------------------------------------------------- */

public int string_split(m_string *string, const m_string *pattern);

/**
 * @ingroup string
 * @fn int string_split(m_string *string, const m_string *pattern)
 * @param string the string to be split.
 * @param pattern the token delimiter.
 * @return -1 if an error occurred, 0 otherwise.
 *
 * This function is simply a wrapper around @ref string_splits(), please see
 * the documentation of @ref string_splits().
 *
 */
 
/* -------------------------------------------------------------------------- */

public int string_merges(m_string *string, const char *pattern, size_t len);

/**
 * @ingroup string
 * @fn int string_merges(m_string *string, const char *pattern, size_t len)
 * @param string the string to be merged
 * @param pattern the new token delimiter
 * @param len the size of the delimiter
 * @return -1 if an error occurred, 0 otherwise.
 *
 * This function replaces all the delimiters between tokens of the
 * target string with the new delimiter given in parameter.
 *
 */
 
/* -------------------------------------------------------------------------- */

public int string_merge(m_string *string, const m_string *pattern);

/**
 * @ingroup string
 * @fn int string_merge(m_string *string, const m_string *pattern)
 * @param string the string to be merged
 * @param pattern the new token delimiter
 * @return -1 if an error occurred, 0 otherwise.
 *
 * This function is simply a wrapper around @ref string_merges(), please see
 * the documentation of @ref string_merges().
 *
 */

/* -------------------------------------------------------------------------- */

public m_string *string_reps(m_string *string, const char *search, size_t slen,
                             const char *rep, size_t rlen                     );

/**
 * @ingroup string
 * @fn m_string *string_reps(m_string *string, const char *search, size_t slen,
                             const char *rep, size_t rlen                      )
 * @param string the string where an expression should be replaced.
 * @param search the expression to be replaced.
 * @param slen the length of this expression.
 * @param rep the replacement string.
 * @param rlen the length of the replacement string.
 * @return NULL if an error occurred, a pointer to the main string otherwise.
 *
 * This function searches for the given @b search string inside the main
 * @b string, and replaces each occurrence by the provided @b rep string.
 *
 * If a NULL @b rep arguement is given, the target substring is deleted
 * instead and the string resized accordingly.
 *
 * The original string is automatically resized to fit the modifications, but
 * since it is a destructive transformation, all its token will be deleted
 * in order to avoid corruptions.
 *
 */

/* -------------------------------------------------------------------------- */

public m_string *string_rep(m_string *string, const m_string *search,
                            const char *rep, size_t rlen             );

/**
 * @ingroup string
 * @fn m_string *string_rep(m_string *string, const m_string *search,
                            const char *rep, size_t rlen             )
 * @param string the string where the an expression should be replaced.
 * @param search the expression to be replaced.
 * @param rep the replacement string.
 * @param rlen the length of the replacement string.
 * @return NULL if an error occurred, a pointer to the main string otherwise.
 *
 * This function is simply a wrapper around @ref string_reps(), please see
 * the documentation of @ref string_reps().
 *
 */

/* -------------------------------------------------------------------------- */

public m_string *string_rems(m_string *str, const char *rem, size_t len);

/**
 * @ingroup string
 * @fn m_string *string_rems(m_string *str, const char *rem, size_t len)
 * @param str the string to be processed
 * @param rem the substring which must be removed
 * @param len the length of the substring
 * @return NULL if an error occurred, a pointer to the main string otherwise.
 *
 * This function is simply a wrapper around @ref string_reps() with a NULL
 * replacement string. This way, all occurrences of the target substring
 * are dropped.
 *
 * Please see the documentation of @ref string_reps() for further details.
 *
 * @note Since it calls @ref string_reps(), this function cause the destruction
 * of all the tokens of the main string.
 *
 */

/* -------------------------------------------------------------------------- */

public m_string *string_rem(m_string *string, const m_string *rem);

/**
 * @ingroup string
 * @fn m_string *string_rem(m_string *string, const m_string *rem)
 * @param string the string to be processed
 * @param rem the substring which must be removed
 * @return NULL if an error occurred, a pointer to the main string otherwise.
 *
 * This function is simply a wrapper around @ref string_reps() with a NULL
 * replacement string. This way, all occurrences of the target substring
 * are dropped.
 *
 * Please see the documentation of @ref string_reps() for further details.
 *
 * @note Since it calls @ref string_reps(), this function cause the destruction
 * of all the tokens of the main string.
 *
 */

/* -------------------------------------------------------------------------- */

public m_string *string_select(m_string *string, unsigned int off, size_t len);

/* -------------------------------------------------------------------------- */

public m_string *string_add_token(m_string *s, off_t start, off_t end);

/* -------------------------------------------------------------------------- */

public int string_push_tokens(m_string *string, const char *token, size_t len);

/**
 * @ingroup string
 * @fn int string_push_tokens(m_string *string, const char *token, size_t len)
 * @param string the string to which the token will be appended
 * @param token the data to append
 * @param len the length of the data
 * @return -1 if an error happens, 0 otherwise
 *
 * This function will append the data at the end of the given @ref string and
 * create an associated token.
 *
 */

/* -------------------------------------------------------------------------- */

public int string_push_token(m_string *string, m_string *token);

/**
 * @ingroup string
 * @fn int string_push_token(m_string *string, m_string *token)
 * @param string the string to which the token will be appended
 * @param token the data to append
 * @return -1 if an error happens, 0 otherwise
 *
 * This function is simply a wrapper around @ref string_push_tokens(), please
 * see the documentation of @ref string_push_tokens().
 *
 */

/* -------------------------------------------------------------------------- */

public m_string *string_suppr_token(m_string *s, unsigned int index);

/* -------------------------------------------------------------------------- */

public m_string *string_pop_token(m_string *string);

/* -------------------------------------------------------------------------- */
#ifdef HAS_PCRE
/* -------------------------------------------------------------------------- */

public int string_parse(m_string *string, const char *pattern, size_t len);

/**
 * @ingroup string
 * @fn int string_parse(m_string *string, const char *pattern, size_t len)
 * @param string the string to be processed
 * @param pattern the C string holding the regular expression to match
 * @param len the length of the pattern
 * @return -1 if an error happens, 0 otherwise
 *
 * This function will match the regular expression @ref pattern with the
 * given @ref string. If a match is found, it will be stored as a token.
 *
 * If you use the parenthesis to extract substrings, you can get the
 * substrings as subtokens of the matching token.
 *
 */

/* -------------------------------------------------------------------------- */
#endif
/* -------------------------------------------------------------------------- */

public m_string *string_b58s(const char *s, size_t size);

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

public m_string *string_b58(const m_string *s);

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

public m_string *string_deb58s(const char *s, size_t size);

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

public m_string *string_deb58(const m_string *s);

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
public m_string *string_b64s(const char *s, size_t size, size_t linesize);

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

public m_string *string_b64(const m_string *s, size_t linesize);

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

public m_string *string_deb64s(const char *s, size_t size);

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

public m_string *string_deb64(const m_string *s);

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
#ifdef HAS_ZLIB
/* -------------------------------------------------------------------------- */

public m_string *string_compress(m_string *s);

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

public m_string *string_uncompress(m_string *s, size_t original_size);

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

public m_string *string_sha1s(const char *string, size_t len);

/* -------------------------------------------------------------------------- */

public m_string *string_sha1(m_string *s);

/* -------------------------------------------------------------------------- */
#ifdef _ENABLE_HTTP
/* -------------------------------------------------------------------------- */

public char *string_rawurlencode(const char *url, size_t len, int flags);

/* -------------------------------------------------------------------------- */

public int string_urlencode(m_string *url, int flags);

/* -------------------------------------------------------------------------- */
#endif
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
#ifdef _ENABLE_JSON
/* -------------------------------------------------------------------------- */

public int string_parse_json(m_string *s, char strict, m_json_parser *ctx);

/**
 * @ingroup string
 * @fn m_string *string_parse_json(m_string *s, int strict, m_json_parser *ctx)
 * @param s the string to be parsed
 * @param strict boolean - enable or disable strict parsing
 * @param ctx optional parser context
 * @return -1 if an error occurred, 0 otherwise
 *
 * This function creates tokens for each JSON element in the provided string.
 *
 * In STRICT mode, the function rigorously follows RFC 7159 and will reject
 * any non-compliant or malformed input.
 *
 * In QUIRKS mode, the function loosely follows the JSON5 guidelines and will
 * accept several popular extensions to the JSON specification:
 * - unquoted keys in objects (ECMAScript 5.1 IdentifierName),
 * - single quoted strings,
 * - single- and multi-line comments,
 * - unsigned hexadecimal numbers,
 * - real numbers without fractional part (e.g.: 123. ) (ActionScript3),
 * - extra commas and missing values in objects and arrays,
 * - escaped and unescaped tabs and CRLF in strings.
 * Unsupported:
 * - leading decimal point,
 * - leading plus sign,
 * - NaN, Infinity, -Infinity.
 * 
 * This function tokenizes the input and checks for correctness but will not
 * interpret the data. This task is left to the parser. The builtin parser
 * ( @ref module::parser ) performs UTF-8 validation, escape sequences
 * conversions and turns JSON numbers into native IEEE754 double-precision
 * floating-point numbers.
 *
 * The function is designed to handle streaming and partial input. If several
 * JSON messages are concatenated, each will be parsed as distinct tokens.
 * Partial input will not trigger an error, but will be flagged, so that
 * valid tokens can be processed first, and parsing resumed later when more
 * data become available.
 *
 * @note Use the macro @ref IS_ERROR to test if a token is incomplete.
 *
 */

/* -------------------------------------------------------------------------- */
#endif
/* -------------------------------------------------------------------------- */

public void string_free_token(m_string *string);

/**
 * @ingroup string
 * @fn void string_free_token(m_string *string)
 * @param string the string to be cleaned.
 * @return void
 *
 * This function deletes all the tokens of the given string.
 * See @ref m_string or @ref string_splits() for more informations about
 * the tokens.
 *
 */

/* -------------------------------------------------------------------------- */

public void string_api_cleanup(void);

/* -------------------------------------------------------------------------- */

#endif
