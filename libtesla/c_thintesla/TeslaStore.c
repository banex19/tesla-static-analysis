#include "TeslaStore.h"

bool TeslaStore_Create(StoreType type, size_t initialCapacity, size_t dataSize, TeslaStore* store)
{
    store->type = type;
    store->dataSize = dataSize;

    if (type == TESLA_STORE_HT)
    {
        return TeslaHT_Create(initialCapacity, dataSize, &store->store.hashtable);
    }
    else if (type == TESLA_STORE_SINGLE)
    {
        assert(initialCapacity == 1);

        uint8_t* data = TeslaMalloc(dataSize + sizeof(TeslaTemporalTag));
        if (data == NULL)
            return false;

        memset(data, 0, dataSize + sizeof(TeslaTemporalTag));

        store->store.data = data;
    }

    assert(false && "StoreType value not defined");
    return false;
}

void TeslaStore_Destroy(TeslaStore* store)
{
    if (store->type == TESLA_STORE_HT)
    {
        TeslaHT_Destroy(&store->store.hashtable);
    }
    else if (store->type == TESLA_STORE_SINGLE)
    {
        TeslaFree(store->store.data);
    }

    store->type = TESLA_STORE_INVALID;
}

void TeslaStore_Clear(TeslaStore* store)
{
    if (store->type == TESLA_STORE_HT)
    {
        TeslaHT_Clear(&store->store.hashtable);
    }
    else if (store->type == TESLA_STORE_SINGLE)
    {
        memset(store->store.data, 0, store->dataSize);
    }
}

bool TeslaStore_Insert(TeslaStore* store, TeslaTemporalTag tag, void* data)
{
    if (store->type == TESLA_STORE_HT)
    {
        return TeslaHT_Insert(&store->store.hashtable, tag, data);
    }
    else if (store->type == TESLA_STORE_SINGLE)
    {
        TeslaTemporalTag* storeTag = (TeslaTemporalTag*)(store->store.data + store->dataSize);
        if (*storeTag == 0)
        {
            memcpy(store->store.data, data, store->dataSize);
            *storeTag |= tag;
        }
        else
        {
            size_t cmp = memcmp(data, store->store.data, store->dataSize);
            if (cmp != 0)
            {
                assert(false && "Multiple values inserted");
            }
            *storeTag |= tag;
        }

        return true;
    }

    printf("Type: %d\n", store->type);

    assert(false);
    return false;
}

TeslaTemporalTag TeslaStore_Get(TeslaStore* store, void* data)
{
    if (store->type == TESLA_STORE_HT)
    {
        return TeslaHT_LookupTag(&store->store.hashtable, data);
    }
    else if (store->type == TESLA_STORE_SINGLE)
    {
        TeslaTemporalTag* storeTag = (TeslaTemporalTag*)(store->store.data + store->dataSize);
        return *storeTag;
    }

    assert(false);
    return 0;
}