# 实验二：物理内存管理（Lab2）实验报告

## 摘要

本实验围绕物理内存管理（PMM）展开，按“从易到难”的路径实现并分析：
- First-Fit 连续物理内存分配算法（阅读理解与源码走读）；
- Best-Fit 连续物理内存分配算法（编码实现与验证）；
- Challenge：Buddy System（伙伴系统）按页分配与合并；
- Challenge：SLUB 面向任意大小对象的两层分配器。 

在实现中结合 uCore 风格的 `pmm_manager` 抽象，通过切换 `pmm.c` 中的 `pmm_manager`，可以分别运行不同分配器的自检逻辑，验证分配与释放的行为正确性。最终我们给出了与参考答案的差异、关键知识点与原理的对应关系，并讨论了改进方向。

## 实验环境与代码入口

- 架构：QEMU RISC-V 64
- 关键入口：`kern/mm/pmm.c` 中初始化与选择分配器
- 自检输出：`run.log` 与串口输出

```12:45:/home/fjc/labcode/lab2/kern/mm/pmm.c
static void init_pmm_manager(void) {
    // pmm_manager = &default_pmm_manager;     // First-fit allocator
    // pmm_manager = &best_fit_pmm_manager;    // Best-fit allocator  
    pmm_manager = &buddy_pmm_manager;          // Buddy system allocator
    cprintf("memory management: %s\n", pmm_manager->name);
    pmm_manager->init();
}
```

## 1. 练习一：理解 First-Fit 连续物理内存分配算法

### 1.1 数据结构与核心思路

- `free_area_t` 管理空闲块链表与空闲页计数；`struct Page` 的 `property` 字段记录“以该页为块头的连续空闲页数”，`PG_property` 标明是否为空闲块头。
- First-Fit 从低地址到高地址顺序扫描空闲链表，遇到第一个满足 `property >= n` 的块即使用，必要时在块内切分并把剩余部分重新挂回有序链表。

```34:61:/home/fjc/labcode/lab2/kern/mm/memlayout.h
struct Page {
    int ref;
    uint64_t flags;
    unsigned int property;          // 以本页为首的连续空闲页数
    list_entry_t page_link;         // 挂接 free list
};
```

### 1.2 关键函数走读

- 初始化与建表：

```62:94:/home/fjc/labcode/lab2/kern/mm/default_pmm.c
static void default_init(void) {
    list_init(&free_list);
    nr_free = 0;
}

static void default_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p ++) {
        assert(PageReserved(p));
        p->flags = p->property = 0;
        set_page_ref(p, 0);
    }
    base->property = n;
    SetPageProperty(base);
    nr_free += n;
    // 将块头按地址有序插入 free_list
    ...
}
```

- 分配：顺序扫描找到首个可用块；若 `property > n`，在 `page + n` 处形成余块重新入链；`nr_free -= n`。

```96:124:/home/fjc/labcode/lab2/kern/mm/default_pmm.c
static struct Page * default_alloc_pages(size_t n) {
    assert(n > 0);
    if (n > nr_free) return NULL;
    struct Page *page = NULL; list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        if (p->property >= n) { page = p; break; }
    }
    if (page != NULL) {
        list_entry_t* prev = list_prev(&(page->page_link));
        list_del(&(page->page_link));
        if (page->property > n) {
            struct Page *p = page + n;               // 余块
            p->property = page->property - n;
            SetPageProperty(p);
            list_add(prev, &(p->page_link));
        }
        nr_free -= n;
        ClearPageProperty(page);
    }
    return page;
}
```

- 释放与合并：把释放块头按地址有序插回，向前/后检查相邻块是否连续，若连续则更新 `property` 并从链表摘除被合并的块。

```126:174:/home/fjc/labcode/lab2/kern/mm/default_pmm.c
static void default_free_pages(struct Page *base, size_t n) {
    ...
    base->property = n; SetPageProperty(base); nr_free += n;
    // 地址有序插入
    ...
    // 与前块合并
    list_entry_t* le = list_prev(&(base->page_link));
    if (le != &free_list) {
        struct Page* p = le2page(le, page_link);
        if (p + p->property == base) {
            p->property += base->property;
            ClearPageProperty(base);
            list_del(&(base->page_link));
            base = p;
        }
    }
    // 与后块合并
    le = list_next(&(base->page_link));
    if (le != &free_list) {
        struct Page* p = le2page(le, page_link);
        if (base + base->property == p) {
            base->property += p->property;
            ClearPageProperty(p);
            list_del(&(p->page_link));
        }
    }
}
```

### 1.3 过程描述与作用分解

- `default_init`：初始化空闲链表与计数。
- `default_init_memmap`：将 [base, base+n) 标记为空闲块，`base->property=n`，把块头按地址顺序插入 `free_list`。
- `default_alloc_pages(n)`：从 `free_list` 顺序扫描，找到首个 `property>=n` 的块切分并返回；更新 `nr_free`。
- `default_free_pages(base,n)`：把块插回并尝试和相邻块合并，减少外部碎片。

### 1.4 改进空间

- 地址有序链表下 First-Fit 时间复杂度为 O(空闲块数)。可改为“按 size 有序 + 次关键字地址有序”或引入分级空闲链（segregated free lists）、平衡树（如红黑树）以降低查找复杂度。
- 可维护统计信息（分配失败率、平均扫描长度）以评估策略；支持“延迟合并/即时合并”的策略切换；针对大块请求与小块请求分流。

## 2. 练习二：实现 Best-Fit 连续物理内存分配算法

### 2.1 实现要点

- 在 `best_fit_alloc_pages` 中遍历空闲链表，选择满足 `property >= n` 的“最小可用块”，即最接近 n 的块；若大于 n，切分余块并按地址有序插回。
- 初始化与释放逻辑与 First-Fit 基本一致，核心差异在“选块策略”。

```104:163:/home/fjc/labcode/lab2/kern/mm/best_fit_pmm.c
static struct Page * best_fit_alloc_pages(size_t n) {
    assert(n > 0); if (n > nr_free) return NULL;
    struct Page *page = NULL; list_entry_t *le = &free_list;
    size_t min_size = nr_free + 1; list_entry_t *best_le = NULL;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        if (p->property >= n && p->property < min_size) {
            min_size = p->property; page = p; best_le = le;
        }
    }
    if (page != NULL) {
        list_del(&(page->page_link));
        if (page->property > n) {
            struct Page *p = page + n;           // 余块
            p->property = page->property - n; SetPageProperty(p);
            // 重新按地址有序插回
            list_entry_t* new_le = &free_list; int inserted = 0;
            while ((new_le = list_next(new_le)) != &free_list) {
                struct Page* new_page = le2page(new_le, page_link);
                if (p < new_page) { list_add_before(new_le, &(p->page_link)); inserted = 1; break; }
                else if (list_next(new_le) == &free_list) { list_add(new_le, &(p->page_link)); inserted = 1; break; }
            }
            if (!inserted) { list_add(&free_list, &(p->page_link)); }
        }
        nr_free -= n; ClearPageProperty(page);
    }
    return page;
}
```

### 2.2 自检与特性验证

- `best_fit_check` 利用交错释放制造多个候选空闲块，验证 Best-Fit 会优先挑选最紧凑的块，体现最小浪费：

```284:364:/home/fjc/labcode/lab2/kern/mm/best_fit_pmm.c
assert((p1 = alloc_pages(1)) != NULL);
assert(alloc_pages(2) != NULL);      // best fit feature
```

### 2.3 改进空间

- 仍是 O(空闲块数) 的线性扫描；可用“按大小组织”的多级桶/红黑树；
- 为减少外部碎片，Best-Fit 容易产生许多小余块，可考虑“最优切分阈值”“微碎片合并阈值”。

## 3. Challenge：伙伴系统（Buddy System）

### 3.1 设计与接口

- 以二叉树数组 `longest[]` 维护各节点对应子树的最大空闲块大小；分配时自顶向下选择满足请求的最小节点，释放时自底向上检测并与伙伴合并。
- 实现了标准 `pmm_manager` 接口，可在 `pmm.c` 中一键切换测试。

```232:241:/home/fjc/labcode/lab2/kern/mm/buddy_system_pmm.c
void buddy_init(void) { buddy_allocator = NULL; }
void buddy_init_memmap(struct Page *base, size_t n) { ... 初始化 size、max_order、longest 树 ... }
struct Page *buddy_alloc_pages(size_t n) { ... 自顶向下查找并分裂 ... }
void buddy_free_pages(struct Page *base, size_t n) { ... 自底向上合并 ... }
```

### 3.2 关键路径示意与测试

- 分配：标准化到 2 的幂 `fixsize(n)`，树上向下走到 `node_size==size`；置 0 并清子树，回溯更新父节点。
- 释放：定位 `(offset,size)` 对应节点，`set_subtree_full` 恢复为空闲，父节点在左右子均“满空闲”时合并。

```239:299:/home/fjc/labcode/lab2/kern/mm/buddy_system_pmm.c
// 分配
size_t size = fixsize(n);
... for (; node_size != size; node_size /= 2) { index = (left>=size?left:right); }
buddy_allocator->longest[index] = 0; clear_subtree_zero(...);
... while (index) { index = PARENT(index); longest[index] = MAX(left,right); }
```

```301:362:/home/fjc/labcode/lab2/kern/mm/buddy_system_pmm.c
// 释放与合并
buddy_allocator->longest[index] = size; set_subtree_full(...);
while (index) { index=PARENT(index); size<<=1; if (left==right==size/2) longest[index]=size; else longest[index]=MAX(...); }
```

### 3.3 特点与对比

- 优点：O(log N) 分配/释放，自动合并外部碎片；
- 缺点：块大小为 2 的幂，存在内部碎片；对小对象不经济，需上层分配器（如 SLUB）配合。

## 4. Challenge：SLUB 任意大小对象分配

### 4.1 两层结构与数据结构

- 下层：Buddy System 批发页；上层：SLUB 将单页切为若干对象，通过对象头复用 next 指针建立 `freelist`；按对象尺寸划分 `kmem_cache`。

```17:28:/home/fjc/labcode/lab2/kern/mm/slub_pmm.h
struct kmem_cache_cpu { void *freelist; struct Page *page; };
struct kmem_cache {
    const char *name; size_t object_size; int objects_per_slab; int slab_order;
    list_entry_t slabs_partial; struct kmem_cache_cpu cpu_cache[1];
};
```

### 4.2 核心流程

- 建 cache：从 buddy 申请 1 页作为元数据与初始 slab，切割页并建立 `freelist`，把页挂入 `slabs_partial`。
- 分配：优先走 per-CPU `freelist`；若空则从 `slabs_partial` 取页迁入本地并分配一个对象；必要时 `grow_slab()`。
- 释放：若回收到当前活跃页，放回本地 `freelist`，若该页 `inuse==0` 则立即整页还给 buddy；否则放回页内 `freelist`，确保在 `slabs_partial` 上；当 `inuse==0` 时归还伙伴。

```83:118:/home/fjc/labcode/lab2/kern/mm/slub_pmm.c
void *kmem_cache_alloc(struct kmem_cache *cache) {
    if (cc->freelist) { ... fast path ... }
    if (list_empty(&cache->slabs_partial)) { if (grow_slab(cache)==NULL) return NULL; }
    // 迁入一页到本地并分配1个对象
}
```

```120:157:/home/fjc/labcode/lab2/kern/mm/slub_pmm.c
void kmem_cache_free(struct kmem_cache *cache, void *obj) {
    if (cc->page == page) { ... 若 inuse==0 则整页 free_pages ... return; }
    // 否则回收到页内 freelist，并保证在 partial 上；若 inuse==0 也整页归还
}
```

### 4.3 与 Linux SLUB 的差异

- 教学实现仅保留 `partial` 列表，未实现 full/empty 状态细分、构造/析构回调、poison/guard 等调试与安全特性；单核无锁，逻辑简洁易验证。

## 5. 硬件可用物理内存范围的获取方法（思考题）

- 通过设备树 DTB 获取：本实验已在 `pmm.c` 中调用 `get_memory_base/get_memory_size` 从 DTB 解析 `mem_begin`/`mem_end`，并据此初始化可用物理内存映射。
- 其他方法：
  - 固件/BIOS 接口（x86: E820/UEFI Memory Map；RISC-V: SBI/HART 附带信息）；
  - 启动引导加载传参（Bootloader 将内存布局传给内核）；
  - 主动探测（极少采用，风险大）。

```69:106:/home/fjc/labcode/lab2/kern/mm/pmm.c
uint64_t mem_begin = get_memory_base();
uint64_t mem_size  = get_memory_size();
... if (freemem < mem_end) { init_memmap(pa2page(mem_begin), (mem_end - mem_begin) / PGSIZE); }
```

## 6. 与参考答案的差异

- First-Fit：实现与参考思路一致，保证空闲链表地址有序、相邻合并；
- Best-Fit：在选择块时线性遍历比较 `min_size`，并在切分后保持链表地址有序；
- Buddy：采用数组化完全二叉树维护 `longest`，提供更丰富的调试输出（层级统计、状态 dump），并含边界测试用例；
- SLUB：采用“单页 slab、仅 partial 列表、单核无锁”的最简版本，强化了“整页及时归还”以减少囤积。

## 7. 实验知识点与操作系统原理对应

- 连续物理内存分配策略：First-Fit / Best-Fit；碎片类型与合并策略；
- 伙伴系统：2 的幂分配、树结构维护、上下分裂与合并；
- SLUB：两层分配器、对象切割与页内 freelist、per-CPU 快路径；
- 启动期可用内存发现：设备树/固件接口到内核内存映射的落地过程。

## 8. 尚未覆盖但在原理中重要的内容

- 多核并发下的内存分配器扩展（锁、禁抢占、per-CPU 批量迁移策略）；
- NUMA/节点拓扑感知；
- 调试与安全：poison、red-zone、越界检测、统计与可视化工具；
- 更复杂的页分配策略（order 自适应、色彩/对齐策略、反碎片化）。

## 9. 结论

通过本实验，我们从线性扫描的连续分配（First-Fit/Best-Fit）出发，进阶到对数时间复杂度的伙伴系统分配，并在其上实现了 SLUB 小对象分配，形成了“页级批发 + 对象零售”的两层内存管理体系。配合完善的自检与调试输出，验证了实现的正确性并为后续引入多核与虚拟内存管理奠定了基础。


