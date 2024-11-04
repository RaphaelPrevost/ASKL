#include "../lib/m_string.h"

#ifdef _ENABLE_RANDOM
#include "../lib/m_random.h"
#endif

#define DUMMY_LEN 256

/* -------------------------------------------------------------------------- */

static void print_tokens(const m_string *s, unsigned int indent)
{
    unsigned int i = 0, j = 0, k = 0;
    const m_string *cur = NULL, *parent = NULL;

    if (! s) return;

    printf("%.*s%s%.*s %s",
           indent, "", (indent) ? " " : "+ ", (int) SIZE(s), DATA(s),
           (IS_OBJECT(s) ? "(object)" :
            IS_ARRAY(s) ? "(array)" :
            IS_STRING(s) ? "(string)" :
            IS_PRIMITIVE(s) ? "(primitive)" : ""));
    if (IS_ERROR(s)) printf(" (!) ");
    if (! IS_TYPE(s, JSON_TYPE))
        printf("(size=%zu)", SIZE(s));
    printf("\n");

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

    return;
}

/* -------------------------------------------------------------------------- */

int test_string(void)
{
    const char *str = "这个服务器有没有问题";
    #ifdef HAS_ICONV
    const char *gb18030 = "\xd5\xe2\xb8\xf6\xb7\xfe\xce\xf1\xc6\xf7"
                          "\xd3\xd0\xc3\xbb\xd3\xd0\xce\xca\xcc\xe2";
    #endif
    #ifdef _ENABLE_JSON
    const char *good_json = "{\"obj\":{\"b\":false,\"z\":[\"\"]},\"unicode\":\"\\u611b\",\"matrix\":[[[1,2],[2,3]]],\"empty\":{}}";
    const char *incomplete_json1 = "{\"0\": {}, \"a\":[1,2,3],\"b\":[4,5";
    const char *incomplete_json2 = ",6],\"c\":[7,8,9]}";
    const char *json_stream1 = "{\"a\": \"stream_part\"}[[[[{\"b\"";
    const char *json_stream2 = ": \"partial stream part\"}]]]]";
    const char *json_stream3 = "{\"a\": [1, true] }";
    const char *json_stream4 = "{\"b\": [0, false] }";
    const char *json5 = "{ // single-line comment\nunquoted_key: 'single quoted string w/ nested \"double quoted string\"',\n'single quoted key'/* multi-line\ncomment */: \"stray CRLF\\\n\tunescaped tab\",\"hex\": [0xA, 0xBAD, 0xc0ffee, 0x00000000064B175]/* max 64 bits */,\n\"no fractional part\": 123.,\n, \"trailing commas in object\":\n[\"and\",,\"in array\", ],\n}";
    const char *incomplete_string1 = "\"incomplete, ";
    const char *incomplete_string2 = "string\"";
    unsigned int bad_json = 27;
    const char *bad[] = { 
        "{\"a\":}",
        "{\"a\"}",
        "{\"a\":01}",
        "{\"a\": 0. }",
        "{\"a\" \"b\"}",
        "{\"a\":'b'}",
        "{\"a\" ::::: \"b\"}",
        "{\"a\": [1 \"b\"] }",
        "{\"a\"\"\"}",
        "{\"a\":1\"\"}",
        "{\"a\":1\"b\":1}",
        "{\"a\":\"b\",{\"c\":\"d\"}}",
        "[\"a\":\"b\"]",
        "{ {\"a\": 1 } {\"b\": 2}}",
        "{,}",
        "{ : }",
        "[1,,3]",
        "\"bad\":",
        "\"u\\\\qn\nescaped\t\t\tstring\"",
        "\"bad escaped unicode \\u1?34\"",
        "{ : 1}",
        "{\"a\": 1,,}",
        "[1,2]]",
        "[1,2],",
        "{\"a\": 0},",
        "{ },",
        "{ }:" };
    #endif
    const char *cs = "Random string1234";
    m_string *a = NULL, *w = NULL, *z = NULL;
    unsigned int i = 0;
    off_t pos = 0;
    char buffer[256];
    size_t item_size = 0;
    #ifdef _ENABLE_RANDOM
    uint32_t random_token[10];
    m_random *r = NULL;
    #endif

    setlocale(LC_CTYPE, "zh_CN.UTF8");

    if (! (z = string_alloc(NULL, DUMMY_LEN)) || string_len(z) != DUMMY_LEN) {
        printf("(!) Allocating a "STR(DUMMY_LEN)" bytes buffer: FAILURE\n");
        return 0;
    } else
        printf("(*) Allocating a "STR(DUMMY_LEN)" bytes buffer: SUCCESS\n");

    printf("(*) Double the allocated space:\n");
    string_dim(z, DUMMY_LEN * 2);
    printf("(-) SIZE: %zu\n", string_len(z));
    printf("(-) SPACE: %zu\n", string_spc(z));

    /* string_free(simple string) */
    printf("(*) Destroying a simple m_string:");
    z = string_free(z);
    printf(" SUCCESS\n");

    /* base58 and base64 encoding */
    z = string_b58s("test string", strlen("test string"));
    printf("(*) Base58 encoding: test string -> %.*s [%zu]\n",
           (int) SIZE(z), DATA(z), SIZE(z));
    z = string_free(z);
    z = string_deb58s("Vs5LyRhXt9nUp14", strlen("Vs5LyRhXt9nUp14"));
    printf("(*) Base58 encoding: Vs5LyRhXt9nUp14 -> %.*s [%zu]\n",
           (int) SIZE(z), DATA(z), SIZE(z));
    z = string_free(z);

    #ifdef _ENABLE_RANDOM
    r = random_arrayinit(NULL, 0);
    for (i = 0; i < 10; i ++)
        random_token[i] = random_uint32(r);

    z = string_b58s((char *) random_token, 10 * sizeof(*random_token));
    printf("(*) Base58 encoded random token:\n(-) %.*s [%zu]\n",
           (int) SIZE(z), DATA(z), SIZE(z));
    z = string_free(z);
    r = random_free(r);
    #endif

    #ifdef HAS_ICONV
    z = string_alloc(gb18030, 20);
    i = string_convs(gb18030, 20, "GB18030", NULL, 0, "UTF-8");
    printf("(-) Buffer length required for conversion: %i\n", i);
    string_conv(z, "GB18030", "");
    printf("(-) Unicode buffer: %.*s\n", (int) SIZE(z), DATA(z));
    string_free(z);
    #endif

    #ifdef _ENABLE_JSON
    printf("(*) Parsing correct JSON:\n");
    z = string_alloc(good_json, strlen(good_json));
    string_parse_json(z, JSON_STRICT, NULL);
    print_tokens(z, 0);
    string_free(z);
    printf("(*) Checking if incorrect JSON is rejected:\n");
    for (i = 0; i < bad_json; i ++) {
        printf("(-) %s\n", bad[i]);
        z = string_alloc(bad[i], strlen(bad[i]));
        if (string_parse_json(z, JSON_STRICT, NULL) != -1) {
            print_tokens(z, 0);
        }
        z = string_free(z);
    }
    printf("(*) Parsing incomplete JSON:\n");
    z = string_alloc(incomplete_json1, strlen(incomplete_json1));
    if (string_parse_json(z, JSON_STRICT, NULL) == -1)
        printf("(!) Error!\n");
    print_tokens(z, 0);
    /* XXX remove all but the last token */
    printf("(*) Flushing parsed data:\n");
    string_suppr(z, 0, DATA(LAST_TOKEN(LAST_TOKEN(z))) - DATA(z));
    print_tokens(z, 0);
    printf("(*) Adding the missing JSON:\n");
    string_cats(z, incomplete_json2, strlen(incomplete_json2));
    print_tokens(z, 0);
    string_parse_json(z, JSON_STRICT, NULL);
    print_tokens(z, 0);
    z = string_free(z);

    printf("(*) Parsing incomplete JSON stream:\n");
    z = string_alloc(json_stream1, strlen(json_stream1));
    if (string_parse_json(z, JSON_STRICT, NULL) == -1)
        printf("(!) Error!\n");
    print_tokens(z, 0);
    string_cats(z, json_stream2, strlen(json_stream2));
    string_parse_json(z, JSON_STRICT, NULL);
    print_tokens(z, 0);
    z = string_free(z);

    printf("(*) Parsing JSON stream with complete object:\n");
    z = string_alloc(json_stream3, strlen(json_stream3));
    if (string_parse_json(z, JSON_STRICT, NULL) == -1)
        printf("(!) Error with complete object in the stream!\n");
    print_tokens(z, 0);
    string_cats(z, json_stream4, strlen(json_stream4));
    string_parse_json(z, JSON_STRICT, NULL);
    print_tokens(z, 0);
    z = string_free(z);

    printf("(*) Parsing JSON stream with incomplete string:\n");
    z = string_alloc(incomplete_string1, strlen(incomplete_string1));
    if (string_parse_json(z, JSON_STRICT, NULL) == -1)
        printf("(!) Error!\n");
    print_tokens(z, 0);
    string_cats(z, incomplete_string2, strlen(incomplete_string2));
    string_parse_json(z, JSON_STRICT, NULL);
    print_tokens(z, 0);
    z = string_free(z);

    printf("(*) Parsing JSON5 extensions in QUIRKS mode:\n");
    z = string_alloc(json5, strlen(json5));
    if (string_parse_json(z, JSON_QUIRKS, NULL) == -1)
        printf("(!) Error!\n");
    print_tokens(z, 0);
    z = string_free(z);
    #endif

    /* catch integer overflow */
    if (string_alloc(NULL, 4294967293)) {
        printf("(!) Catching integer overflow in string_alloc(): FAILURE\n");
        return -1;
    } else printf("(*) Catching integer overflow in string_alloc(): SUCCESS\n");

    #ifdef _ENABLE_HTTP
    if (string_rawurlencode("http://integeroverflow.net", 1431655765, 0x0)) {
        printf("(!) Catching integer overflow in string_rawurlencode(): FAILURE\n");
        return -1;
    } else printf("(*) Catching integer overflow in string_rawurlencode(): SUCCESS\n");
    #endif

    /* string_allocw(wide, len) */
    if (! (w = string_alloc(str, strlen(str))) ) {
        printf("(!) Allocating a wide string \"%s\": FAILURE\n", str);
        return -1;
    } else printf("(*) Allocating a wide string \"%s\": SUCCESS\n", DATA(w));

    /* string_alloc(ansi, len) */
    if (! (a = string_alloc(cs, strlen(cs))) ) {
        w = string_free(w);
        printf("(!) Allocating an ANSI string \"%s\": FAILURE\n", cs);
        return -1;
    } else
        printf("(*) Allocating an ANSI string \"%s\": SUCCESS\n", DATA(a));

    #if (_ENABLE_PCRE && HAS_PCRE)
    printf("(*) Looking for \"Random string(.*)\"\n");
    string_parse(a, "Random string(.*)", strlen("Random string(.*)"));
    print_tokens(a, 0);

    printf("(*) Looking for \"Random string(.)\"\n");
    string_parse(a, "Random string(.)", strlen("Random string(.)"));
    print_tokens(a, 0);

    printf("(*) Looking for \"(Random)\"\n");
    string_parse(a, "(Random)", strlen("(Random)"));
    print_tokens(a, 0);

    printf("(*) Looking for \"test_no_match\"\n");
    string_parse(a, "test_no_match", strlen("test_no_match"));
    print_tokens(a, 0);

    printf("(*) Looking for \"a\"\n");
    string_parse(a, "a", strlen("a"));
    print_tokens(a, 0);
    #endif

    /* looking for "je" */
    if ( (pos = string_finds(a, 0, "st", strlen("st"))) == -1) {
        printf("i = %i\n", i);
        printf("(!) Searching for \"st\" in \"%s\": FAILURE\n", DATA(a));
        w = string_free(w);
        a = string_free(a);
        return -1;
    } else
        printf("(*) Searching for \"st\" => \"%s\": SUCCESS\n", DATA(a) + pos);

    /* converting to wide */
    if (string_wchar(w) == -1) {
        printf("(!) Mbyte to wide string conversion \"%s\": "
                "FAILURE\n", DATA(w));
        w = string_free(w);
        a = string_free(a);
        return -1;
    } else
        printf("(*) Mbyte to wide string conversion \"%ls\": "
                "SUCCESS\n", (wchar_t *) DATA(w));

    /* converting back to multibyte */
    if (string_mbyte(w) == -1) {
        printf("(!) Wide to mbyte string conversion \"%s\": "
                "FAILURE\n", DATA(w));
        w = string_free(w);
        a = string_free(a);
        return -1;
    } else
        printf("(*) Wide to mbyte string conversion \"%s\": "
                "SUCCESS\n", DATA(w));

    /* appending */
    if (string_cat(w, a) == NULL) {
        printf("(!) Appending \"%s\" to \"%s\": "
                "FAILURE\n", DATA(a), DATA(w));
        w = string_free(w);
        a = string_free(a);
        return -1;
    } else printf("(*) Appending \"%s\": SUCCESS\n", DATA(w));

    /* looking for Random ! */
    pos = string_finds(w, 0, "Random", strlen("Random"));
    if (pos == -1) {
        printf("i = %i\n", i);
        printf("(!) Searching for \"Random\" in \"%s\": FAILURE\n", DATA(w));
        w = string_free(w);
        a = string_free(a);
        return -1;
    } else
        printf("(*) Searching for \"Random\" => \"%s\": "
                "SUCCESS\n", DATA(w) + pos);

    /* looking for "问题" */
    if ( (pos = string_finds(w, 0, "问题", strlen("问题"))) == -1) {
        printf("(!) Searching for \"问题\" in \"%s\": FAILURE\n", DATA(w));
        w = string_free(w);
        a = string_free(a);
        return -1;
    } else
        printf("(*) Searching for \"问题\" => \"%s\": "
                "SUCCESS\n", DATA(w) + pos);

    w = string_free(w);
    a = string_free(a);

    w = string_alloc("13-10-15:Forever", strlen("13-10-15:Forever"));

    /* split on ":" */
    if (string_splits(w, ":", strlen(":")) == -1) {
        printf("(!) Splitting on \":\" in \"%s\": FAILURE\n", DATA(w));
        w = string_free(w);
        return -1;
    } else printf("(*) Splitting on \":\" in \"%s\": SUCCESS\n", DATA(w));

    print_tokens(w, 0);

    /* split first token on "-" */
    if (PARTS(w) == 2) {
        if (string_splits(TOKEN(w, 0), "-", strlen("-")) == -1) {
            printf(
                "(!) Splitting on \"-\" in \"%.*s\": FAILURE\n",
                (int) TOKEN_SIZE(w, 0), TOKEN_DATA(w, 0)
            );
            w = string_free(w);
            return -1;
        } else printf(
            "(*) Splitting on \"-\" in \"%.*s\": SUCCESS\n",
            (int) TOKEN_SIZE(w, 0), TOKEN_DATA(w, 0)
        );

        print_tokens(w, 0);

        if (PARTS(TOKEN(w, 0)) == 3) {
            if (! string_pres(TOKEN(TOKEN(w, 0), 2), "20", strlen("20"))) {
                printf("(!) Prepending to a token: FAILURE\n");
                w = string_free(w);
                return -1;
            } else printf(
                "(*) Prepending to a token (\"%.*s\" in \"%.*s\"): SUCCESS\n",
                (int) TOKEN_SIZE(TOKEN(w, 0), 2), TOKEN_DATA(TOKEN(w, 0), 2),
                (int) TOKEN_SIZE(w, 0), TOKEN_DATA(w, 0)
            );
        }
    }

    if (string_merges(TOKEN(w, 0), "/", strlen("/")) == -1) {
        printf(
            "(!) Merging token \"%.*s\" on \"/\": FAILURE\n",
            (int) TOKEN_SIZE(w, 0), TOKEN_DATA(w, 0)
        );
        w = string_free(w);
        return -1;
    } else printf("(*) Merging token on \"/\": SUCCESS\n");

    print_tokens(w, 0);

    if (string_merges(w, " -> ", strlen(" -> ")) == -1) {
        printf(
            "(!) Merging string \"%.*s\" on \" -> \": FAILURE\n",
            (int) SIZE(w), DATA(w)
        );
        w = string_free(w);
        return -1;
    } else printf("(*) Merging string on \" -> \": SUCCESS\n");

    print_tokens(w, 0);

    if (string_dim(TOKEN(w, 0), strlen("13/10")) == -1) {
        printf("(!) Shrinking token: FAILURE\n");
        w = string_free(w);
        return -1;
    } else if (SIZE(w) != strlen("13/10 -> Forever")) {
        printf("(!) Shrinking token (Size differs: %zu): FAILURE\n", SIZE(w));
        w = string_free(w);
        return -1;
    } else printf("(*) Shrinking token (%s): SUCCESS\n", DATA(w));

    print_tokens(w, 0);

    if (string_suppr(w, TOKEN_SIZE(w, 0), strlen(" -> ")) == -1) {
        printf("(!) Suppressing substring: FAILURE\n");
        w = string_free(w);
        return -1;
    } else printf("(*) Suppressing substring \" -> \": SUCCESS\n");

    print_tokens(w, 0);

    /* try resplitting */
    if (string_splits(w, "/", strlen("/")) == -1) {
        printf("(!) Replitting on \"/\" in \"%s\": FAILURE\n", DATA(w));
        w = string_free(w);
        return -1;
    } else printf("(*) Replitting on \"/\" in \"%s\": SUCCESS\n", DATA(w));

    print_tokens(w, 0);

    #ifdef _ENABLE_HTTP
    /* insert special chars in a token and urlencode it */
    if (! string_pres(TOKEN(w, 0), "http://www.test.com/?parm=",
                      strlen("http://www.test.com/?parm="))) {
        printf("(!) Prepending to a token: FAILURE\n");
        w = string_free(w);
        return -1;
    } else printf(
        "(*) Prepending to a token (\"%.*s\"): SUCCESS\n",
        (int) TOKEN_SIZE(w, 0), TOKEN_DATA(w, 0)
    );

    if (string_urlencode(TOKEN(w, 0), 0x0) == -1) {
        printf("(!) Urlencode: FAILURE\n");
        w = string_free(w);
        return -1;
    } else printf("(*) Urlencode %s: SUCCESS\n", TOKEN_DATA(w, 0));

    item_size = m_snprintf(
        buffer, sizeof(buffer),
        "http://www.test.com/?parm=%.*url",
        5, ".#{0}"
    );
    printf("(*) Urlencode %s: SUCCESS\n", buffer);

    for (i = 0; i < PARTS(w); i ++)
        printf(
            "(-) token[%i] = %.*s (size=%zu)\n",
            i, (int) TOKEN_SIZE(w, i), TOKEN_DATA(w, i), TOKEN_SIZE(w, i)
        );
    #endif

    z = string_sha1(TOKEN(w, 1));

    if (string_cmps(z, "2fe5db4a1ddad9423d4e174bb78eb6a8c80ea6db",
                    strlen("2fe5db4a1ddad9423d4e174bb78eb6a8c80ea6db"))) {
        printf(
            "(!) SHA1(%.*s [%zu]) == %s: FAILURE\n",
            (int) TOKEN_SIZE(w, 1), TOKEN_DATA(w, 1),
            TOKEN_SIZE(w, 1),
            DATA(z)
        );
        w = string_free(w);
        z = string_free(z);
        return -1;
    } else printf("(*) SHA1: SUCCESS\n");

    /* append formatted output */
    if (! string_catfmt(z, "%s%i", "formatted-output", 1234)) {
        printf("(!) Append formatted string: FAILURE\n");
        z = string_free(z);
        return -1;
    } else printf("(*) Append formatted string %s: SUCCESS\n", DATA(z));

    if (string_splits(z, "-", 1) == -1 || PARTS(z) < 2) {
        printf("(!) Splitting: FAILURE\n");
        z = string_free(z);
        return -1;
    } else printf("(*) Splitting: SUCCESS\n");

    if (string_cat(z, z) != z) {
        printf("(!) Keeping original pointer when appending to split string: FAILURE\n");
        z = string_free(z);
        return -1;
    } else printf("(*) Keeping original pointer when appending to split string: SUCCESS\n");

    string_flush(TOKEN(z, 1));

    z = string_free(TOKEN(z, 1));

    /* test string_catfmt */
    z = string_alloc(NULL, 0);

    if (! string_catfmt(z, "%lli,0,\"/%s\",%li,0\n", 0LL, "filename", 123456789)) {
        printf("(!) String formatted concatenation: FAILURE\n");
        z = string_free(z);
        return -1;
    } else printf("(*) String formatted concatenation: SUCCESS\n");

    z = string_free(z);

    /* string push/pop */
    z = string_alloc("One", strlen("One"));
    string_select(z, 0, SIZE(z));

    if (string_push_tokens(z, "Two", strlen("Two")) == -1) {
        printf("(!) String formatted concatenation: FAILURE\n");
        z = string_free(z);
        return -1;
    }
    if (string_push_tokens(z, "Three", strlen("Three")) == -1) {
        printf("(!) String formatted concatenation: FAILURE\n");
        z = string_free(z);
        return -1;
    }
    if (string_push_tokens(z, "Four", strlen("Four")) == -1) {
        printf("(!) String formatted concatenation: FAILURE\n");
        z = string_free(z);
        return -1;
    }
    if (string_push_tokens(z, "Five", strlen("Five")) == -1) {
        printf("(!) String formatted concatenation: FAILURE\n");
        z = string_free(z);
        return -1;
    }

    string_upper(TOKEN(z, 1));

    print_tokens(z, 0);
    int limit = 0;
    while ( (a = string_pop_token(z)) ) {
        printf("(*) Dequeued token: %.*s\n", (int) SIZE(a), DATA(a));
        print_tokens(z, 0);
        string_free(a);
        if (limit ++ > 5) exit(EXIT_FAILURE);
    }

    z = string_free(z);

    printf("(*) Splitting and merging a pseudo HTTP request:\n");

    z = string_alloc("HTTP 888 TEST\r\nheader: value\r\nheader: value----1",
                     strlen("HTTP 888 TEST\r\nheader: value\r\nheader: value----1"));

    string_splits(z, "----", 4);

    print_tokens(z, 0);

    string_splits(TOKEN(z, 0), "\r\n", 2);
    string_merges(TOKEN(z, 0), "X", 1);

    print_tokens(z, 0);

    printf("===========================\n");
    printf("(*) Flushing the request data:\n");

    string_flush(TOKEN(z, 1));

    print_tokens(z, 0);

    printf("===========================\n");
    printf("(*) Appending new data:\n");

    string_cats(z, "incoming data", strlen("incoming data"));

    print_tokens(z, 0);

    z = string_free(z);

    z = string_alloc("Test merging with a 0-length delimiter", 38);
    string_splits(z, " ", 1);

    print_tokens(z, 0);

    if (string_merges(z, NULL, 1) != -1) {
        printf("(!) NULL pattern with positive length must be rejected: FAILURE\n");
        z = string_free(z);
        return -1;
    } else printf("(*) NULL pattern with positive length must be rejected: SUCCESS\n");

    string_merges(z, NULL, 0);

    print_tokens(z, 0);

    z = string_free(z);

    #ifdef HAS_ZLIB
    /* compress string */
    if (! (z = string_compress(w)) ) {
        printf("(!) Compressing a string: FAILURE\n");
        return -1;
    } else printf("(*) Compressing a string: SUCCESS\n");

    item_size = SIZE(w); w = string_free(w);

    if (! (w = string_uncompress(z, item_size)) ) {
        printf("(!) Uncompressing a string: FAILURE\n");
        return -1;
    } else printf("(*) Uncompressing a string (%s): SUCCESS\n", DATA(w));

    z = string_free(z);
    #endif

    w = string_free(w);

    /* test complex string generation */
    if (! (z = string_from_uint32(htonl(0x82))) ) {
        printf("(!) Creating a string from uint32: FAILURE\n");
        return -1;
    } else printf("(*) Creating a string from uint32: SUCCESS\n");

    printf("(-) string alloc=%zu len=%zu\n", z->_alloc, z->_len);

    item_size = m_snprintf(buffer, sizeof(buffer),
                           "%i%.*s%i%.*s%s%.*s%i%.*s%i%.*s%i"
                           "%.*s%i%.*s%s%.*s%s%.*s%i%.*s%i%.*s",
                           140, 1, "|", 350, 1, "|", "nickname_test_1234",
                           1, "|", 0x08, 1, "|", 0x0, 1, "|", 25,
                           1, "|", 75, 1, "|", "Paris", 1, "|",
                           "e9d607c4d54e3427e4d4cb56cae1801f.jpg",
                           1, "|", 0, 1, "|", 1, 1, "\n");

    if (! string_cats(z, buffer, item_size)) {
        printf("(!) Concatenating a string to uint32: FAILURE\n");
        z = string_free(z);
        return -1;
    } else printf("(*) Concatenating a string to uint32: SUCCESS\n");

    printf("(-) string alloc=%zu len=%zu\n", z->_alloc, z->_len);

    if (! string_cats(z, (char *) & item_size, sizeof(item_size))) {
        printf("(!) Concatenating an int to string: FAILURE\n");
        z = string_free(z);
        return -1;
    } else printf("(*) Concatenating an int to string: SUCCESS\n");

    printf("(-) string alloc=%zu len=%zu\n", z->_alloc, z->_len);

    z = string_free(z);

    setlocale(LC_CTYPE, "en_US.UTF8");

    return 0;
}

/* -------------------------------------------------------------------------- */
