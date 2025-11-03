SLUB 分配器设计文档（简化版）

1. 背景与目标
- 在伙伴系统（按页分配/合并）的基础上，提供面向“任意大小对象”的高效分配与释放层，减少小对象直接以页为粒度造成的内部碎片与初始化开销。
- 满足教学内核接口，尽量小改动接入，便于阅读与验证。

2. 总体架构
- 二层结构：
  - 底层：Buddy System，负责页级别的批发进/出（alloc_pages/free_pages）。
  - 上层：SLUB，按对象尺寸“零售”，对页进行切割与管理。
- 关键思路：将“页 Page”视作一个 Slab，页内通过空闲对象单向链表（freelist）进行管理；按对象种类/尺寸划分到不同的 kmem_cache。

3. 数据结构
- struct kmem_cache（见 kern/mm/slub_pmm.h）
  - name：缓存名，便于调试。
  - object_size：对象大小（至少对齐到指针大小，以便复用对象头部存储 next 指针）。
  - objects_per_slab：每个 slab（一页，order=0）可容纳的对象数。
  - slab_order：本实现固定为 0（单页 slab），便于简单可靠。
  - slabs_partial：部分可用 slab 页的链表头（list_head），也是“公共货架”。
  - cpu_cache[SLUB_MAX_CPUS]：每 CPU 的“私人工具箱”（本实现为单核，大小为 1）。
- struct kmem_cache_cpu
  - freelist：当前活跃 slab 的本地空闲对象单链表头。
  - page：当前活跃 slab 的 struct Page*。
- struct Page.slub（见 memlayout.h 增补）
  - freelist：该 slab 页的空闲对象单链表头（供 slow path 回收和 refill）。
  - inuse：当前页已分配对象计数。
  - slab_cache：此页归属的 kmem_cache。
  - slab_link：挂接到 slabs_partial 的链表节点。
  - on_partial：布尔，标记是否已在 partial 列表中。

4. 关键参数与计算
- 对齐：object_size = max(request_size, sizeof(void*))，确保对象最小能容纳一个 next 指针。
- 容量：objects_per_slab = (PGSIZE << slab_order) / object_size。
- slab_order：固定为 0，避免多页切割带来的复杂度（可扩展）。

5. 核心流程
5.1 kmem_cache_create(name, size)
- 从 buddy 申请 1 页用于 kmem_cache 元数据（简化做法，避免循环依赖）。
- 初始化字段与 slabs_partial，预先调用 grow_slab() 进货 1 页并建立页内对象 freelist。

5.2 grow_slab(cache)
- 调用 alloc_pages(1<<slab_order) 拿到页。
- 初始化 Page.slub 字段：inuse=0, freelist=NULL, slab_cache=cache, on_partial=0。
- 将整页按 object_size 切分，并用对象头部（前 sizeof(void*) 字节）串成单向链表，更新 page->slub.freelist。
- 把该页挂到 cache->slabs_partial，on_partial=1。

5.3 分配 kmem_cache_alloc(cache)
- 快路径（per-CPU）：若 cpu_cache->freelist 非空，直接弹出一个对象，cpu_cache->page->slub.inuse++，返回。
- 慢路径：若本地空，尝试从 slabs_partial 取一页（若为空则 grow_slab 进货），将该页设为 cpu_cache->page，把其 freelist 迁入 cpu_cache->freelist，然后再按快路径分配一个对象。

5.4 释放 kmem_cache_free(cache, obj)
- 通过 virt_to_page(obj) 找到所属页。
- 若属于当前活跃页 cpu_cache->page：将对象头插回 cpu_cache->freelist，inuse--。若 inuse==0，直接整页 free_pages 归还伙伴，并清空 cpu_cache->{page,freelist}。
- 否则：将对象头插回该页的 page->slub.freelist，inuse--；若该页不在 partial，则挂入 slabs_partial。若 inuse==0，从 partial 脱链并整页归还伙伴。

6. 并发与锁
- 本实现假定单核执行，不引入锁；SLUB_MAX_CPUS=1。
- 若扩展到多核：
  - slabs_partial 操作需 spinlock 保护。
  - 引入 per-CPU 批量上/下货策略以降低锁竞争与抖动。

7. 内存与安全性考虑
- 地址转换：
  - page_to_virt(page) = page2pa(page) + PHYSICAL_MEMORY_OFFSET。
  - virt_to_page(ptr) = pa2page((uintptr_t)ptr - PHYSICAL_MEMORY_OFFSET)。
- 边界：当对象尺寸接近 PGSIZE 时，objects_per_slab 可能为 1，逻辑仍成立；若计算出 0（极端不合理尺寸），则放弃该 slab 并归还页（实现里检测并处理）。
- 泄露防御：页 inuse==0 时积极归还伙伴，避免 cache 无限囤积页。

8. 失败处理
- kmem_cache_create 进货失败则回滚元数据页。
- kmem_cache_alloc 在 slabs_partial 为空且 grow_slab 失败时返回 NULL。

9. 测试方案（见 slub_pmm.c::slub_check）
- basic test：创建 cache(128B)，分配两个对象，校验非空且地址不同，释放后不崩溃。
- fill and release：objsz=64B，填满单页全部对象再全部释放，断言活跃页被归还给伙伴（cpu_cache.page==NULL）。
- multi-slab large object：objsz≈4000B，分配两个对象触发多 slab，释放后断言活跃页归还。
- 进一步可扩展：交错分配/释放、多 cache 并行、对象尺寸多样化（16B、32B、128B、1024B、近 PGSIZE）。

10. 与 Linux SLUB 的差异与取舍
- 省略 full/empty 列表，仅使用 partial 简化状态切换。
- 未实现构造/析构回调、slab poisoning、red-zoning、对象对齐优化、NUMA/节点感知等特性。
- 单核无锁，便于教学与验证；可按需逐步引入并发控制与调试特性。

11. 潜在优化方向
- 按对象大小选择更合适的 slab_order，减少外部碎片与进货频次。
- 引入 per-CPU 批量上/下货（迁移 N 个对象）以降低共享列表访问。
- 引入 full/empty 列表与成熟的状态机，提升热点行为与重用率。
- 调试/安全：guard/poison、统计信息、碎片率评估、可视化 dump。

12. 文件与接口
- 源码：kern/mm/slub_pmm.h、kern/mm/slub_pmm.c。
- 集成：pmm.c 在 buddy 自检后调用 slub_check()。
- 结构：memlayout.h 的 struct Page 增加 slub 字段与 struct kmem_cache 前置声明。

参考
- IBM developerWorks：Linux 的 SLUB 分配算法（中文介绍）。
- Linux 内核文档：Documentation/vm/slub.txt（上游设计思路）。

13. 逐段代码与解读（基于当前实现）
- 说明：本节逐段引用当前实现中的关键代码，并给出对齐到实现细节的解读，便于对照阅读与排错。

13.1 头文件与核心数据结构（slub_pmm.h, memlayout.h）
- struct kmem_cache_cpu：每 CPU 活跃 slab 及本地 freelist。
```12:15:/home/fjc/labcode/lab2/kern/mm/slub_pmm.h
struct kmem_cache_cpu {
    void *freelist;      // local freelist for current active slab
    struct Page *page;   // current active slab page
};
```
- 解读：`freelist` 为当前 CPU 活跃 slab 的对象单链表头；`page` 指向该活跃 slab 所在的 `struct Page`。快路径分配/释放只在这两个字段上操作，避免访问共享结构。

- struct kmem_cache：每类对象的 cache 描述符。
```17:28:/home/fjc/labcode/lab2/kern/mm/slub_pmm.h
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
```
- 解读：`object_size` 至少会被提升到 `sizeof(void*)`；`objects_per_slab` 在 `grow_slab` 时计算并回填；`slab_order=0` 表示单页 slab；`slabs_partial` 维护“可再分配”的 slab 页集合。

- 对外 API：
```30:36:/home/fjc/labcode/lab2/kern/mm/slub_pmm.h
// API
struct kmem_cache *kmem_cache_create(const char *name, size_t size);
void *kmem_cache_alloc(struct kmem_cache *cache);
void kmem_cache_free(struct kmem_cache *cache, void *obj);

// test entry
void slub_check(void);
```
- 解读：接口最小化，创建/分配/释放/自检；便于在教学内核中直接接线。

- struct Page.slub：页级别的 SLUB 元数据。
```37:50:/home/fjc/labcode/lab2/kern/mm/memlayout.h
struct Page {
    int ref;                        // page frame's reference counter
    uint64_t flags;                 // array of flags that describe the status of the page frame
    unsigned int property;          // the num of free block, used in first fit pm manager
    list_entry_t page_link;         // free list link
    // SLUB per-page metadata (used by slub_pmm.c)
    struct {
        void *freelist;             // freelist head inside this slab page
        int inuse;                  // number of allocated objects in this slab page
        struct kmem_cache *slab_cache; // owner cache
        list_entry_t slab_link;     // link node for cache->slabs_partial
        int on_partial;             // whether on slabs_partial list
    } slub;
};
```
- 解读：每个 slab 页内部另有自己的 `freelist` 与 `inuse` 计数；`slab_link` 用于把页挂到 `kmem_cache::slabs_partial`；`on_partial` 防止重复入链。

- le2page 宏：从链表节点还原为 `struct Page*`。
```63:66:/home/fjc/labcode/lab2/kern/mm/memlayout.h
#define le2page(le, member)                 \
    to_struct((le), struct Page, member)
```
- 解读：与 `list_*` 辅助函数配合，将 `slab_link` 节点转换回承载它的 `struct Page` 对象。

13.2 工具函数与地址换算（slub_pmm.c）
- CPU 获取（单核）：
```7:10:/home/fjc/labcode/lab2/kern/mm/slub_pmm.c
// Helper: get cpu id (single CPU for lab)
static inline int get_cpu_id(void) {
    return 0;
}
```
- 解读：实验环境中固定返回 0；若扩展多核，需改为读取 hart/cpu id。

- 物理地址与内核虚拟地址互转：
```12:21:/home/fjc/labcode/lab2/kern/mm/slub_pmm.c
static inline void *page_to_virt(struct Page *page) {
    // physical address to kernel virtual address: pa + PHYSICAL_MEMORY_OFFSET
    return (void *)(page2pa(page) + PHYSICAL_MEMORY_OFFSET);
}

static inline struct Page *virt_to_page(void *ptr) {
    uintptr_t va = (uintptr_t)ptr;
    uintptr_t pa = va - PHYSICAL_MEMORY_OFFSET;
    return pa2page(pa);
}
```
- 解读：与 `memlayout.h` 中的偏移约定一致，保证 slab carving 与 obj->page 逆向定位可逆。

13.3 grow_slab：向伙伴系统“进货”并切割页
```23:56:/home/fjc/labcode/lab2/kern/mm/slub_pmm.c
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
```
- 解读：
- **页获取**：按 `slab_order` 计算 `npages` 并从伙伴系统申请；失败直接返回。
- **元数据初始化**：绑定 `slab_cache`，清零 `inuse`/`freelist`，`on_partial=0`。
- **页内切割**：计算 `max_objs`，当对象尺寸过大导致为 0 时立刻回滚归还页；随后自尾到头将对象串成 freelist（头插法）。
- **入 partial**：空 slab 也视作 partial，加入 `slabs_partial`，置位 `on_partial`。

13.4 kmem_cache_create：创建对象缓存并预热
```58:81:/home/fjc/labcode/lab2/kern/mm/slub_pmm.c
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
```
- 解读：
- **元数据页**：为 `kmem_cache` 自身从伙伴申请一页并零化（避免循环依赖 kmalloc）；
- **字段初始化**：提升 `object_size` 至指针对齐；初始化 `slabs_partial` 与 per-CPU 缓存；
- **预热**：预先 `grow_slab` 一页，失败则回滚释放元数据页。

13.5 kmem_cache_alloc：分配对象
```83:118:/home/fjc/labcode/lab2/kern/mm/slub_pmm.c
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
```
- 解读：
- **快路径**：直接从本地 `freelist` 弹出，并在已知活跃页时 `inuse++`。
- **慢路径补货**：若 `slabs_partial` 为空则 `grow_slab`；否则取表头页并临时“摘链”用于本 CPU 独占，迁移其 freelist 至本地，再完成一次分配。
- **一致性**：本地 `cc->page` 与 `cc->freelist` 保持同步；若出现空 freelist（极端），直接返回 `NULL`。

13.6 kmem_cache_free：释放对象
```120:157:/home/fjc/labcode/lab2/kern/mm/slub_pmm.c
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
```
- 解读：
- **活跃页本地释放**：O(1) 头插回 `cc->freelist`；当 `inuse` 归零，主动归还整页并清空本地活跃状态，避免囤积。
- **非活跃页释放**：O(1) 头插回该页 `freelist`；确保该页入 `slabs_partial`；如对象数归零，先从 partial 脱链再归还伙伴。

13.7 测试用例：功能与边界覆盖
- basic test：基本功能与不同地址性。
```160:171:/home/fjc/labcode/lab2/kern/mm/slub_pmm.c
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
```

- fill and release：填满单页并释放，验证活跃页释放策略。
```172:193:/home/fjc/labcode/lab2/kern/mm/slub_pmm.c
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
```

- multi-slab large object：逼近页大小的对象，触发多 slab。
```195:207:/home/fjc/labcode/lab2/kern/mm/slub_pmm.c
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
```

- 统一入口：
```209:213:/home/fjc/labcode/lab2/kern/mm/slub_pmm.c
void slub_check(void) {
    slub_basic_test();
    slub_fill_and_release_test();
    slub_multi_slab_large_obj_test();
}
```
- 解读：三组测试分别覆盖基本正确性、满载/回收路径与大对象场景，聚焦活跃页释放和多 slab 触发逻辑。

13.8 关键边界与异常路径梳理
- `grow_slab` 中 `max_objs < 1`：归还页并返回 `NULL`，上层 `kmem_cache_create/alloc` 感知失败。
- `kmem_cache_alloc` 慢路径可能遇到空 `freelist`（极端并发或未来多核改造时），守护式返回 `NULL`。
- `kmem_cache_free` 对 `inuse` 做下溢保护（`>0` 再自减），避免意外多次释放带来的计数错乱。

13.9 与上游 SLUB 的命名/状态对照
- 本实现用 `slabs_partial` 统一承载 empty/partial 状态页；`list_del` 时表示该页正被某 CPU 独占为活跃页；空页在释放时直接回收而非进入 `slabs_empty`。
