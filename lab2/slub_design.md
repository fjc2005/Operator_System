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
