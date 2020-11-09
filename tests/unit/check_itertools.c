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

START_TEST(ric_basic)
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

START_TEST(ric_with_null_elements)
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
 |                              rbh_iter_chain()                              |
 *----------------------------------------------------------------------------*/

/*
 *      chain       This test builds a chain iterator consisting in 4 array
 *        |         iterators. The iter_next function on chain should consume
 *  -------------   iterators following the order they are added in,
 *  |   |   |   |   from 0 to 4.
 *  0   1   2   3
 */
START_TEST(rih_basic)
{
    size_t TEST_CHAIN_SIZE = 4;
    struct rbh_iterator *tests[TEST_CHAIN_SIZE];
    struct rbh_iterator *chain;

    const char STRING[] = "abcdefghijklmno";

    for (size_t i = 0; i < TEST_CHAIN_SIZE; ++i)
        tests[i] = rbh_iter_array(&STRING[4 * i], sizeof(*STRING), 4);

    chain = rbh_iter_chain(4, tests[0], tests[1], tests[2], tests[3]);

    for (size_t i = 0; i < sizeof(STRING); ++i)
        ck_assert_mem_eq(rbh_iter_next(chain), &STRING[i], sizeof(*STRING));

    errno = 0;
    ck_assert_ptr_null(rbh_iter_next(chain));
    ck_assert_int_eq(errno, ENODATA);

    rbh_iter_destroy(chain);
}
END_TEST

/*
 *      chain       This test builds a chain iterator consisting in 4 array
 *        |         iterators distributed among 2 sub chain iterators.
 *      -----       The iter_next function on chain should consume
 *      |   |       iterators following the given order, from 0 to 4.
 *     sc1 sc2
 *      |   |
 *  -----   -----
 *  |   |   |   |
 *  0   1   2   3
 */
START_TEST(rih_double_layer)
{
    size_t TEST_CHAIN_SIZE = 4;
    struct rbh_iterator *tests[TEST_CHAIN_SIZE];
    struct rbh_iterator *subchains[2];
    struct rbh_iterator *chain;

    const char STRING[] = "abcdefghijklmno";

    for (size_t i = 0; i < TEST_CHAIN_SIZE; ++i)
        tests[i] = rbh_iter_array(&STRING[4 * i], sizeof(*STRING), 4);

    subchains[0] = rbh_iter_chain(2, tests[0], tests[1]);
    subchains[1] = rbh_iter_chain(2, tests[2], tests[3]);
    chain = rbh_iter_chain(2, subchains[0], subchains[1]);

    for (size_t i = 0; i < sizeof(STRING); ++i)
        ck_assert_mem_eq(rbh_iter_next(chain), &STRING[i], sizeof(*STRING));

    errno = 0;
    ck_assert_ptr_null(rbh_iter_next(chain));
    ck_assert_int_eq(errno, ENODATA);

    rbh_iter_destroy(chain);
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
    tcase_add_test(tests, ric_basic);
    tcase_add_test(tests, ric_with_null_elements);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_iter_tee()");
    tcase_add_test(tests, rit_basic);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_chain_iter()");
    tcase_add_test(tests, rih_basic);
    tcase_add_test(tests, rih_double_layer);

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
