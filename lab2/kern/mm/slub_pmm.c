#include <slub_pmm.h>
#include <memlayout.h>
#include <pmm.h>
#include <string.h>
#include <stdio.h>

// Helper: get cpu id (single CPU for lab)
static inline int get_cpu_id(void) {
    return 0;
}

static inline void *page_to_virt(struct Page *page) {
    // physical address to kernel virtual address: pa + PHYSICAL_MEMORY_OFFSET
    return (void *)(page2pa(page) + PHYSICAL_MEMORY_OFFSET);
}

static inline struct Page *virt_to_page(void *ptr) {
    uintptr_t va = (uintptr_t)ptr;
    uintptr_t pa = va - PHYSICAL_MEMORY_OFFSET;
    return pa2page(pa);
}

static struct Page *grow_slab(struct kmem_cache *cache) {
    size_t npages = (size_t)1 << cache->slab_order;
    struct Page *page = alloc_pages(npages);
    if (page == NULL) {
        return NULL;
    }

    page->slub.slab_cache = cache;
    page->slub.inuse = 0;
    page->slub.freelist = NULL;
    page->slub.on_partial = 0;

    // carve freelist
    uint8_t *base = (uint8_t *)page_to_virt(page);
    size_t total_bytes = npages * PGSIZE;
    int max_objs = (int)(total_bytes / cache->object_size);
    if (max_objs < 1) {
        // should not happen with slab_order=0 and reasonable size
        free_pages(page, npages);
        return NULL;
    }
    cache->objects_per_slab = max_objs;

    for (int i = 0; i < cache->objects_per_slab; i++) {
        void *obj = base + (size_t)i * cache->object_size;
        *(void **)obj = page->slub.freelist;
        page->slub.freelist = obj;
    }

    // put into partial list (empty slab is also partial in our simplified model)
    list_add(&cache->slabs_partial, &page->slub.slab_link);
    page->slub.on_partial = 1;
    return page;
}

struct kmem_cache *kmem_cache_create(const char *name, size_t size) {
    if (size == 0) return NULL;
    // allocate one cache descriptor from buddy (one page is enough)
    struct Page *meta = alloc_pages(1);
    if (!meta) return NULL;
    void *mem = page_to_virt(meta);
    memset(mem, 0, PGSIZE);
    struct kmem_cache *cache = (struct kmem_cache *)mem;
    cache->name = name;
    cache->object_size = size < sizeof(void*) ? sizeof(void*) : size; // ensure space for next pointer
    cache->slab_order = 0; // single page slabs by default
    list_init(&cache->slabs_partial);
    for (int i = 0; i < SLUB_MAX_CPUS; i++) {
        cache->cpu_cache[i].freelist = NULL;
        cache->cpu_cache[i].page = NULL;
    }
    // pre-grow one slab for quicker first allocation
    if (grow_slab(cache) == NULL) {
        // return metadata page as we failed
        free_pages(meta, 1);
        return NULL;
    }
    return cache;
}

void *kmem_cache_alloc(struct kmem_cache *cache) {
    int cpu = get_cpu_id();
    struct kmem_cache_cpu *cc = &cache->cpu_cache[cpu];

    // fast path from per-cpu
    if (cc->freelist) {
        void *obj = cc->freelist;
        cc->freelist = *(void **)obj;
        if (cc->page) {
            cc->page->slub.inuse++;
        }
        return obj;
    }

    // slow path: refill from partial list
    if (list_empty(&cache->slabs_partial)) {
        if (grow_slab(cache) == NULL) {
            return NULL;
        }
    }

    list_entry_t *le = list_next(&cache->slabs_partial);
    struct Page *page = le2page(le, slub.slab_link);
    // detach selected page from partial for exclusive use in cpu cache
    list_del(le);
    page->slub.on_partial = 0;
    cc->page = page;
    cc->freelist = page->slub.freelist;

    // allocate one object now
    if (cc->freelist == NULL) return NULL;
    void *obj = cc->freelist;
    cc->freelist = *(void **)obj;
    page->slub.inuse++;
    return obj;
}

void kmem_cache_free(struct kmem_cache *cache, void *obj) {
    if (!obj) return;
    struct Page *page = virt_to_page(obj);
    int cpu = get_cpu_id();
    struct kmem_cache_cpu *cc = &cache->cpu_cache[cpu];

    if (cc->page == page) {
        *(void **)obj = cc->freelist;
        cc->freelist = obj;
        if (page->slub.inuse > 0) page->slub.inuse--;
        // if active slab becomes completely free, return it to buddy
        if (page->slub.inuse == 0) {
            free_pages(page, (size_t)1 << cache->slab_order);
            cc->page = NULL;
            cc->freelist = NULL;
        }
        return;
    }

    // return to its slab freelist and ensure the page is on partial list
    *(void **)obj = page->slub.freelist;
    page->slub.freelist = obj;
    if (page->slub.inuse > 0) page->slub.inuse--;

    if (!page->slub.on_partial) {
        list_add(&cache->slabs_partial, &page->slub.slab_link);
        page->slub.on_partial = 1;
    }

    // if slab becomes completely free, return back to buddy to avoid hoarding
    if (page->slub.inuse == 0) {
        if (page->slub.on_partial) {
            list_del(&page->slub.slab_link);
            page->slub.on_partial = 0;
        }
        free_pages(page, (size_t)1 << cache->slab_order);
    }
}

// =============== Tests ===============
static void slub_basic_test(void) {
    cprintf("[SLUB] basic test begin\n");
    struct kmem_cache *c = kmem_cache_create("test_cache", 128);
    assert(c != NULL);
    void *a = kmem_cache_alloc(c);
    void *b = kmem_cache_alloc(c);
    assert(a && b && a != b);
    kmem_cache_free(c, a);
    kmem_cache_free(c, b);
    cprintf("[SLUB] basic test ok\n");
}

static void slub_fill_and_release_test(void) {
    cprintf("[SLUB] fill and release test begin\n");
    const size_t objsz = 64;
    struct kmem_cache *c = kmem_cache_create("fill_cache", objsz);
    assert(c != NULL);
    int cap = (int)(PGSIZE / objsz);
    void *objs[128];
    assert(cap <= 128);
    for (int i = 0; i < cap; i++) {
        objs[i] = kmem_cache_alloc(c);
        assert(objs[i] != NULL);
        for (int j = 0; j < i; j++) {
            assert(objs[i] != objs[j]);
        }
    }
    // free all and ensure active page is released
    for (int i = 0; i < cap; i++) {
        kmem_cache_free(c, objs[i]);
    }
    assert(c->cpu_cache[0].page == NULL);
    cprintf("[SLUB] fill and release test ok\n");
}

static void slub_multi_slab_large_obj_test(void) {
    cprintf("[SLUB] multi-slab large object test begin\n");
    const size_t objsz = 4000; // close to a page
    struct kmem_cache *c = kmem_cache_create("large_cache", objsz);
    assert(c != NULL);
    void *p1 = kmem_cache_alloc(c);
    void *p2 = kmem_cache_alloc(c); // should trigger a second slab
    assert(p1 && p2 && p1 != p2);
    kmem_cache_free(c, p1);
    kmem_cache_free(c, p2);
    assert(c->cpu_cache[0].page == NULL);
    cprintf("[SLUB] multi-slab large object test ok\n");
}

void slub_check(void) {
    slub_basic_test();
    slub_fill_and_release_test();
    slub_multi_slab_large_obj_test();
}


