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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef ENABLE_THREADING
#  include <pthread.h>
#  include <signal.h>
#endif /* ENABLE_THREADING */

#include "sentence.h"

static void sentence_build_inner(struct sentence_info *si,
                                 char *write_pos,
                                 char **phrases,
                                 size_t phrase_count,
                                 size_t depth);
#ifdef ENABLE_THREADING
/* Wrapper to call sentence_build() in a thread */
static void *run_thread(void *si);
#endif /* ENABLE_THREADING */

void
sentence_info_init(struct sentence_info *si, pool_t *pool)
{
    if (si == NULL)
        return;

    si->phrase_list = NULL;
    si->phrase_count = 0;
    si->pool = pool;
    si->sentence = NULL;
    si->max_length = 0;

    si->check_cb = NULL;
    si->done_cb = NULL;
    si->user_data = NULL;

    si->step = 1;
    si->offset = 0;
}

void
sentence_build(struct sentence_info *si)
{
    struct phrase_list *lp; /* list pointer */
    char **phrases, **dst;

    if ((si == NULL)
        || (si->pool == NULL)
        || (si->phrase_list == NULL)
        || (si->phrase_count == 0))
        return;

    /* Allocate enough memory for the longest possible sentence:
     * all single-letter words with a space or '\0' after each. */
    si->max_length = 2 * pool_count_all(si->pool);
    si->sentence = malloc(si->max_length * sizeof(char));
    if (si->sentence == NULL)
        return; /* this could be a problem */
    memset(si->sentence, 0, si->max_length * sizeof(char));

    phrases = malloc((si->phrase_count + 1) * sizeof(char*));
    if (phrases == NULL)
        return; /* this may be a problem */

    /* Flatten the phrase list into an array of char* pointers with a
     * terminating NULL pointer. This is a more practical working format
     * than the original linked list since it requires only one (slow)
     * memory allocation to construct a duplicate, as we do when we filter
     * our working list in sentence_build_inner(). */
    for (lp = si->phrase_list, dst = phrases;
         lp != NULL;
         lp = lp->next) {
        *dst++ = lp->phrase;
    }
    *dst = NULL;

    sentence_build_inner(si, NULL, phrases, si->phrase_count, 0);

    free(si->sentence);
    free(phrases);
    si->sentence = NULL;
    si->max_length = 0;
}

void sentence_build_inner(struct sentence_info *si,
                          char *write_pos,
                          char **phrases,
                          size_t phrase_count,
                          size_t depth)
{
    char **prev, **curr, *n, *p;
    size_t i;

    if ((si == NULL)
        || (si->pool == NULL)
        || (phrases == NULL)
        || (phrase_count == 0))
        return;

    /* Add the next word starting at this position in si->sentence. */
    if (write_pos == NULL)
        write_pos = si->sentence;

    /* Filter our working list to remove phrases we can't spell with the
     * letters in the current pool. If a check_cb function was specified,
     * also remove phrases that don't pass validation. */
    for (prev = curr = phrases; *prev != NULL; ++prev) {
        if (!pool_can_spell(si->pool, *prev)) {
            --phrase_count;
            continue;
        } else if (!((si->check_cb == NULL)
                   || si->check_cb(si, *prev))) {
            --phrase_count;
            continue;
        }
        *curr++ = *prev;
    }
    *curr = NULL;

    curr = phrases;
    if (depth == 0) {
        /* If this is the outermost loop, skip the number of phrases
         * specified by 'offset'. */
        for (i = 0; i < si->offset; ++i) {
            ++curr;
            if (*curr == NULL)
                return;
        }
    }

    while (*curr != NULL) {
        /* Remove this phrase's letters from the pool. */
        pool_subtract(si->pool, *curr);

        /* Add this phrase to our sentence.
         * Copying bytes manually is more efficient than using the standard
         * library functions, whose execution time grows proportionally with
         * the length of the destination string. We can safely assume this
         * won't overflow because something else would have already overflowed
         * long before we got here. */
        n = write_pos;
        p = *curr;
        while (*p != '\0')
            *n++ = *p++;

        if (pool_is_empty(si->pool)) {
            /* We've completed a sentence! */
            *n = '\0';
            if (si->done_cb == NULL)
                printf("%s\n", si->sentence);
            else
                si->done_cb(si);
        } else {
            char **new_phrases = malloc((phrase_count + 1) * sizeof(char*));
            if (new_phrases != NULL) {
                memcpy(new_phrases, phrases,
                       (phrase_count + 1) * sizeof(char*));

                /* Note that si->sentence may not be null-terminated yet,
                 * but this is fine since it's only used within this function
                 * until we exhaust our letter pool. */
                *n++ = ' ';

                /* Call this function recursively to extend the sentence. */
                sentence_build_inner(si, n,
                                     new_phrases, phrase_count, depth + 1);
                free(new_phrases);
            }
        }

        /* Restore used letters to the pool for the next cycle. */
        pool_add(si->pool, *curr);

        /* If this is the outermost loop, advance by the number of phrases
         * specified by 'step'. Otherwise, advance to the next phrase. */
        for (i = 0; i < (depth == 0 ? si->step : 1); ++i) {
            ++curr;
            if (*curr == NULL)
                break;
        }
    }
}

void
sentence_build_threaded(struct sentence_info *si,
                        unsigned short num_threads)
{
#ifdef ENABLE_THREADING
    int s;
    unsigned short i;
    pthread_t *thread_id;
    pthread_attr_t attr;
    struct sentence_info **tsi;

    if (si == NULL)
        return;

    if (num_threads <= 0) {
        fprintf(stderr,
                "Invalid number of threads: %d\n",
                num_threads);
        return;
    } else if (num_threads == 1) {
        /* We don't need all this overhead for one thread */
        sentence_build(si);
        return;
    }

    thread_id = NULL;
    tsi = NULL;

    s = pthread_attr_init(&attr);
    if (s != 0)
        goto cleanup;

    thread_id = malloc(num_threads * sizeof(pthread_t));
    if (thread_id == NULL)
        goto cleanup;

    /* Create a struct sentence_info for each thread */
    tsi = malloc(num_threads * sizeof(struct sentence_info*));
    if (tsi == NULL)
        goto cleanup;

    for (i = 0; i < num_threads; ++i) {
        tsi[i] = malloc(sizeof(struct sentence_info));
        if (tsi[i] == NULL)
            goto cleanup;

        /* Divide the phrase list among the threads */
        memcpy(tsi[i], si, sizeof(struct sentence_info));
        tsi[i]->step = num_threads;
        tsi[i]->offset = i;

        /* Each thread needs its own letter pool */
        tsi[i]->pool = malloc(POOL_SIZE * sizeof(pool_t));
        if (tsi[i]->pool == NULL)
            goto cleanup;
        pool_copy(si->pool, tsi[i]->pool);
    }

    /* Start each thread */
    for (i = 0; i < num_threads; ++i) {
        s = pthread_create(&thread_id[i],
                           &attr,
                           run_thread,
                           tsi[i]);
        if (s != 0) {
            while (--i >= 0)
                pthread_kill(thread_id[i], SIGTERM);
            goto cleanup;
        }
    }

    /* Wait for each thread to finish */
    for (i = 0; i < num_threads; ++i) {
        s = pthread_join(thread_id[i], NULL);
        if (s != 0) {
            while (--i >= 0)
                pthread_kill(thread_id[i], SIGTERM);
            goto cleanup;
        }
    }

cleanup:
    pthread_attr_destroy(&attr);

    if (thread_id != NULL)
        free(thread_id);

    if (tsi != NULL) {
        for (i = 0; i < num_threads; ++i) {
            if (tsi[i] != NULL) {
                if (tsi[i]->pool != NULL)
                    free(tsi[i]->pool);
                free(tsi[i]);
            }
        }
        free(tsi);
    }
#else /* ENABLE_THREADING */
    if (num_threads <= 0) {
        fprintf(stderr,
                "Invalid number of threads: %d\n",
                num_threads);
        return;
    } else if (num_threads > 1)
        fprintf(stderr,
                "Warning: Threading unavailable\n");
    sentence_build(si);
#endif /* ENABLE_THREADING */
}

#ifdef ENABLE_THREADING
void *
run_thread(void *si)
{
    sentence_build(si);
    return NULL;
}
#endif /* ENABLE_THREADING */
