#ifndef __KERN_MM_SLUB_PMM_H__
#define __KERN_MM_SLUB_PMM_H__

#include <defs.h>
#include <list.h>
#include <pmm.h>

#define SLUB_MAX_CPUS 1

struct kmem_cache;

struct kmem_cache_cpu {
    void *freelist;      // local freelist for current active slab
    struct Page *page;   // current active slab page
};

struct kmem_cache {
    const char *name;
    size_t object_size;
    int objects_per_slab;
    int slab_order; // number of pages as 2^order, currently 0

    // partial list bookkeeping
    list_entry_t slabs_partial; // list of struct Page::slub.slab_link

    // per-cpu cache (simplified to single CPU environment)
    struct kmem_cache_cpu cpu_cache[SLUB_MAX_CPUS];
};

// API
struct kmem_cache *kmem_cache_create(const char *name, size_t size);
void *kmem_cache_alloc(struct kmem_cache *cache);
void kmem_cache_free(struct kmem_cache *cache, void *obj);

// test entry
void slub_check(void);

#endif


