/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <errno.h>
#include <stdlib.h>

#include "robinhood/hashmap.h"

#include "check-compat.h"

static bool
strequals(const void *x, const void *y)
{
    return strcmp(x, y) == 0;
}

static size_t
djb2(const void *key)
{
    const char *string = key;
    size_t hash = 5381;
    int c;

    while ((c = *string++))
        hash = ((hash << 5) + hash) + c;

    return hash;
}

/*----------------------------------------------------------------------------*
 |                              rbh_hashmap_new                               |
 *----------------------------------------------------------------------------*/

START_TEST(rhn_zero)
{
    struct rbh_hashmap *hashmap;

    errno = 0;
    hashmap = rbh_hashmap_new(strequals, djb2, 0);
    ck_assert_ptr_null(hashmap);
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(rhn_basic)
{
    struct rbh_hashmap *hashmap;

    hashmap = rbh_hashmap_new(strequals, djb2, 1);
    ck_assert_ptr_nonnull(hashmap);

    rbh_hashmap_destroy(hashmap);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                              rbh_hashmap_set                               |
 *----------------------------------------------------------------------------*/

START_TEST(rhs_basic)
{
    struct rbh_hashmap *hashmap;
    int rc;

    hashmap = rbh_hashmap_new(strequals, djb2, 1);
    ck_assert_ptr_nonnull(hashmap);

    rc = rbh_hashmap_set(hashmap, "abcdefg", "hijklmn");
    ck_assert_int_eq(rc, 0);

    rbh_hashmap_destroy(hashmap);
}
END_TEST

START_TEST(rhs_replace)
{
    struct rbh_hashmap *hashmap;
    const char *value;
    int rc;

    hashmap = rbh_hashmap_new(strequals, djb2, 1);
    ck_assert_ptr_nonnull(hashmap);

    rc = rbh_hashmap_set(hashmap, "abcdefg", "hijklmn");
    ck_assert_int_eq(rc, 0);

    rc = rbh_hashmap_set(hashmap, "abcdefg", "opqrstu");
    ck_assert_int_eq(rc, 0);

    value = rbh_hashmap_get(hashmap, "abcdefg");
    ck_assert_ptr_nonnull(value);
    ck_assert_str_eq(value, "opqrstu");

    rbh_hashmap_destroy(hashmap);
}
END_TEST

START_TEST(rhs_full)
{
    struct rbh_hashmap *hashmap;
    int rc;

    hashmap = rbh_hashmap_new(strequals, djb2, 1);
    ck_assert_ptr_nonnull(hashmap);

    rc = rbh_hashmap_set(hashmap, "abcdefg", "hijklmn");
    ck_assert_int_eq(rc, 0);

    errno = 0;
    rc = rbh_hashmap_set(hashmap, "opqrstu", "vwxyz01");
    ck_assert_int_eq(rc, -1);
    ck_assert_int_eq(errno, ENOBUFS);

    rbh_hashmap_destroy(hashmap);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                              rbh_hashmap_get                               |
 *----------------------------------------------------------------------------*/

START_TEST(rhg_basic)
{
    struct rbh_hashmap *hashmap;
    const void *value;
    int rc;

    hashmap = rbh_hashmap_new(strequals, djb2, 1);
    ck_assert_ptr_nonnull(hashmap);

    rc = rbh_hashmap_set(hashmap, "abcdefg", "hijklmn");
    ck_assert_int_eq(rc, 0);

    value = rbh_hashmap_get(hashmap, "abcdefg");
    ck_assert_ptr_nonnull(value);
    ck_assert_str_eq(value, "hijklmn");

    rbh_hashmap_destroy(hashmap);
}
END_TEST

START_TEST(rhg_missing)
{
    struct rbh_hashmap *hashmap;
    const void *value;

    hashmap = rbh_hashmap_new(strequals, djb2, 1);
    ck_assert_ptr_nonnull(hashmap);

    errno = 0;
    value = rbh_hashmap_get(hashmap, "abcdefg");
    ck_assert_ptr_null(value);
    ck_assert_int_eq(errno, ENOENT);

    rbh_hashmap_destroy(hashmap);
}
END_TEST

START_TEST(rhg_null)
{
    struct rbh_hashmap *hashmap;
    const void *value;
    int rc;

    hashmap = rbh_hashmap_new(strequals, djb2, 1);
    ck_assert_ptr_nonnull(hashmap);

    rc = rbh_hashmap_set(hashmap, "abcdefg", NULL);
    ck_assert_int_eq(rc, 0);

    errno = 0;
    value = rbh_hashmap_get(hashmap, "abcdefg");
    ck_assert_ptr_null(value);
    ck_assert_int_eq(errno, 0);

    rbh_hashmap_destroy(hashmap);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                              rbh_hashmap_pop                               |
 *----------------------------------------------------------------------------*/

START_TEST(rhp_missing)
{
    struct rbh_hashmap *hashmap;
    const void *value;

    hashmap = rbh_hashmap_new(strequals, djb2, 1);
    ck_assert_ptr_nonnull(hashmap);

    errno = 0;
    value = rbh_hashmap_pop(hashmap, "abcdefg");
    ck_assert_ptr_null(value);
    ck_assert_int_eq(errno, ENOENT);

    rbh_hashmap_destroy(hashmap);
}
END_TEST

START_TEST(rhp_basic)
{
    struct rbh_hashmap *hashmap;
    const void *value;
    int rc;

    hashmap = rbh_hashmap_new(strequals, djb2, 1);
    ck_assert_ptr_nonnull(hashmap);

    rc = rbh_hashmap_set(hashmap, "abcdefg", "hijklmn");
    ck_assert_int_eq(rc, 0);

    value = rbh_hashmap_pop(hashmap, "abcdefg");
    ck_assert_ptr_nonnull(value);
    ck_assert_str_eq(value, "hijklmn");

    errno = 0;
    value = rbh_hashmap_pop(hashmap, "abcdefg");
    ck_assert_ptr_null(value);
    ck_assert_int_eq(errno, ENOENT);

    rbh_hashmap_destroy(hashmap);
}
END_TEST

static Suite *
unit_suite(void)
{
    Suite *suite;
    TCase *tests;

    suite = suite_create("unit tests");

    tests = tcase_create("rbh_hashmap_new");
    tcase_add_test(tests, rhn_zero);
    tcase_add_test(tests, rhn_basic);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_hashmap_set");
    tcase_add_test(tests, rhs_basic);
    tcase_add_test(tests, rhs_replace);
    tcase_add_test(tests, rhs_full);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_hashmap_get");
    tcase_add_test(tests, rhg_basic);
    tcase_add_test(tests, rhg_missing);
    tcase_add_test(tests, rhg_null);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_hashmap_pop");
    tcase_add_test(tests, rhp_missing);
    tcase_add_test(tests, rhp_basic);

    suite_add_tcase(suite, tests);

    return suite;
}

int
main(void)
{
    int number_failed;
    SRunner *runner;

    runner = srunner_create(unit_suite());

    srunner_run_all(runner, CK_NORMAL);
    number_failed = srunner_ntests_failed(runner);
    srunner_free(runner);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
