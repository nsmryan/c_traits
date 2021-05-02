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
// It also does not provide calloc for any allocator.

typedef struct Allocator Allocator;

typedef void* (*AllocatorAlloc)(Allocator *allocator, size_t size);
typedef void (*AllocatorFree)(Allocator *allocator, void *ptr);
typedef void* (*AllocatorRealloc)(Allocator *allocator, void *old_ptr, size_t new_size);

// The allocator has functions for allocation, free, and reallocation. Calloc is not
// included for simplicity.
// Some allocator interfaces would also require a size to be provide to free, which can
// be helpful to the implementation.
typedef struct Allocator {
    AllocatorAlloc alloc;
    AllocatorFree free;
    AllocatorRealloc realloc;
} Allocator;

// The heap allocator just wraps the system allocator in the Allocator trait.
typedef struct HeapAllocator {
    Allocator allocator;
} HeapAllocator;

// The Arena allocator wraps another allocator, providing a simple stack of allocations
// that grow a memory area as more memory is allocated. There are much better and most
// sophisticated arena allocation strategies out there.
// The point of including this allocator was to show how to compose two objects, each of
// which has different implementations of the trait, and one of which provides more
// functionality on top of the other. There are many possible uses of this, such as
// logging allocations or checking for misused allocations (use-after-free, double-free,
// etc).
typedef struct ArenaAllocator {
    Allocator allocator;
    Allocator *backing_allocator;
    uint8_t *memory;
    size_t count;
    size_t length;
} ArenaAllocator;

// The Bump allocator is a trivial allocator which just allows allocations within
// an existing block of memory. The users managers the memory- allocating it statically
// or dynamically. The Bump allocator just gives back pointers within the given block,
// allowing fast allocations that can be freed as a group. As with the arena, better
// implementations are possible.
// The point of including this allocator was to show how memory might be used in a
// fast and simple fashion, such as by allocating a block that is re-used (freed) every
// frame of a game, or where no dynamic allocation should be used and the statically
// allocated block is all the memory you plan to use (and no memory will be freed).
typedef struct BumpAllocator {
    Allocator allocator;
    uint8_t *memory;
    size_t count;
    size_t length;
} BumpAllocator;

// HeapAllocator functions
HeapAllocator heap_allocator_create(void);
void *heap_allocator_alloc(Allocator *allocator, size_t size);
void heap_allocator_free(Allocator *allocator, void *ptr);
void *heap_allocator_realloc(Allocator *allocator, void *old_ptr, size_t new_size);

// ArenaAllocator functions
ArenaAllocator arena_allocator_create(Allocator *backing_allocator);
void arena_allocator_destroy(ArenaAllocator *arena_allocator);
void arena_allocator_clear(ArenaAllocator *arena_allocator);

void *arena_allocator_alloc(Allocator *allocator, size_t size);
void arena_allocator_free(Allocator *allocator, void *ptr);
void *arena_allocator_realloc(Allocator *allocator, void *old_ptr, size_t size);

// BumpAllocator functions
BumpAllocator bump_allocator_create(size_t capacity, uint8_t *memory);
void bump_allocator_destroy(BumpAllocator *bump_allocator);
void *bump_allocator_alloc(Allocator *allocator, size_t size);
void bump_allocator_free(Allocator *allocator, void *ptr);
void *bump_allocator_realloc(Allocator *allocator, void *old_ptr, size_t size);
void *bump_allocator_free_all(Allocator *allocator);


int main(int argc, char *argv[]) {
    printf("\nHeap allocator test:\n");
    {
        // The heap allocator simply uses the system allocator by calling
        // malloc, realloc, and free.
        HeapAllocator heap_allocator = heap_allocator_create();

        // Check that we can allocate memory.
        char *memory = heap_allocator.allocator.alloc(&heap_allocator.allocator, 100);
        assert(NULL != memory);

        // Check that we can reallocate with a larger size.
        memory = heap_allocator.allocator.realloc(&heap_allocator.allocator, memory, 200);
        assert(NULL != memory);

        // Try freeing memory we allocated.
        heap_allocator.allocator.free(&heap_allocator.allocator, memory);
        printf("Heap allocator test complete\n");
    }

    printf("\nArena allocator test\n");
    {
        // Use the heap allocator (aka the system allocator) for our backing allocator.
        // This allows us to make better use of memory that the system gives us- allocating
        // in larger blocks, and using this memory in a restricted but fast manner.
        HeapAllocator heap_allocator = heap_allocator_create();
        ArenaAllocator arena_allocator = arena_allocator_create(&heap_allocator.allocator);

        // there is initially no allocated memory.
        assert(NULL == arena_allocator.memory);

        char *memory = arena_allocator.allocator.alloc(&arena_allocator.allocator, 100);
        assert(NULL != memory);
        assert(NULL != arena_allocator.memory);

        // ask for more, requiring a new allocation.
        uint8_t *old_memory = arena_allocator.memory;
        memory = arena_allocator.allocator.realloc(&arena_allocator.allocator, memory, 10000);
        assert(NULL != memory);

        // Try a free and show that it does nothing
        ArenaAllocator arena_copy = arena_allocator;
        arena_allocator.allocator.free(&arena_allocator.allocator, memory);
        assert(memcmp(&arena_allocator, &arena_copy, sizeof(arena_allocator)) == 0);

        // allocate a bunch of memory to show that the size grows.
        size_t old_length = arena_allocator.length;
        for (int index = 0; index < 100; index++) {
            memory = arena_allocator.allocator.alloc(&arena_allocator.allocator, 100);
            assert(NULL != memory);
        }
        assert(old_length < arena_allocator.length);

        arena_allocator_destroy(&arena_allocator);
        assert(NULL == arena_allocator.memory);
        printf("Arena allocator test complete\n");
    }

    printf("\nBump allocator test\n");
    {
        // allocate some memory to use for the bump allocator.
        const int LENGTH = 1024;
        char *base_memory = malloc(LENGTH);
        assert(NULL != base_memory);

        // create our bump allocator with reallocated memory
        BumpAllocator bump_allocator = bump_allocator_create(LENGTH, base_memory);
        assert(LENGTH == bump_allocator.length);

        char *memory = bump_allocator.allocator.alloc(&bump_allocator.allocator, 100);
        assert(NULL != memory);
        assert(100 == bump_allocator.count);

        memory = bump_allocator.allocator.alloc(&bump_allocator.allocator, 200);
        assert(NULL != memory);
        assert(300 == bump_allocator.count);

        // Try to allocate too much memory...
        char *new_memory = bump_allocator.allocator.alloc(&bump_allocator.allocator, LENGTH);
        assert(new_memory != memory);
        assert(300 == bump_allocator.count);

        // Realloc into a new space.
        memory = bump_allocator.allocator.realloc(&bump_allocator.allocator, memory, 200);
        assert(NULL != memory);
        assert(500 == bump_allocator.count);

        // Create a copy and show that freeing does not modify the bump allocator.
        // Ideally this allocator would free the top of the stack to provide a little
        // more flexibility.
        BumpAllocator bump_copy = bump_allocator;
        bump_allocator.allocator.free(&bump_allocator.allocator, memory);
        assert(memcmp(&bump_allocator, &bump_copy, sizeof(bump_allocator)) == 0);

        // Free the whole block at once, showing that the count is 0 and the length doesn't change.
        size_t old_length = bump_allocator.length;
        bump_allocator_free_all(&bump_allocator.allocator);
        assert(bump_allocator.count == 0);
        assert(bump_allocator.length == old_length);

        // Show that allocation still works after the free, and starts at the front again.
        memory = bump_allocator.allocator.alloc(&bump_allocator.allocator, 200);
        assert(NULL != memory);
        assert(200 == bump_allocator.count);
        assert((uint8_t*)memory == bump_allocator.memory);

        bump_allocator_destroy(&bump_allocator);
        assert(NULL == bump_allocator.memory);

        free(base_memory);

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

    size_t new_count = arena_allocator->count + size;

    // if there is not enough memory, create a larger stack
    if (new_count > arena_allocator->length) {
        size_t new_length = arena_allocator->length * 2;

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

// As with the bump allocator, we could check if the ptr is to the last allocation
// and free if so, allowing a stack allocation pattern. This is omitted for
// simplicity.
void arena_allocator_free(Allocator *allocator, void *ptr) {
    // we don't free anything- we free all at once or not at all.
    (void)allocator;
    (void)ptr;
}

void *arena_allocator_realloc(Allocator *allocator, void *old_ptr, size_t size) {
    ArenaAllocator *arena_allocator = (ArenaAllocator*)container_of(allocator, ArenaAllocator, allocator);

    // just allocate at the end, like a normal allocation.
    size_t new_count = arena_allocator->count + size;

    // if there is not enough memory, create a larger stack
    if (new_count > arena_allocator->length) {
        size_t new_length = arena_allocator->length * 2;

        // if we asked for more then twice the amount, just allocate enough to allow the allocation to occur.
        if (new_length < new_count) {
            new_length = new_count;
        }

        arena_allocator->memory =
            arena_allocator->backing_allocator->realloc(arena_allocator->backing_allocator, arena_allocator->memory, new_length);
        // NOTE should check for a NULL result.

        arena_allocator->length = new_length;
    }

    char *ptr = &arena_allocator->memory[arena_allocator->count];
    arena_allocator->count = new_count;

    return ptr;
}


/* Bump Allocator */
BumpAllocator bump_allocator_create(size_t capacity, uint8_t *memory) {
    Allocator allocator = (Allocator){ bump_allocator_alloc, bump_allocator_free, bump_allocator_realloc };
    return (BumpAllocator){ allocator, memory, 0, capacity };
}

void bump_allocator_destroy(BumpAllocator *bump_allocator) {
    if (NULL != bump_allocator->memory) {
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

// We should really check if the ptr is the last allocated block, and free if so. This makes
// the bump allocator a stack instead of being purely append-only. However, for simplicity
// I have omitted this detail.
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

// Free the whole allocation at once.
void *bump_allocator_free_all(Allocator *allocator) {
    BumpAllocator *bump_allocator = (BumpAllocator*)container_of(allocator, BumpAllocator, allocator);
    bump_allocator->count = 0;
}

