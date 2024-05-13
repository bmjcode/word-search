/*
 * Functions for building a sentence from a phrase list.
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

#ifndef SENTENCE_H
#define SENTENCE_H

#include <stdbool.h>

#include "letter_pool.h"
#include "phrase_list.h"

/*
 * Structure representing the state of sentence_build().
 *
 * To access the completed sentence, assign a callback function to
 * sentence_cb, which receives it as one of its parameters.
 */
struct sentence_info {
    pool_t pool[POOL_SIZE];
    struct phrase_list *phrase_list;
    size_t phrase_count;
    void *user_data; /* arbitrary data passed to callback functions */

    /* Use these to impose constraints on sentence building */
    size_t max_words; /* max number of words in a sentence (0 for unlimited) */

    /* Use these to divide the phrase list among multiple threads */
    size_t step;    /* use every nth word */
    size_t offset;  /* skip the first n words */

    /*
     * Callback function to interrupt sentence_build().
     * Return true if the operation has been canceled, false otherwise.
     *
     * Note sentence_build() uses a recursive inner loop, so this function
     * may be called multiple times before the former returns. This means
     * that, for example, to stop after a certain number of cycles you should
     * use >= rather than == in your test condition.
     */
    bool (*canceled_cb)(void *user_data);

    /*
     * Callback function to implement a phrase filter.
     * Return true to accept a candidate, false to reject it.
     *
     * This is called once just before sentence-building begins.
     * You might use it to implement a profanity filter or to only
     * accept words of a certain length.
     *
     * If no filter is specified, all candidates are accepted.
     */
    bool (*phrase_filter_cb)(char *candidate, void *user_data);

    /*
     * Callback function to implement a phrase check.
     * Return true to accept a candidate, false to reject it.
     *
     * This is called each time a new phrase is about to be added
     * to the sentence. You might use it to restrict where a particular
     * phrase may be added to the sentence.
     *
     * The sentence parameter contains the sentence in progress before
     * the candidate phrase is added.
     *
     * If no filter is specified, all candidates are accepted.
     */
    bool (*phrase_check_cb)(char *candidate, char *sentence, void *user_data);

    /*
     * Callback function to indicate we have a new first phrase.
     * This is called before building any sentences with it.
     */
    void (*first_phrase_cb)(char *candidate, void *user_data);

    /*
     * Callback function to track sentence_build()'s progress.
     * This is called after all sentences starting with the current
     * first phrase have been built.
     */
    void (*progress_cb)(void *user_data);

    /*
     * Callback function when a sentence is completed.
     * If none is specified, the sentence is printed to stdout.
     *
     * Memory for the 'sentence' parameter belongs to sentence_build(),
     * which frees it when it returns. If you need the completed sentence
     * longer, copy it to your own memory.
    */
    void (*sentence_cb)(char *sentence, void *user_data);
};

/*
 * Initialize a sentence_info structure.
 */
void sentence_info_init(struct sentence_info *si);

/*
 * Build a "sentence" using phrases formed from letters in the pool.
 * For our purposes a sentence is any combination of one or more phrases
 * separated by spaces.
 *
 * To run in multiple threads, create a separate struct sentence_info
 * for each thread. All threads may share the same phrase list (and should
 * to reduce memory usage). Set si->step to the total number of threads,
 * and si->offset to the index of the individual thread.
 */
void sentence_build(struct sentence_info *si);

#endif /* SENTENCE_H */
