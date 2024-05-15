/*
 * Functions for working with phrase lists.
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
 * Phrases are the basic unit of sentence construction. For our purposes,
 * a "phrase" consists of one or more words (in the everyday sense) joined
 * by spaces. Phrases are always considered for addition to a sentence as
 * a whole, so for their constituent words to be considered individually
 * they must also be listed that way. Phrases may include punctuation, to
 * allow for contractions, but not digits.
 */

#ifndef PHRASE_LIST_H
#define PHRASE_LIST_H

#include <ctype.h>          /* for is*() */
#include <stdio.h>          /* for FILE */

#include "letter_pool.h"    /* for pool_t */

/* Path to our default phrase list */
#define PHRASE_LIST_DEFAULT "web2.txt"
#ifdef __unix__ /* this isn't reliably available on other platforms */
#  define PHRASE_LIST_SYSTEM "/usr/share/dict/words"
#endif

struct phrase_list {
    char *phrase;
    size_t length;
    struct phrase_list *next;
};

/*
 * Prototype for a phrase filter function.
 *
 * The phrase filter sanitizes candidate phrases and determines whether
 * they are suitable for constructing anagrams. The default phrase filter
 * is good for most applications, but you can define your own to implement
 * more complex rules like only accepting phrases of a certain length.
 *
 * If the candidate phrase is acceptable, this should return its length
 * excluding the trailing '\0' a la strlen(). Otherwise, it should return 0.
 */
typedef size_t (*phrase_filter_cb)(char *candidate, void *user_data);

/*
 * The default phrase filter.
 *
 * This checks that phrases contain at least one letter and no digits.
 * It allows spaces and punctuation so long as they make up no more than
 * half the characters. It also removes trailing newlines.
 */
size_t phrase_filter_default(char *candidate, void *user_data);

/*
 * Add a phrase to a list.
 *
 * If prev is NULL, this starts a new list.
 * Returns a pointer to the newly added phrase.
 */
struct phrase_list *phrase_list_add(struct phrase_list *prev,
                                    const char *phrase, size_t length,
                                    size_t *count);

/*
 * Free memory used by a phrase list.
 */
void phrase_list_free(struct phrase_list *first);

/*
 * Read a phrase list from a file.
 *
 * The file pointed to by 'fp' should be opened in mode "r".
 * The number of phrases is stored in 'count'.
 *
 * If 'letter_pool' is provided, only words spellable using the letters
 * in the pool will be included in the list. This prevents us from
 * considering phrases we can never use -- a significant optimization.
 *
 * Returns a pointer to the first item in the list.
 */
struct phrase_list *phrase_list_read(struct phrase_list *prev,
                                     FILE *fp,
                                     size_t *count,
                                     pool_t *letter_pool);

/*
 * The same as above, but using your custom phrase filter instead of
 * the default.
 *
 * The 'user_data' parameter is arbitrary data passed directly to the
 * filter function.
 */
struct phrase_list *phrase_list_read_filtered(struct phrase_list *prev,
                                              FILE *fp,
                                              size_t *count,
                                              pool_t *letter_pool,
                                              phrase_filter_cb phrase_filter,
                                              void *user_data);

/*
 * Return the path to our default phrase list.
 *
 * The return value is a static string. Don't free it!
 */
const char *phrase_list_default(void);

/*
 * Use this in your phrase filter to identify non-alphabetic characters
 * that cannot be included in a phrase. Phrases that do contain such
 * characters should be rejected immediately.
 */
#define phrase_cannot_include(c) \
        (iscntrl(c) || !(((c) == ' ') || ispunct(c)))

#endif /* PHRASE_LIST_H */
