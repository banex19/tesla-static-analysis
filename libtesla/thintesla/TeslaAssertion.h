#pragma once

#include <limits>
#include <memory>
#include <vector>

#include "ThinTesla.h"

const size_t MAX_SUCCESSORS = 7;

typedef size_t event_id;

struct TeslaSite
{
	size_t siteId = std::numeric_limits<std::size_t>::max();
};

class TeslaEvent
{
  public:
	TeslaEvent(event_id id, TeslaSite siteId)
		: id(id), siteId(siteId)
	{
	}

	bool IsDeterministic()
	{
		return true;
	}

	void AddSuccessor(event_id successorId)
	{
		DEBUG_ASSERT(numSuccessors < MAX_SUCCESSORS);
		DEBUG_ASSERT(successorId > id);
		DEBUG_ASSERT(successorId - id <= std::numeric_limits<uint8_t>::max());

		successorIncrementalIds[numSuccessors] = successorId - id;
		numSuccessors++;
	}

  private:
	event_id id;
	TeslaSite siteId;

	uint8_t numSuccessors = 0;
	uint8_t successorIncrementalIds[MAX_SUCCESSORS] = {0};
};

enum TeslaAssertionFlags
{
	ASSERTION_DETERMINISTIC = 0,
	ASSERTION_NONDETERMINISTIC
};

class TeslaAssertion
{
  public:
  private:
	TeslaAssertion(std::vector<TeslaEvent> &events, size_t flags)
		: events(events), flags(flags)
	{
		events.shrink_to_fit();
	}

	std::vector<TeslaEvent> events;
	size_t flags;

	friend class TeslaAssertionBuilder;
};
