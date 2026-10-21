#ifndef NUMFORGE_APPLICATION_RETENTION_H
#define NUMFORGE_APPLICATION_RETENTION_H

#include <stdbool.h>
#include <stddef.h>

/* Optional private retention policy. A successful resize reserves the new
 * committed size; no fallible work may follow before ownership is exchanged.
 * Zero initialization leaves standalone sessions unrestricted. Owner is borrowed. */
typedef struct ApplicationRetention {
    size_t limit;
    void *owner;
    bool (*resize)(void *owner, size_t bytes);
} ApplicationRetention;

static inline bool application_retention_resize(const ApplicationRetention *policy, size_t bytes)
{
    return (!policy->limit || bytes <= policy->limit) &&
        (!policy->resize || policy->resize(policy->owner, bytes));
}

#endif
