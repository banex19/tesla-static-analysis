#include "TeslaAllocator.h"

#include <iostream>
#include <vector>

void TestPassed(const std::string& name)
{
    std::cout << "Test [" << name << "] passed\n";
}

const size_t ELEM_SIZE = 150;

void TestAllocFree()
{
    TeslaAllocator allocator;
    if (!TeslaAllocator_Create(150, 25, &allocator))
        assert(false && "Could not allocate memory");

    uint8_t* firstElem = (uint8_t*)TeslaAllocator_Allocate(&allocator);
    uint8_t* secondElem = (uint8_t*)TeslaAllocator_Allocate(&allocator);
    uint8_t* thirdElem = (uint8_t*)TeslaAllocator_Allocate(&allocator);

    //  printf("first: %p\nsecond:%p\nthird:%p\n", firstElem, secondElem, thirdElem);

    assert(secondElem - firstElem == ELEM_SIZE);
    assert(thirdElem - secondElem == ELEM_SIZE);

    TeslaAllocator_Free(&allocator, secondElem);
    uint8_t* secondAgain = (uint8_t*)TeslaAllocator_Allocate(&allocator);
    assert(secondElem == secondAgain);

    TeslaAllocator_Free(&allocator, firstElem);
    TeslaAllocator_Free(&allocator, thirdElem);

    uint8_t* firstAgain = (uint8_t*)TeslaAllocator_Allocate(&allocator);
    assert(firstElem == firstAgain);

    TeslaAllocator_Destroy(&allocator);
}

void TestMany()
{
    TeslaAllocator allocator;
    if (!TeslaAllocator_Create(150, 32, &allocator))
    {
        assert(false && "Could not allocate memory");
    }

    std::vector<uint8_t*> elems;

    for (size_t i = 0; i < 20; ++i)
    {
        elems.push_back((uint8_t*)TeslaAllocator_Allocate(&allocator));
    }

    for (size_t i = 1; i < 16; ++i)
    {
        assert(elems[i] - elems[i - 1] == ELEM_SIZE);
    }

    assert(elems[16] - elems[15] == ELEM_SIZE + 8); // Header.

    for (size_t i = 18; i < 20; ++i)
    {
        assert(elems[i] - elems[i - 1] == ELEM_SIZE);
    }

    TeslaAllocator_Free(&allocator, elems[0]);
    elems.push_back((uint8_t*)TeslaAllocator_Allocate(&allocator));
    assert(elems[20] == elems[0]);

    elems.push_back((uint8_t*)TeslaAllocator_Allocate(&allocator));
    assert(elems[21] - elems[19] == ELEM_SIZE);

    TeslaAllocator_Destroy(&allocator);
}

int main()
{
    TestAllocFree();

    TestMany();

    TestPassed("Allocator");
    return 0;
}