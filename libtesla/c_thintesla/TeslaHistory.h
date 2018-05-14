#pragma once
#include "ThinTesla.h"
#include "TeslaVector.h"
#include "TeslaHash.h"


typedef struct ObservationHeader{
    uint32_t numEvent;
} ObservationHeader;

typedef struct Observation{
    ObservationHeader header;
    HashSize hash;
} Observation;

typedef struct TeslaHistory{
    TeslaVector* data;
    bool valid;
} TeslaHistory;

bool TeslaHistory_Create(TeslaHistory* history);
void TeslaHistory_Destroy(TeslaHistory* history);
void TeslaHistory_Clear(TeslaHistory* history);
bool TeslaHistory_Add(TeslaHistory* history, size_t numEvent, size_t dataSize, void* data);
Observation* TeslaHistory_GetObservations(TeslaHistory* history, size_t* numObservations);