/**
 * @file   skew_heap.h
 * @brief  Skew heap: a mergeable priority queue used by the Stride scheduler (lab6).
 *
 * A skew heap is a binary tree with the heap property under a user-supplied comparison function.
 * Every operation is built on skew_heap_merge(), which has amortised O(log n) cost. The nodes are
 * embedded into the objects that are queued (proc_struct::lab6_run_pool), so no memory is
 * allocated. A heap is represented by a pointer to its root; an empty heap is NULL. Insert and
 * remove return the new root, which the caller must store back.
 */
#ifndef __LIBS_SKEW_HEAP_H__
#define __LIBS_SKEW_HEAP_H__

/**
 * @brief Node of a skew heap, embedded in the queued object.
 * @var skew_heap_entry::parent  parent node, NULL for the root
 * @var skew_heap_entry::left    left child, NULL if none
 * @var skew_heap_entry::right   right child, NULL if none
 */
struct skew_heap_entry {
    struct skew_heap_entry *parent, *left, *right;
};

/** @brief Type name of a skew-heap node. */
typedef struct skew_heap_entry skew_heap_entry_t;

/**
 * @brief Comparison function that defines the order of the heap.
 * @param a first node
 * @param b second node
 * @return -1 if a must be nearer to the root than b (a has the higher priority), otherwise 0 or 1;
 *         skew_heap_merge() only tests for -1
 */
typedef int (*compare_f)(void *a, void *b);

/**
 * @brief Make a node a single, unlinked tree (all three links NULL).
 * @param a node to initialise
 */
static inline void skew_heap_init(skew_heap_entry_t *a) __attribute__((always_inline));
/**
 * @brief Merge two heaps into one and return its root.
 * @param a root of the first heap, or NULL
 * @param b root of the second heap, or NULL
 * @param comp comparison function of the heap order
 * @return root of the merged heap
 */
static inline skew_heap_entry_t *skew_heap_merge(skew_heap_entry_t *a, skew_heap_entry_t *b,
                                                 compare_f comp);
/**
 * @brief Insert the node b into the heap with root a.
 * @param a root of the heap, or NULL if the heap is empty
 * @param b node to insert; it is re-initialised first, so its old links are discarded
 * @param comp comparison function of the heap order
 * @return the new root of the heap
 */
static inline skew_heap_entry_t *skew_heap_insert(skew_heap_entry_t *a, skew_heap_entry_t *b,
                                                  compare_f comp) __attribute__((always_inline));
/**
 * @brief Remove the node b from the heap with root a.
 * @param a root of the heap
 * @param b node to remove; it must be in the heap
 * @param comp comparison function of the heap order
 * @return the new root of the heap, NULL if it became empty
 * @note The links of b are left as they were; re-initialise b before putting it into a heap again
 *       (skew_heap_insert() does that).
 */
static inline skew_heap_entry_t *skew_heap_remove(skew_heap_entry_t *a, skew_heap_entry_t *b,
                                                  compare_f comp) __attribute__((always_inline));

static inline void skew_heap_init(skew_heap_entry_t *a) {
    a->left = a->right = a->parent = NULL;
}

static inline skew_heap_entry_t *skew_heap_merge(skew_heap_entry_t *a, skew_heap_entry_t *b,
                                                 compare_f comp) {
    if (a == NULL)
        return b;
    else if (b == NULL)
        return a;

    skew_heap_entry_t *l, *r;
    if (comp(a, b) == -1) {
        // a is the new root: its right subtree is merged with b and swapped to the left.
        r = a->left;
        l = skew_heap_merge(a->right, b, comp);

        a->left = l;
        a->right = r;
        if (l)
            l->parent = a;

        return a;
    } else {
        r = b->left;
        l = skew_heap_merge(a, b->right, comp);

        b->left = l;
        b->right = r;
        if (l)
            l->parent = b;

        return b;
    }
}

static inline skew_heap_entry_t *skew_heap_insert(skew_heap_entry_t *a, skew_heap_entry_t *b,
                                                  compare_f comp) {
    skew_heap_init(b);
    return skew_heap_merge(a, b, comp);
}

static inline skew_heap_entry_t *skew_heap_remove(skew_heap_entry_t *a, skew_heap_entry_t *b,
                                                  compare_f comp) {
    skew_heap_entry_t *p = b->parent;
    skew_heap_entry_t *rep = skew_heap_merge(b->left, b->right, comp);
    if (rep)
        rep->parent = p;

    if (p) {
        if (p->left == b)
            p->left = rep;
        else
            p->right = rep;
        return a;
    } else
        return rep;
}

#endif /* !__LIBS_SKEW_HEAP_H__ */
