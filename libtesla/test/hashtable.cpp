#include "TeslaHashTable.h"

#include <iostream>
#include <vector>

void TestPassed(const std::string& name)
{
    std::cout << "Test [" << name << "] passed\n";
}

const size_t DATA_SIZE = 150;

void Test()
{
    TeslaHashTable table{119, DATA_SIZE};

    uint8_t* data = new uint8_t[DATA_SIZE];
    for (size_t i = 0; i < DATA_SIZE; ++i)
    {
        data[i] = i;
    }

    table.Insert(123456789, data);

    assert(table.LookupTimestamp(data) == 123456789);

    data[0] = 2;

    assert(table.LookupTimestamp(data) == 0);

    table.Insert(987654321, data);
    assert(table.LookupTimestamp(data) == 987654321);

    data[0] = 0;

    assert(table.LookupTimestamp(data) == 123456789);

    data[0] = 100;

    assert(table.LookupTimestamp(data) == 0);

    delete[] data;
}

void TestResize()
{
    TeslaHashTable table{1, DATA_SIZE};

    uint8_t* data = new uint8_t[DATA_SIZE];
    for (size_t i = 0; i < DATA_SIZE; ++i)
    {
        data[i] = i;
    }

    table.Insert(123456789, data);

    assert(table.LookupTimestamp(data) == 123456789);

    data[0] = 2;

    assert(table.LookupTimestamp(data) == 0);

    table.Insert(987654321, data);

    assert(table.LookupTimestamp(data) == 987654321);

    data[0] = 0;

    assert(table.LookupTimestamp(data) == 123456789);

    data[0] = 100;

    assert(table.LookupTimestamp(data) == 0);

    table.Insert(555, data);

    assert(table.LookupTimestamp(data) == 555);

    data[0] = 0;

    assert(table.LookupTimestamp(data) == 123456789);

    delete[] data;
}

int main()
{
    Test();
    TestResize();

    TestPassed("HashTable");
    return 0;
}