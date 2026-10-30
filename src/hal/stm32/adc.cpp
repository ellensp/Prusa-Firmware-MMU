/// @file adc.cpp
#include "../adc.h"
#include "../gpio.h"
#include "../../pins.h"
#include "cmsis.h"

namespace hal {
namespace adc {

void Init() {
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
    (void)RCC->APB2ENR;

    // the ADC runs from its own 14 MHz oscillator
    RCC->CR2 |= RCC_CR2_HSI14ON;
    while (!(RCC->CR2 & RCC_CR2_HSI14RDY))
        ;

    gpio::Configure(BUTTONS_PIN, 3, 0); // analog mode

    ADC1->CFGR2 = 0; // asynchronous clock
    ADC1->CR = ADC_CR_ADCAL;
    while (ADC1->CR & ADC_CR_ADCAL)
        ;

    // 239.5 cycles sampling time - the internal reference needs at least 4 us
    ADC1->SMPR = 7U << ADC_SMPR_SMP_Pos;
    ADC1->CFGR1 = 0; // 12 bit, right aligned, single conversion
    ADC->CCR |= ADC_CCR_VREFEN;

    // ADEN may get ignored right after the calibration (errata), keep setting it until the ADC is ready
    ADC1->ISR = ADC_ISR_ADRDY;
    do {
        ADC1->CR = ADC_CR_ADEN;
    } while (!(ADC1->ISR & ADC_ISR_ADRDY));
}

uint16_t ReadADC(uint8_t channel) {
    ADC1->CHSELR = 1UL << channel;
    ADC1->CR |= ADC_CR_ADSTART;
    while (!(ADC1->ISR & ADC_ISR_EOC))
        ;
    return ADC1->DR;
}

} // namespace adc
} // namespace hal
