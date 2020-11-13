/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <stdlib.h>

#include <sys/stat.h>

/* This backend uses libmongoc, from the "mongo-c-driver" project to interact
 * with a MongoDB database.
 *
 * The documentation for the project can be found at: https://mongoc.org
 */

#include <bson.h>
#include <mongoc.h>

#include "robinhood/backends/mongo.h"
#include "robinhood/itertools.h"
#ifndef HAVE_STATX
# include "robinhood/statx.h"
#endif

#include "mongo.h"

/* libmongoc imposes that mongoc_init() be called before any other mongoc_*
 * function; and mongoc_cleanup() after the last one.
 */

__attribute__((constructor))
static void
mongo_init(void)
{
    mongoc_init();
}

__attribute__((destructor))
static void
mongo_cleanup(void)
{
    mongoc_cleanup();
}

    /*--------------------------------------------------------------------*
     |                     bson_pipeline_from_filter                      |
     *--------------------------------------------------------------------*/

static bson_t *
bson_pipeline_from_filter(const struct rbh_filter *filter)
{
    bson_t *pipeline = bson_new();
    bson_t array;
    bson_t stage;

    if (BSON_APPEND_ARRAY_BEGIN(pipeline, "pipeline", &array)
     && BSON_APPEND_DOCUMENT_BEGIN(&array, "0", &stage)
     && BSON_APPEND_UTF8(&stage, "$unwind", "$" MFF_NAMESPACE)
     && bson_append_document_end(&array, &stage)
     && BSON_APPEND_DOCUMENT_BEGIN(&array, "1", &stage)
     && BSON_APPEND_RBH_FILTER(&stage, "$match", filter)
     && bson_append_document_end(&array, &stage)
     && bson_append_array_end(pipeline, &array))
        return pipeline;

    bson_destroy(pipeline);
    errno = ENOBUFS;
    return NULL;
}

/*----------------------------------------------------------------------------*
 |                               mongo_iterator                               |
 *----------------------------------------------------------------------------*/

struct mongo_iterator {
    struct rbh_mut_iterator iterator;
    mongoc_cursor_t *cursor;
};

static void *
mongo_iter_next(void *iterator)
{
    struct mongo_iterator *mongo_iter = iterator;
    int save_errno = errno;
    const bson_t *doc;

    errno = 0;
    if (mongoc_cursor_next(mongo_iter->cursor, &doc)) {
        errno = save_errno;
        return fsentry_from_bson(doc);
    }

    errno = errno ? : ENODATA;
    return NULL;
}

static void
mongo_iter_destroy(void *iterator)
{
    struct mongo_iterator *mongo_iter = iterator;

    mongoc_cursor_destroy(mongo_iter->cursor);
    free(mongo_iter);
}

static const struct rbh_mut_iterator_operations MONGO_ITER_OPS = {
    .next = mongo_iter_next,
    .destroy = mongo_iter_destroy,
};

static const struct rbh_mut_iterator MONGO_ITER = {
    .ops = &MONGO_ITER_OPS,
};

static struct mongo_iterator *
mongo_iterator_new(mongoc_cursor_t *cursor)
{
    struct mongo_iterator *mongo_iter;

    mongo_iter = malloc(sizeof(*mongo_iter));
    if (mongo_iter == NULL)
        return NULL;

    mongo_iter->iterator = MONGO_ITER;
    mongo_iter->cursor = cursor;

    return mongo_iter;
}

/*----------------------------------------------------------------------------*
 |                             MONGO_BACKEND_OPS                              |
 *----------------------------------------------------------------------------*/

struct mongo_backend {
    struct rbh_backend backend;
    mongoc_uri_t *uri;
    mongoc_client_t *client;
    mongoc_database_t *db;
    mongoc_collection_t *entries;
};

    /*--------------------------------------------------------------------*
     |                               update                               |
     *--------------------------------------------------------------------*/

static mongoc_bulk_operation_t *
_mongoc_collection_create_bulk_operation(
        mongoc_collection_t *collection, bool ordered,
        mongoc_write_concern_t *write_concern
        )
{
#if MONGOC_CHECK_VERSION(1, 9, 0)
    bson_t opts;

    bson_init(&opts);

    if (!BSON_APPEND_BOOL(&opts, "ordered", ordered)) {
        errno = ENOBUFS;
        return NULL;
    }

    if (write_concern && !mongoc_write_concern_append(write_concern, &opts)) {
        errno = EINVAL;
        return NULL;
    }

    return mongoc_collection_create_bulk_operation_with_opts(collection, &opts);
#else
    return mongoc_collection_create_bulk_operation(collection, ordered,
                                                   write_concern);
#endif
}

static bool
_mongoc_bulk_operation_update_one(mongoc_bulk_operation_t *bulk,
                                  const bson_t *selector, const bson_t *update,
                                  bool upsert)
{
#if MONGOC_CHECK_VERSION(1, 7, 0)
    bson_t opts;

    bson_init(&opts);
    if (!BSON_APPEND_BOOL(&opts, "upsert", upsert)) {
        errno = ENOBUFS;
        return NULL;
    }

    /* TODO: handle errors */
    return mongoc_bulk_operation_update_one_with_opts(bulk, selector, update,
                                                      &opts, NULL);
#else
    mongoc_bulk_operation_update_one(bulk, selector, update, upsert);
    return true;
#endif
}

static bool
_mongoc_bulk_operation_remove_one(mongoc_bulk_operation_t *bulk,
                                  const bson_t *selector)
{
#if MONGOC_CHECK_VERSION(1, 7, 0)
    /* TODO: handle errors */
    return mongoc_bulk_operation_remove_one_with_opts(bulk, selector, NULL,
                                                      NULL);
#else
    mongoc_bulk_operation_remove_one(bulk, selector);
    return true;
#endif
}

static bson_t *
bson_selector_from_fsevent(const struct rbh_fsevent *fsevent)
{
    bson_t *selector = bson_new();
    bson_t namespace;
    bson_t elem_match;

    if (!BSON_APPEND_RBH_ID(selector, MFF_ID, &fsevent->id))
        goto out_bson_destroy;

    if (fsevent->type != RBH_FET_XATTR || fsevent->ns.parent_id == NULL)
        return selector;
    assert(fsevent->ns.name);

    if (BSON_APPEND_DOCUMENT_BEGIN(selector, MFF_NAMESPACE, &namespace)
     && BSON_APPEND_DOCUMENT_BEGIN(&namespace, "$elemMatch", &elem_match)
     && BSON_APPEND_RBH_ID(&elem_match, MFF_PARENT_ID, fsevent->ns.parent_id)
     && BSON_APPEND_UTF8(&elem_match, MFF_NAME, fsevent->ns.name)
     && bson_append_document_end(&namespace, &elem_match)
     && bson_append_document_end(selector, &namespace))
        return selector;

out_bson_destroy:
    bson_destroy(selector);
    errno = ENOBUFS;
    return NULL;
}

static bool
mongo_bulk_append_fsevent(mongoc_bulk_operation_t *bulk,
                          const struct rbh_fsevent *fsevent);

static bool
mongo_bulk_append_unlink_from_link(mongoc_bulk_operation_t *bulk,
                                   const struct rbh_fsevent *link)
{
    const struct rbh_fsevent unlink = {
        .type = RBH_FET_UNLINK,
        .id = {
            .data = link->id.data,
            .size = link->id.size,
        },
        .link = {
            .parent_id = link->link.parent_id,
            .name = link->link.name,
        },
    };

    return mongo_bulk_append_fsevent(bulk, &unlink);
}

static bool
mongo_bulk_append_fsevent(mongoc_bulk_operation_t *bulk,
                          const struct rbh_fsevent *fsevent)
{
    bool upsert = false;
    bson_t *selector;
    bson_t *update;
    bool success;

    selector = bson_selector_from_fsevent(fsevent);
    if (selector == NULL)
        return false;

    switch (fsevent->type) {
    case RBH_FET_DELETE:
        success = _mongoc_bulk_operation_remove_one(bulk, selector);
        break;
    case RBH_FET_LINK:
        success = mongo_bulk_append_unlink_from_link(bulk, fsevent);
        if (!success)
            break;
        __attribute__((fallthrough));
    case RBH_FET_UPSERT:
        upsert = true;
        __attribute__((fallthrough));
    default:
        update = bson_update_from_fsevent(fsevent);
        if (update == NULL) {
            int save_errno = errno;

            bson_destroy(selector);
            errno = save_errno;
            return false;
        }

        success = _mongoc_bulk_operation_update_one(bulk, selector, update,
                                                    upsert);
        bson_destroy(update);
    }
    bson_destroy(selector);

    if (!success)
        /* > returns false if passed invalid arguments */
        errno = EINVAL;
    return success;
}

static ssize_t
mongo_bulk_init_from_fsevents(mongoc_bulk_operation_t *bulk,
                              struct rbh_iterator *fsevents)
{
    int save_errno = errno;
    size_t count = 0;

    do {
        const struct rbh_fsevent *fsevent;

        errno = 0;
        fsevent = rbh_iter_next(fsevents);
        if (fsevent == NULL) {
            if (errno == ENODATA)
                break;
            return -1;
        }

        if (!mongo_bulk_append_fsevent(bulk, fsevent))
            return -1;
        count++;
    } while (true);

    errno = save_errno;
    return count;
}

static ssize_t
mongo_backend_update(void *backend, struct rbh_iterator *fsevents)
{
    struct mongo_backend *mongo = backend;
    mongoc_bulk_operation_t *bulk;
    bson_error_t error;
    ssize_t count;
    bson_t reply;
    uint32_t rc;

    bulk = _mongoc_collection_create_bulk_operation(mongo->entries, false,
                                                    NULL);
    if (bulk == NULL) {
        /* XXX: from libmongoc's documentation:
         *      > "Errors are propagated when executing the bulk operation"
         *
         * We will just assume any error here is related to memory allocation.
         */
        errno = ENOMEM;
        return -1;
    }

    count = mongo_bulk_init_from_fsevents(bulk, fsevents);
    if (count <= 0) {
        int save_errno = errno;

        /* Executing an empty bulk operation is considered an error by mongoc,
         * which is why we return early in this case too
         */
        mongoc_bulk_operation_destroy(bulk);
        errno = save_errno;
        return count;
    }

    rc = mongoc_bulk_operation_execute(bulk, &reply, &error);
    mongoc_bulk_operation_destroy(bulk);
    if (!rc) {
        int errnum = RBH_BACKEND_ERROR;

        snprintf(rbh_backend_error, sizeof(rbh_backend_error), "mongoc: %s",
                 error.message);
#if MONGOC_CHECK_VERSION(1, 11, 0)
        if (mongoc_error_has_label(&reply, "TransientTransactionError"))
            errnum = EAGAIN;
#endif
        bson_destroy(&reply);
        errno = errnum;
        return -1;
    }
    bson_destroy(&reply);

    return count;
}

    /*--------------------------------------------------------------------*
     |                                root                                |
     *--------------------------------------------------------------------*/

static const struct rbh_filter ROOT_FILTER = {
    .op = RBH_FOP_EQUAL,
    .compare = {
        .field = {
            .fsentry = RBH_FP_PARENT_ID,
        },
        .value = {
            .type = RBH_VT_BINARY,
            .binary = {
                .size = 0,
            },
        },
    },
};

static struct rbh_fsentry *
mongo_root(void *backend, const struct rbh_filter_projection *projection)
{
    return rbh_backend_filter_one(backend, &ROOT_FILTER, projection);
}

    /*--------------------------------------------------------------------*
     |                               filter                               |
     *--------------------------------------------------------------------*/

static struct rbh_mut_iterator *
mongo_backend_filter(void *backend, const struct rbh_filter *filter,
                     const struct rbh_filter_options *options)
{
    struct mongo_backend *mongo = backend;
    struct mongo_iterator *mongo_iter;
    mongoc_cursor_t *cursor;
    bson_t *pipeline;

    printf("ENTRY m_backend_filter\n");

    if (rbh_filter_validate(filter))
        return NULL;

    pipeline = bson_pipeline_from_filter(filter);
    if (pipeline == NULL)
        return NULL;

char *tmp = bson_as_json(pipeline, NULL);
printf("%s\n", tmp);
free(tmp);

    cursor = mongoc_collection_aggregate(mongo->entries, MONGOC_QUERY_NONE,
                                         pipeline, NULL, NULL);
    bson_destroy(pipeline);
    if (cursor == NULL) {
        errno = EINVAL;
        return NULL;
    }

    mongo_iter = mongo_iterator_new(cursor);
    if (mongo_iter == NULL) {
        int save_errno = errno;

        mongoc_cursor_destroy(cursor);
        errno = save_errno;
    }

    printf("EXIT m_backend_filter\n");
    return &mongo_iter->iterator;
}

    /*--------------------------------------------------------------------*
     |                              destroy                               |
     *--------------------------------------------------------------------*/

static void
mongo_backend_destroy(void *backend)
{
    struct mongo_backend *mongo = backend;

    mongoc_collection_destroy(mongo->entries);
    mongoc_database_destroy(mongo->db);
    mongoc_client_destroy(mongo->client);
    mongoc_uri_destroy(mongo->uri);
    free(mongo);
}

    /*--------------------------------------------------------------------*
     |                              branch                                |
     *--------------------------------------------------------------------*/

struct mongo_branch_backend {
    struct mongo_backend mongo;
    struct rbh_id id;
};

struct mongo_branch_iterator {
    struct rbh_mut_iterator     iterator;
    struct rbh_backend         *backend;
    struct rbh_mut_iterator    *directories;
    struct rbh_mut_iterator    *fsentries;
    struct rbh_filter          *filter;
    struct rbh_filter_options  *options;
    struct rbh_fsentry         *directory;
};

static struct rbh_mut_iterator *
__list_child_fsentries(struct rbh_backend *backend,
                       const struct rbh_id *id, const struct rbh_filter *filter,
                       const struct rbh_filter_options *options)
{
    const struct rbh_filter parent_id_filter = {
        .op = RBH_FOP_EQUAL,
        .compare = {
            .field = {
                .fsentry = RBH_FP_PARENT_ID,
            },
            .value = {
                .type = RBH_VT_BINARY,
                .binary = {
                    .size = id->size,
                    .data = id->data,
                },
            },
        },
    };

    const struct rbh_filter *filters[2] = {
        &parent_id_filter,
        filter,
    };

    const struct rbh_filter and_filter = {
        .op = RBH_FOP_AND,
        .logical = {
            .count = 2,
            .filters = filters,
        },
    };

    printf("ENTRY __list_child_fsentries\n");

    return mongo_backend_filter(backend, &and_filter, options);
}

static const struct rbh_filter ISDIR_FILTER = {
    .op = RBH_FOP_EQUAL,
    .compare = {
        .field = {
            .fsentry = RBH_FP_STATX,
            .statx = STATX_TYPE,
        },
        .value = {
            .type = RBH_VT_INT32,
            .int32 = S_IFDIR,
        },
    },
};

static struct rbh_mut_iterator *
mongo_next_fsentries(struct mongo_branch_iterator *iterator)
{
    struct rbh_mut_iterator *_directories;
    struct rbh_mut_iterator *directories;
    struct rbh_mut_iterator *fsentries;

    printf("ENTRY mongo_next_fsentries\n");

    if (iterator->directory != NULL)
        goto recurse_directories;

    iterator->directory = rbh_mut_iter_next(iterator->directories);
    if (iterator->directory == NULL)
        return NULL;

recurse_directories:
    _directories = __list_child_fsentries(iterator->backend, &iterator->directory->id,
                                        &ISDIR_FILTER, iterator->options);
    if (_directories == NULL)
        return NULL;

    directories = _directories;
    fsentries = __list_child_fsentries(iterator->backend, &iterator->directory->id,
                                     iterator->filter, iterator->options);

    if (fsentries == NULL) {
        rbh_mut_iter_destroy(_directories);
        return NULL;
    }

    printf("NEW CHAIN\n");
    iterator->directories = rbh_mut_iter_chain(2, directories, iterator->directories);
    free(iterator->directory);
    iterator->directory = NULL;

    return fsentries;
}

static void *
mongo_branch_iter_next(void *iterator)
{
    struct mongo_branch_iterator *iter = (struct mongo_branch_iterator *)iterator;
    struct rbh_fsentry *fsentry;
    int old_errno = errno;

    printf("ENTRY mongo_branch_iter_next\n");

    if (iter->fsentries == NULL) {
posix_traversal:
        printf("Next fsentries\n");
        iter->fsentries = mongo_next_fsentries(iter);
        if (iter->fsentries == NULL) {
            printf("err1\n");
            return NULL;
        }
    }

    errno = 0;
    fsentry = rbh_mut_iter_next(iter->fsentries);
    if (fsentry != NULL) {
        printf("EXIT mongo_branch_iter_next\n");
        errno = old_errno;
        return fsentry;
    }

    if (errno != 0) {
        printf("err3: %d: %s\n", errno, strerror(errno));
        return NULL;
    }

    rbh_mut_iter_destroy(iter->fsentries);
    iter->fsentries = NULL;

    goto posix_traversal;
}

static void
mongo_branch_iter_destroy(void *iterator)
{
    struct mongo_branch_iterator *iter = (struct mongo_branch_iterator *)iterator;

    printf("ENTRY mongo_branch_iter_destroy\n");

    free(iter->directory);
    free(iter->filter);

    if (iter->directories != NULL)
        rbh_mut_iter_destroy(iter->directories);

    if (iter->fsentries != NULL)
        rbh_mut_iter_destroy(iter->fsentries);

    free(iter);
}

static const struct rbh_mut_iterator_operations MONGO_BRANCH_ITER_OPS = {
    .next    = mongo_branch_iter_next,
    .destroy = mongo_branch_iter_destroy,
};

static const struct rbh_mut_iterator MONGO_BRANCH_ITERATOR = {
    .ops = &MONGO_BRANCH_ITER_OPS,
};

// helper sur une entree -> rbh_backend_filter
static struct rbh_mut_iterator *
_retrieve_branch_root(struct rbh_backend *backend, const struct rbh_id *id,
               const struct rbh_filter *filter,
               const struct rbh_filter_options *options)
{
    const struct rbh_filter id_filter = {
        .op = RBH_FOP_EQUAL,
        .compare = {
            .field = {
                .fsentry = RBH_FP_ID,
            },
            .value = {
                .type = RBH_VT_BINARY,
                .binary = {
                    .size = id->size,
                    .data = id->data,
                },
            },
        },
    };

    const struct rbh_filter *filters[2] = {&id_filter, filter};
    const struct rbh_filter and_filter = {
        .op = RBH_FOP_AND,
        .logical = {
            .count = 2,
            .filters = filters,
        },
    };

    printf("ENTRY _retrieve_branch_root\n");

    return mongo_backend_filter(backend, &and_filter, options);
}

#include "robinhood/uri.h"

static struct rbh_mut_iterator *
mongo_branch_backend_filter(void *backend, const struct rbh_filter *filter,
                            const struct rbh_filter_options *options)
{
    struct mongo_branch_backend *mongo = backend;
    struct mongo_branch_iterator *iter;
    int old_errno = errno;

    printf("ENTRY m_branch_backend_filter\n");

    if (mongo->id.data == NULL)
        return mongo_backend_filter(backend, filter, options);

    iter = malloc(sizeof(*iter));
    if (iter == NULL) {
        errno = ENOMEM;
        goto out_error;
    }

    iter->iterator = MONGO_BRANCH_ITERATOR;
    iter->backend = backend;
    iter->directories = NULL;
    iter->directory = NULL;
    iter->fsentries = _retrieve_branch_root(backend, &mongo->id, filter, options);
    printf("Branch root retrieved\n");
    if (iter->fsentries == NULL)
        goto out_free_directory;

    char tampon[1024];
    for (size_t i = 0; i < mongo->id.size; ++i)
            printf("%%%.2x", (unsigned char) mongo->id.data[i]);
    printf("\n");
    ssize_t sz = rbh_percent_decode(tampon, mongo->id.data, mongo->id.size);
    tampon[sz] = '\0';
    printf("[%lu] '%s'\n", sz, tampon);

    errno = 0;
    iter->filter = filter ? rbh_filter_clone(filter) : NULL;
    if (iter->filter == NULL && errno != 0)
        goto out_close_fsentries;

    errno = old_errno;
//    iter->options = options;
    iter->options = NULL;

    printf("EXIT m_branch_backend_filter\n");
    return &iter->iterator;

out_close_fsentries:
    rbh_mut_iter_destroy(iter->fsentries);

out_free_directory:
    free(iter->directory);
    free(iter);

out_error:
    return NULL;
}

static struct rbh_backend *
mongo_backend_branch(void *backend, const struct rbh_id *id);

static const struct rbh_backend_operations MONGO_BRANCH_BACKEND_OPS = {
    .root = mongo_root,
    .branch = mongo_backend_branch,
    .filter = mongo_branch_backend_filter,
    .destroy = mongo_backend_destroy,
};

static const struct rbh_backend MONGO_BRANCH_BACKEND = {
    .name = RBH_MONGO_BACKEND_NAME,
    .ops = &MONGO_BRANCH_BACKEND_OPS,
};

static struct rbh_backend *
mongo_backend_branch(void *backend, const struct rbh_id *id)
{
    struct mongo_backend *mongo = backend;
    struct mongo_branch_backend *branch;
    int save_errno = errno;
    size_t data_size;
    char *data;

    char tampon[1024];

    printf("ENTRY m_backend_branch\n");
    for (size_t i = 0; i < id->size; ++i)
            printf("%%%.2x", (unsigned char) id->data[i]);
    printf("\n");
    ssize_t sz = rbh_percent_decode(tampon, id->data, id->size);
    tampon[sz] = '\0';
    printf("[%lu] '%s'\n", sz, tampon);

    data_size = id->size;
    branch = malloc(sizeof(*branch) + data_size);
    if (branch == NULL)
        return NULL;

    data = (char *)branch + sizeof(*branch);

    branch->mongo.uri = mongoc_uri_copy(mongo->uri);
    if (branch->mongo.uri == NULL) {
        save_errno = EINVAL;
        goto free_malloc;
    }

    branch->mongo.client = mongoc_client_new_from_uri(branch->mongo.uri);
    if (branch->mongo.client == NULL) {
        save_errno = ENOMEM;
        goto free_uri;
    }

    branch->mongo.db = mongoc_client_get_database(branch->mongo.client,
                                                  id->data);
    if (branch->mongo.db == NULL) {
        save_errno = ENOMEM;
        goto free_client;
    }

    branch->mongo.entries = mongoc_database_get_collection(branch->mongo.db,
                                                           "entries");
    if (branch->mongo.entries == NULL) {
        save_errno = ENOMEM;
        goto free_db;
    }

    rbh_id_copy(&branch->id, id, &data, &data_size);
    branch->mongo.backend = MONGO_BRANCH_BACKEND;

    return &branch->mongo.backend;

free_db:
    mongoc_database_destroy(branch->mongo.db);
free_client:
    mongoc_client_destroy(branch->mongo.client);
free_uri:
    mongoc_uri_destroy(branch->mongo.uri);
free_malloc:
    free(branch);

    errno = save_errno;

    return NULL;
}

static const struct rbh_backend_operations MONGO_BACKEND_OPS = {
    .branch = mongo_backend_branch,
    .root = mongo_root,
    .update = mongo_backend_update,
    .filter = mongo_backend_filter,
    .destroy = mongo_backend_destroy,
};

/*----------------------------------------------------------------------------*
 |                               MONGO_BACKEND                                |
 *----------------------------------------------------------------------------*/

static const struct rbh_backend MONGO_BACKEND = {
    .id = RBH_BI_MONGO,
    .name = RBH_MONGO_BACKEND_NAME,
    .ops = &MONGO_BACKEND_OPS,
};

/*----------------------------------------------------------------------------*
 |                         rbh_mongo_backend_create()                         |
 *----------------------------------------------------------------------------*/

struct rbh_backend *
rbh_mongo_backend_new(const char *fsname)
{
    struct mongo_backend *mongo;
    int save_errno;

    mongo = malloc(sizeof(*mongo));
    if (mongo == NULL) {
        save_errno = errno;
        goto out;
    }
    mongo->backend = MONGO_BACKEND;

    mongo->uri = mongoc_uri_new("mongodb://localhost:27017");
    if (mongo->uri == NULL) {
        save_errno = EINVAL;
        goto out_free_mongo;
    }

    mongo->client = mongoc_client_new_from_uri(mongo->uri);
    if (mongo->client == NULL) {
        save_errno = ENOMEM;
        goto out_mongoc_uri_destroy;
    }

#if MONGOC_CHECK_VERSION(1, 4, 0)
    if (!mongoc_client_set_error_api(mongo->client,
                                     MONGOC_ERROR_API_VERSION_2)) {
        /* Should never happen */
        save_errno = EINVAL;
        goto out_mongoc_client_destroy;
    }
#endif

    mongo->db = mongoc_client_get_database(mongo->client, fsname);
    if (mongo->db == NULL) {
        save_errno = ENOMEM;
        goto out_mongoc_client_destroy;
    }

    mongo->entries = mongoc_database_get_collection(mongo->db, "entries");
    if (mongo->entries == NULL) {
        save_errno = ENOMEM;
        goto out_mongoc_database_destroy;
    }

    return &mongo->backend;

out_mongoc_database_destroy:
    mongoc_database_destroy(mongo->db);
out_mongoc_client_destroy:
    mongoc_client_destroy(mongo->client);
out_mongoc_uri_destroy:
    mongoc_uri_destroy(mongo->uri);
out_free_mongo:
    free(mongo);
out:
    errno = save_errno;
    return NULL;
}
