#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

#include <assert.h>


// Simple container_of implementation to get the containing structure
// from a pointer to a struct's field.
#ifndef container_of
#define container_of(ptr, type, member) ((char*)ptr - offsetof(type, member))
#endif


// Intrusive iterator type to embed in your structs.
typedef struct Iter Iter;

// The iterator next function IterNext updates the data in the result
// pointer, and returns whether or not it had data to provide.
typedef bool (*IterNext)(Iter *iter, void *result);

// The iterator type itself, with a field for each function that needs to
// be implemented (only one in this case).
typedef struct Iter {
    IterNext next;
} Iter;

// The Range type uses an iterator to provide a sequence of number from a starting
// value to an ending value.
typedef struct Range {
    Iter iter;
    uint32_t current;
    uint32_t end;
} Range;


// We need to forward-declare List in order to use it recursively as a pointer
// to the next link in the list.
typedef struct List List;

// The list type, with a pointer to the next node and a dummy data item as an
// example.
typedef struct List {
    List *next;
    int data;
} List;

// The list iterator type, which implements Iter by having a field of that type,
// and keeps a current state which is just a pointer to the current list node
// in its traveral of a list.
typedef struct ListIter {
    Iter iter;
    List *current;
} ListIter;


Range range_create(uint32_t start, uint32_t end);
bool range_next(Iter *iter, void *value);

List list_create(List *next, int data);
ListIter list_iter_create(List *root);
bool list_iter_next(Iter *iter, void *value);

int main(int argc, char *argv[]) {
    printf("\ncontainer_of test:\n");
    {
        // ensure that container_of actually provides a pointer to the containing struct
        Range range;
        assert(&range == (Range*)container_of(&range.iter, Range, iter));

        ListIter list_iter;
        assert(&list_iter == (ListIter*)container_of(&list_iter.iter, ListIter, iter));

        printf("container_of test passed\n");
    }

    printf("\nfor loop style 1:\n");
    {
        uint32_t expected = 0;

        Range range = range_create(0, 10);
        for (uint32_t index = 0; range.iter.next(&range.iter, &index);) {
            printf("index = %d\n", index);
            assert(index == expected);
            expected++;
        }
    }

    printf("\nfor loop style 2:\n");
    {
        uint32_t expected = 0;

        uint32_t index = 0;
        for (Range range = range_create(0, 10); range.iter.next(&range.iter, &index);) {
            printf("index = %d\n", index);
            assert(index == expected);
            expected++;
        }
    }

    printf("\nwhile loop style:\n");
    {
        Range range = range_create(0, 10);
        uint32_t index = 0;
        uint32_t expected = 0;
        while (range.iter.next(&range.iter, &index)) {
            printf("index = %d\n", index);
            assert(index == expected);
            expected++;
        }
    }

    printf("\nlists:\n");
    {
        List third = list_create(NULL, 3);
        List second = list_create(&third, 2);
        List root = list_create(&second, 1);

        ListIter list_iter = list_iter_create(&root);

        uint32_t expected = 1;

        List *current_node = NULL;
        while (list_iter.iter.next(&list_iter.iter, &current_node)) {
            assert(NULL != current_node);
            assert(expected == current_node->data);
            expected++;

            printf("node data = %d\n", current_node->data);
        }
    }
}

// Create a new range given the initial and ending value.
Range range_create(uint32_t start, uint32_t end) {
    // the maximum uint32_t would cause an overflow of the iterator index.
    assert(0xFFFFFFFF != end);

    return (Range){ { range_next }, start, end };
}

// Range type iterator implementation.
bool range_next(Iter *iter, void *value) {
    Range *range = (Range*)container_of(iter, Range, iter);

    uint32_t *result = (uint32_t*)value;

    // update the iterator result with the current range value.
    *result = range->current;

    // step to the next value.
    range->current++;

    // we are done only if we move past the range end.
    return range->current <= range->end;
}

// Create a new list node given its next node in the
// sequence, and the data that the node will contain.
List list_create(List *next, int data) {
    return (List){ next, data };
}

ListIter list_iter_create(List *root) {
    return (ListIter){ { list_iter_next }, root };
}

// List type iterator implementation.
bool list_iter_next(Iter *iter, void *value) {
    ListIter *list_iter = (ListIter*)container_of(iter, ListIter, iter);

    List **result = (List**)value;

    // the result is the current list node
    *result = list_iter->current;

    // there are more nodes if the current node has a non-null link.
    bool more_nodes = list_iter->current != NULL;
    if (more_nodes) {
        // move our current location in the list to the next node.
        list_iter->current = list_iter->current->next;
    }

    // if the current node is NULL, we reached the end of the list,
    // so stop the iteration.
    return more_nodes;
}

