#ifndef __KERN_MM_BUDDY_SYSTEM_PMM_H__
#define __KERN_MM_BUDDY_SYSTEM_PMM_H__

#include <pmm.h>
#include <memlayout.h>
#include <defs.h>
#include <assert.h>

/**
 * 伙伴系统物理内存管理器
 * 
 * 本模块为内核物理内存管理实现了伙伴系统分配器。
 * 伙伴系统使用二叉树结构维护2的幂次大小的空闲内存块，
 * 提供高效的分配和释放操作，并通过自动合并来最小化碎片。
 * 
 * 主要特性:
 * - 2的幂次大小分配
 * - 释放时自动伙伴合并
 * - O(log n) 分配和释放时间复杂度
 * - 最小化外部碎片
 * - 与现有PMM框架集成
 */

/**
 * struct buddy_system - 伙伴系统核心数据结构
 * @size: 管理的总页数（必须是2的幂次）
 * @longest: 存储每个节点最大空闲块大小的二叉树数组
 * @base_page: 管理内存区域的基页指针
 * @max_order: 最大阶数（总大小的log2值）
 * 
 * 二叉树以数组形式存储：
 * - 索引0是根节点（代表整个内存空间）
 * - 对于节点i：左子节点=2*i+1，右子节点=2*i+2，父节点=(i-1)/2
 * - 每个节点存储其子树中最大空闲块的大小
 */
struct buddy_system {
    size_t size;                    
    unsigned int *longest;          
    struct Page *base_page;         
    size_t max_order;              
};

// Tree navigation macros
#define LEFT_LEAF(index)    ((index) * 2 + 1)
#define RIGHT_LEAF(index)   ((index) * 2 + 2)
#define PARENT(index)       (((index) + 1) / 2 - 1)

// Utility macros
#define IS_POWER_OF_2(x)    (!((x) & ((x) - 1)))
#define MAX(a, b)           ((a) > (b) ? (a) : (b))

// Global buddy system instance
extern struct buddy_system *buddy_allocator;

// PMM interface functions
void buddy_init(void);
void buddy_init_memmap(struct Page *base, size_t n);
struct Page *buddy_alloc_pages(size_t n);
void buddy_free_pages(struct Page *base, size_t n);
size_t buddy_nr_free_pages(void);
void buddy_check(void);

// Utility functions
static inline int is_power_of_2(size_t n);
static size_t fixsize(size_t size);
static inline size_t left_leaf(size_t index);
static inline size_t right_leaf(size_t index);
static inline size_t parent(size_t index);

// Tree navigation and conversion functions
static size_t index_to_offset(struct buddy_system *buddy, size_t index, size_t node_size);
static size_t offset_to_index(struct buddy_system *buddy, size_t offset, size_t size);

// Debugging and diagnostic functions
static void dump_buddy_state(struct buddy_system *buddy);
static int validate_tree_consistency(struct buddy_system *buddy);
static int validate_allocation_tracking(struct buddy_system *buddy);

// PMM manager structure
extern const struct pmm_manager buddy_pmm_manager;

// Inline utility function implementations
static inline int is_power_of_2(size_t n) {
    return n > 0 && !(n & (n - 1));
}

static inline size_t left_leaf(size_t index) {
    return LEFT_LEAF(index);
}

static inline size_t right_leaf(size_t index) {
    return RIGHT_LEAF(index);
}

static inline size_t parent(size_t index) {
    return PARENT(index);
}

#endif /* !__KERN_MM_BUDDY_SYSTEM_PMM_H__ */