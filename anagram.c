/*
 * Find anagrams of a word or phrase.
 * Copyright (c) 2023 Benjamin Johnson <bmjcode@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Usage: ./anagram_search phrase /path/to/phrase/list
 *
 * For example, to find anagrams of the word "leprechaun", you could try:
 * ./anagram_search leprechaun /usr/share/dict/words
 *
 * The formatting of the phrase list is one per line, case-sensitive.
 * Found anagrams are printed to stdout.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "letter_pool.h"
#include "phrase_list.h"
#include "sentence.h"

static void usage(FILE *stream, char *prog_name);

void
usage(FILE *stream, char *prog_name)
{
    fprintf(stream,
            "Find anagrams of a word or phrase.\n"
            "Usage: %s [-h] [-l PATH] subject\n"
            "  -h       Display this help message and exit\n"
            "  -l PATH  Override the default phrase list\n",
            prog_name);
}

int
main(int argc, char **argv)
{
    FILE *fp;
    struct sentence_info si;
    char *list_path;
    unsigned int pool[POOL_SIZE];
    int i, opt;

    pool_reset(pool);
    sentence_info_init(&si);
    si.pool = pool;

    list_path = NULL;
    while ((opt = getopt(argc, argv, "hl:")) != -1) {
        switch (opt) {
            case 'h':
                /* Display help and exit */
                usage(stdout, argv[0]);
                return 0;
            case 'l':
                /* Override the default phrase list */
                list_path = optarg;
                break;
        }
    }

    /* The remaining command-line arguments specify the subject */
    if (optind >= argc) {
        usage(stderr, argv[0]);
        return 1;
    }
    for (i = optind; i < argc; ++i)
        pool_add(pool, argv[i]);

    /* Prefer our included phrase list if none is specified */
    /* FIXME: This is not a safe way to locate this file */
    if (list_path == NULL) {
        if (access("web2.txt", R_OK) == 0)
            list_path = "web2.txt";
#ifdef __unix__
        else if (access("/usr/share/dict/words", R_OK) == 0)
            list_path = "/usr/share/dict/words";
#endif
    }

    if ((fp = fopen(list_path, "r")) == NULL) {
        fprintf(stderr,
                "Failed to open: %s\n",
                list_path);
        return 1;
    } else {
        si.phrase_list = phrase_list_read(NULL, fp, &si.phrase_count);
        fclose(fp);
    }

    if (si.phrase_list == NULL) {
        fprintf(stderr,
                "Failed to read phrase list: %s\n",
                list_path);
        return 1;
    }

    /* Search for valid sentences */
    sentence_build(&si);

    phrase_list_free(si.phrase_list);
    return 0;
}