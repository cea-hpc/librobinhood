/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_ITERATOR_H
#define ROBINHOOD_ITERATOR_H

#include <stddef.h>

/** @file
 * An iterator is a standard interface to traverse a collection of objects
 *
 * This interface distinguishes mutable from immutable iterators which
 * respectively yield mutable and immutable references.
 */

/*----------------------------------------------------------------------------*
 |                             immutable iterator                             |
 *----------------------------------------------------------------------------*/

/**
 * Pointers returned by the `next' method of an immutable iterator are
 * guaranteed to stay valid until the iterator that yielded them is destroyed.
 */

struct rbh_iterator {
    const struct rbh_iterator_operations *ops;
};

struct rbh_iterator_operations {
    const void *(*next)(void *iterator);
    void (*destroy)(void *iterator);
};

/**
 * Yield an immutable reference on the next element of an iterator
 *
 * @param iterator  an iterator
 *
 * @return          a const pointer to the next element in the iterator on
 *                  success, NULL on error and errno is set appropriately
 *
 * @error EAGAIN    temporary failure, retry later
 * @error ENODATA   the iterator is exhausted
 */
static inline const void *
_rbh_iter_next(struct rbh_iterator *iterator)
{
    return iterator->ops->next(iterator);
}

/**
 * Yield an immutable reference on the next element of an iterator
 *
 * @param iterator  an iterator
 *
 * @return          a const pointer to the next element in the iterator on
 *                  success, NULL on error and errno is set appropriately
 *
 * @error ENODATA   the iterator is exhausted
 *
 * This function handles temporary failures (errno == EAGAIN) itself.
 */
static inline const void *
rbh_iter_next(struct rbh_iterator *iterator)
{
    int save_errno = errno;
    const void *element;

    do {
        errno = 0;
        element = _rbh_iter_next(iterator);
    } while (element == NULL && errno == EAGAIN);

    errno = errno ? : save_errno;
    return element;
}

/**
 * Free resources associated to a struct rbh_iterator
 *
 * @param iterator  a pointer to the struct rbh_iterator to reclaim
 *
 * \p iterator does not need to be exhausted.
 */
static inline void
rbh_iter_destroy(struct rbh_iterator *iterator)
{
    return iterator->ops->destroy(iterator);
}

struct rbh_tree_iterator {
    const struct rbh_iterator_operations *ops;
    const struct rbh_tree_iterator_operations *t_ops;
};

struct rbh_tree_iterator_operations {
    void *(*browse_sibling)(const void *iterator);
    void (*add_sibling)(void *iterator, void *sibling);
    void (*add_child)(void *iterator, void *child);
};

/**
 * Yield an immutable reference on the next element of a hierarchical iterator
 *
 * @param iterator  a hierarchical iterator
 *
 * @return          a const pointer to the next element in the iterator on
 *                  success, NULL on error and errno is set appropriately
 *
 * @error EAGAIN    temporary failure, retry later
 * @error ENODATA   the iterator is exhausted
 */
static inline const void *
_rbh_tree_iter_next(struct rbh_tree_iterator *iterator)
{
    return iterator->ops->next(iterator);
}

/**
 * Yield an immutable reference on the next element of a hierarchical iterator
 *
 * @param iterator  a hierarchical iterator
 *
 * @return          a const pointer to the next element in the iterator on
 *                  success, NULL on error and errno is set appropriately
 *
 * @error ENODATA   the iterator is exhausted
 *
 * This function handles temporary failures (errno == EAGAIN) itself.
 */
static inline const void *
rbh_tree_iter_next(struct rbh_tree_iterator *iterator)
{
    int save_errno = errno;
    const void *element;

    do {
        errno = 0;
        element = _rbh_tree_iter_next(iterator);
    } while (element == NULL && errno == EAGAIN);

    errno = errno ? : save_errno;
    return element;
}

/**
 * Free resources associated to a struct rbh_tree_iterator
 *
 * @param iterator  a pointer to the struct rbh_tree_iterator to reclaim
 *
 * \p iterator does not need to be exhausted.
 */
static inline void
rbh_tree_iter_destroy(struct rbh_tree_iterator *iterator)
{
    return iterator->ops->destroy(iterator);
}

/**
 * Retrieve a tree iterator associated to the next sibling
 *
 * @param iterator  a pointer to the struct rbh_tree_iterator to browse
 */
static inline void *
rbh_tree_iter_browse_sibling(const struct rbh_tree_iterator *iterator)
{
    return iterator->t_ops->browse_sibling(iterator);
}

/**
 * Add a sibling to a tree iterator
 *
 * @param iterator  a pointer to the struct rbh_tree_iterator to enrich
 * @param sibling   a pointer to the struct rbh_tree_iterator to append
 */
static inline void
rbh_tree_iter_add_sibling(struct rbh_tree_iterator *iterator,
                          struct rbh_tree_iterator *sibling)
{
    return iterator->t_ops->add_sibling(iterator, sibling);
}

/**
 * Add a child to a tree iterator
 *
 * @param iterator  a pointer to the struct rbh_tree_iterator to enrich
 * @param child     a pointer to the struct rbh_tree_iterator to append
 */
static inline void
rbh_tree_iter_add_child(struct rbh_tree_iterator *iterator,
                        struct rbh_tree_iterator *child)
{
    return iterator->t_ops->add_child(iterator, child);
}

/*----------------------------------------------------------------------------*
 |                              mutable iterator                              |
 *----------------------------------------------------------------------------*/

/**
 * Pointers returned by the `next' method of a mutable iterator are "owned" by
 * the caller and are not cleaned up when the iterator is destroyed.
 */

struct rbh_mut_iterator {
    const struct rbh_mut_iterator_operations *ops;
};

struct rbh_mut_iterator_operations {
    void *(*next)(void *iterator);
    void (*destroy)(void *iterator);
};

/**
 * Yield a mutable reference on the next element of an iterator
 *
 * @param iterator  an iterator
 *
 * @return          a const pointer to the next element in the iterator on
 *                  success, NULL on error and errno is set appropriately
 *
 * @error EAGAIN    temporary failure, retry later
 * @error ENODATA   the iterator is exhausted
 */
static inline void *
_rbh_mut_iter_next(struct rbh_mut_iterator *iterator)
{
    return iterator->ops->next(iterator);
}

/**
 * Yield a mutable reference on the next element of an iterator
 *
 * @param iterator  an iterator
 *
 * @return          a const pointer to the next element in the iterator on
 *                  success, NULL on error and errno is set appropriately
 *
 * @error ENODATA   the iterator is exhausted
 *
 * This function handles temporary failures (errno == EAGAIN) itself.
 */
static inline void *
rbh_mut_iter_next(struct rbh_mut_iterator *iterator)
{
    int save_errno = errno;
    void *element;

    do {
        errno = 0;
        element = _rbh_mut_iter_next(iterator);
    } while (element == NULL && errno == EAGAIN);

    errno = errno ? : save_errno;
    return element;
}

/**
 * Free resources associated to a struct rbh_iterator
 *
 * @param iterator  a pointer to the struct rbh_mut_iterator to reclaim
 *
 * \p iterator does not need to be exhausted.
 */
static inline void
rbh_mut_iter_destroy(struct rbh_mut_iterator *iterator)
{
    return iterator->ops->destroy(iterator);
}

struct rbh_mut_tree_iterator {
    const struct rbh_mut_iterator_operations *ops;
    const struct rbh_mut_tree_iterator_operations *t_ops;
};

struct rbh_mut_tree_iterator_operations {
    void *(*browse_sibling)(const void *iterator);
    void (*add_sibling)(void *iterator, void *sibling);
    void (*add_child)(void *iterator, void *child);
};

/**
 * Yield a mutable reference on the next element of a hierarchical iterator
 *
 * @param iterator  a hierarchical iterator
 *
 * @return          a pointer to the next element in the iterator on
 *                  success, NULL on error and errno is set appropriately
 *
 * @error ENODATA   the iterator is exhausted
 */
static inline void *
_rbh_mut_tree_iter_next(struct rbh_mut_tree_iterator *iterator)
{
    return iterator->ops->next(iterator);
}

/**
 * Yield a mutable reference on the next element of a hierarchical iterator
 *
 * @param iterator  a hierarchical iterator
 *
 * @return          a pointer to the next element in the iterator on
 *                  success, NULL on error and errno is set appropriately
 *
 * @error ENODATA   the iterator is exhausted
 *
 * This function handles temporary failures (errno == EAGAIN) itself.
 */
static inline void *
rbh_mut_tree_iter_next(struct rbh_mut_tree_iterator *iterator)
{
    int save_errno = errno;
    void *element;

    do {
        errno = 0;
        element = _rbh_mut_tree_iter_next(iterator);
    } while (element == NULL && errno == EAGAIN);

    errno = errno ? : save_errno;
    return element;
}

/**
 * Free resources associated to a struct rbh_mut_tree_iterator
 *
 * @param iterator  a pointer to the struct rbh_mut_tree_iterator to reclaim
 *
 * \p iterator does not need to be exhausted.
 */
static inline void
rbh_mut_tree_iter_destroy(struct rbh_mut_tree_iterator *iterator)
{
    return iterator->ops->destroy(iterator);
}

/**
 * Retrieve a mutable tree iterator associated to the next sibling
 *
 * @param iterator  a pointer to the struct rbh_mut_tree_iterator to browse
 */
static inline void *
rbh_mut_tree_iter_browse_sibling(const struct rbh_mut_tree_iterator *iterator)
{
    return iterator->t_ops->browse_sibling(iterator);
}

/**
 * Add a sibling to a mutable tree iterator
 *
 * @param iterator  a pointer to the struct rbh_mut_tree_iterator to enrich
 * @param sibling   a pointer to the struct rbh_mut_tree_iterator to append
 */
static inline void
rbh_mut_tree_iter_add_sibling(struct rbh_mut_tree_iterator *iterator,
                              struct rbh_mut_tree_iterator *sibling)
{
    return iterator->t_ops->add_sibling(iterator, sibling);
}

/**
 * Add a child to a mutable tree iterator
 *
 * @param iterator  a pointer to the struct rbh_mut_tree_iterator to enrich
 * @param child     a pointer to the struct rbh_mut_tree_iterator to append
 */
static inline void
rbh_mut_tree_iter_add_child(struct rbh_mut_tree_iterator *iterator,
                            struct rbh_mut_tree_iterator *child)
{
    return iterator->t_ops->add_child(iterator, child);
}

#endif
