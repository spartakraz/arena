This is a simple C implementation of an arena, a region-based memory management technique.
It uses memory blocks with pre-allocated space to provide memory for external
memory allocation requests. The same memory block can be used for allocating
several objects. The memory block which currently provides the requested
memory is called the current memory block. If the current memory block runs
out of all available space, a new  memory block is created to become the
current one. Technically, all created memory blocks form a linked list.