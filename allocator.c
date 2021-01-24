/*
 * This is a simple implementation of a custom API for memory management.
 * This API uses memory blocks with pre-allocated space to provide memory for external
 * memory allocation requests. The same memory block can be used for allocating
 * several objects. The memory block which currently provides the requested
 * memory is called the current memory block. If the current memory block runs
 * out of all available space, a new  memory block is created to become the
 * current one. Technically, all created memory blocks form a linked list.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <inttypes.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

// forward declarations

typedef uint8_t byte_t;

struct block_s;
struct region_s;
union alignment_u;
union header_u;

static struct block_s *block_new(const size_t);
extern struct region_s *region_new(void);
extern bool region_dispose(struct region_s **);
extern byte_t *region_request(struct region_s *, size_t);

// definitions

#if !defined(NULL)
#define NULL ((void *) 0)
#endif

// error symbols
#define BLOCK_ERROR (-1)
#define REGION_ERROR (-2)

// symbols representing events
#define BLOCK_CREATED (-1)
#define BLOCK_DISPOSED (-2)

#if !defined(BLOCK_MIN_SIZE)
// size of the preallocated space in a memory block is to never be below this value
#define BLOCK_MIN_SIZE 1024 // the optimal value can be, for example, 4 * 1024, or the page size on your computer
#endif

#if !defined(BLOCK_NEW_SIZE)
// by default the preallocated space in a memory block has this size
#define BLOCK_NEW_SIZE (SIZEOF_HEADER_U + BLOCK_MIN_SIZE)
#endif

#if !defined(MAX_BLOCK_COUNT)
// max number of memory blocks allowed to be created
#define MAX_BLOCK_COUNT 3
#endif

// when we provide memory for external memory allocation requests we want the
// size of the allocated memory to be aligned on this value
#if !defined(ALIGNMENT)
#define ALIGNMENT 16
#endif

#if !defined(NULL_BYTE_PTR)
// assigned to a pointer of type byte_t to show that it is a (NULL) pointer
#define NULL_BYTE_PTR ((byte_t *) NULL)
#endif

#if !defined(NULL_BLOCK_PTR)
// assigned to a pointer of type block_s to show that it is a (NULL) pointer
#define NULL_BLOCK_PTR ((struct block_s *) NULL)
#endif

#if !defined(NULL_REGION_PTR)
// assigned to a pointer of type region_s to show that it is a (NULL) pointer
#define NULL_REGION_PTR ((struct region_s *) NULL)
#endif

#if !defined(IS_NULL_PTR)
// checks if a (void *) pointer is a (NULL) pointer
#define IS_NULL_PTR(P) ((void *)(P) == NULL)
#endif

#if !defined(IS_NULL_BYTE_PTR)
// checks if a (byte_t *) pointer is a (NULL) pointer
#define IS_NULL_BYTE_PTR(P) ((byte_t *)(P) == NULL_BYTE_PTR)
#endif

#if !defined(IS_NULL_BLOCK_PTR)
// checks if a (block_s *) pointer is a (NULL) pointer
#define IS_NULL_BLOCK_PTR(P) ((struct block_s *)(P) == NULL_BLOCK_PTR)
#endif

#if !defined(IS_NULL_REGION_PTR)
// checks if a (region_s *) pointer is a (NULL) pointer
#define IS_NULL_REGION_PTR(P) ((struct region_s *)(P) == NULL_REGION_PTR)
#endif

#if !defined(SIZEOF_BLOCK_S)
// returns the size of memory occupied by type block_s
#define SIZEOF_BLOCK_S (sizeof(struct block_s))
#endif

#if !defined(SIZEOF_REGION_S)
// returns the size of memory occupied by type region_s
#define SIZEOF_REGION_S (sizeof(struct region_s))
#endif

#if !defined(SIZEOF_ALIGNMENT_U)
// returns the size of memory occupied by type alignment_u
#define SIZEOF_ALIGNMENT_U (sizeof(union alignment_u))
#endif

#if !defined(SIZEOF_HEADER_U)
// returns the size of memory occupied by type header_u
#define SIZEOF_HEADER_U (sizeof(union header_u))
#endif

#if !defined(get_avail_space)
// returns the size of the space available for allocation in the given block
#define get_avail_space(block) ((block)->space.size - (block)->space.offset)
#endif

#if !defined(align_up)
// rounds up the given size to the next value aligned on ALIGNMENT
// (see #define ALIGNMENT))
#define align_up(nbytes) (((nbytes) + SIZEOF_ALIGNMENT_U - 1) / (SIZEOF_ALIGNMENT_U)) * (SIZEOF_ALIGNMENT_U)
#endif

#if !defined(memnew)
// allocates the given number of bytes for a pointer of the given type
#define memnew(type, nbytes) ((type *) malloc((nbytes)))
#endif

#if !defined(NO_DEBUG_TRACE)

// the file where all tracing messages go
#define trace_stream stderr

// macro for tracing an error symbol
#define trace_error_symbol(errsym)      \
do {                                    \
    (void) fprintf(trace_stream,        \
            "[TRACE] (%s:%d): %s\n",    \
            __FILE__,                   \
            __LINE__,                   \
            #errsym);                   \
    (void) fflush(trace_stream);        \
} while (0)

// macro for tracing a symbol representing an event
#define trace_event(event)              \
do {                                    \
    (void) fprintf(trace_stream,        \
            "[TRACE] (%s:%d): %s\n",    \
            __FILE__,                   \
            __LINE__,                   \
            #event);                    \
    (void) fflush(trace_stream);        \
} while (0)

#else

#define trace_error_symbol(errsym) do {} while (0)
#define trace_event(event) do {} while (0)

#endif

// a memory block

struct block_s {
    // link to the next block
    struct block_s *next;

    // space used for providing memory for external allocation requests

    struct {
        byte_t *start; // where the space begins
        size_t size; // size of the space
        size_t offset; // position from where memory for the next external allocation
        // request will be taken
    } space;
};

// A storage for all created memory blocks. Actually, external memory allocation
// requests can access the allocated memory only thru this storage.

struct region_s {
    struct block_s *root; // the very first memory block
    struct block_s *current; // the current memory block
    size_t count; // the total number of all created memory blocks
};

// The purpose of this union is merely to provide padding space to enable
// the specified alignment. Nowhere in the code we access the union's fields
// directly.

union alignment_u {
    byte_t padding[ALIGNMENT];
};

// The purpose of this union is merely to provide space for a memory block's header.
// The header is a reserved space which is never used for allocating an object.
// Nowhere in the code we access the union's fields directly.

union header_u {
    struct block_s block;
    union alignment_u alignment;
};

// serves as a test function which emulates external memory allocation requests

int main(int argc, char **argv) {
    struct point;

    struct point {
        int32_t x, y;
    };
    struct region_s *region = region_new();
    struct point *p;
    int32_t i;
    for (i = 0; i < 20; i++) {
        p = (struct point *) region_request(region, sizeof (struct point));
        {
            p->x = i, p->y = i + 1;
        }
        printf("%d %d\n", p->x, p->y);
    }
    printf("blocks count = %d\n", (int32_t) region->count);
    (void) region_dispose((struct region_s **) &region);
    return 0;
}

// Constructor for an instance of block_s. The parameter is the size
// of the preallocated space for providing memory to external memory
// allocation requests.

static struct block_s *block_new(const size_t size) {
    if (!size) {
        trace_error_symbol(BLOCK_ERROR);
        return NULL_BLOCK_PTR;
    }
    struct block_s *p = memnew(struct block_s, SIZEOF_BLOCK_S + size);
    if (IS_NULL_BLOCK_PTR(p)) {
        trace_error_symbol(BLOCK_ERROR);
        return NULL_BLOCK_PTR;
    }
    p->next = NULL_BLOCK_PTR;
    p->space.start = (byte_t *) ((struct block_s *) p + 1);
    if (IS_NULL_BYTE_PTR(p->space.start)) {
        trace_error_symbol(BLOCK_ERROR);
        return NULL_BLOCK_PTR;
    }
    p->space.size = size;
    p->space.offset = 0;
    trace_event(BLOCK_CREATED);
    return p;
}

// constructor for an instance of region_s

extern struct region_s *region_new(void) {
    struct region_s *p = memnew(struct region_s, SIZEOF_REGION_S);
    if (IS_NULL_REGION_PTR(p)) {
        trace_error_symbol(REGION_ERROR);
        return NULL_REGION_PTR;
    }
    p->current = block_new(BLOCK_NEW_SIZE);
    if (IS_NULL_BLOCK_PTR(p->current)) {
        free(p);
        trace_error_symbol(REGION_ERROR);
        return NULL_REGION_PTR;
    }
    p->count = 1;
    p->root = p->current;
    return p;
}

// Frees all the memory allocated for the given instance of region_s. Returns
// true on success, false-otherwise.

extern bool region_dispose(struct region_s **pp) {
    struct region_s *p = *pp;
    if (IS_NULL_REGION_PTR(p) || IS_NULL_BLOCK_PTR(p->current)) {
        trace_error_symbol(REGION_ERROR);
        return false;
    }
    struct block_s *next, *cursor;
    cursor = p->root;
    while (!IS_NULL_BLOCK_PTR(cursor)) {
        next = cursor->next;
        //free(cursor->space.start);
        free(cursor);
        trace_event(BLOCK_DISPOSED);
        cursor = next;
        p->count--;
    }
    free(cursor);
    p->current = NULL_BLOCK_PTR;
    p->count = 0;
    free(p);
    p = NULL_REGION_PTR;
    return true;
}

// Interface for an external memory allocation request. The first parameter
// is an instance of region_s to which the request is made. The second parameter
// is the requested memory size. On success, returns the requested memory as an
// array of raw bytes. On failure, returns NULL_BYTE_PTR.

extern byte_t *region_request(struct region_s *region, size_t nbytes) {
    if (IS_NULL_REGION_PTR(region) || !nbytes || (nbytes > BLOCK_MIN_SIZE)) {
        trace_error_symbol(REGION_ERROR);
        return NULL_BYTE_PTR;
    }

    struct block_s *current = region->current;
    if (IS_NULL_BLOCK_PTR(current)) {
        trace_error_symbol(REGION_ERROR);
        return NULL_BYTE_PTR;
    }
    nbytes = align_up(nbytes);
    if (get_avail_space(current) > nbytes) {
        current->space.offset += nbytes;
        return current->space.start + (current->space.offset - nbytes);
    }
    current->next = block_new(BLOCK_NEW_SIZE); // block_new(SIZEOF_HEADER_U + nbytes + 10 *1024) would be better
    struct block_s *next = current->next;
    next->space.offset += nbytes;
    region->current = next;
    region->count++;
    return region->current->space.start;
}