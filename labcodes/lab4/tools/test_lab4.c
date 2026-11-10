/**
 * @file   test_lab4.c
 * @brief  Grading tests for lab4: physical memory and page tables, process creation, and scheduling.
 *
 * This file is not part of the student tree. tools/inject.py copies it to kern/test/ and inserts
 * the calls below into pmm_init (test_lab4_pmm_basic), proc_init (test_lab4_process_create) and
 * init_main (test_lab4_schedule). Each test prints "[Test <name>] PASS" when all its checks pass;
 * a failed assert() panics the kernel, so the output line is then missing and grade.sh scores it 0.
 */
#include <defs.h>
#include <assert.h>
#include <pmm.h>
#include <proc.h>
#include <sched.h>
#include <stdio.h>

/**
 * @brief Print the pass line for a test.
 * @param[in] name  Test name shown in the output, e.g. "PMM".
 */
static void lab4_pass(const char *name)
{
    cprintf("[Test %s] PASS\n", name);
}

/**
 * @brief Basic test of the page allocator and the Sv39 page-table functions.
 *
 * Allocates three pages and maps them with page_insert() at virtual addresses 0, PGSIZE and
 * 2*PGSIZE in boot_pgdir_va. It checks the PTE contents (valid bit, PTE_U/PTE_W), the reference
 * counts from page_ref(), that remapping a page with new permissions keeps the free-page count
 * unchanged, and that page_remove() drops all references. Prints "[Test PMM] PASS" on success.
 * Called at the end of pmm_init().
 */
void test_lab4_pmm_basic(void)
{
    struct Page *p1 = alloc_page();
    struct Page *p2 = alloc_page();
    struct Page *p3 = alloc_page();
    pte_t *ptep;
    size_t free_before;

    assert(p1 != NULL && p2 != NULL && p3 != NULL);

    assert(page_insert(boot_pgdir_va, p1, 0, 0) == 0);
    assert((ptep = get_pte(boot_pgdir_va, 0, 0)) != NULL);
    assert((*ptep & PTE_V) && pte2page(*ptep) == p1 && page_ref(p1) == 1);

    assert(page_insert(boot_pgdir_va, p2, PGSIZE, PTE_U | PTE_W) == 0);
    assert(page_insert(boot_pgdir_va, p1, PGSIZE, 0) == 0);
    assert(page_ref(p1) == 2 && page_ref(p2) == 0);
    assert((ptep = get_pte(boot_pgdir_va, PGSIZE, 0)) != NULL);
    assert(pte2page(*ptep) == p1 && (*ptep & (PTE_U | PTE_W)) == 0);

    assert(page_insert(boot_pgdir_va, p3, 2 * PGSIZE, 0) == 0);
    free_before = nr_free_pages();
    assert(page_insert(boot_pgdir_va, p3, 2 * PGSIZE, PTE_W) == 0);
    assert(page_ref(p3) == 1 && nr_free_pages() == free_before);

    page_remove(boot_pgdir_va, 0);
    page_remove(boot_pgdir_va, PGSIZE);
    page_remove(boot_pgdir_va, 2 * PGSIZE);
    page_remove(boot_pgdir_va, 2 * PGSIZE);
    assert(page_ref(p1) == 0 && page_ref(p2) == 0 && page_ref(p3) == 0);

    lab4_pass("PMM");
}

/**
 * @brief Check the state of the idle process (pid 0) and the init process (pid 1) after proc_init().
 *
 * Verifies that idleproc is the current process, is RUNNABLE, uses bootstack as its kernel stack,
 * and has no mm or trap frame; that initproc has pid 1, is RUNNABLE, has idleproc as parent, and has
 * a valid context (ra and sp set). Prints "[Test Process Create] PASS" on success.
 * Called at the end of proc_init().
 */
void test_lab4_process_create(void)
{
    assert(current == idleproc);
    assert(idleproc != NULL && idleproc->pid == 0);
    assert(idleproc->state == PROC_RUNNABLE);
    assert(idleproc->kstack == (uintptr_t)bootstack);
    assert(idleproc->parent == NULL && idleproc->mm == NULL);
    assert(idleproc->tf == NULL && idleproc->runs == 0);
    assert(idleproc->flags == 0 && idleproc->context.ra == 0);

    assert(initproc != NULL && initproc->pid == 1);
    assert(initproc->state == PROC_RUNNABLE);
    assert(initproc->parent == idleproc && initproc->mm == NULL);
    assert(initproc->context.ra != 0 && initproc->context.sp != 0);

    lab4_pass("Process Create");
}

/** @brief Set to 1 once kernel thread 1 has run; written by test_lab4_thread. */
static volatile int test_thread1_ran;
/** @brief Set to 1 once kernel thread 2 has run; written by test_lab4_thread. */
static volatile int test_thread2_ran;

/**
 * @brief Body of the two test kernel threads.
 *
 * Runs three rounds: set this thread's "ran" flag, request a reschedule (need_resched = 1) and call
 * schedule(), so control passes to the other thread. After the rounds it keeps yielding forever.
 *
 * @param[in] arg  Thread id cast to a pointer: 1 selects test_thread1_ran, anything else test_thread2_ran.
 */
static void test_lab4_thread(void *arg)
{
    int id = (int)(intptr_t)arg;
    int i;
    for (i = 0; i < 3; i++)
    {
        if (id == 1) test_thread1_ran = 1;
        else test_thread2_ran = 1;
        current->need_resched = 1;
        schedule();
    }
    while (1)
    {
        current->need_resched = 1;
        schedule();
    }
}

/**
 * @brief Scheduling test: create two kernel threads and check that both get to run.
 *
 * Creates two threads with kernel_thread() running test_lab4_thread (ids 1 and 2), forces eight
 * reschedules from the calling process, and asserts that both threads set their flags. Prints
 * "[Test Schedule] PASS" on success. Called at the start of init_main().
 */
void test_lab4_schedule(void)
{
    int i;
    test_thread1_ran = test_thread2_ran = 0;
    assert(kernel_thread(test_lab4_thread, (void *)(intptr_t)1, 0) > 0);
    assert(kernel_thread(test_lab4_thread, (void *)(intptr_t)2, 0) > 0);

    for (i = 0; i < 8; i++)
    {
        current->need_resched = 1;
        schedule();
    }
    assert(test_thread1_ran && test_thread2_ran);
    lab4_pass("Schedule");
}