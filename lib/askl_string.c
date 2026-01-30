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

#include "askl_string.h"
#include "arcane/bitops.c"

#define _UINT(c) ((unsigned int) ((unsigned char) c))

struct _String_Pattern {
    size_t _len;
    size_t _shift;
    uint8_t _lut[UCHAR_MAX + 1];
};

/* -------------------------------------------------------------------------- */

ASKL_API int string_api_setup(void)
{
    return 0;
}

/* -------------------------------------------------------------------------- */

ASKL_API String *string_reserve(const char *string, size_t len, size_t extra)
{
    /** @brief allocate a String and initialize it with the given data */

    String *new = NULL;
    int32_t allocsize = 0;
    size_t total = len + extra;

    if (total < len) {
        debug("string_reserve(): integer overflow.\n");
        return NULL;
    }

    if (! (new = malloc(sizeof(*new))) ) {
        perror(ERR(string_reserve, malloc));
        return NULL;
    }

    /* allocate the internal buffer, ensure it is on wchar_t boundary */
    if (! total) {
        new->data = NULL;
        new->len = new->internal.capacity = 0;
        new->internal.flags = 0;
        new->parent = NULL;
        new->internal.tokens_capacity = new->count = 0;
        new->tokens = NULL;
        return new;
    }

    allocsize = (total + sizeof(wchar_t)) * sizeof(*new->data);

    if ((size_t) allocsize < total) {
        debug("string_reserve(): integer overflow.\n");
        free(new);
        return NULL;
    }

    if (! (new->data = malloc(allocsize)) ) {
        perror(ERR(string_reserve, malloc));
        free(new);
        return NULL;
    }
    new->internal.capacity = total * sizeof(*new->data);

    /* copy the given data in the internal buffer */
    if (string) memcpy(new->data, string, len);

    memset(new->data + len, 0, sizeof(wchar_t));

    new->len = len; new->internal.flags = 0;
    new->parent = NULL; new->tokens = NULL;
    new->internal.tokens_capacity = new->count = 0;

    return new;
}

/* -------------------------------------------------------------------------- */

ASKL_API String *string_alloc(const char *string, size_t len)
{
    return string_reserve(string, len, 0);
}

/* -------------------------------------------------------------------------- */

ASKL_API String *string_encaps(const char *string, size_t len)
{
    /** @brief encapsulate an existing buffer in a static String structure */

    String *new = NULL;

    if (! string || ! len) return NULL;

    if (! (new = string_alloc(NULL, 0)) ) {
        debug("string_encaps(): out of memory.\n");
        return NULL;
    }

    new->data = (char *) string;
    new->len = len; new->internal.capacity = len;
    new->internal.flags = _STRING_ENCAPSULATED;
    new->parent = NULL;
    new->internal.tokens_capacity = new->count = 0;
    new->tokens = NULL;

    return new;
}

/* -------------------------------------------------------------------------- */

ASKL_API uint8_t string_fetch_uint8(String *string)
{
    uint8_t ret = 0;

    string_cut(string, 0, sizeof(ret), (char *) & ret);

    return ret;
}

/* -------------------------------------------------------------------------- */

ASKL_API uint16_t string_fetch_uint16(String *string)
{
    uint16_t ret = 0;

    string_cut(string, 0, sizeof(ret), (char *) & ret);

    return ret;
}

/* -------------------------------------------------------------------------- */

ASKL_API uint32_t string_fetch_uint32(String *string)
{
    uint32_t ret = 0;

    string_cut(string, 0, sizeof(ret), (char *) & ret);

    return ret;
}

/* -------------------------------------------------------------------------- */

ASKL_API uint64_t string_fetch_uint64(String *string)
{
    uint64_t ret = 0;

    string_cut(string, 0, sizeof(ret), (char *) & ret);

    return ret;
}

/* -------------------------------------------------------------------------- */

ASKL_API int string_fetch_buffer(String *string, char *out, size_t len)
{
    /** @brief move data from the string to an external buffer */

    if (! string || ! string->data || ! out || ! len) return -1;

    string_cut(string, 0, len, out);

    return 0;
}

/* -------------------------------------------------------------------------- */

ASKL_API void string_flush(String *string)
{
    if (! string) return;

    string_free_token(string);

    string_cut(string, 0, string->len, NULL);
}

/* -------------------------------------------------------------------------- */

ASKL_API int string_wchar(String *string)
{
    /** @brief convert a multibyte string to a wide character string */

    char *buffer = NULL;
    size_t bufsize = 0;
    size_t ret = 0;

    if (! string || ! string->data) return -1;

    /* check if writing and resizing the string is allowed */
    if (string->internal.flags & _STRING_IMMUTABLE) return -1;

    /* find the size of the wchar string */
    if ( (bufsize = mbstowcs(NULL, string->data, 0)) != (size_t) -1) {
        /* allocate the wchar buffer */
        buffer = malloc( (bufsize + 1) * sizeof(wchar_t));
        if (buffer == NULL) {
            perror(ERR(string_wchar, malloc)); return -1;
        }

        /* convert the multibyte string to wchar */
        ret = mbstowcs((wchar_t *) buffer, string->data, bufsize + 1);

        if (ret == (size_t) -1) {
            free(buffer); perror(ERR(string_wchar, mbstowcs));
            return -1;
        }

        /* update the string */
        free(string->data); string->data = buffer;
        string->len = bufsize * sizeof(wchar_t);
        string->internal.capacity = (bufsize + 1) * sizeof(wchar_t);
    } else {
        perror(ERR(string_wchar, mbstowcs)); return -1;
    }

    return 0;
}

/* -------------------------------------------------------------------------- */

ASKL_API int string_mbyte(String *string)
{
    /** @brief convert a wide character string to a multibyte string */

    char *buffer = NULL;
    size_t l = 0;

    if (! string || ! string->data) return -1;

    /* check if writing and resizing the string is allowed */
    if (string->internal.flags & _STRING_IMMUTABLE) return -1;

    /* find the size of the multibyte buffer */
    if ( (l = wcstombs(NULL, (wchar_t *) string->data, 0)) != (size_t) -1) {
        /* allocate it */
        buffer = malloc( (l + 1) * sizeof(*buffer));
        if (buffer == NULL) {
            perror(ERR(string_mbyte, malloc)); return -1;
        }

        /* perform the conversion */
        if (wcstombs(buffer, (wchar_t *) string->data, l + 1) == (size_t) -1) {
            perror(ERR(string_mbyte, wcstombs));
            free(buffer); return -1;
        }

        /* update the string */
        free(string->data); string->data = buffer;
        string->len = l; string->internal.capacity = l + 1;
    } else {
        perror(ERR(string_mbyte, wcstombs)); return -1;
    }

    return 0;
}

/* -------------------------------------------------------------------------- */
#ifdef HAS_ICONV
/* -------------------------------------------------------------------------- */

ASKL_API size_t string_convs(const char *src, size_t srclen, const char *src_enc,
                           char *dst, size_t dstlen, const char *dst_enc)
{
    char buffer[BUFSIZ], *out = dst;
    size_t inlen = srclen, outlen = dstlen;
    #ifdef __APPLE__
    #if ! defined(MAC_OS_X_VERSION_10_5) && ! defined(__MAC_10_5)
    const /* SUSv2 definition */
    #endif
    #endif
    char *in = (char *) src;
    iconv_t conv;
    size_t allocsize = 0;

    if (! src || ! srclen) {
        debug("string_convs(): bad parameters.\n");
        return -1;
    }

    if (! (conv = iconv_open(dst_enc, src_enc)) ) {
        perror(ERR(string_convs, iconv_open)); return -1;
    }

    /* dry run: use a static buffer for output */
    if (! dst) { out = buffer; outlen = sizeof(buffer); }

    while (iconv(conv, & in, & inlen, & out, & outlen) == (size_t) -1) {
        if (errno == E2BIG && ! dst) {
            allocsize += sizeof(buffer) - outlen;
            out = buffer; outlen = sizeof(buffer);
            continue;
        }
        /* conversion failure */
        perror(ERR(string_convs, iconv));
        goto _err_conv;
    }

    /* dry run: evaluate the size required for the output */
    if (! dst) allocsize += sizeof(buffer) - outlen;

    iconv_close(conv);

    return allocsize;

_err_conv:
    iconv_close(conv);

    return -1;
}

/* -------------------------------------------------------------------------- */

ASKL_API int string_conv(String *s, const char *src_enc, const char *dst_enc)
{
    char buffer[BUFSIZ], *out = buffer;
    size_t inlen = 0, outlen = sizeof(buffer);
    #ifdef __APPLE__
    #if ! defined(MAC_OS_X_VERSION_10_5) && ! defined(__MAC_10_5)
    const /* SUSv2 definition */
    #endif
    #endif
    char *in = NULL;
    iconv_t conv;
    size_t allocsize = 0;

    if (! s) {
        debug("string_conv(): bad parameters.\n");
        return -1;
    }

    if (! (conv = iconv_open(dst_enc, src_enc)) ) {
        perror(ERR(string_conv, iconv_open)); return -1;
    }

    in = s->data; inlen = s->len;

    while (iconv(conv, & in, & inlen, & out, & outlen) == (size_t) -1) {
        if (errno == E2BIG) {
            allocsize += sizeof(buffer) - outlen;
            out = buffer; outlen = sizeof(buffer);
            continue;
        }
        /* incomplete input or invalid multibyte sequence */
        perror(ERR(string_conv, iconv));
        goto _err_conv;
    }

    allocsize += sizeof(buffer) - outlen;

    if (allocsize != s->len) {
        if (allocsize > s->len) {
            /* extend the string */
            if (string_extend(s, allocsize) == -1)
                goto _err_conv;
        }
        string_free_token(s);
    }

    if (allocsize > sizeof(buffer)) {
        in = s->data; inlen = s->len;
        out = s->data; outlen = allocsize;
        /* convert again */
        if (iconv(conv, & in, & inlen, & out, & outlen) == (size_t) -1) {
            perror(ERR(string_conv, iconv));
            goto _err_conv;
        }
    } else memcpy(s->data, buffer, allocsize);

    s->len = allocsize;

    iconv_close(conv);

    return 0;

_err_conv:
    iconv_close(conv);

    return -1;
}

/* -------------------------------------------------------------------------- */
#endif
/* -------------------------------------------------------------------------- */

static int _string_copy_tokens(String *dst, const String *src)
{
    unsigned int i = 0;

    if (! dst || ! src || ! src->count) return 0;

    dst->count = src->count;

    if (! (dst->tokens = malloc(dst->count * sizeof(*dst->tokens))) ) return -1;

    for (i = 0; i < dst->count; i ++) {
        dst->tokens[i].data = dst->data + (src->tokens[i].data - src->data);
        dst->tokens[i].len = src->tokens[i].len;
        dst->tokens[i].internal.capacity = src->tokens[i].internal.capacity;
        dst->tokens[i].internal.flags = src->tokens[i].internal.flags;
        dst->tokens[i].parent = dst;
        /* the parts and token members of the struct will be
           modified by subsequent calls */
        dst->tokens[i].internal.tokens_capacity = dst->tokens[i].count = 0;
        dst->tokens[i].tokens = NULL;
        if (_string_copy_tokens(& dst->tokens[i], & src->tokens[i]) == -1)
            return -1;
    }

    dst->internal.tokens_capacity = dst->count;

    return 0;
}

/* -------------------------------------------------------------------------- */

ASKL_API String *string_clone(const String *string)
{
    /** @brief duplicate a string */

    return string_clone_reserve(string, 0);
}

/* -------------------------------------------------------------------------- */

ASKL_API String *string_clone_reserve(const String *s, size_t x)
{
    String *ret = NULL;

    if (! s) return NULL;

    /* copy the string */
    if (! (ret = string_reserve(s->data, s->len, x)) ) return NULL;

    /* copy the tokens */
    if (_string_copy_tokens(ret, s) == -1) return string_free(ret);

    return ret;
}

/* -------------------------------------------------------------------------- */

ASKL_API String *string_free(String *string)
{
    /** @brief clean up a String and its internal buffer */

    String *parent = string;

    if (! string) return NULL;

    /* check this is a parent string */
    while (parent->parent) parent = parent->parent;
    #ifdef DEBUG
    if (string != parent)
        debug("string_free(): warning: freeing the parent of the token.\n");
    #endif
    string = parent;

    string_free_token(string);

    if (string->internal.flags & _STRING_STATIC_ALLOC) return NULL;

    if (~string->internal.flags & _STRING_DISABLE_FREE)
        free(string->data);
    free(string);

    return NULL;
}

/* -------------------------------------------------------------------------- */

ASKL_API size_t string_len(const String *string)
{
    /** @brief return the length of the string data */

    return string ? string->len : (size_t) -1;
}

/* -------------------------------------------------------------------------- */

ASKL_API size_t string_capacity(const String *string)
{
    /** @brief return the string allocated buffer size */

    return string ? string->internal.capacity : (size_t) -1;
}

/* -------------------------------------------------------------------------- */

ASKL_API size_t string_available(const String *string)
{
    /** @brief return the space still available without extending the string */

    return string ? string->internal.capacity - string->len : (size_t) -1;
}

/* -------------------------------------------------------------------------- */

static void _string_rebase_token(String *s, char *oldbase, char *newbase)
{
    unsigned int i = 0;

    if (! s || ! s->tokens || ! oldbase || ! newbase) return;

    if (oldbase == newbase) return;

    for (i = 0; i < s->count; i ++) {
        char *addr = newbase + (s->tokens[i].data - oldbase);

        if (addr <= (s->data + s->internal.capacity)) {
            s->tokens[i].data = addr;
        } else {
            unsigned int j = 0;

            /* clear all subsequent tokens */
            for (j = i; j < s->count; j ++)
                string_free_token(& s->tokens[j]);
            memset(& s->tokens[i], 0, (s->count - i) * sizeof(String));

            s->count = i;
            break;
        }

        /* this token is rebased, rebase its subtoken too */
        _string_rebase_token(& s->tokens[i], oldbase, newbase);
    }
}

/* -------------------------------------------------------------------------- */

static void _move_subtokens(String *token, int shift, const char *guardian)
{
    unsigned int i = 0;

    if (token) {
        /* recursively shift the token and its subtokens, if any */
        for (i = 0; i < token->count; i ++)
            _move_subtokens(& token->tokens[i], shift, guardian);
        if (! guardian || token->data > guardian)
            token->data += shift;
    }
}

/* -------------------------------------------------------------------------- */

static void _string_shift_token(String *s, unsigned int off,
                                unsigned int end, int shift)
{
    unsigned int i = 0, j = 0;

    if (! s || ! s->count || ! shift) {
        debug("_string_shift_token(): bad parameters.\n");
        return;
    }

    /* find the beginning of the shift */
    for (i = 0; i < s->count; i ++) {
        if (s->tokens[i].data >= s->data + off) {
            /* update the position of all subsequent tokens */
            for (j = i; j < s->count; j ++) {
                if (j == i && shift < 0)
                    _move_subtokens(& s->tokens[j], shift, s->data + off + end);
                else
                    _move_subtokens(& s->tokens[j], shift, NULL);
            }

            s = & s->tokens[i];
            do {
                if ( (int) s->len + shift < 0 || (int) s->internal.capacity + shift < 0) {
                    String *parent = s->parent;
                    /* check parent's tokens bound */
                    for (i = 0; i < parent->count; i ++) {
                        if (string_end(& parent->tokens[i]) >= string_end(parent)) {
                            while ( (-- parent->count) > i + 1)
                                string_free_token(& parent->tokens[parent->count]);
                            break;
                        }
                    }
                }
            } while (s->count && (s = & s->tokens[s->count - 1]) );

            break;
        }
    }
}

/* -------------------------------------------------------------------------- */

ASKL_API int string_resize(String *string, size_t size)
{
    /** @brief force resize the internal buffer of a string */

    char *data = NULL, *base = NULL;
    String *token = NULL;
    unsigned int off = 0, len = 0;
    int diff = 0, need = 0;
    int32_t allocsize = 0;

    /* sanity checks */
    if (! string || ! size) {
        debug("string_resize(): bad parameters.\n");
        return -1;
    }

    if (string->internal.capacity == size + sizeof(wchar_t)) {
        debug("string_resize(): correctly sized.\n");
        return 0;
    }

    /* XXX "need" is the number of bytes that should be added or removed
       to fit the new string size. "diff" is the difference between the
       new and the previous length of the string content. */
    need = size - string->internal.capacity; diff = size - string->len;

    /* if this string is a token, we need to resize its parent instead */
    if (string->parent) {
        token = string;
        while (string->parent) string = string->parent;
        /* store the token relative position */
        off = token->data - string->data; len = token->len;
    }

    /* check if the resize makes sense */
    if ( (string->internal.flags & _STRING_ENCAPSULATED && need > 0) ||
         (need < 0 && (uint32_t) (-need) > string->internal.capacity)) {
        debug("string_resize(): illegal resize attempt.\n");
        return -1;
    }

    /* if a token is shrunk the data need to be moved beforehand */
    if (token && token != last_token(string) && need < 0) {
        data = string->data + off + len;
        memmove(data + diff, data, string->len - (off + len));
        _string_shift_token(string, off, len, diff);
    }

    /* resize the internal buffer, ensure the buffer is on wchar_t boundary */
    if (need > 0 && ~string->internal.flags & _STRING_STATIC_ALLOC) {
        /* a string is never actually shrunk, only realloc to expand */
        allocsize = (string->internal.capacity + need + sizeof(wchar_t)) * sizeof(*data);
        if ((size_t) allocsize < string->len) {
            debug("string_resize(): integer overflow.\n");
            return -1;
        }

        base = string->data;

        if (! (data = realloc(string->data, allocsize)) ) {
            perror(ERR(string_resize, realloc)); return -1;
        }

        string->data = data; string->internal.capacity = allocsize;
        _string_rebase_token(string, base, data);
    }

    /* if the string was a token, move the data along */
    if (token) {
        if (diff > 0) {
            data = string->data + off + len;
            memmove(data + diff, data, string->len - (off + len));
            /* XXX only move subsequent tokens */
            _string_shift_token(string, off + len + 1, 0, diff);
        }

        token->len = token->internal.capacity = size;

        /* update the parents' size */
        while ( (token = token->parent) ) {
            token->len += diff;
            if (token != string) token->internal.capacity = token->len;
        }
    } else if (diff < 0) string->len += diff;

    /* zero out the remainder of the buffer */
    if (string->len < string->internal.capacity)
        string->data[string->len] = '\0';

    return 0;
}

/* -------------------------------------------------------------------------- */

ASKL_API int string_extend(String *string, size_t size)
{
    /** @brief resize a string only if the new size is greater */

    if (! string || ! size) {
        debug("string_extend(): bad parameters.\n");
        return -1;
    }

    return (size >= string->internal.capacity) ? string_resize(string, size) : 0;
}

/* -------------------------------------------------------------------------- */

ASKL_API int string_shrink(String *string, size_t size)
{
    /** @brief resize a string only if the new size is lower */

    if (! string || ! size) {
        debug("string_shrink(): bad parameters.\n");
        return -1;
    }

    return (size < string->internal.capacity) ? string_resize(string, size) : 0;
}

/* -------------------------------------------------------------------------- */

static String *_splice(String *to, off_t o, const char * from, size_t l)
{
    String *parent = to;
    int within = 0, alloc = 0;
    ptrdiff_t within_offset = 0;

    /* noop */
    if (l == 0) return to;

    /* sanity checks */
    if (o < 0 || ! from) {
        debug("string_splice(): bad parameters.\n");
        goto _error;
    }

    /* if the destination string does not exist, allocate it */
    if (! to) {
        if (! (to = string_alloc(NULL, l)) ) {
            debug("string_splice(): allocation failure.\n");
            goto _error;
        } else alloc = 1;
    } else while (parent->parent) parent = parent->parent;

    /* check if the source is inside the destination */
    if ( (from >= parent->data) && (from < string_end(parent)) )
        within = 1, within_offset = from - parent->data;

    /* reject out of bound offsets and resize the destination string */
    if (string_extend(to, o + l) == -1) {
        debug("string_splice(): resize failure.\n");
        goto _error;
    } else if (within) {
        /* if the source was within destination, correct the pointer */
        from = parent->data + within_offset;

        /* check if the offset is still in bound */
        if ( (from < parent->data) || (from >= string_end(parent)) ) {
            debug("string_splice(): offset out of bound.\n");
            goto _error;
        }
    }

    memmove(to->data + o, from, l);

    /* update the string */
    to->len = (to->len < o + l) ? o + l : to->len;
    if (! to->parent)
        memset(to->data + to->len, 0x0, to->internal.capacity - to->len);

    /* NOTE it is up to the caller to deal with the tokens */

    return to;

_error:
    if (alloc) string_free(to);
    return NULL;
}

/* -------------------------------------------------------------------------- */

ASKL_API String *string_splice(String *to, off_t o, const char *from, size_t l)
{
    /** @brief move a C string or a part of it into a String */

    String *ret = NULL;
    unsigned int i = 0, deleted = 0;

    if ( (ret = _splice(to, o, from, l)) ) {
        if (o) {
            /* preserve tokens before the offset */
            for (i = 0; i < ret->count; i ++) {
                if (string_end(& ret->tokens[i]) >= ret->data + o) {
                    string_free_token(& ret->tokens[i]);
                    deleted ++;
                }
            }
            ret->count -= deleted;
        } else string_free_token(ret);
    }

    return ret;
}

/* -------------------------------------------------------------------------- */

static void _update_size(String *s, ssize_t diff)
{
    while (s) {
        s->len += diff;
        if (s->parent)
            s->internal.capacity = s->len;
        else if (s->internal.capacity > s->len)
            s->data[s->len] = '\0';
        s = s->parent;
    }
}

/* -------------------------------------------------------------------------- */

ASKL_API int string_cut(String *string, off_t offset, size_t length, char *out)
{
    /** @brief remove a subsection of a string */
    unsigned int i = 0, j = 0;
    off_t off = 0, end = 0;
    int within = -1, after = -1, straddling = 0;
    String *parent = string, *top = NULL, *last = NULL;

    if (! string || offset < 0 || ! length || offset + length > string->len) {
        debug("string_cut(): bad parameters.\n");
        return -1;
    }

    /* get the parent */
    while (parent->parent) parent = parent->parent;

    off = offset + string->data - parent->data;
    end = off + length;
    top = parent;

    /* copy the subsection */
    if (out) memmove(out, parent->data + off, length);

    /* if the subsection is actually the last token, cut to the chase */
    if (parent->count && (last_token(parent)->data == parent->data + off)) {
        if (likely(string_end(last_token(parent)) == parent->data + end))
            goto _update_length;
    }

    /* find the first tokens within or after the deleted subsection */
    do {
        straddling = 0;
        for (i = 0; i < top->count; i ++) {
            off_t token_off, token_end;
            token_off = top->tokens[i].data - parent->data;
            token_end = string_end(& top->tokens[i]) - parent->data;

            if (within == -1 && token_off >= off && token_off < end) {
                /* found the first token within the subsection */
                if (token_end > end) {
                    /* found a straddling token, check subtokens instead */
                    top = & top->tokens[i];
                    straddling = 1;
                    break;
                } else within = i;
            } else if (after == -1 && token_off >= end) {
                /* found the first token after the subsection */
                after = i; break;
            }
        }
    } while (straddling);

    if (! _splice(parent, off, parent->data + end, parent->len + 1 - end))
        return -1;

    if (within > 0) {
        /* truncate the last tokens before the subsection, if any */
        for (last = & top->tokens[within - 1]; last->count; last = last_token(last)) {
            int diff = parent->data + off - string_end(last_token(last));
            if (diff > 0) {
                last->len -= diff;
                last->internal.capacity -= diff;
            }
        }
    }

    /* move the subsequent tokens, if any */
    if (after > 0) {
        for (i = after; i < top->count; i ++) {
            _move_subtokens(& top->tokens[i], -length, NULL);
            if (within != -1) {
                if (within < (int) i) {
                    string_free_token(& top->tokens[within]);
                    top->tokens[within] = top->tokens[i];
                    for (j = 0; j < top->tokens[within].count; j ++)
                        top->tokens(within, j).parent = & top->tokens[within];
                    within ++;
                } else within = -1;
            }
        }
    }

    /* update the subtoken count */
    if (within != -1 && after != -1) top->count = within;

_update_length:
    /* find the last subtoken aligned with the end of the parent string */
    for (last = top; last->count; last = last_token(last))
        if (string_end(last_token(last)) != string_end(parent)) break;

    /* truncate from that last subtoken up to the parent string */
    _update_size(last, -length);

    return 0;
}

/* -------------------------------------------------------------------------- */

ASKL_API String *string_append_buffer(String *to, const char *from, size_t len)
{
    /** @brief append a C string to a String */

    String *orig = to;

    /* find the last token first */
    if (to) {
        while (to->parent) to = to->parent;
        while (to->count) {
            if (string_end(to) == string_end(last_token(to))) {
                if (last_token(to)->internal.flags & _STRING_VALIDATED) break;
                to = last_token(to);
            } else break;
        }
    }

    return (to = string_splice(to, (to) ? to->len : 0, from, len)) ?
           ((orig) ? orig : to) :
           NULL;
}

/* -------------------------------------------------------------------------- */

ASKL_API String *string_prepend_buffer(String *to, const char *from, size_t len)
{
    /** @brief prepend a C string to a String */

    /* move the original string to the right */
    if (to && ! (string_splice(to, len, to->data, to->len)) ) return NULL;

    return string_splice(to, 0, from, len);
}

/* -------------------------------------------------------------------------- */

ASKL_API int string_compare_buffer(const String *a, const char *b, size_t len)
{
    /** @brief compare a String and a C string */

    if (! a || ! a->data || ! b || ! len) {
        debug("string_compare_buffer(): bad parameters.\n");
        return 2;
    }

    return memcmp(a->data, b, MIN(a->len, len));
}

/* -------------------------------------------------------------------------- */

ASKL_API String *string_append(String *to, const String *from)
{
    /** @brief append a string to another */

    if (! from) return NULL;

    return string_append_buffer(to, from->data, from->len);
}

/* -------------------------------------------------------------------------- */

ASKL_API String *string_prepend(String *to, const String *from)
{
    /** @brief prepend a string to another */

    if (! from) return NULL;

    return string_prepend_buffer(to, from->data, from->len);
}

/* -------------------------------------------------------------------------- */

ASKL_API int string_compare(const String *a, const String *b)
{
    /** @brief compare two strings */

    if (! b) return 2;

    return string_compare_buffer(a, b->data, b->len);
}

/* -------------------------------------------------------------------------- */

ASKL_API int string_upper(String *string)
{
    /** @brief convert an ASCII string to upper case */

    unsigned int i = 0;

    if (! string || ! string->data) {
        debug("string_upper(): bad parameters.\n");
        return -1;
    }

    /* check if writing to the string is allowed */
    if (string->internal.flags & _STRING_READ_ONLY) {
        debug("string_upper(): illegal write attempt.\n");
        return -1;
    }

    for (i = 0; i < string->len && string->data[i]; i ++)
        string->data[i] = toupper(string->data[i]);

    /* since it is an in place transformation, keep existings tokens */

    return 0;
}

/* -------------------------------------------------------------------------- */

ASKL_API int string_lower(String *string)
{
    /** @brief convert an ASCII string to lower case */

    unsigned int i = 0;

    if (! string || ! string->data) {
        debug("string_lower(): bad parameters.\n");
        return -1;
    }

    /* check if writing to the string is allowed */
    if (string->internal.flags & _STRING_READ_ONLY) {
        debug("string_lower(): illegal write attempt.\n");
        return -1;
    }

    for (i = 0; i < string->len && string->data[i]; i ++)
        string->data[i] = tolower(string->data[i]);

    /* since it is an in place transformation, keep existings tokens */

    return 0;
}

/* -------------------------------------------------------------------------- */

ASKL_API int string_pattern_compile(String_Pattern *p, const char *s, size_t l)
{
    /** @brief compute the Boyer-Moore lookup table for the given substring */

    unsigned int i = 0;

    if (! p || ! s || ! l || l > 255) {
        debug("string_pattern_compile(): bad parameters.\n");
        return -1;
    }

    if (l <= 4) {
        /* pattern is too short */
        p->_len = l - 1;
        return 0;
    }

    /* register the distance between each possible byte and the last byte
       of the substring in a lookup table */
    memset(p->_lut, l, UCHAR_MAX + 1);

    for (l --, i = 0; i < l; i ++) p->_lut[_UINT(s[i])] = l - i;

    /* get the shift length and set up the guard char */
    p->_shift = p->_lut[_UINT(s[l])];
    p->_lut[_UINT(s[l])] = 0; p->_len = l;

    return 0;
}

/* -------------------------------------------------------------------------- */

ASKL_API off_t string_find_pattern(
    const String *s,
    size_t o,
    const char *sub,
    String_Pattern *p
)
{
    /** @brief find a C substring within a string using a precompiled LUT */

    size_t i = 0, k = 0;
    const char *ptr = NULL;

    if (! s || ! sub || ! p) {
        debug("string_find_pattern(): bad parameters.\n");
        return -1;
    }

    if (o + p->_len > s->len) {
        debug("string_find_pattern(): needle larger than haystack!\n");
        return -1;
    }

    if (p->_len < 4) {
        /* naive search algorithm is faster for short strings */
        uint32_t first_char = sub[0] * 0x01010101U;
        ptr = s->data + o;
        do {
            uint32_t bytes;
            memcpy(& bytes, ptr, sizeof(bytes));
            bytes ^= first_char;
            if ( (bytes = __zero(bytes)) ) {
                if ((ptr += __zero_idx(bytes)) + p->_len >= string_end(s))
                    return -1;
                switch (p->_len) {
                case 3: if (ptr[3] == sub[3])
                case 2: if (ptr[2] == sub[2])
                case 1: if (ptr[1] == sub[1])
                case 0: return ptr - s->data;
                }
                ptr ++; continue;
            }
            ptr += sizeof(bytes);
        } while (ptr < string_end(s));
        /* not found */
        return -1;
    }

    /* search loop (Tuned Boyer-Moore algorithm) */
    for (i = o; i < s->len; i += p->_shift) {
        /* use the lookup table to skip impossible matches */
        do {
            if ( (k = i + p->_len) >= s->len)
                return -1;
            k = p->_lut[_UINT(s->data[k])];
        } while ( (k != 0) && (i += k) );

        /* try a possible match */
        if (! memcmp(sub, s->data + i, p->_len)) break;
    }

    return (i >= s->len) ? -1 : (off_t) i;
}

/* -------------------------------------------------------------------------- */

ASKL_API off_t string_find(
    const String *str,
    size_t o,
    const char *sub,
    size_t len
)
{
    String_Pattern p;

    if (! str || ! str->data || ! sub || ! len) return -1;

    if (string_pattern_compile(& p, sub, len) == -1) return -1;

    return string_find_pattern(str, o, sub, & p);
}

/* -------------------------------------------------------------------------- */

ASKL_API int string_split(String *s, const char *pattern, size_t len)
{
    /** @brief split the string into multiple tokens using a separator */

    off_t offset[16] = { -1 }, off = 0, prev = 0, o = 0, p = 0;
    unsigned int i = 0, j = 0;
    String *t = NULL;
    String_Pattern compiled_pattern;

    if (! s || ! s->data || ! pattern || ! len) {
        debug("string_split(): bad parameters.\n");
        return -1;
    }

    /* the pattern delimiter cannot be longer than the source */
    if (s->len <= len + 1) {
        debug("string_split(): delimiter out of bound.\n");
        return -1;
    }

    if (string_pattern_compile(& compiled_pattern, pattern, len) == -1)
        return -1;

    if (s->internal.tokens_capacity) {
        /* try to reuse existing tokens */
        for (i = 0; i < s->count; i ++)
            string_free_token(& s->tokens[i]);
        s->count = 0; t = s->tokens;
    }

    /* fill the offsets array with the locations of the pattern */
    while (off != -1) {
        off = string_find_pattern(s, prev, pattern, & compiled_pattern);

        offset[j ++] = prev = off; prev += len;

        if (off == -1 || j >= 16) {
            /* allocate enough room for new tokens */
            if (s->internal.tokens_capacity < s->count + j) {
                if (! (t = realloc(s->tokens, (s->count + j) * sizeof(*t))) )
                    goto _err_realloc;
                s->tokens = t; s->internal.tokens_capacity = s->count + j;
            }

            if (s->count) p = t[s->count - 1].data - s->data;

            /* write the tokens */
            for (i = s->count; i < s->count + j; i ++, p = o + len) {
                /* the token inherit parent's flags and set the "no free" bit */
                o = offset[i - s->count];
                t[i].parent = s;
                t[i].internal.flags = s->internal.flags | _STRING_DISABLE_FREE;
                t[i].data = s->data + p;
                t[i].len = (o != -1) ? (size_t) o : s->len;
                t[i].len -= p;
                t[i].internal.capacity = t[i].len;
                t[i].internal.tokens_capacity = t[i].count = 0;
                t[i].tokens = NULL;
            }

            j = 0; s->count = i;
        }
    }

    return 0;

_err_realloc:
    perror(ERR(string_split, realloc));
    string_free_token(s);
    return -1;
}

/* -------------------------------------------------------------------------- */

static void _bcpy(const char *src, char *dst, size_t len)
{
    size_t c = 0;

    src += len;
    dst += len;

    c = len >> 2;

    if (c) do {
        src -= 4; dst -= 4;
        *(unaligned_uint32_t *) dst = *(unaligned_uint32_t *) src;
    } while (-- c);

    c = len & 0x3;

    if (c) do { *-- dst = *-- src; } while (-- c);
}

/* -------------------------------------------------------------------------- */

ASKL_API int string_merge(String *string, const char *pattern, size_t len)
{
    /** @brief replaces separators in a split string by a new separator */
    unsigned int i = 0, new_size = 0;
    int p = 0, diff = 0;
    unsigned int last = 0, known_good = 0;

    if (! string || ! string->data) {
        debug("string_merge(): bad parameters.\n");
        return -1;
    }

    /* allow NULL pattern only if a 0 length is specified */
    if (! pattern && len) {
        debug("string_merge(): NULL pattern with non-0 length.\n");
        return -1;
    }

    /* at least 2 tokens are required to merge anything */
    if (! string->tokens || string->count < 2) {
        debug("string_merge(): not enough tokens.\n");
        return -1;
    }

    /* get the size difference with the previous delimiter length */
    for (i = 0; i < string->count - 1; i ++) {
        diff = len - (string->tokens[i + 1].data - string_end(& string->tokens[i]));
        if (! (p += diff) && ! diff) {
            known_good = i + 1;
            if (likely(len == 1))
                *((char *) string_end(& string->tokens[i])) = *pattern;
            else memcpy((char *) string_end(& string->tokens[i]), pattern, len);
        }
    }

    if (known_good == i) return 0;

    new_size = string->len + p;

    if (p > 0) {
        string_extend(string, new_size);

        /* move every token starting from the end */
        for (last = string->count - 1; last -- > known_good; p -= diff) {
            diff = len - (string->tokens[last + 1].data - string_end(& string->tokens[last]));

            _bcpy(
                string->tokens[last + 1].data,
                (char *) string->tokens[last + 1].data + p,
                string->tokens[last + 1].len
            );

            _move_subtokens(& string->tokens[last + 1], p, NULL);

            if (likely(len == 1))
                *((char *) string->tokens[last + 1].data - len) = *pattern;
            else memcpy((char *) string->tokens[last + 1].data - len, pattern, len);
        }

        _update_size(string, new_size - string->len);
    } else {
        /* move every token from the start */
        for (i = known_good; i < string->count - 1; i ++) {
            diff = len - (string->tokens[i + 1].data - string_end(& string->tokens[i]));

            memmove(
                (char *) string->tokens[i + 1].data + diff,
                string->tokens[i + 1].data,
                string->tokens[i + 1].len
            );

            _move_subtokens(& string->tokens[i + 1], diff, NULL);

            if (likely(len == 1))
                *((char *) string->tokens[i + 1].data - len) = *pattern;
            else memcpy((char *) string->tokens[i + 1].data - len, pattern, len);
        }

        string_resize(string, new_size);
    }

    return 0;
}

/* -------------------------------------------------------------------------- */

ASKL_API String *string_replace_all(
    String *string,
    const char *search,
    size_t slen,
    const char *rep,
    size_t rlen
)
{
    /** @brief replace all occurences of a substring in a string */

    size_t count = 0;
    off_t *offset = NULL, off = 0, src = 0, dst = 0;
    unsigned int i = 0;

    /* a NULL replacement string is allowed (deletion) */
    if (! string || ! string->data || ! search || ! slen) {
        debug("string_replace_all(): bad parameters.\n");
        return NULL;
    }

    /* we can't replace a substring longer than the source string */
    if (string->len <= slen + 1) {
        debug("string_replace_all(): search string out of bound.\n");
        return NULL;
    }

    /* find the maximal number of replacement and allocate the offset array */
    count = string->len / slen + 1;

    if (! (offset = malloc(count * sizeof(*offset))) ) {
        perror(ERR(string_reps, malloc));
        return NULL;
    }

    /* fill the offset array with the location of the search strings */
    while ( (offset[i] = string_find(string, off, search, slen)) >= 0) {
        off = offset[i] + slen; if (i < count) i ++; else break;
    }

    if ( (count = i) == 0) {
        debug("string_replace_all(): search string not found.\n");
        goto _err_nfnd;
    }

    /* XXX ensure the string has enough room for the replacement
       we could rely on string_splice for the resizing, but we really don't
       want subsequent calls to string_splice() to fail with a possible
       out-of-memory error and end up with a garbled string */
    if (string_extend(string, string->len + count * (rlen - slen)) == -1) {
        debug("string_replace_all(): resize failure.\n");
        goto _err_size;
    }

    /* perform the replacement */
    for (i = 0; i < count; i ++) {
        /* copy the data between the strings that are going to be replaced */
        if (! string_splice(string, dst, string->data + src, offset[i] - src))
            goto _err_move;
        dst += offset[i] - src; src = offset[i] + slen;

        /* loop no further without proper replacement string */
        if (! rep || ! rlen) continue;

        /* replace the string */
        if (! string_splice(string, dst, rep, rlen)) goto _err_move;
        dst += rlen;
    }

    _update_size(string, count * (rlen - slen));

    /* ensure the string is NUL terminated */
    if (! string->parent) string->data[string->len] = '\0';

    free(offset);

    return string;

_err_move: /* this should never happen */
    debug("string_replace_all(): string_splice() failed !\n");
_err_size: /* string_extend() failure */
_err_nfnd: /* not found */
    free(offset);
    return NULL;
}

/* -------------------------------------------------------------------------- */

ASKL_API String *string_remove_all(String *str, const char *rem, size_t len)
{
    /** @brief remove all occurences of a C substring in a string */

    if (! str || ! rem || ! len) {
        debug("string_remove_all(): bad parameters.\n");
        return NULL;
    }

    return string_replace_all(str, rem, len, NULL, 0);
}

/* -------------------------------------------------------------------------- */

ASKL_API String *string_select(String *string, unsigned int off, size_t len)
{
    /** @brief create a string token encompassing the given part of string */

    if (! string || ! len) {
        debug("string_select(): bad parameters.\n");
        return NULL;
    }

    string_free_token(string);

    return string_add_token(string, off, off + len);
}

/* -------------------------------------------------------------------------- */

ASKL_API String *string_add_token(String *s, off_t start, off_t end)
{
    /** @brief adds a new token to the string, selecting the delimited part */

    String *tokens = NULL, *token = NULL;
    unsigned int i = 0, j = 0;
    int p = 0;

    if (unlikely(! s || s->count == 65535)) {
        debug("string_add_token(): cannot add token.\n");
        return NULL;
    }

    #ifdef DEBUG
    if (! s->data || (size_t) end > s->len || end <= start) {
        debug("string_add_token(): bad parameters.\n");
        return NULL;
    }
    #endif

    if (s->internal.tokens_capacity < s->count + 1) {
        /* 1.5 growth factor */
        if (s->internal.tokens_capacity < 43690)
            p = (s->internal.tokens_capacity >> 1) + ! (s->internal.tokens_capacity >> 1);
        else p = 65535 - s->internal.tokens_capacity;

        tokens = realloc(s->tokens, (s->internal.tokens_capacity + p) * sizeof(*tokens));
        if (unlikely(! tokens)) {
            perror(ERR(string_add_token, realloc));
            return NULL;
        }

        /* XXX update all the subtokens' pointers */
        if (unlikely(tokens != s->tokens)) {
            for (i = 0; i < s->count; i ++) {
                for (j = 0; j < tokens[i].count; j ++)
                    tokens[i].tokens[j].parent = & tokens[i];
            }
            s->tokens = tokens;
        }

        s->internal.tokens_capacity += p;
    }

    s->count ++; token = last_token(s);

    /* the token inherits the parent's flags and adds the "no free" bit */
    token->parent = s;
    token->internal.flags = s->internal.flags | _STRING_DISABLE_FREE;
    token->data = s->data + start;
    token->internal.capacity = token->len = end - start;
    token->internal.tokens_capacity = token->count = 0;
    token->tokens = NULL;

    return token;
}

/* -------------------------------------------------------------------------- */

ASKL_API int string_push_token(String *s, const char *strtoken, size_t len)
{
    size_t slen = 0;

    if (! s || ! strtoken || ! len) {
        debug("string_push_token(): bad parameters.\n");
        return -1;
    }

    if (s->count)
        slen = last_token(s)->data - s->data + last_token(s)->len;
    else slen = 0;

    /* make room for the new data */
    if (string_extend(s, slen + len) == -1) return -1;

    /* copy the data */
    memcpy(s->data + slen, strtoken, len);
    s->len = slen + len;
    if (s->internal.capacity > s->len)
        s->data[s->len] = '\0';

    /* append the token */
    if (! string_add_token(s, slen, slen + len)) return -1;

    return 0;
}

/* -------------------------------------------------------------------------- */

ASKL_API String *string_suppr_token(String *s, unsigned int i)
{
    if (! s || i >= s->count) {
        debug("string_suppr_token(): bad parameters.\n");
        return NULL;
    }

    if (string_cut(s, s->tokens[i].data - s->data, s->tokens[i].len, NULL) == -1)
        return NULL;

    if (i == -- s->count && ! s->parent) s->data[s->len] = '\0';

    return NULL;
}

/* -------------------------------------------------------------------------- */

ASKL_API String *string_pop_token(String *s)
{
    String *ret = NULL;

    if (! s || ! s->data || ! s->count || ! s->tokens[0].len) {
        debug("string_pop_token(): bad parameters.\n");
        return NULL;
    }

    if (! (ret = string_alloc(s->tokens[0].data, s->tokens[0].len)) )
        return NULL;

    string_cut(& s->tokens[0], 0, s->tokens[0].len, NULL);

    return ret;
}

/* -------------------------------------------------------------------------- */
#ifdef HAS_PCRE
/* -------------------------------------------------------------------------- */

ASKL_API int string_parse(String *string, const char *pattern, size_t len)
{
    /** @brief parses a string into multiple tokens matching a regex */

    int *off = NULL;
    unsigned int i = 0, j = 0, k = 0, last = 0, size = 0;
    String *token = NULL, *new_token = NULL;
    pcre *regex = NULL;
    int erroff = 0;
    int r = 0;
    const char *err = NULL;

    if (! string || ! string->data || ! pattern || ! len) {
        debug("string_parse(): bad parameters.\n");
        return -1;
    }

    if (! (regex = pcre_compile(pattern, PCRE_DOTALL, & err, & erroff, NULL)) ) {
        debug("string_parse(): %s\n", err);
        return -1;
    }

    size = SIZE(string);

    if (size * sizeof(*off) < size) {
        debug("string_parse(): integer overflow.\n");
        return -1;
    }

    if (! (off = malloc(size * sizeof(*off))) ) {
        perror(ERR(string_parse, malloc));
        goto _err_malloc;
    }

    for (i = 0, last = 0; ; last = off[1], i ++) {
        /* match the pattern */
        r = pcre_exec(regex, NULL, string->data, string->len, last, 0x0, off, size);

        if (r <= 0) {
            if (i) break;
            debug("string_parse(): error matching pattern %s\n", pattern);
            goto _err_exec;
        }

        /* found something, add a token and possibly subtokens */
        if (! (new_token = realloc(token, (i + 1) * sizeof(*token))) ) {
            perror(ERR(string_parse, realloc));
            if (token) while (i --) free(token[i].token); free(token);
            goto _err_token;
        }

        token = new_token;
        token[i].parent = string;
        token[i].internal.flags = string->internal.flags | _STRING_DISABLE_FREE;
        token[i].data = string->data + off[0];
        token[i].len = off[1] - off[0] + ! (off[1] - off[0]);
        token[i].internal.capacity = token[i].len;

        if (r - 1 < 1) {
            token[i].internal.tokens_capacity = token[i].count = 0;
            token[i].tokens = NULL;
            continue;
        }

        token[i].internal.tokens_capacity = token[i].count = r - 1;

        if (! (token[i].tokens = malloc(token[i].count * sizeof(token[i]))) ) {
            perror(ERR(string_parse, malloc));
            goto _err_token;
        }

        for (j = 2, k = 0; j + 1 < (unsigned) r * 2; j += 2, k ++) {
            token[i].tokens[k].parent = & token[i];
            token[i].tokens[k].internal.flags = string->internal.flags | _STRING_DISABLE_FREE;
            token[i].tokens[k].data = string->data + off[j];
            token[i].tokens[k].len = off[j + 1] - off[j] + ! (off[j + 1] - off[j]);
            token[i].tokens[k].internal.capacity = token[i].tokens[k].len;
            token[i].tokens[k].internal.tokens_capacity = token[i].tokens[k].count = 0;
            token[i].tokens[k].tokens = NULL;
        }
    }

    pcre_free(regex);

    free(off);

    string_free_token(string); string->count = i; string->tokens = token;

    return 0;

_err_token:
    if (token) while (i --) free(token[i].tokens); free(token);
_err_exec:
    free(off);
_err_malloc:
    pcre_free(regex);

    return -1;
}

/* -------------------------------------------------------------------------- */
#endif
/* -------------------------------------------------------------------------- */

static void _free_token(String *string)
{
    unsigned int i = 0;

    /* recursively clean the tokens' tokens, if any */
    for (i = 0; i < string->count; i ++) {
        if (likely(string->tokens[i].tokens))
            _free_token(string->tokens + i);
    }

    free(string->tokens);
}

ASKL_API void string_free_token(String *string)
{
    if (string && string->tokens) {
        _free_token(string);
        string->tokens = NULL;
        string->internal.tokens_capacity = string->count = 0;
    }
}

/* -------------------------------------------------------------------------- */

ASKL_API void string_api_cleanup(void)
{
    return;
}

/* -------------------------------------------------------------------------- */
