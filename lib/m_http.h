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

#ifndef M_HTTP_H

#define M_HTTP_H

#ifdef _ENABLE_HTTP

#include "askl.h"
#include "m_string.h"
#include "m_file.h"
#include "askl_random.h"

/* -------------------------------------------------------------------------- */

#define HTTP_BUFFER 65535

typedef struct _m_http {
    m_string *_headers;
    char *name;
    char *value;
    size_t len;
    #ifdef _ENABLE_FILE
    m_file *file;
    #endif
    off_t offset;
    size_t length;
    struct _m_http *prev;
    struct _m_http *next;
} m_http;

#define HTTP_REPLY   0x0
#define HTTP_OPTIONS 0x1
#define HTTP_GET     0x2
#define HTTP_HEAD    0x3
#define HTTP_POST    0x4
#define HTTP_PUT     0x5
#define HTTP_DELETE  0x6
#define HTTP_TRACE   0x7
#define HTTP_CONNECT 0x8

#define HTTP_METHOD(s) (((s)->_flags & 0xf000) >> 12)

/* -------------------------------------------------------------------------- */

public m_http *http_open(void);

/* -------------------------------------------------------------------------- */

public int http_set_header(m_http *h, const char *header, const char *value);

/* -------------------------------------------------------------------------- */

public int http_set_cookie(m_http *h, const char *cookie, const char *value);

/* -------------------------------------------------------------------------- */

public m_http *http_set_var(m_http *h, const char *k, const char *v, size_t l);

/* -------------------------------------------------------------------------- */

public m_http *http_set_fmt(m_http *h, const char *k, const char *fmt, ...);

/* -------------------------------------------------------------------------- */
#ifdef _ENABLE_FILE
/* -------------------------------------------------------------------------- */

public m_http *http_set_file(m_http *h, const char *k, const char *filename,
                             m_file *f, off_t off, size_t len);

/* -------------------------------------------------------------------------- */
#endif
/* -------------------------------------------------------------------------- */

public m_string *http_compile(m_http *h, int method, const char *action,
                              const char *host, size_t inline_file_size);

/* -------------------------------------------------------------------------- */

public m_string *http_format(m_http *h, int method, const char *action,
                             const char *host);

/* -------------------------------------------------------------------------- */

public m_string *http_get_request(int *r, m_string **buf, m_string *input);

/* -------------------------------------------------------------------------- */

public const char *http_get_header(m_string *buffer, const char *name);

/* -------------------------------------------------------------------------- */

public uint32_t http_get_contentlength(m_string *buffer);

/* -------------------------------------------------------------------------- */

public uint32_t http_get_progress(m_string *buffer);

/* -------------------------------------------------------------------------- */

public int http_set_progress(m_string *buffer, uint32_t progress);

/* -------------------------------------------------------------------------- */

public int http_inc_progress(m_string *buffer, uint32_t progress);

/* -------------------------------------------------------------------------- */

public m_http *http_close(m_http *h);

/* -------------------------------------------------------------------------- */

/* _ENABLE_HTTP */
#endif

#endif
