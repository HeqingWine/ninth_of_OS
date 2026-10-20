#include <pmm.h>
#include <list.h>
#include <buddy_pmm.h>
#include <defs.h>
#include <stdio.h>

//lab2 function2-3 2414113

// =====================================================================
// 任务三：buddy system（伙伴系统）分配算法
//
//
//   1) buddy_init           —— 初始化 MAX_ORDER 个空闲链表
//   2) buddy_init_memmap    —— 把连续空闲页切成 2 的幂大小块，加入对应链表
//   3) buddy_alloc_pages    —— 按 2 的幂向上取整分配，必要时分裂大块
//   4) buddy_free_pages     —— 释放并尝试与「伙伴」块合并
//   5) buddy_nr_free_pages  —— 统计空闲页总数
//   6) buddy_check          —— 设计测试，验证分配/释放/合并的正确性
//   7) buddy_pmm_manager    —— 注册 struct pmm_manager（本文件底部）
//
// 提示：
//   - 每个空闲块大小必须是 2 的幂，可用 Page 的 property 字段记录块的「阶」
//     order（块大小 = 1 << order 页）；
//   - 分配时请求 n 页，先向上取整到 2 的幂，再从高阶链表中取块逐级「分裂」；
//   - 释放时把块放回对应阶的链表，并检查「伙伴」是否空闲：若空闲则合并并升阶；
//   - 伙伴地址计算：ppn ^ (1 << order)，即地址相邻且恰好拼成上一阶的块。
// =====================================================================

#define MAX_ORDER 16   // 阶 0..15：支持 1 页到 2^15 页的块

static size_t buddy_nr_free_pages(void);   // 前置声明（buddy_alloc_pages 中引用）

static free_area_t free_area[MAX_ORDER];

#define buddy_free_list(order) (free_area[order].free_list)
#define buddy_nr(order)        (free_area[order].nr_free)

static int buddy_get_order(size_t n) {
    if (n == 0) return 0;
    int order = 0;
    size_t size = 1;
    while (size < n && order < MAX_ORDER - 1) {
        size <<= 1;
        order++;
    }
    return order;
}


static void
buddy_init(void) {
    for (int i = 0; i < MAX_ORDER; i++) {
        list_init(&(free_area[i].free_list));
        buddy_nr(i) = 0;
    }
}


// 切成若干 2 的幂块（每块首地址必须按 2^order 对齐，否则伙伴关系错位），
// 逐块设置 property = order、SetPageProperty、加入对应链表、nr 计数
static void
buddy_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p++) {
        assert(PageReserved(p));
        p->flags = 0;
        p->property = 0;
        set_page_ref(p, 0);
    }
    while(n>0){
        int order = buddy_get_order(n);
        size_t pnum = 1UL << order;
        if (pnum > n) {
            order -= 1;
            pnum >>= 1;
        }
        while (order > 0 && (page2ppn(base) & (pnum - 1)) != 0) {
            order -= 1;
            pnum >>= 1;
        }
        base->property = order;
        SetPageProperty(base);
        list_add(&buddy_free_list(order), &(base->page_link));
        buddy_nr(order) += 1;
        base += pnum;
        n -= pnum;
    }
}


// n 向上取整到 2 的幂 order；从 order 开始向上找第一个非空链表；
// 取块后逐级「分裂」，右半块作为新空闲块加入下一阶链表；
// 返回块首页指针，块 property = order
static struct Page *
buddy_alloc_pages(size_t n) {
    assert(n > 0);
    if(n > (1ULL << (MAX_ORDER - 1))) {
        return NULL; // 请求超过最大块大小
    }
    int order = buddy_get_order(n);
    for(int current_order = order; current_order < MAX_ORDER; current_order++) {
        if(!list_empty(&(buddy_free_list(current_order)))) {
            struct Page *page = le2page(list_next(&(buddy_free_list(current_order))), page_link);
            list_del(&(page->page_link));
            buddy_nr(current_order)--;
            ClearPageProperty(page);
            while(current_order > order) {
                current_order--;
                struct Page *buddy = page + (1ULL << current_order);
                buddy->property = current_order;
                SetPageProperty(buddy);
                list_add(&(buddy_free_list(current_order)), &buddy->page_link);
                buddy_nr(current_order)++;
            }
            page->property = order;
            return page;
        }
    }
    return NULL; // 没有足够的空闲块
}


// 在对应阶链表中循环检查「伙伴」（ppn ^ (1 << order)）是否空闲，
// 空闲则摘除伙伴合并、order+1；合并完成后把块挂入对应链表
static void
buddy_free_pages(struct Page *base, size_t n) {
    assert(PageReserved(base) == 0);
    int order = buddy_get_order(n);
    struct Page *p = base;
    while (order < MAX_ORDER) {
        size_t buddy_ppn = page2ppn(p) ^ (1ULL << order);
        struct Page *buddy = &pages[buddy_ppn - nbase];
        if (!PageProperty(buddy) || buddy->property != order) {
            break; // 伙伴不空闲或阶数不匹配，停止合并
        }
        list_del(&(buddy->page_link));
        buddy_nr(order)--;
        ClearPageProperty(buddy);
        if (buddy < p) {
            p = buddy; // 合并后的块首地址取较小的伙伴
        }
        order++;
    }
    p ->property = order;
    SetPageProperty(p);
    list_add(&(buddy_free_list(order)), &p->page_link);
    buddy_nr(order)++;
}


// （注意 buddy_nr(order) 记的是「块数」，每块 2^order 页）
static size_t
buddy_nr_free_pages(void) {
    size_t total = 0;
    for (int i = 0; i < MAX_ORDER; i++) {
        total += buddy_nr(i) * (1ULL << i);
    }
    return total;
}


// 建议自建一块可控测试区（借用 alloc_pages(1024)），至少覆盖：
// 基本分配/释放、计数守恒、伙伴分裂、伙伴合并、分配失败返回 NULL。
static void
buddy_check(void) {
        cprintf("checking buddy system ...\n");
    size_t nr_total = buddy_nr_free_pages();
    assert(nr_total > 0);

    // 借用 1024 页连续空闲内存作为可控测试区
    struct Page *area = alloc_pages(1024);
    assert(area != NULL);
    assert((page2ppn(area) & 1023) == 0);   // 伙伴系统保证块 1024 对齐

    // 保存当前空闲链表状态，测试结束后恢复
    free_area_t saved[MAX_ORDER];
    for (int i = 0; i < MAX_ORDER; i++) {
        saved[i] = free_area[i];
    }

    // 重建测试区：清空链表，把 1024 页做成一个空闲块
    buddy_init();
    for (int i = 0; i < 1024; i++) {
        area[i].flags = 0;
        area[i].property = 0;
        set_page_ref(&area[i], 0);
        SetPageReserved(&area[i]);
    }
    buddy_init_memmap(area, 1024);
    assert(buddy_nr_free_pages() == 1024);
    assert(buddy_nr(10) == 1);   // 1024 页恰好一个 2^10 块

    // 1) 基本分配/释放与计数守恒（3->4 页、1->1 页、5->8 页）
    struct Page *p0 = alloc_pages(3);
    assert(p0 != NULL);
    assert(p0 == area);
    assert(buddy_nr_free_pages() == 1024 - 4);
    struct Page *p1 = alloc_pages(1);
    assert(p1 != NULL);
    assert(p1 == area + 4);
    struct Page *p2 = alloc_pages(5);
    assert(p2 != NULL);
    assert(p2 == area + 8);
    assert(buddy_nr_free_pages() == 1024 - 4 - 1 - 8);
    // 返回块按 2 的幂对齐（分裂正确性的直接体现）
    assert((page2ppn(p0) & 3) == 0);
    assert((page2ppn(p2) & 7) == 0);
    free_pages(p0, 3);
    free_pages(p1, 1);
    free_pages(p2, 5);
    assert(buddy_nr_free_pages() == 1024);
    // 释放后应完全合并回单个 1024 页块
    assert(buddy_nr(10) == 1 && list_empty(&buddy_free_list(9)));

    // 2) 大块分裂与地址关系（与 ucore 官方 buddy_check 同款断言）
    p0 = alloc_pages(70);   // 占用 128 页块
    assert(p0 != NULL && p0 == area);
    p1 = alloc_pages(35);   // 占用 64 页块，应紧跟在 p0 之后
    assert(p1 != NULL);
    assert(p1 == p0 + 128);
    p2 = alloc_pages(80);   // 占用 128 页块，应紧跟在 p1 之后
    assert(p2 != NULL);
    assert(p2 == p0 + 256);
    free_pages(p0, 70);
    free_pages(p1, 35);     // 释放后 p0(128) 与 p1(64)+残留 64 合并回 256 页块
    p1 = alloc_pages(256);
    assert(p1 == p0);       // 能直接分配 256 页块说明合并确实发生
    free_pages(p1, 256);
    free_pages(p2, 80);
    assert(buddy_nr_free_pages() == 1024);
    assert(buddy_nr(10) == 1);   // 全部归还后重新合并回单个 1024 页块

    // 3) 小块伙伴合并：两个 1 页伙伴释放后合并成 2 页块，再合并回大块
    p0 = alloc_pages(1);
    p1 = alloc_pages(1);
    assert(p0 != NULL && p1 != NULL && p0 != p1);
    free_pages(p0, 1);
    free_pages(p1, 1);
    struct Page *p3 = alloc_pages(2);
    assert(p3 != NULL);
    free_pages(p3, 2);
    assert(buddy_nr_free_pages() == 1024);

    // 4) 边界：请求超过总空闲时返回 NULL
    assert(alloc_pages(1025) == NULL);
    assert(buddy_nr_free_pages() == 1024);

    // 5) 归还借用页并恢复原空闲链表状态
    buddy_init();
    for (int i = 0; i < MAX_ORDER; i++) {
        free_area[i] = saved[i];
    }
    for (int i = 0; i < 1024; i++) {
        area[i].flags = 0;
        area[i].property = 0;
        set_page_ref(&area[i], 0);
    }
    free_pages(area, 1024);   // 归还给原系统，会与相邻伙伴正常合并
    assert(buddy_nr_free_pages() == nr_total);

    cprintf("buddy check succeeded\n");
}

const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager",
    .init = buddy_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check = buddy_check,
};
