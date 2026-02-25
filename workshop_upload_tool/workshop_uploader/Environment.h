#pragma once

#include <atomic> //g_bShutdownRequested
#include <cstdint> //uint64

using uint64 = std::uint64_t;

#define STEAM_APPID_SLRR 497180
#define STEAM_APPID_SL1 1571280

extern std::atomic<bool> g_bUserInterrupted;