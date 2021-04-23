#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <string.h>
#include <assert.h>


// Simple container_of implementation to get the containing structure
// from a pointer to a struct's field.
#ifndef container_of
#define container_of(ptr, type, member) ((char*)ptr - offsetof(type, member))
#endif

// NOTE this implementation is a toy example. It does not account for
// pointer alignment, nor does it check for arithmatic overflow, nor
// perhaps all kinds of other issue!

typedef struct Allocator Allocator;

typedef void* (*AllocatorAlloc)(Allocator *allocator, size_t size);
typedef void (*AllocatorFree)(Allocator *allocator, void *ptr);
typedef void* (*AllocatorRealloc)(Allocator *allocator, void *old_ptr, size_t new_size);

// I didn't include calloc for simplicity
typedef struct Allocator {
    AllocatorAlloc alloc;
    AllocatorFree free;
    AllocatorRealloc realloc;
} Allocator;

typedef struct HeapAllocator {
    Allocator allocator;
} HeapAllocator;

typedef struct ArenaAllocator {
    Allocator allocator;
    Allocator *backing_allocator;
    uint8_t *memory;
    uint32_t count;
    uint32_t length;
} ArenaAllocator;

typedef struct BumpAllocator {
    Allocator allocator;
    uint8_t *memory;
    uint32_t count;
    uint32_t length;
} BumpAllocator;

// HeapAllocator functions
HeapAllocator heap_allocator_create(void);
void *heap_allocator_alloc(Allocator *allocator, size_t size);
void heap_allocator_free(Allocator *allocator, void *ptr);
void *heap_allocator_realloc(Allocator *allocator, void *old_ptr, size_t new_size);

ArenaAllocator arena_allocator_create(Allocator *backing_allocator);
void arena_allocator_destroy(ArenaAllocator *arena_allocator);
void arena_allocator_clear(ArenaAllocator *arena_allocator);

void *arena_allocator_alloc(Allocator *allocator, size_t size);
void arena_allocator_free(Allocator *allocator, void *ptr);
void *arena_allocator_realloc(Allocator *allocator, void *old_ptr, size_t size);

BumpAllocator bump_allocator_create(size_t capacity);
void bump_allocator_destroy(BumpAllocator *bump_allocator);
void *bump_allocator_alloc(Allocator *allocator, size_t size);
void bump_allocator_free(Allocator *allocator, void *ptr);
void *bump_allocator_realloc(Allocator *allocator, void *old_ptr, size_t size);


int main(int argc, char *argv[]) {
    printf("\nHeap allocator test:\n");
    {
        HeapAllocator heap_allocator = heap_allocator_create();

        char *memory = heap_allocator.allocator.alloc(&heap_allocator.allocator, 100);
        assert(NULL != memory);

        memory = heap_allocator.allocator.realloc(&heap_allocator.allocator, memory, 200);
        assert(NULL != memory);

        heap_allocator.allocator.free(&heap_allocator.allocator, memory);
        printf("Heap allocator test complete\n");
    }

    printf("\nArena allocator test\n");
    {
        printf("Arena allocator test complete\n");
    }

    printf("\nBump allocator test\n");
    {
        printf("Bump allocator test complete\n");
    }
}

/* Heap Allocator */
HeapAllocator heap_allocator_create(void) {
    return (HeapAllocator) { { heap_allocator_alloc, heap_allocator_free, heap_allocator_realloc, } };
}

void *heap_allocator_alloc(Allocator *allocator, size_t size) {
    // we don't actually need the allocator.
    //HeapAllocator *heap_allocator = (HeapAllocator*)container_of(allocator, HeapAllocator, allocator);
    return malloc(size);
}

void heap_allocator_free(Allocator *allocator, void *ptr) {
    // we don't actually need the allocator.
    //HeapAllocator *heap_allocator = (HeapAllocator*)container_of(allocator, HeapAllocator, allocator);
    free(ptr);
}

void *heap_allocator_realloc(Allocator *allocator, void *old_ptr, size_t new_size) {
    // we don't actually need the allocator.
    //HeapAllocator *heap_allocator = (HeapAllocator*)container_of(allocator, HeapAllocator, allocator);
    return realloc(old_ptr, new_size);
}

/* Arena Allocator */
ArenaAllocator arena_allocator_create(Allocator *backing_allocator) {
    Allocator allocator = (Allocator) { arena_allocator_alloc, arena_allocator_free, arena_allocator_realloc, };

    // we start with no memory allocated here, to make allocator creation fast
    return (ArenaAllocator){ allocator, backing_allocator, NULL, 0, 0 };
}

void arena_allocator_destroy(ArenaAllocator *arena_allocator) {
    if (NULL != arena_allocator->memory) {
        // free using the same allocator that allocated the memory.
        arena_allocator->backing_allocator->free(arena_allocator->backing_allocator, arena_allocator->memory);

        // null our pointer to ensure no one uses it accidentally.
        arena_allocator->memory = NULL;
    }
}

// Clearing an arena allocator frees all allocations at once, reseting the stack allocations to the
// front (the count of used bytes = 0).
void arena_allocator_clear(ArenaAllocator *arena_allocator) {
    assert(NULL != arena_allocator);

    arena_allocator->count = 0;
}

void *arena_allocator_alloc(Allocator *allocator, size_t size) {
    ArenaAllocator *arena_allocator = (ArenaAllocator*)container_of(allocator, ArenaAllocator, allocator);

    uint32_t new_count = arena_allocator->count + size;

    // if there is not enough memory, create a larger stack
    if (new_count < arena_allocator->length) {
        uint32_t new_length = arena_allocator->length * 2;

        // if we asked for more then twice the amount, just allocate enough to allow the allocation to occur.
        if (new_length < new_count) {
            new_length = new_count;
        }

        arena_allocator->memory =
            arena_allocator->backing_allocator->alloc(arena_allocator->backing_allocator, new_count);
        // NOTE should check for a NULL result.

        arena_allocator->length = new_length;
    }

    char *ptr = &arena_allocator->memory[arena_allocator->count];
    arena_allocator->count = new_count;

    return ptr;
}

void arena_allocator_free(Allocator *allocator, void *ptr) {
    // we don't free anything- we free all at once or not at all.
    (void)allocator;
    (void)ptr;
}

void *arena_allocator_realloc(Allocator *allocator, void *old_ptr, size_t size) {
    ArenaAllocator *arena_allocator = (ArenaAllocator*)container_of(allocator, ArenaAllocator, allocator);

    // just allocate at the end, like a normal allocation.
    uint32_t new_count = arena_allocator->count + size;

    // if there is not enough memory, create a larger stack
    if (new_count < arena_allocator->length) {
        uint32_t new_length = arena_allocator->length * 2;

        // if we asked for more then twice the amount, just allocate enough to allow the allocation to occur.
        if (new_length < new_count) {
            new_length = new_count;
        }

        arena_allocator->memory =
            arena_allocator->backing_allocator->realloc(arena_allocator->backing_allocator, arena_allocator->memory, new_count);
        // NOTE should check for a NULL result.

        arena_allocator->length = new_length;
    }

    char *ptr = &arena_allocator->memory[arena_allocator->count];
    arena_allocator->count = new_count;

    return ptr;
}


/* Bump Allocator */
BumpAllocator bump_allocator_create(size_t capacity) {
    Allocator allocator = (Allocator){ bump_allocator_alloc, bump_allocator_free, bump_allocator_realloc };
    char *memory = malloc(capacity);
    return (BumpAllocator){ allocator, memory, 0, capacity };
}

void bump_allocator_destroy(BumpAllocator *bump_allocator) {
    if (NULL != bump_allocator->memory) {
        free(bump_allocator->memory);
        bump_allocator->memory = NULL;
    }
}

void *bump_allocator_alloc(Allocator *allocator, size_t size) {
    BumpAllocator *bump_allocator = (BumpAllocator*)container_of(allocator, BumpAllocator, allocator);

    char *ptr = NULL;
    // there is an overflow issue here...
    if ((bump_allocator->count + size) < bump_allocator->length) {
        ptr = &bump_allocator->memory[bump_allocator->count];
        bump_allocator->count += size;
    }

    return ptr;
}

void bump_allocator_free(Allocator *allocator, void *ptr) {
    // the bump allocator doesn't free anything
    (void)allocator;
    (void)ptr;
}

void *bump_allocator_realloc(Allocator *allocator, void *old_ptr, size_t size) {
    BumpAllocator *bump_allocator = (BumpAllocator*)container_of(allocator, BumpAllocator, allocator);

    // just allocate new memory, there is no need to clena up the old pointer.
    return bump_allocator->allocator.alloc(&bump_allocator->allocator, size);
}

