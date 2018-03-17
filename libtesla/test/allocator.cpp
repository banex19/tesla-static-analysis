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
    TeslaAllocator allocator{150, 25};

    uint8_t* firstElem = (uint8_t*)allocator.Allocate();
    uint8_t* secondElem = (uint8_t*)allocator.Allocate();
    uint8_t* thirdElem = (uint8_t*)allocator.Allocate();

    //  printf("first: %p\nsecond:%p\nthird:%p\n", firstElem, secondElem, thirdElem);

    assert(secondElem - firstElem == ELEM_SIZE);
    assert(thirdElem - secondElem == ELEM_SIZE);

    allocator.Free(secondElem);
    uint8_t* secondAgain = (uint8_t*)allocator.Allocate();
    assert(secondElem == secondAgain);

    allocator.Free(firstElem);
    allocator.Free(thirdElem);

    uint8_t* firstAgain = (uint8_t*)allocator.Allocate();
    assert(firstElem == firstAgain);
}

void TestMany()
{
    TeslaAllocator allocator{150, 32};

    std::vector<uint8_t*> elems;

    for (size_t i = 0; i < 20; ++i)
    {
        elems.push_back((uint8_t*)allocator.Allocate());
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

    allocator.Free(elems[0]);
    elems.push_back((uint8_t*)allocator.Allocate());
    assert(elems[20] == elems[0]);

    elems.push_back((uint8_t*)allocator.Allocate());
    assert(elems[21] - elems[19] == ELEM_SIZE);
}

int main()
{
    TestAllocFree();

    TestMany();

    TestPassed("Allocator");
    return 0;
}