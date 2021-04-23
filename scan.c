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


// Forward declare the scan type so we can define the function typedefs below.
typedef struct Scan Scan;

// The return function extracts a result from the scan. This can be different
// from the type appended into the scan.
// The return function can be called multiple times, even during a scan,
// to get incremental results.
typedef void (*ScanReturn)(Scan *scan, void *result);

// The append function appends a new element to the scan. This is usually
// a monoid, but we don't ensure that here in any way.
// In principal there could be a monoid struct with an append and identity
// function, which the scan function could use.
typedef void (*ScanAppend)(Scan *scan, void *value);

// The Scan type is the interface to a scan, with a way to append
// new values and to extract the current value.
typedef struct Scan {
    ScanReturn ret;
    ScanAppend append;
} Scan;

// The Sum type is a simple scan which is given numbers and adds them up,
// returning their sum.
typedef struct Sum {
    Scan scan;
    uint32_t sum;
} Sum;

// The StringBuilder is a scan that accumulates strings, and will concatenate
// the strings together when asked for a result. This can be more efficient
// then naively concatenating strings.
typedef struct StringBuilder {
    Scan scan;
    // strings is the array of strings provided to the builder.
    char **strings;
    // count is the current number of strings collected.
    uint32_t count;
    // length is the maximum number of strings- the length of the strings array
    uint32_t length;
} StringBuilder;

// Sum functions
Sum sum_create(void);
void sum_return(Scan *scan, void *result);
void sum_append(Scan *scan, void *value);

// StringBuilder functions
StringBuilder string_builder_create(uint32_t capacity);
void string_builder_destroy(StringBuilder *builder);
void string_builder_return(Scan *scan, void *result);
void string_builder_append(Scan *scan, void *value);


int main(int argc, char *argv[]) {
    printf("\nSum test:\n");
    {
        // Create a Sum and add the numbers from 0 to 10.
        Sum sum = sum_create();
        const uint32_t loop_cap = 10;
        for (uint32_t value = 0; value <= loop_cap; value++) {
            sum.scan.append(&sum.scan, &value);
        }

        // (N * (N + 1)) / 2 is the closed form solution
        // of the sum from 0 to N.
        uint32_t expected = ((loop_cap * (loop_cap + 1)) / 2);
        assert(sum.sum == expected);
        printf("sum of 0..10 = %d\n", sum.sum);

        uint32_t result = 0;
        sum.scan.ret(&sum.scan, &result);
        assert(result == expected);
        printf("sum result = %d\n", result);
    }

    // Create a StringBuilder and concatenate a few strings as a test.
    printf("\nStringBuilder test\n");
    {
        // create a string builder with a small capacity, so we have
        // to realloc (see string_builder_append).
        StringBuilder string_builder = string_builder_create(2);

        // the StringBuilder is keeping these pointers internally, so
        // be sure to keep them allocated if they are not string literals...
        string_builder.scan.append(&string_builder.scan, "building ");
        string_builder.scan.append(&string_builder.scan, "a ");
        string_builder.scan.append(&string_builder.scan, "string ");
        string_builder.scan.append(&string_builder.scan, "incrementally.");

        char result_string[1024];
        string_builder.scan.ret(&string_builder.scan, result_string);
        printf("result string is '%s'\n", result_string);

        // did the string builder create the exact string we wanted?
        assert(strcmp("building a string incrementally.", result_string) == 0);

        // we have to clean up since the string_builder contains a heap
        // allocated pointer.
        string_builder_destroy(&string_builder);
    }
}

// Create a Sum scan
Sum sum_create(void) {
    return (Sum){ { sum_return, sum_append }, 0 };
}

// Return the current sum.
void sum_return(Scan *scan, void *value) {
    Sum *sum = (Sum*)container_of(scan, Sum, scan);
    uint32_t *result = (uint32_t*)value;

    *result = sum->sum;
}

// Add a given value to the running sum.
void sum_append(Scan *scan, void *value) {
    Sum *sum = (Sum*)container_of(scan, Sum, scan);
    uint32_t *addend = (uint32_t*)value;

    sum->sum += *addend;
}

// Create a StringBuilder scan using the given initial capacity.
StringBuilder string_builder_create(uint32_t capacity) {
    char **strings = calloc(sizeof(char*), capacity);

    return (StringBuilder){
        { string_builder_return, string_builder_append },
        strings,
        0,
        capacity,
    };
}

// Clean up the string builders string array.
void string_builder_destroy(StringBuilder *builder) {
    if (NULL != builder->strings) {
        free(builder->strings);
        builder->strings = NULL;
    }
}

// Return the concatenated strings. This is an example of how the
// result of a scan may require some processing to produce.
void string_builder_return(Scan *scan, void *result) {
    StringBuilder *builder = (StringBuilder*)container_of(scan, StringBuilder, scan);

    // we just assume there is enough room in the provided string, for simplicity.
    // ideally the input type would come with a size_t parameter as well.
    char *final_string = (char*)result;

    uint32_t chr_offset = 0;
    for (uint32_t index = 0; index < builder->count; index++) {
        // such unsafe!
        uint32_t len = strlen(builder->strings[index]);
        memcpy(&final_string[chr_offset], builder->strings[index], len);
        chr_offset += len;
    }
    // ensure null-termination, as we copied the strings as memory blocks without
    // terminators.
    final_string[chr_offset] = '\0';
}

// Append a string to the string builder.
void string_builder_append(Scan *scan, void *value) {
    StringBuilder *builder = (StringBuilder*)container_of(scan, StringBuilder, scan);

    char *string = (char*)value;

    // if we have run out of capacity, just realloc with a larger array.
    // This could be done better, or with a stretchy buffer, but this is
    // simpler as an example.
    if (builder->count == builder->length) {
        uint32_t new_length = builder->length * 2;
        builder->strings = realloc(builder->strings, new_length);
        builder->length = new_length;
    }

    builder->strings[builder->count] = string;
    builder->count++;
}

