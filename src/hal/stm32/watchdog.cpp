/// @file watchdog.cpp
#include "../watchdog.h"
#include "cmsis.h"

namespace hal {
namespace watchdog {

void Enable(const configuration &config) {
    IWDG->KR = 0xCCCC; // start - the LSI turns on by itself
    IWDG->KR = 0x5555; // unlock PR and RLR
    IWDG->PR = config.prescalerBits;
    IWDG->RLR = config.reload;
    while (IWDG->SR)
        ;
    IWDG->KR = 0xAAAA;
}

void Disable() {
    // the IWDG cannot be stopped once running
}

void Reset() {
    IWDG->KR = 0xAAAA;
}

} // namespace watchdog
} // namespace hal
