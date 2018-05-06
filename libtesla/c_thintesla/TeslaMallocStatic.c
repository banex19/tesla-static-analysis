#include "TeslaMalloc.h"

_Static_assert(TESLA_STATIC_STORAGE_SIZE > 0, "Invalid static storage size");

char STATIC_STORAGE[TESLA_STATIC_STORAGE_SIZE];
char* currentPtr = STATIC_STORAGE;

bool ThereIsEnoughSpace(char* ptr, size_t size)
{
    return (ptr + size) <= (STATIC_STORAGE + TESLA_STATIC_STORAGE_SIZE);
}

void* TeslaMallocStatic(size_t size)
{
    char* ptr = currentPtr;

    while (ThereIsEnoughSpace(ptr, size))
    {
        char* newPtr = ptr + size;

        if (__sync_bool_compare_and_swap(&currentPtr, ptr, newPtr)) // Try to reserve a chunk of memory.
        {
            return ptr;
        }
        else // Somebody got it before us.
        {
            ptr = currentPtr;
        }
    }

    return NULL;
}