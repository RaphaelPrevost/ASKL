/*******************************************************************************
 *  Concrete Server                                                            *
 *  Copyright (c) 2005-2024 Raphael Prevost <raph@el.bzh>                      *
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

#include "../../lib/m_core_def.h"
#include "../../lib/m_string.h"
#include "../../lib/m_parser.h"

/* -------------------------------------------------------------------------- */

static void print_tokens(const m_string *s, unsigned int indent)
{
    unsigned int i = 0, j = 0, k = 0;
    const m_string *cur = NULL, *parent = NULL;

    if (! s) return;

    printf(
        "%.*s%s%.*s %s",
        indent, "", (indent) ? " " : "+ ", (int) SIZE(s), DATA(s),
        (IS_OBJECT(s) ? "(object)" :
         IS_ARRAY(s) ? "(array)" :
         IS_STRING(s) ? "(string)" :
         IS_PRIMITIVE(s) ? "(primitive)" : "")
    );
    if (IS_ERROR(s)) printf(" (!) ");
    if (! IS_TYPE(s, JSON_TYPE))
        printf("(size=%zu)", SIZE(s));
    printf("\n");

    if (s->token) {
        for (i = 0; i < PARTS(s); i ++) {
            for (j = 0; j < indent; j ++) {
                for (parent = s, k = j; k < indent; k ++) {
                    cur = parent; parent = parent->parent;
                }
                printf("%c  ", (LAST_TOKEN(parent) != cur) ? '|' : ' ');
            }
            printf("|-[%i]", i);
            print_tokens(TOKEN(s, i), indent + 1);
        }
    }

    return;
}

/* -------------------------------------------------------------------------- */

int main(int argc, char **argv)
{
    char *src = NULL;
    ssize_t len = 0;
    m_string *json = NULL;
    unsigned int i = 0;
    struct stat info;
    int fd = -1, ret = 0;
    m_json_parser ctx;

    jsonpath_init(& ctx);

    if (argc > 1) { /* whole document in memory */
        if (stat(argv[1], & info) == -1) {
            fprintf(stderr, "%s: failed to stat %s.\n", argv[0], argv[1]);
            goto _failure;
        }

        if ( (fd = open(argv[1], O_RDONLY)) == -1) {
            fprintf(stderr, "%s: failed to open %s.\n", argv[0], argv[1]);
            goto _failure;
        }

        len = info.st_size;
        src = mmap(NULL, len, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, 0);

        if (! src) {
            fprintf(stderr, "%s: failed to map the file.\n", argv[0]);
            goto _failure;
        }

        if (! (json = string_encaps(src, len)) ) {
            munmap(src, len);
            goto _failure;
        }

        do {
            ret = string_parse_json(json, JSON_STRICT, & ctx);
        } while (ret > 0);

        munmap(src, len);

        if (ret == 0) goto _success;

    } else { /* streaming */
        char buffer[65536], key[65536];

        if ( (len = read(STDIN_FILENO, buffer, sizeof(buffer))) == -1) {
            fprintf(stderr, "%s: cannot read stdin.\n", argv[0]);
            goto _failure;
        }

        if (! (json = string_alloc(buffer, len)) ) {
            fprintf(stderr, "%s: cannot allocate buffer.\n", argv[0]);
            goto _failure;
        }

        while (1) {
            if ( (ret = string_parse_json(json, JSON_STRICT, & ctx) == -1) )
                goto _failure;

            if ( (len = read(STDIN_FILENO, buffer, sizeof(buffer))) == -1) {
                fprintf(stderr, "%s: cannot read stdin.\n", argv[0]);
                goto _failure;
            } else if (len == 0) goto _success; /* EOF */

            /* copy the context current key, if any, before flushing the data */
            if (ctx.key.current) {
                memcpy(key, ctx.key.current, ctx.key.len);
                ctx.key.current = key;
            }

            /* flush parsed data */
            if (PARTS(json) && PARTS(FIRST_TOKEN(json)) > 1) {
                m_string *penultimate = & (
                    FIRST_TOKEN(json)->token[FIRST_TOKEN(json)->parts - 2]
                );
                string_suppr(json, 0, DATA(penultimate) - DATA(json));
            }

            /* append the buffer */
            string_cats(json, buffer, len);
        }
    }

    fprintf(stderr, "%s: parse error.\n", argv[0]);
_failure:
    jsonpath_free(& ctx);
    exit(EXIT_FAILURE);

_success:
    //jsonpath_free(& ctx);
    exit(EXIT_SUCCESS);
}

/* -------------------------------------------------------------------------- */
