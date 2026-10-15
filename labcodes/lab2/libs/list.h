/**
 * @file   list.h
 * @brief  Intrusive doubly linked circular list implementation.
 *
 * A list node list_entry_t is embedded in a host structure and carries no data
 * field of its own. To reach the data, recover the host structure pointer from
 * the node pointer with to_struct (or le2page in memlayout.h). The list head
 * (sentinel) is also a list_entry_t and holds no valid data; an empty list
 * satisfies head->next == head. Every operation is O(1), allocates no memory,
 * takes no locks, so concurrent access must be synchronised by the caller.
 *
 * @code
 *   struct Page {
 *       ...
 *       list_entry_t page_link;   // embedded list node
 *   };
 *   list_entry_t free_list;       // list head
 *   list_init(&free_list);
 *   list_add(&free_list, &p->page_link);
 *   list_del_init(&p->page_link);
 * @endcode
 */

#ifndef __LIBS_LIST_H__
#define __LIBS_LIST_H__

#ifndef __ASSEMBLER__

#include <defs.h>

/**
 * @brief List node structure, also used as the list head (sentinel).
 *
 * Holds only the predecessor and successor pointers; all data is owned by the host structure.
 */
struct list_entry {
    struct list_entry *prev, *next; /**< Predecessor and successor node pointers */
};

/**
 * @brief Type alias for struct list_entry.
 */
typedef struct list_entry list_entry_t;

/**
 * @brief Initialise a node as an empty single-element list (it points to itself).
 *
 * Used for newly allocated or reused nodes, and for initialising list heads.
 * Does not check whether the node is currently linked into another list.
 *
 * @param[in] elm  Node to initialise, must not be NULL.
 * @note O(1).
 */
static inline void list_init(list_entry_t *elm) __attribute__((always_inline));

/**
 * @brief Insert a new node after the given node (same as list_add_after).
 *
 * When @p listelm is the sentinel of a list, this inserts @p elm at the front
 * of the list (stack-like insertion).
 *
 * @param[in,out] listelm  Reference node already in the list (usually the head), must not be NULL.
 * @param[in,out] elm      Node to insert; it must not currently belong to any list.
 * @note O(1).
 */
static inline void list_add(list_entry_t *listelm, list_entry_t *elm)
    __attribute__((always_inline));

/**
 * @brief Insert a new node before the given node.
 *
 * When @p listelm is the sentinel of a list, this inserts @p elm at the end
 * of the list (queue-like insertion).
 *
 * @param[in,out] listelm  Reference node already in the list (usually the head), must not be NULL.
 * @param[in,out] elm      Node to insert; it must not currently belong to any list.
 * @note O(1).
 */
static inline void list_add_before(list_entry_t *listelm, list_entry_t *elm)
    __attribute__((always_inline));

/**
 * @brief Insert a new node after the given node.
 *
 * Same functionality as list_add. This name is kept so callers can express
 * "insert after this node".
 *
 * @param[in,out] listelm  Reference node already in the list, must not be NULL.
 * @param[in,out] elm      Node to insert; it must not currently belong to any list.
 * @note O(1).
 */
static inline void list_add_after(list_entry_t *listelm, list_entry_t *elm)
    __attribute__((always_inline));

/**
 * @brief Unlink a node from its list without re-initialising it.
 *
 * Only the predecessor's and successor's pointers are updated so that they
 * point to each other. The removed node keeps its old prev/next values.
 *
 * @param[in,out] listelm  Node to remove. It must currently be in a list and must not be removed twice.
 * @warning After this call list_empty(listelm) is not guaranteed to be true; the node is
 *          in an undefined state. To reuse the node or test its state again, use list_del_init.
 * @note O(1).
 */
static inline void list_del(list_entry_t *listelm) __attribute__((always_inline));

/**
 * @brief Unlink a node from its list and re-initialise it as an empty list.
 *
 * Calls list_del to unlink, then list_init so the node forms a one-element ring.
 *
 * @param[in,out] listelm  Node to remove. It must currently be in a list.
 * @note After this call list_empty(listelm) returns true. O(1).
 */
static inline void list_del_init(list_entry_t *listelm) __attribute__((always_inline));

/**
 * @brief Test whether a list is empty.
 *
 * Only checks whether the successor of @p list points back to @p list itself,
 * so it is meaningful for list heads (or nodes after list_init). The result is
 * undefined for a node that was removed with list_del.
 *
 * @param[in] list  List head, must not be NULL.
 * @retval non-zero  The list contains no elements.
 * @retval 0         The list contains at least one element.
 * @note O(1).
 */
static inline bool list_empty(list_entry_t *list) __attribute__((always_inline));

/**
 * @brief Get the successor of a node.
 *
 * The typical forward traversal is: while ((le = list_next(le)) != &head).
 *
 * @param[in] listelm  Reference node (usually the head), must not be NULL.
 * @return The successor node pointer. If @p listelm is the last element, the head is returned.
 * @note O(1).
 */
static inline list_entry_t *list_next(list_entry_t *listelm) __attribute__((always_inline));

/**
 * @brief Get the predecessor of a node.
 *
 * The typical backward traversal is: while ((le = list_prev(le)) != &head).
 *
 * @param[in] listelm  Reference node (usually the head), must not be NULL.
 * @return The predecessor node pointer. If @p listelm is the first element, the head is returned.
 * @note O(1).
 */
static inline list_entry_t *list_prev(list_entry_t *listelm) __attribute__((always_inline));

/**
 * @brief Insert a node between two known adjacent nodes (internal use only).
 *
 * The caller must already know that @p prev and @p next are adjacent, i.e.
 * prev->next == next and next->prev == prev. Otherwise the list is corrupted.
 *
 * @param[in,out] elm   Node to insert.
 * @param[in,out] prev  Predecessor at the insertion point.
 * @param[in,out] next  Successor at the insertion point.
 * @note O(1).
 */
static inline void __list_add(list_entry_t *elm, list_entry_t *prev, list_entry_t *next)
    __attribute__((always_inline));

/**
 * @brief Remove the node between @p prev and @p next by linking them to each other (internal use only).
 *
 * The caller must know that @p prev and @p next are the predecessor and
 * successor of the node being removed. The removed node's own pointers are not modified.
 *
 * @param[in,out] prev  Predecessor of the node being removed.
 * @param[in,out] next  Successor of the node being removed.
 * @note O(1).
 */
static inline void __list_del(list_entry_t *prev, list_entry_t *next)
    __attribute__((always_inline));

/**
 * @brief Initialise a node as an empty list.
 *
 * @details Points both prev and next at the node itself.
 */
static inline void list_init(list_entry_t *elm) {
    elm->prev = elm->next = elm;
}

/**
 * @brief Insert a node after @p listelm.
 *
 * @details Delegates to list_add_after, with the same implementation.
 */
static inline void list_add(list_entry_t *listelm, list_entry_t *elm) {
    list_add_after(listelm, elm);
}

/**
 * @brief Insert a node before @p listelm.
 *
 * @details Implemented as __list_add(elm, listelm->prev, listelm).
 */
static inline void list_add_before(list_entry_t *listelm, list_entry_t *elm) {
    __list_add(elm, listelm->prev, listelm);
}

/**
 * @brief Insert a node after @p listelm.
 *
 * @details Implemented as __list_add(elm, listelm, listelm->next).
 */
static inline void list_add_after(list_entry_t *listelm, list_entry_t *elm) {
    __list_add(elm, listelm, listelm->next);
}

/**
 * @brief Unlink a node without re-initialising it.
 *
 * @details Implemented as __list_del(listelm->prev, listelm->next). The removed node's pointers are left unchanged.
 */
static inline void list_del(list_entry_t *listelm) {
    __list_del(listelm->prev, listelm->next);
}

/**
 * @brief Unlink a node and re-initialise it.
 *
 * @details Calls list_del followed by list_init.
 */
static inline void list_del_init(list_entry_t *listelm) {
    list_del(listelm);
    list_init(listelm);
}

/**
 * @brief Test whether a list is empty.
 *
 * @details Checks whether list->next is equal to list itself.
 */
static inline bool list_empty(list_entry_t *list) {
    return list->next == list;
}

/**
 * @brief Get the successor of a node.
 *
 * @details Returns listelm->next directly.
 */
static inline list_entry_t *list_next(list_entry_t *listelm) {
    return listelm->next;
}

/**
 * @brief Get the predecessor of a node.
 *
 * @details Returns listelm->prev directly.
 */
static inline list_entry_t *list_prev(list_entry_t *listelm) {
    return listelm->prev;
}

/**
 * @brief Insert a node between two adjacent nodes (internal use only).
 *
 * @details Updates prev->next, next->prev and the two pointers of elm. Requires
 *          that prev and next are already adjacent.
 */
static inline void __list_add(list_entry_t *elm, list_entry_t *prev, list_entry_t *next) {
    prev->next = next->prev = elm;
    elm->next = next;
    elm->prev = prev;
}

/**
 * @brief Remove the node between two adjacent nodes (internal use only).
 *
 * @details Sets prev->next = next and next->prev = prev. The removed node's pointers are not modified.
 */
static inline void __list_del(list_entry_t *prev, list_entry_t *next) {
    prev->next = next;
    next->prev = prev;
}

#endif /* !__ASSEMBLER__ */

#endif /* !__LIBS_LIST_H__ */
