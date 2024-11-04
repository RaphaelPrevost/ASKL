/*******************************************************************************
 *  Concrete Server                                                            *
 *  Copyright (c) 2005-2023 Raphael Prevost <raph@el.bzh>                      *
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

#include "m_http.h"

/* -------------------------------------------------------------------------- */
#ifdef _ENABLE_HTTP
/* -------------------------------------------------------------------------- */

static m_search_string c = {
    3, 4,
    { 0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,
      0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,
      0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,
      0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,
      0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x3,0x4,0x4,0x4,0x4,0x4,0x4,0x4,
      0x0,0x4,0x4,0x4,0x1,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,
      0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,
      0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,
      0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,
      0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,
      0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,
      0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,
      0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,
      0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,
      0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,
      0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4
    }
};

static char *http_method[9] = { "\0",
                                "\7OPTIONS",
                                "\3GET",
                                "\4HEAD",
                                "\4POST",
                                "\3PUT",
                                "\6DELETE",
                                "\5TRACE",
                                "\7CONNECT"
                              };

#define HTTP_SET_METHOD(s, m) \
do { (s)->_flags |= ((m) << 12) & 0xf000; } while(0)

static unsigned char http_matrix[26][26] = {
/*      A B C D E F G H I J K L M N O P Q R S T U V W X Y Z */
/*A*/ { 0,0,0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
/*B*/ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
/*C*/ { 0,0,0,0,7,0,0,0,0,0,0,0,0,0,8,0,0,0,0,8,0,0,0,0,0,0 },
/*D*/ { 0,0,0,0,6,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
/*E*/ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0 },
/*F*/ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
/*G*/ { 0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
/*H*/ { 0,0,0,0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
/*I*/ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
/*J*/ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
/*K*/ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
/*L*/ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
/*M*/ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
/*N*/ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0 },
/*O*/ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0 },
/*P*/ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,4,0,0,0,0,0,5,0,0,0,0,0 },
/*Q*/ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
/*R*/ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
/*S*/ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4,0,0,0,0,0,0 },
/*T*/ { 0,0,0,0,6,0,0,0,0,0,0,0,0,0,0,0,0,7,0,0,0,0,0,0,0,0 },
/*U*/ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5,0,0,0,0,0,0 },
/*V*/ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
/*W*/ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
/*X*/ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
/*Y*/ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
/*Z*/ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 }
};

/* -------------------------------------------------------------------------- */

public m_http *http_open(void)
{
    m_http *h = malloc(sizeof(*h));

    if (! h) {
        perror(ERR(http_open, malloc));
        return NULL;
    }

    h->_headers = NULL;
    h->name = h->value = NULL;
    h->len = 0;

    #ifdef _ENABLE_FILE
    h->file = NULL;
    #endif
    h->offset = 0;
    h->length = 0;

    h->prev = h->next = NULL;

    return h;
}

/* -------------------------------------------------------------------------- */

public int http_set_header(m_http *h, const char *header, const char *value)
{
    if (! h || ! header || ! value) {
        debug("http_set_header(): bad parameters.\n");
        return -1;
    }

    h->_headers = string_catfmt(h->_headers, "%s: %s\r\n", header, value);
    if (! h->_headers) return -1;

    return 0;
}

/* -------------------------------------------------------------------------- */

public m_http *http_set_var(m_http *h, const char *k, const char *v, size_t l)
{
    m_http *new_part = NULL;

    if (! h || ! k) {
        debug("http_set_var(): bad parameters.\n");
        return h;
    }

    /* only insert at the end of the list */
    while (h->next) h = h->next;

    if (h->name ||
        #ifdef _ENABLE_FILE
        h->file ||
        #endif
        ! h->prev) {
        if (! (new_part = malloc(sizeof(*new_part))) ) {
            perror(ERR(http_set_var, malloc));
            return h;
        }

        /* initialize the structure */
        new_part->_headers = NULL;
        new_part->prev = h;
        h->next = new_part;
        new_part->next = NULL;

        #ifdef _ENABLE_FILE
        /* no file */
        new_part->file = NULL;
        new_part->offset = 0;
        new_part->length = 0;
        #endif

        h = new_part;
    }

    h->name = string_dups(k, strlen(k));
    h->value = string_dups(v, l);
    h->len = l;

    return h;
}

/* -------------------------------------------------------------------------- */

public m_http *http_set_fmt(m_http *h, const char *k, const char *fmt, ...)
{
    va_list argv;
    char buffer[65535];
    int len = 0;

    va_start(argv, fmt);

    if ( (len = m_vsnprintf(buffer, sizeof(buffer), fmt, argv)) == -1)
        debug("http_set_fmt(): cannot format the variable.\n");

    va_end(argv);

    return (len > 0) ? http_set_var(h, k, buffer, len) : h;
}

/* -------------------------------------------------------------------------- */
#ifdef _ENABLE_FILE
/* -------------------------------------------------------------------------- */

public m_http *http_set_file(m_http *h, const char *k, const char *filename,
                             m_file *f, off_t off, size_t len)
{
    m_http *new_part = NULL;
    m_file *file = NULL;

    if (! h || ! k || ! filename || ! f) {
        debug("http_set_file(): bad parameters.\n");
        return h;
    }

    if (! (file = fs_reopenfile(f)) ) {
        debug("http_set_file(): couldn't reopen the file.\n");
        return h;
    }

    /* only insert at the end of the list */
    while (h->next) h = h->next;

    if (h->name || h->file || ! h->prev) {
        if (! (new_part = malloc(sizeof(*new_part))) ) {
            perror(ERR(http_set_var, malloc));
            return h;
        }

        /* initialize the structure */
        new_part->_headers = NULL;
        new_part->prev = h;
        h->next = new_part;
        new_part->next = NULL;

        new_part->file = file;
        new_part->offset = off;
        new_part->length = len;

        h = new_part;
    }

    h->name = string_dups(k, strlen(k));
    h->len = strlen(filename);
    h->value = string_dups(filename, h->len);

    return h;
}

/* -------------------------------------------------------------------------- */
#endif
/* -------------------------------------------------------------------------- */

public m_string *http_compile(m_http *h, int method, const char *action,
                              const char *host, size_t inline_file_size)
{
    m_string *req = NULL, *postdata = NULL;
    m_http *head = NULL;
    size_t addlen = 0;
    #ifdef _ENABLE_FILE
    size_t len = 0;
    int i = 0;
    uint32_t random[10];
    m_random *ctx = NULL;
    #endif
    m_string *token = NULL, *multipart = NULL;

    if (! h || ! method || ! action || ! host) {
        debug("http_format(): bad parameters.\n");
        return NULL;
    }

    /* go to the last part */
    while (h->next) h = h->next;

    /* if there is file attachments, there is no choice but to compose
       a multipart request; however, if there is only regular form values,
       it's better to send a shorter x-www-form-urlencoded request */
    while (h->prev) {
        #ifdef _ENABLE_FILE
        if (h->file && ! multipart) {
            /* generate a random 320 bits boundary token */
            ctx = random_arrayinit((uint32_t *) h, sizeof(*h));
            for (i = 0; i < 10; i ++) random[i] = random_uint32(ctx);
            ctx = random_free(ctx);

            token = string_b58s((char *) random, sizeof(random));

            /* prepare the header */
            multipart = string_fmt(NULL,
                                   "Content-Type: multipart/form-data; "
                                   "boundary=%.*s\r\n",
                                   (int) SIZE(token), DATA(token));
        }
        #endif
        h = h->prev;
    }

    head = h; if (head->next) h = head->next;

    if (method == HTTP_POST && multipart) do {

        if (! h->name) break;

        #ifdef _ENABLE_FILE
        if (! h->file) {
        #endif
            #define BOUNDARY                                  \
            "--%.*s\r\n"                                      \
            "Content-Disposition: form-data; name=\"%s\"\r\n"

            /* regular value */
            postdata = string_catfmt(postdata, BOUNDARY,
                                     (int) SIZE(token),
                                     DATA(token), h->name);
            /* append local headers */
            if (h->prev && h->_headers)
                string_cat(postdata, h->_headers);
            /* append CRLF */
            string_cats(postdata, "\r\n", 2);
            /* append the value */
            string_cats(postdata, h->value, h->len);
            /* append final CRLF */
            string_cats(postdata, "\r\n", 2);

            #undef BOUNDARY
        #ifdef _ENABLE_FILE
        } else {
            #define BOUNDARY                                              \
            "--%.*s\r\n"                                                  \
            "Content-Disposition: form-data; name=\"%s\"; filename=\"%s\"\r\n"

            /* file */
            postdata = string_catfmt(postdata, BOUNDARY,
                                     (int) SIZE(token),
                                     DATA(token), h->name, h->value);
            /* append local headers */
            if (h->prev && h->_headers)
                string_cat(postdata, h->_headers);
            /* append CRLF */
            string_cats(postdata, "\r\n", 2);
            /* append file data */
            len = (h->length == 0) ? h->file->len - h->offset : h->length;

            /* inline_file_size limits the size of the file data to be
               embedded directy in the request. if the file is too big,
               a placeholder will be used */
            if (len < 1/*inline_file_size*/) {
                /* we are done with the file, release it */
                h->file = fs_closefile(h->file);
            } else {
                addlen += len - strlen("{FILE}");
                string_cats(postdata, "{FILE}", strlen("{FILE}"));
            }
            /* append final CRLF */
            string_cats(postdata, "\r\n", 2);

            #undef BOUNDARY
        }
        #endif

        if (h->next) h = h->next; else {
            /* finalize the multipart postdata */
            string_catfmt(postdata, "--%.*s--\r\n\r\n",
                          (int) SIZE(token), DATA(token));
            break;
        }

    } while (1); else do {

        if (! h->name) break;
        postdata = string_catfmt(postdata, "%s=%url", h->name, h->value);
        if (h->next) {
            postdata = string_cats(postdata, "&", 1);
            h = h->next;
        } else break;

    } while (1);

    if (method == HTTP_POST) {
        req = string_fmt(NULL,
                         "POST %s HTTP/1.1\r\n"
                         "Host: %s\r\n"
                         "Content-Length: %i\r\n"
                         "%s"
                         "%s"
                         "\r\n"
                         "%.*s",
                         action, host,
                         (postdata) ? SIZE(postdata) + addlen : 0,
                         (postdata && multipart) ? DATA(multipart) :
                         (postdata) ?
                         "Content-Type: application/x-www-form-urlencoded\r\n" :
                         "",
                         DATA(head->_headers),
                         (postdata) ? SIZE(postdata) : 0,
                         (postdata) ? DATA(postdata) : "");

        token = string_free(token);
        multipart = string_free(multipart);

    } else if (method == HTTP_GET) {
        /* we ignore files and headers found in the parts, but we collect
           all the form values to compose the GET request */
        req = string_fmt(NULL,
                         "GET %s%s%.*s HTTP/1.1\r\n"
                         "Host: %s\r\n"
                         "%s"
                         "\r\n",
                         action,
                         (postdata) ? "?" : "",
                         (postdata) ? SIZE(postdata) : 0,
                         (postdata) ? DATA(postdata) : "",
                         host,
                         DATA(head->_headers));
    }

    postdata = string_free(postdata);

    /* split the string using the placeholder so the request can be
       easily processed chunk by chunk */
    if (inline_file_size) string_splits(req, "{FILE}", strlen("{FILE}"));

    return req;
}

/* -------------------------------------------------------------------------- */

public m_string *http_format(m_http *h, int method, const char *action,
                             const char *host)
{
    return http_compile(h, method, action, host, 0);
}

/* -------------------------------------------------------------------------- */

static m_string *_http_split_pipeline(m_string *buf, m_string *input, int *http)
{
    /** @brief this function inspects input to delimit each pipelined request */

    off_t off = 0, pos = 0, start = -1, end = -1, stream = 0;
    unsigned int i = 0, j = 0, error = 0, v = 0;
    const char *method = NULL;
    m_string *tokens = NULL;

    if (! input) {
        debug("_http_split_pipeline(): bad parameters.\n");
        return NULL;
    }

    /* buffering or streaming mode? */
    if (PARTS(input)) {
        if (IS_FRAGMENT(TOKEN(input, 0))) {
            stream = off = TOKEN_SIZE(input, 0);
        } else if (PARTS(input) == 2) {
            /* encapsulate the parsed request */
            stream = off = TOKEN_END(input, 1) - DATA(input);
            tokens = input->token; input->token = NULL;
            v = input->parts; input->parts = input->_parts_alloc = 0;
            if (! string_select(input, 0, stream)) {
                input->parts = input->_parts_alloc = v;
                input->token = tokens;
                return NULL;
            }
            for (i = 0; i < v; i ++) tokens[i].parent = TOKEN(input, 0);
            TOKEN(input, 0)->token = tokens;
            TOKEN(input, 0)->parts = TOKEN(input, 0)->_parts_alloc = v;
            TOKEN(input, 0)->_flags |= _STRING_HTTP_REQUEST;
            TOKEN(input, 0)->_flags |= _STRING_PARTIAL_DATA;
        }
    }

    /* find all the occurences of "HTTP" in the incoming buffer */
    do {
        pos = string_compile_find(DATA(input), SIZE(input), off, "HTTP", & c);
        if ( (off = pos) == -1) {
            start = end; off = end = SIZE(input); break;
        }

        if (error == 3) error = 2;

        if (*(DATA(input) + (v = pos + 4)) == '/') {
            /* found a versioned protocol */
            while (++ v < SIZE(input) && v < (unsigned) pos + 8) {
                /* only allow MAJOR.MINOR versioning */
                if (*(DATA(input) + v) == '\r' ||
                    *(DATA(input) + v) == '\n' ||
                    *(DATA(input) + v) == ' ') break;
            }
        }

        if (*(DATA(input) + v) == '\r' || *(DATA(input) + v) == '\n') {
            /* HTTP query, find the real starting offset */
            if (! pos || *(DATA(input) + -- pos) != ' ') {
                debug("_http_split_pipeline(): HTTP query without URI.\n");
                goto _next;
            }
            /* skip the URI */
            while (*(DATA(input) + -- pos) != ' ');
            /* try to identify a method */
            i = *(DATA(input) + pos - 2) - 'A';
            j = *(DATA(input) + pos - 1) - 'A';
            if (i > 25 || j > 25) {
                debug("_http_split_pipeline(): malformed query.\n");
                goto _error;
            }
            method = http_method[http_matrix[i][j]];
            if (*method && (pos -= *method) >= 0) {
                if (memcmp(DATA(input) + pos, method + 1, *method - 2) != 0) {
                    debug("_http_split_pipeline(): unknown HTTP method.\n");
                    goto _error;
                } else HTTP_SET_METHOD(input, http_matrix[i][j]);
            } else {
                debug("_http_split_pipeline(): unknown HTTP method.\n");
                goto _error;
            }
        } else if (*(DATA(input) + v) != ' ') {
            /* should be an HTTP response, do some checking */
            debug("_http_split_pipeline(): malformed response.\n");
            if (! stream) goto _error; else goto _next;
        }

_split:
        if (start == -1) {
            /* first HTTP part */
            if (pos > stream) {
                if (! stream) {
                    /* enqueue to the previous buffer */
                    buf = string_cats(buf, DATA(input), pos);
                    string_suppr(input, 0, pos);
                    start = -1; end = 0; pos = 0; off = stream;
                } else {
                    /* append to the initial token */
                    SUBTOKEN(input, 0, 1)->_len += pos - stream;
                    SUBTOKEN(input, 0, 1)->_alloc = SUBTOKEN_SIZE(input, 0, 1);
                    http_inc_progress(TOKEN(input, 0), pos - stream);
                }
            }
            start = stream; end = pos;
        } else {
            /* handle errors */
            if (! error || error == 2) {
                start = end; end = pos;
            } else error = 3;

            if (error != 2 && end > 0) {
                /* enqueue the request */
                if (! string_add_token(input, start, end))
                    return buf;
            } else error = 0;
        }

_next:
        if ( (unsigned int) (off += 4) > SIZE(input)) break;
    } while (1);

    if (start >= stream) {
        /* add the remainder */
        if (! string_add_token(input, start, SIZE(input)))
            return buf;
        if (http) *http = 1;
    } else if (stream && start == -1) {
        /* merge to the starving request */
        SUBTOKEN(input, 0, 1)->_len += end - stream;
        SUBTOKEN(input, 0, 1)->_alloc = SUBTOKEN_SIZE(input, 0, 1);
        TOKEN(input, 0)->_len += end - stream;
        TOKEN(input, 0)->_alloc = TOKEN_SIZE(input, 0);
        if (! IS_CHUNK(input))
            http_inc_progress(TOKEN(input, 0), end - stream);
        if (http) *http = 1;
    } else if (http) *http = 0;

    return buf;

_error:
    error = 1;
    if (start >= 0) { start = end; end = off; goto _split; }
    else { end = off; goto _next; }
}

/* -------------------------------------------------------------------------- */

static int _http_valid(m_string *input)
{
    uint32_t magic = 0;
    const char *buf = NULL, *method = NULL;
    m_string *header = NULL;
    unsigned int i = 0, j = 0, o = 0;
    size_t len = 0;

    if (! input || SIZE(input) < 4) {
        debug("_http_valid(): bad parameters.\n");
        return -1;
    }

    /* if the input is already split, it has been validated before */
    if (input->parts > 1) goto _check_content_length;

    buf = DATA(input); len = SIZE(input); magic = ntohl(0x48545450);

    if (! HTTP_METHOD(input) && *((uint32_t *) buf) != magic) {
        i = *buf - 'A'; j = *(buf + 1) - 'A';
        if (i > 25 || j > 25) {
            debug("_http_valid(): malformed request.\n");
            return 0;
        }
        method = http_method[http_matrix[i][j]];
        if (memcmp(buf + 2, method + 3, *method - 2) != 0) {
            debug("_http_valid(): unknown HTTP method.\n");
            return 0;
        } else {
            HTTP_SET_METHOD(input, http_matrix[i][j]);
            debug("_http_valid(): HTTP %s request.\n", method);
        }
    }

    /* check if the reply is complete */
    if (string_splits(input, "\r\n\r\n", 4) == -1) {
        debug("_http_valid(): cannot process the buffer.\n");
        return -1;
    }

    if (input->parts < 2) {
        /* incomplete header */
        debug("_http_valid(): incomplete HTTP header.\n");
        return -1;
    }

    header = TOKEN(input, 0);

    /* parse the header */
    if (string_splits(header, "\r\n", 2) == -1) {
        debug("_http_valid(): cannot parse the header.\n");
        /* non fatal */
        return 1;
    }

    if (string_merges(header, "\0\0", 2) == -1) {
        debug("_http_valid(): cannot merge the header.\n");
        return 1;
    }

    /* parse the request: either method/url/protocol or protocol/status code */
    if (string_splits(TOKEN(header, 0), " ", 1) == -1) {
        debug("_http_valid(): cannot parse the request.\n");
        return 1;
    }

    if (! method) {
        /* no need to parse the status code description */
        if (PARTS(TOKEN(header, 0)) > 2) header->token[0].parts = 2;
    } else {
        if (PARTS(TOKEN(header, 0)) > 3) {
            header = TOKEN(header, 0);
            /* seems like the url was not urlencoded... */
            header->token[1]._len = DATA(LAST_TOKEN(header)) - TOKEN_DATA(header, 1);
            header->token[1]._alloc = header->token[1]._len;
            memcpy(& header->token[2], LAST_TOKEN(header), sizeof(m_string));
            header->parts = 2;
        }
    }

    /* merge back the header and body */
    if (string_merges(input, "\0\0\0\0", 4) == -1) {
        debug("_http_valid(): cannot merge the reply.\n");
        return 1;
    }

    /* the request is good enough and properly parsed */
    input->_flags |= _STRING_HTTP_REQUEST;

    /* prepare storage for progress */
    if (http_set_progress(input, TOKEN_SIZE(input, 1)) == -1) {
        debug("_http_valid(): cannot store progress.\n");
        return 1;
    }

_check_content_length:
    /* check the Content-Length header to know if the request is complete */
    if (! IS_CHUNK(input) && (len = http_get_contentlength(input)) ) {
        /* check if this is a big request */
        if (len > 50) input->_flags |= _STRING_LARGE_BUFFER;
        /* request is starved */
        if (http_get_progress(input) < len) return -2;
    } else if (IS_CHUNK(input) || (buf = http_get_header(input, "Transfer-Encoding")) ) {
        if (IS_CHUNK(input) || ! memcmp(buf, "chunked", 7)) {
            input->_flags |= (_STRING_LARGE_BUFFER |
                              _STRING_HTTP_CHUNKED |
                              _STRING_BUFFERING);

            if (! TOKEN_SIZE(input, 1)) return -2;

            /* check the beginning of the token */
            if ( (o = http_get_progress(input)) > TOKEN_SIZE(input, 1) ) o = 0;

            /* check the size of the chunk */
            if (! (len = strtol(TOKEN_DATA(input, 1) + o, (char **) & buf, 16)) ) {
                if (errno != ERANGE || buf != TOKEN_DATA(input, 1)) {
                    /* real zero */
                    if (o) string_dim(TOKEN(input, 1), o);
                    input->_flags &= ~_STRING_BUFFERING;
                    input->_flags &= ~_STRING_HTTP_CHUNKED;
                    return 1;
                } else goto _badchunk;
            }

            /* check if strtol() read past the boundaries of the token */
            if (buf > TOKEN_END(input, 1)) {
                debug("_http_valid(): strtol() out of bound.\n");
                goto _badchunk;
            }

            /* check that there is enough data in the buffer */
            if (buf + 1 + len + 2 > TOKEN_END(input, 1)) return -2;

            /* check CRLF are present both after chunk size and data */
            if (memcmp(buf, "\r\n", 2)) {
                debug("_http_valid(): missing CRLF after chunk size.\n");
                goto _badchunk;
            } else if (memcmp(buf + 2  + len, "\r\n", 2)) {
                debug("_http_valid(): missing CRLF after chunk data.\n");
                goto _badchunk;
            }

            string_suppr(
                TOKEN(input, 1),
                o,
                (buf + 2) - (TOKEN_DATA(input, 1) + o)
            );
            string_dim(TOKEN(input, 1), o + len);
            http_inc_progress(input, len);
            input->_flags &= ~_STRING_HAS_ERROR;
            input->_flags &= ~_STRING_BUFFERING;
            return -2;
        }
    }

    return 1;

_badchunk:
    debug("_http_valid(): bad chunk.\n");
    input->_flags |= _STRING_HAS_ERROR;
    return -1;
}

/* -------------------------------------------------------------------------- */

public m_string *http_get_request(int *r, m_string **buf, m_string *input)
{
    int http_requests = 0;
    unsigned int i = 0;
    int is_http = 0, starved = 0;
    m_string *req = NULL, *next = NULL;

    if (! r || ! buf || ! input) {
        debug("http_get_request(): bad parameters.\n");
        return NULL;
    }

    /* check the state */
    switch ( (http_requests = *r) ) {

    /* finished handling a starved request */
    case -2: {
        for (i = 0; i < PARTS(input); i ++)
            if (! TOKEN_SIZE(input, i)) input->parts --;
        *buf = input;
        return NULL;
    }

    /* completed a buffered request */
    case -1: return (*buf = string_free(*buf));

    /* initial state */
    case  0: *buf = _http_split_pipeline(*buf, input, r); break;

    /* processing a pipeline */
    default: if (http_requests > PARTS(input)) {
                if (PARTS(input) && IS_FRAGMENT(TOKEN(input, 0)))
                    *buf = string_free(input);
                return NULL;
             }
    }

    if (http_requests) *buf = string_free(*buf);

    req = (*buf) ? *buf :
          ((http_requests = *r)) ? TOKEN(input, http_requests - 1) :
          input;

    /* check if the selected request is valid HTTP */
    if ( (is_http = _http_valid(req)) > -1) {
        if (req != *buf) *r = http_requests + 1;
        return (is_http == 1) ? TOKEN(req, 1) : req;
    } else if (is_http == -2) starved = 1;

    /* check if the input was split and if the first request is valid */
    if (http_requests) {
        for (i = ++ http_requests - 1; i < PARTS(input); i ++) {

            next = TOKEN(input, i);

            /* if the next request in the pipeline is not a valid HTTP
               request, append it to the current one */
            if (starved || ! _http_valid(next)) {
                /* append the next token to the current request */
                if (req == *buf) string_cat(req, next);
                else TOKEN(req, 1)->_len += SIZE(next);

                /* track the progress */
                if (! IS_CHUNK(req))
                    http_inc_progress(req, SIZE(next));
                *r = ++ http_requests; next->_len = 0;

                /* check if the request is now valid/complete */
                switch (_http_valid(req)) {
                case -2: starved = 1; break;
                case  1: return TOKEN(req, 1);
                default: starved = 0;
                }
            } else {
                /* current request is complete, return the body */
                *r = http_requests + 1;
                return TOKEN(next, 1);
            }
        }

        /* check if the current request is still starving */
        if (starved) {
            if (! IS_FRAGMENT(req)) {
                if (IS_LARGE(req)) {
                    /* prepare for streaming */
                    *buf = string_dupextend(req, HTTP_BUFFER);
                    (*buf)->_flags |= _STRING_LARGE_BUFFER;
                } else *buf = string_dup(req);
                return NULL;
            } else {
                if (! IS_BUFFER(req)) {
                    *r = -2; return TOKEN(req, 1);
                }
            }
        }

    } else if (*buf && (*buf = string_cat(*buf, input)) ) {
        /* input is not valid HTTP, so append it to the buffer */
        if (! IS_CHUNK(req)) http_inc_progress(req, SIZE(input));

        switch (_http_valid((req = *buf))) {
        /* the request is valid but starved */
        case -2: if (! IS_BUFFER(req) && IS_LARGE(req)) {
                     /* streaming, return the partial body */
                     *r = input->parts + 1; return TOKEN(req, 1);
                 } else { *r = -2; return NULL; }
        /* request is complete, return its body and drop the buffer */
        case 1: *r = -1; return TOKEN(req, 1);
        /* incomplete/bad request, keep on buffering */
        default: return NULL;
        }
    }

    /* check for errors */
    if (IS_ERROR(req)) {
        *buf = string_free(req);
        return NULL;
    }

    /* the end of the pipeline was reached but request is still incomplete */
    *buf = string_dup(req);
    return NULL;
}

/* -------------------------------------------------------------------------- */

static void *_http_find_contentlength(m_string *header)
{
    unsigned char idx = 0;
    uint32_t len = 0;
    const char *buf = NULL;

    if (! (idx = (char) *(TOKEN_END(header, 0) + 1)) ) {

        /* slow path */
        for (idx = 0; idx < PARTS(header); idx ++) {
            if (TOKEN_SIZE(header, idx) < 14) continue;

            if (! memcmp(TOKEN_DATA(header, idx), "Content-Length", 14)) {
                /* found the header */
                *((char *) TOKEN_END(header, 0) + 1) = idx;
                buf = TOKEN_DATA(header, idx) + 14;
                if (*buf ++ == ':') {
                    len = strtol(buf, NULL, 10);
                    if (errno == ERANGE) len = 0;
                }
                /* record the length */
                *((unaligned_uint32_t *) TOKEN_DATA(header, idx)) = len;
                return (void *) TOKEN_DATA(header, idx);
            }
        }

        /* not found */
        *((unsigned char *) TOKEN_END(header, 0) + 1) = 0xff;
        return NULL;

    } else if (idx > PARTS(header)) return NULL;

    return (void *) TOKEN_DATA(header, idx);
}

/* -------------------------------------------------------------------------- */

public const char *http_get_header(m_string *buffer, const char *name)
{
    unsigned int i = 0;
    m_string *header = NULL;
    const char *data = NULL;
    off_t pos = 0;
    size_t len = 0;

    if (! buffer || ! name) {
        debug("http_get_header(): bad parameters.\n");
        return NULL;
    }

    /* if the function is called on a token, fix it */
    while (buffer->parent) {
        if (IS_HTTP(buffer)) break;
        buffer = buffer->parent;
    }

    /* the headers are in the first token */
    if (buffer->parts < 2) {
        debug("http_get_header(): no header found.\n");
        return NULL;
    }

    header = TOKEN(buffer, 0);

    data = DATA(header);

    if ( (len = strlen(name)) == 14 && ! memcmp(name, "Content-Length", 14)) {
        const char *ptr = NULL;
        if (! (ptr = _http_find_contentlength(header)) )
            return NULL;
        pos = ptr - data;
    } else {
        for (i = 0; i < PARTS(header); i ++) {
            if (TOKEN_SIZE(header, i) < len) continue;
            if (! memcmp(TOKEN_DATA(header, i), name, len)) {
                pos = TOKEN_DATA(header, i) - data; break;
            }
        }

        if (! pos) return NULL;
    }

    pos += len;

    if (data[pos ++] == ':' && data[pos] == ' ') pos ++;

    return data + pos;
}

/* -------------------------------------------------------------------------- */

public uint32_t http_get_contentlength(m_string *buffer)
{
    m_string *header = NULL;
    unaligned_uint32_t *len = NULL;

    if (! buffer) {
        debug("http_get_contentlength(): bad parameters.\n");
        return 0;
    }

    /* if the function is called on a token, fix it */
    while (buffer->parent) {
        if (IS_HTTP(buffer)) break;
        buffer = buffer->parent;
    }

    /* the headers are in the first token */
    if (! IS_HTTP(buffer) || buffer->parts < 2) {
        debug("http_get_contentlength(): no header found.\n");
        return 0;
    }

    header = TOKEN(buffer, 0);

    return ( (len = _http_find_contentlength(header)) ) ? *len : 0;
}

/* -------------------------------------------------------------------------- */

public uint32_t http_get_progress(m_string *buffer)
{
    m_string *header = NULL;

    if (! buffer) {
        debug("http_get_progress(): bad parameters.\n");
        return 0;
    }

    /* if the function is called on a token, fix it */
    while (buffer->parent) {
        if (IS_HTTP(buffer)) break;
        buffer = buffer->parent;
    }

    /* the headers are in the first token */
    if (buffer->parts < 2) {
        debug("http_get_progress(): no header found.\n");
        return 0;
    }

    header = TOKEN(buffer, 0);

    if (! header->parts) return 0;

    switch (PARTS( (header = TOKEN(header, 0)) )) {
    case 2: return *((unaligned_uint32_t *) TOKEN_DATA(header, 0));
    case 3: return *((unaligned_uint32_t *) TOKEN_DATA(header, 2));
    default: return 0;
    }
}

/* -------------------------------------------------------------------------- */

static int _http_set_progress(m_string *buffer, uint32_t progress, int inc)
{
    m_string *header = NULL;

    if (! buffer) {
        debug("_http_set_progress(): bad parameters.\n");
        return -1;
    }

    /* if the function is called on a token, fix it */
    while (buffer->parent) {
        if (IS_HTTP(buffer)) break;
        buffer = buffer->parent;
    }

    if (buffer->parts < 2) {
        debug("_http_set_progress(): no header.\n");
        return -1;
    }

    header = TOKEN(buffer, 0);

    if (! header->parts) {
        debug("_http_set_progress(): header is not parsed.\n");
        return -1;
    }

    if (inc) switch (PARTS( ((header = TOKEN(header, 0))) )) {
        case 2: *((unaligned_uint32_t *) TOKEN_DATA(header, 0)) += progress; break;
        case 3: *((unaligned_uint32_t *) TOKEN_DATA(header, 2)) += progress; break;
        default: return -1;
    } else switch (PARTS( ((header = TOKEN(header, 0))) )) {
        case 2: *((unaligned_uint32_t *) TOKEN_DATA(header, 0)) = progress; break;
        case 3: *((unaligned_uint32_t *) TOKEN_DATA(header, 2)) = progress; break;
        default: return -1;
    }

    return 0;
}

/* -------------------------------------------------------------------------- */

public int http_set_progress(m_string *buffer, uint32_t progress)
{
    return _http_set_progress(buffer, progress, 0);
}

/* -------------------------------------------------------------------------- */

public int http_inc_progress(m_string *buffer, uint32_t progress)
{
    return _http_set_progress(buffer, progress, 1);
}

/* -------------------------------------------------------------------------- */

public m_http *http_close(m_http *h)
{
    m_http *prev = NULL;

    if (! h) return NULL;

    /* unwind the request */
    while (h->next) h = h->next;

    while (h) {
        prev = h->prev;
        h->_headers = string_free(h->_headers);
        free(h->name); free(h->value);
        #ifdef _ENABLE_FILE
        h->file = fs_closefile(h->file);
        #endif
        free(h); h = prev;
    }

    return NULL;
}

/* -------------------------------------------------------------------------- */
#else
/* -------------------------------------------------------------------------- */

/* HTTP support will not be compiled in the Concrete Library */
#ifdef __GNUC__
__attribute__ ((unused)) static int __dummy__ = 0;
#endif

/* -------------------------------------------------------------------------- */
#endif
/* -------------------------------------------------------------------------- */
