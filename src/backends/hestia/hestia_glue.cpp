#include <vector>

#include "hestia.h"
#include "hestia_glue.h"

int list_tiers(uint8_t **tiers, size_t *len)
{
    std::vector<uint8_t> hestia_tiers = hestia::list_tiers();

    *len = hestia_tiers.size();
    *tiers = (uint8_t *) calloc(1, sizeof(**tiers) * (*len));
    if (*tiers == NULL)
        return -1;

    std::copy(hestia_tiers.begin(), hestia_tiers.end(), *tiers);
    return 0;
}

int list_objects(uint8_t *tiers, size_t tiers_len,
                 struct hestia_id **ids, size_t *ids_len)
{
    *ids_len = 0;

    for (size_t i = 0; i < tiers_len; ++i) {
        std::vector<struct hestia::hsm_uint> hestia_objects =
            hestia::list(tiers[i]);
        void *tmp;

        if (hestia_objects.size() == 0)
            continue;

        if (*ids == NULL)
            tmp = malloc(sizeof(**ids) * hestia_objects.size());
        else
            tmp = realloc(*ids,
                          sizeof(**ids) * (*ids_len + hestia_objects.size()));

        if (tmp == NULL)
            return -1;

        *ids = (struct hestia_id *) tmp;

        for (size_t j = 0; j < hestia_objects.size(); ++j) {
            (*ids)[j + *ids_len].higher = hestia_objects[j].higher;
            (*ids)[j + *ids_len].lower = hestia_objects[j].lower;
        }
        *ids_len += hestia_objects.size();
    }

    return 0;
}

int list_object_attrs(struct hestia_id *id, char **obj_attrs, size_t *len)
{
    struct hestia::hsm_uint oid(id->higher, id->lower);
    std::string attrs = hestia::list_attrs(oid);

    *obj_attrs = strdup(attrs.c_str());
    if (obj_attrs == NULL)
        return -1;

    *len = attrs.length();
    return 0;
}
