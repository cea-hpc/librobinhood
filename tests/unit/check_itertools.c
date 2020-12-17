/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>

#include "check-compat.h"
#include "robinhood/itertools.h"

/*----------------------------------------------------------------------------*
 |                              rbh_iter_array()                              |
 *----------------------------------------------------------------------------*/

START_TEST(ria_basic)
{
    const char STRING[] = "abcdefghijklmno";
    struct rbh_iterator *letters;

    letters = rbh_iter_array(STRING, sizeof(*STRING), sizeof(STRING));
    ck_assert_ptr_nonnull(letters);

    for (size_t i = 0; i < sizeof(STRING); i++)
        ck_assert_mem_eq(rbh_iter_next(letters), &STRING[i], sizeof(*STRING));

    errno = 0;
    ck_assert_ptr_null(rbh_iter_next(letters));
    ck_assert_int_eq(errno, ENODATA);

    rbh_iter_destroy(letters);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                            rbh_iter_chunkify()                             |
 *----------------------------------------------------------------------------*/

START_TEST(rich_basic)
{
    const char STRING[] = "abcdefghijklmno";
    const size_t CHUNK_SIZE = 4;
    struct rbh_mut_iterator *chunks;
    struct rbh_iterator *letters;

    ck_assert_uint_eq(sizeof(STRING) % CHUNK_SIZE, 0);

    letters = rbh_iter_array(STRING, sizeof(*STRING), sizeof(STRING));
    ck_assert_ptr_nonnull(letters);

    chunks = rbh_iter_chunkify(letters, CHUNK_SIZE);
    ck_assert_ptr_nonnull(chunks);

    for (size_t i = 0; i < sizeof(STRING) / CHUNK_SIZE; i++) {
        struct rbh_iterator *chunk;

        chunk = rbh_mut_iter_next(chunks);
        ck_assert_ptr_nonnull(chunk);

        for (size_t j = 0; j < CHUNK_SIZE; j++)
            ck_assert_mem_eq(rbh_iter_next(chunk), &STRING[i * CHUNK_SIZE + j],
                             sizeof(*STRING));

        errno = 0;
        ck_assert_ptr_null(rbh_iter_next(chunk));
        ck_assert_int_eq(errno, ENODATA);

        rbh_iter_destroy(chunk);
    }

    errno = 0;
    ck_assert_ptr_null(rbh_mut_iter_next(chunks));
    ck_assert_int_eq(errno, ENODATA);

    rbh_mut_iter_destroy(chunks);
}
END_TEST

static const void *
null_iter_next(void *iterator)
{
    return NULL;
}

static void
null_iter_destroy(void *iterator)
{
    ;
}

static const struct rbh_iterator_operations NULL_ITER_OPS = {
    .next = null_iter_next,
    .destroy = null_iter_destroy,
};

static const struct rbh_iterator NULL_ITER = {
    .ops = &NULL_ITER_OPS,
};

START_TEST(rich_with_null_elements)
{
    struct rbh_iterator nulls = NULL_ITER;
    struct rbh_mut_iterator *chunks;
    struct rbh_iterator *chunk;
    const size_t CHUNK_SIZE = 3;

    chunks = rbh_iter_chunkify(&nulls, CHUNK_SIZE);
    ck_assert_ptr_nonnull(chunks);

    chunk = rbh_mut_iter_next(chunks);
    ck_assert_ptr_nonnull(chunk);

    for (size_t i = 0; i < CHUNK_SIZE; i++) {
        errno = 0;
        ck_assert_ptr_null(rbh_iter_next(chunk));
        ck_assert_int_eq(errno, 0);
    }

    errno = 0;
    ck_assert_ptr_null(rbh_iter_next(chunk));
    ck_assert_int_eq(errno, ENODATA);

    rbh_iter_destroy(chunk);
    rbh_mut_iter_destroy(chunks);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                               rbh_iter_tee()                               |
 *----------------------------------------------------------------------------*/

START_TEST(rit_basic)
{
    const char STRING[] = "abcdefghijklmno";
    struct rbh_iterator *tees[2];
    struct rbh_iterator *letters;

    letters = rbh_iter_array(STRING, sizeof(*STRING), sizeof(STRING));
    ck_assert_ptr_nonnull(letters);

    ck_assert_int_eq(rbh_iter_tee(letters, tees), 0);

    for (size_t i = 0; i < sizeof(STRING); i++)
        ck_assert_mem_eq(rbh_iter_next(tees[0]), &STRING[i], sizeof(*STRING));

    errno = 0;
    ck_assert_ptr_null(rbh_iter_next(tees[0]));
    ck_assert_int_eq(errno, ENODATA);

    rbh_iter_destroy(tees[0]);

    for (size_t i = 0; i < sizeof(STRING); i++)
        ck_assert_mem_eq(rbh_iter_next(tees[1]), &STRING[i], sizeof(*STRING));

    errno = 0;
    ck_assert_ptr_null(rbh_iter_next(tees[1]));
    ck_assert_int_eq(errno, ENODATA);

    rbh_iter_destroy(tees[1]);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                            rbh_iter_constify()                             |
 *----------------------------------------------------------------------------*/

struct ascii_iterator {
    struct rbh_mut_iterator iterator;
    unsigned char c;
};

static void *
ascii_iter_next(void *iterator)
{
    struct ascii_iterator *ascii = iterator;
    char *c;

    c = malloc(sizeof(*c));
    if (c == NULL)
        return NULL;

    *c = ascii->c++;
    return c;
}

static void
ascii_iter_destroy(void *iterator)
{
    return;
}

static const struct rbh_mut_iterator_operations ASCII_ITER_OPS = {
    .next = ascii_iter_next,
    .destroy = ascii_iter_destroy,
};

static const struct rbh_mut_iterator ASCII_ITERATOR = {
    .ops = &ASCII_ITER_OPS,
};

/* We have to rely on libasan to test that memory is properly deallocated */
START_TEST(rico_basic)
{
    const char STRING[] = "abcdefghijklmno";
    struct ascii_iterator _ascii;
    struct rbh_iterator *ascii;

    _ascii.iterator = ASCII_ITERATOR;
    _ascii.c = 'a';

    ascii = rbh_iter_constify(&_ascii.iterator);
    ck_assert_ptr_nonnull(ascii);

    for (size_t i = 0; i < sizeof(STRING) - 1; i++)
        ck_assert_mem_eq(rbh_iter_next(ascii), &STRING[i], sizeof(*STRING));

    rbh_iter_destroy(ascii);
}
END_TEST

static Suite *
unit_suite(void)
{
    Suite *suite;
    TCase *tests;

    suite = suite_create("itertools");
    tests = tcase_create("rbh_iter_array()");
    tcase_add_test(tests, ria_basic);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_iter_chunkify()");
    tcase_add_test(tests, rich_basic);
    tcase_add_test(tests, rich_with_null_elements);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_iter_tee()");
    tcase_add_test(tests, rit_basic);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_iter_constify()");
    tcase_add_test(tests, rico_basic);

    suite_add_tcase(suite, tests);

    return suite;
}

int
main(void)
{
    int number_failed;
    Suite *suite;
    SRunner *runner;

    suite = unit_suite();
    runner = srunner_create(suite);

    srunner_run_all(runner, CK_NORMAL);
    number_failed = srunner_ntests_failed(runner);
    srunner_free(runner);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
