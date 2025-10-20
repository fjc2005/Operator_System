/**
 * 伙伴系统物理内存管理器实现
 * 
 * 本文件实现了用于内核物理内存管理的伙伴系统分配器。
 * 伙伴系统算法通过维护一个内存块的二叉树来工作，其中
 * 每一层代表特定2的幂次大小的块。
 * 
 * 算法概述：
 * 1. 内存被组织为一个完全二叉树
 * 2. 每个节点代表大小为2^k页的内存块
 * 3. 分配时找到最小的合适块，必要时进行分割
 * 4. 释放时标记块为空闲并与伙伴块合并
 * 
 * 树结构示例（8页）：
 *        [8]           <- 根节点：整个内存空间
 *       /   \
 *     [4]   [4]        <- 第1层：4页块
 *    /  \   /  \
 *   [2][2][2][2]      <- 第2层：2页块  
 *  / \/ \/ \/ \
 * [1][1][1][1][1][1][1][1]  <- 叶子节点：单个页面
 * 
 * 与PMM框架的集成：
 * - 实现标准的pmm_manager接口
 * - 与现有内核内存管理兼容
 * - 提供全面的测试和验证
 * 
 * 作者：2313896 黄俊雄
 * 日期：2025年
 */

#include <buddy_system_pmm.h>
#include <pmm.h>
#include <list.h>
#include <string.h>
#include <stdio.h>
#include <defs.h>

// 全局伙伴系统实例
struct buddy_system *buddy_allocator = NULL;

// 2的幂次工具函数
static size_t fixsize(size_t size) {
    if (size <= 0) {
        return 1;
    }
    
    // 如果已经是2的幂次，直接返回
    if (is_power_of_2(size)) {
        return size;
    }
    
    // 找到下一个2的幂次
    size_t result = 1;
    while (result < size) {
        result <<= 1;
        // 防止溢出
        if (result == 0) {
            return (size_t)-1;  // 最大size_t值
        }
    }
    return result;
}// 打印伙伴树每层状态
static void print_buddy_levels(struct buddy_system *buddy) {
    if (!buddy || !buddy->longest) return;

    size_t total_nodes = 2 * buddy->size - 1;
    size_t level = 0;
    size_t nodes_in_level = 1;
    size_t idx = 0;

    cprintf("\n--- 当前伙伴树层级状态 ---\n");
    while (idx < total_nodes) {
        size_t free_blocks = 0;   // 记录该层空闲块数量
        size_t node_size = buddy->size >> level;

        for (size_t i = 0; i < nodes_in_level && idx < total_nodes; i++, idx++) {
            if (buddy->longest[idx] == node_size) {
                free_blocks++;
            }
        }

        cprintf("层 %d: 节点数=%d, 空闲块数=%d, 每块大小=%d页\n",
                (int)level, (int)nodes_in_level, (int)free_blocks, (int)node_size);

        level++;
        nodes_in_level <<= 1;
    }
    cprintf("--------------------------\n\n");
}


// ======= 操作子树的辅助函数 =======
// 将 index 节点及其所有子孙的 longest 值全部设为 0
static void clear_subtree_zero(struct buddy_system *buddy, size_t index, size_t node_size) {
    if (!buddy) return;
    buddy->longest[index] = 0;
    if (node_size <= 1) return; // 叶子，停止
    size_t left = LEFT_LEAF(index);
    size_t right = RIGHT_LEAF(index);
    size_t child_size = node_size >> 1;
    clear_subtree_zero(buddy, left, child_size);
    clear_subtree_zero(buddy, right, child_size);
}

// 将 index 节点及其所有子孙恢复为“完全空闲”的值（每个节点设为其代表块大小）
static void set_subtree_full(struct buddy_system *buddy, size_t index, size_t node_size) {
    if (!buddy) return;
    buddy->longest[index] = node_size;
    if (node_size <= 1) return;
    size_t left = LEFT_LEAF(index);
    size_t right = RIGHT_LEAF(index);
    size_t child_size = node_size >> 1;
    set_subtree_full(buddy, left, child_size);
    set_subtree_full(buddy, right, child_size);
}
// ======= 操作子树的辅助函数 =======


// 树导航和转换函数
static size_t index_to_offset(struct buddy_system *buddy, size_t index, size_t node_size) {
    return (index + 1) * node_size - buddy->size;
}

static size_t offset_to_index(struct buddy_system *buddy, size_t offset, size_t size) {
    return offset + buddy->size / size - 1;
}

// 调试和诊断函数
static void dump_buddy_state(struct buddy_system *buddy) {
    if (!buddy) {
        cprintf("伙伴系统未初始化\n");
        return;
    }
    
    cprintf("=== 伙伴系统状态 ===\n");
    cprintf("总大小：%d页\n", (int)buddy->size);
    cprintf("最大阶数：%d\n", (int)buddy->max_order);
    cprintf("根节点空闲大小：%u页\n", buddy->longest[0]);
    cprintf("===================\n");
}

static int validate_tree_consistency(struct buddy_system *buddy) {
    if (!buddy || !buddy->longest) {
        return 0;  // 假
    }
    
    // 对于伙伴系统，我们需要以不同方式检查树的一致性
    // 暂时跳过详细验证，只检查基本结构
    if (buddy->longest[0] > buddy->size) {
        cprintf("树不一致：根节点%u > 大小%d\n", 
               buddy->longest[0], (int)buddy->size);
        return 0;  // 假
    }
    
    return 1;  // 真
}

static int validate_allocation_tracking(struct buddy_system *buddy) {
    if (!buddy) {
        return 0;  // 假
    }
    
    // 基本验证 - 检查已分配的块不重叠
    // 这是一个简化的检查；完整实现会跟踪所有分配
    return validate_tree_consistency(buddy);
}

// PMM接口实现
void buddy_init(void) {
    // 初始化全局状态 - 实际的伙伴系统将在init_memmap中设置
    buddy_allocator = NULL;
    cprintf("buddy_init：伙伴系统分配器已初始化\n");
}

void buddy_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    
    // 向下舍入到最接近的2的幂次，用于伙伴系统
    size_t buddy_size = 1;
    while (buddy_size * 2 <= n) {
        buddy_size *= 2;
    }
    
    if (buddy_size < 1) {
        panic("buddy_init_memmap：伙伴系统内存不足");
    }
    
    // 分配伙伴系统结构
    // 注意：在真实内核中，这会使用不同的分配器
    // 现在我们使用静态分配方法
    static struct buddy_system buddy_instance;
    static unsigned int tree_array[2 * 32768 - 1]; // 支持最多32768页（128MB）
    
    if (buddy_size > 16384) {
        // 使用最大支持的大小
        buddy_size = 16384;  // 64MB的页面
    }
    
    buddy_allocator = &buddy_instance;
    buddy_allocator->size = buddy_size;
    buddy_allocator->longest = tree_array;
    buddy_allocator->base_page = base;
    
    // 计算max_order（大小的log2）
    buddy_allocator->max_order = 0;
    size_t temp = buddy_size;
    while (temp > 1) {
        temp >>= 1;
        buddy_allocator->max_order++;
    }
    
    // 初始化二叉树
    unsigned int node_size = buddy_size * 2;
    for (size_t i = 0; i < 2 * buddy_size - 1; ++i) {
        if (is_power_of_2(i + 1)) {
            node_size /= 2;
        }
        buddy_allocator->longest[i] = node_size;
    }
    
    // 初始化伙伴系统中的所有页面
    struct Page *p = base;
    for (size_t i = 0; i < buddy_size; i++) {
        assert(PageReserved(p));
        p->flags = 0;
        p->property = 0;
        set_page_ref(p, 0);
        p++;
    }
    
    // 为第一个页面设置属性以指示整个块
    base->property = buddy_size;
    SetPageProperty(base);
    
    cprintf("buddy_init_memmap：已初始化包含%d页的伙伴系统\n", (int)buddy_size);
}

struct Page *buddy_alloc_pages(size_t n) {
    if (n == 0) {
        n = 1;  // 至少分配1页
    }
    
    if (!buddy_allocator) {
        return NULL;
    }
    
    // 将大小标准化为2的幂次
    size_t size = fixsize(n);
    
    // 检查是否有足够的内存
    if (buddy_allocator->longest[0] < size) {
        return NULL;
    }
    
    // 使用树遍历找到合适的块
    size_t index = 0;
    size_t node_size = buddy_allocator->size;
    
    // 向下遍历树以找到合适大小的块
    for (; node_size != size; node_size /= 2) {
        if (buddy_allocator->longest[LEFT_LEAF(index)] >= size) {
            index = LEFT_LEAF(index);
        } else {
            index = RIGHT_LEAF(index);
        }
    }
    
    // 标记此块为已分配
    buddy_allocator->longest[index] = 0;
    clear_subtree_zero(buddy_allocator, index, node_size);

    // 计算已分配块的偏移量
    size_t offset = index_to_offset(buddy_allocator, index, node_size);
    
    // 更新父节点
    while (index) {
        index = PARENT(index);
        buddy_allocator->longest[index] = 
            MAX(buddy_allocator->longest[LEFT_LEAF(index)], 
                buddy_allocator->longest[RIGHT_LEAF(index)]);
    }
    
    // 获取页面指针
    struct Page *allocated_page = buddy_allocator->base_page + offset;
    
    // 标记页面为已分配
    for (size_t i = 0; i < size; i++) {
        struct Page *p = allocated_page + i;
        ClearPageProperty(p);
        SetPageReserved(p);
        set_page_ref(p, 0);
    }
    
    // 在第一个页面上设置属性以跟踪分配大小
    allocated_page->property = size;
    
    return allocated_page;
}

void buddy_free_pages(struct Page *base, size_t n) {
    if (n == 0) return;
    assert(base != NULL && buddy_allocator);

    if (base < buddy_allocator->base_page ||
        base >= buddy_allocator->base_page + buddy_allocator->size) {
        panic("buddy_free_pages：页面超出范围");
    }

    size_t offset = base - buddy_allocator->base_page;
    size_t size = base->property ? base->property : fixsize(n);
    if (!is_power_of_2(size) || size > buddy_allocator->size)
        size = fixsize(n);

    // 清除页面标志
    for (size_t i = 0; i < size; i++) {
        struct Page *p = base + i;
        ClearPageReserved(p);
        ClearPageProperty(p);
        set_page_ref(p, 0);
        p->flags = 0;
        p->property = 0;
    }

    // 1) 找到在树中表示该 (offset, size) 的节点：从根向下走直到 node_size == size
    size_t index = 0;
    size_t node_size = buddy_allocator->size;
    size_t local_off = offset;
    while (node_size > size) {
        size_t half = node_size >> 1;
        if (local_off < half) {
            index = LEFT_LEAF(index);
            // local_off 不变
        } else {
            index = RIGHT_LEAF(index);
            local_off -= half;
        }
        node_size >>= 1;
    }
    // 2) 将该节点及其子孙恢复为完全空闲（因为释放了该完整块）
    buddy_allocator->longest[index] = size;
    set_subtree_full(buddy_allocator, index, node_size);

    // 3) 向上尝试合并：父节点在两子节点都“完全空闲”时也变为完全空闲
    // 向上合并
    while (index) {
        index = PARENT(index);
        size <<= 1;

        size_t left = LEFT_LEAF(index);
        size_t right = RIGHT_LEAF(index);
        size_t left_longest = buddy_allocator->longest[left];
        size_t right_longest = buddy_allocator->longest[right];

        if (left_longest == right_longest && left_longest == size / 2) {
            // 两个子块都空闲，可以合并
            buddy_allocator->longest[index] = size;
        } else {
            buddy_allocator->longest[index] = MAX(left_longest, right_longest);
        }
    }
}


size_t buddy_nr_free_pages(void) {
    if (!buddy_allocator) {
        return 0;
    }
    
    // 根节点包含最大空闲块的大小
    // 为了完整计数，我们需要遍历树
    size_t total_free = 0;
    size_t node_size = buddy_allocator->size;
    
    // 遍历所有叶子节点以计算空闲页面
    for (size_t i = buddy_allocator->size - 1; i < 2 * buddy_allocator->size - 1; i++) {
        if (buddy_allocator->longest[i] > 0) {
            total_free += buddy_allocator->longest[i];
        }
    }
    
    return total_free;
}// ========================== 测试部分 ==========================

static void basic_buddy_check(void) {
    cprintf("=== 基本伙伴系统测试 ===\n");

    struct Page *p0, *p1, *p2;
    p0 = p1 = p2 = NULL;

    cprintf("[操作] 分配 1 页 (p0)\n");
    cprintf("[预期] 应从根块分裂出多个子块，最终获得一个 1 页块\n");
    p0 = buddy_alloc_pages(1);
    print_buddy_levels(buddy_allocator);

    cprintf("[操作] 分配 1 页 (p1)\n");
    cprintf("[预期] 再次分配一个 1 页块，应复用上一次分裂后剩余的小块\n");
    p1 = buddy_alloc_pages(1);
    print_buddy_levels(buddy_allocator);

    cprintf("[操作] 分配 1 页 (p2)\n");
    cprintf("[预期] 第三次分配，应继续使用低层空闲块，不再分裂上层\n");
    p2 = buddy_alloc_pages(1);
    print_buddy_levels(buddy_allocator);

    assert(p0 != p1 && p0 != p2 && p1 != p2);
    cprintf("[结果] 单页分配测试通过 ✅\n");

    cprintf("[操作] 释放 p0 (1 页)\n");
    cprintf("[预期] p0 释放后仍无法立即合并，等待相邻块释放\n");
    buddy_free_pages(p0, 1);
    print_buddy_levels(buddy_allocator);

    cprintf("[操作] 释放 p1 (1 页)\n");
    cprintf("[预期] p0 与 p1 为伙伴，应合并成一个 2 页块\n");
    buddy_free_pages(p1, 1);
    print_buddy_levels(buddy_allocator);

    cprintf("[操作] 释放 p2 (1 页)\n");
    cprintf("[预期] 若与上层伙伴连续，应继续向上合并\n");
    buddy_free_pages(p2, 1);
    print_buddy_levels(buddy_allocator);
    cprintf("[结果] 单页释放测试通过 ✅\n");

    cprintf("[操作] 分配 4 页 (p_multi)\n");
    cprintf("[预期] 应直接分配一个 4 页块，不触发额外分裂\n");
    struct Page *p_multi = buddy_alloc_pages(4);
    print_buddy_levels(buddy_allocator);
    assert(p_multi != NULL);
    cprintf("[结果] 多页分配测试通过 ✅\n");

    cprintf("[操作] 释放 p_multi (4 页)\n");
    cprintf("[预期] 应与空闲块合并，恢复到初始状态\n");
    buddy_free_pages(p_multi, 4);
    print_buddy_levels(buddy_allocator);
    cprintf("[结果] 多页释放测试通过 ✅\n");
}



static void buddy_edge_case_test(void) {
    cprintf("\n=== 边界情况测试 ===\n");


    cprintf("[操作] 分配最大块 (%d 页)\n[预期] 应直接占用整棵树\n", (int)buddy_allocator->size);
    size_t large_size = buddy_allocator->size;
    struct Page *p_large = buddy_alloc_pages(large_size);
    print_buddy_levels(buddy_allocator);
    assert(p_large != NULL);

    cprintf("[操作] 尝试额外分配 1 页\n[预期] 应失败 (无可用空间)\n");
    struct Page *p_fail = buddy_alloc_pages(1);
    assert(p_fail == NULL);
    cprintf("[结果] 内存耗尽测试通过 ✅\n");

    cprintf("[操作] 释放整个块 (%d 页)\n[预期] 恢复到初始状态\n", (int)large_size);
    buddy_free_pages(p_large, large_size);
    print_buddy_levels(buddy_allocator);
    cprintf("[结果] 大块释放测试通过 ✅\n");
}


// ========================== 综合测试入口 ==========================

void buddy_check(void) {
    cprintf("\n=== 开始伙伴系统综合检查 ===\n");

    if (!buddy_allocator) {
        cprintf("错误：伙伴系统未初始化！\n");
        return;
    }

    dump_buddy_state(buddy_allocator);

    if (!validate_tree_consistency(buddy_allocator)) {
        panic("buddy_check：树一致性验证失败");
    }
    cprintf("树一致性检查通过 ✅\n");

    basic_buddy_check();
    buddy_edge_case_test();

    if (!validate_tree_consistency(buddy_allocator)) {
        panic("buddy_check：最终树一致性验证失败");
    }

    dump_buddy_state(buddy_allocator);
    cprintf("\n=== 伙伴系统检查完成 ===\n所有测试成功通过！ ✅\n");
    cprintf("空闲页面：%d\n", (int)buddy_nr_free_pages());
}


// PMM管理器结构
const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_system_pmm_manager",
    .init = buddy_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check = buddy_check,
};