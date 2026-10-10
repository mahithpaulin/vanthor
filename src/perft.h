// VANTHOR perft.h — move-generation correctness counter.
#pragma once
#include "position.h"
#include <cstdint>
uint64_t perft(Position& pos, int depth);
