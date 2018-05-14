#include "TeslaHistory.h"
#include "TeslaMalloc.h"

#define OBSERVATION_SIZE (sizeof(Observation))

bool TeslaHistory_Create(TeslaHistory* history)
{
    memset(history, 0, sizeof(TeslaHistory));

    history->data = TeslaMalloc(sizeof(TeslaVector));
    if (history->data == NULL)
    {
        return false;
    }

    if (!TeslaVector_Create(OBSERVATION_SIZE, history->data))
    {
        TeslaFree(history->data);
        return false;
    }

    return true;
}
void TeslaHistory_Destroy(TeslaHistory* history)
{
    TeslaVector_Destroy(history->data);
}
void TeslaHistory_Clear(TeslaHistory* history)
{
    TeslaVector_Clear(history->data);
}

bool TeslaHistory_Add(TeslaHistory* history, size_t numEvent, size_t dataSize, void* data)
{
    Observation newElem;
    newElem.header.numEvent = numEvent;
    newElem.hash = data != NULL ? Hash64(data, dataSize) : 0;

    return TeslaVector_Add(history->data, &newElem);
}

Observation* TeslaHistory_GetObservations(TeslaHistory* history, size_t* numObservations)
{
    if (numObservations != NULL)
        *numObservations = history->data->size;

    return (Observation*)history->data->data;
}