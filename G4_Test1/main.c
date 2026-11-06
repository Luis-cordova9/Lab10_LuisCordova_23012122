#include "stm32g4xx.h"

// lab 10 - input capture y uart
// PA8  -> salida pwm de 5 kHz al 50%
// PA0  -> entrada del input capture (voltaje del capacitor)
// PA9  -> TX    PA10 -> RX

static volatile uint32_t periodo = 0;   // cuentas entre dos flancos de subida
static volatile uint32_t ancho   = 0;   // cuentas desde la subida hasta la bajada


void pwm_config(void)
{
    RCC->AHB2ENR |= (1u << 0);
    RCC->APB2ENR |= (1u << 11);

    GPIOA->MODER  &= ~(3u << 16);
    GPIOA->MODER  |=  (2u << 16);
    GPIOA->AFR[1] &= ~(0xFu << 0);
    GPIOA->AFR[1] |=  (6u << 0);

    TIM1->PSC   = 0;        // 16 MHz
    TIM1->ARR   = 3199;     // 16 MHz / 3200 = 5 kHz
    TIM1->CCR1  = 1600;     // la mitad de 3200 = 50%

    TIM1->CCMR1 = (6u << 4) | (1u << 3);
    TIM1->CCER |= (1u << 0);
    TIM1->BDTR |= (1u << 15);
    TIM1->CR1  |= (1u << 7);
    TIM1->EGR   = (1u << 0);
    TIM1->CR1  |= (1u << 0);
}


void ic_config(void)
{
    RCC->AHB2ENR |= (1u << 0);
    RCC->APB1ENR1 |= (1u << 0);

    GPIOA->MODER  &= ~(3u << 0);
    GPIOA->MODER  |=  (2u << 0);
    GPIOA->AFR[0] &= ~(0xFu << 0);
    GPIOA->AFR[0] |=  (1u << 0);

    TIM2->PSC = 0;              // 16 MHz, cada cuenta son 62.5 ns
    TIM2->ARR = 0xFFFFFFFF;

    TIM2->CCMR1 = (1u << 0) | (2u << 8);    // CC1 y CC2 escuchan el mismo pin
    TIM2->CCER  = (1u << 0) | (1u << 4) | (1u << 5);  // CC2 en flanco de bajada
    TIM2->SMCR  = (5u << 4) | (4u << 0);    // la subida reinicia el contador

    TIM2->DIER |= (1u << 1);
    NVIC_EnableIRQ(TIM2_IRQn);

    TIM2->EGR  = (1u << 0);
    TIM2->CR1 |= (1u << 0);
}


void uart_config(void)
{
    RCC->AHB2ENR  |= (1u << 0);
    RCC->APB2ENR  |= (1u << 14);

    GPIOA->MODER  &= ~(3u << 18);
    GPIOA->MODER  |=  (2u << 18);
    GPIOA->MODER  &= ~(3u << 20);
    GPIOA->MODER  |=  (2u << 20);
    GPIOA->AFR[1] &= ~(0xFu << 4);
    GPIOA->AFR[1] |=  (7u << 4);
    GPIOA->AFR[1] &= ~(0xFu << 8);
    GPIOA->AFR[1] |=  (7u << 8);

    USART1->BRR = 139;      // 16 MHz / 115200
    USART1->CR1 = (1u << 2) | (1u << 3) | (1u << 5);   // RX, TX y la interrupcion de RX
    USART1->CR1 |= (1u << 0);

    NVIC_EnableIRQ(USART1_IRQn);
}


void uart_envia(char c)
{
    while ((USART1->ISR & (1u << 7)) == 0) { }
    USART1->TDR = c;
}


void uart_texto(char *s)
{
    while (*s != 0) {
        uart_envia(*s);
        s++;
    }
}


void uart_numero(uint32_t n)
{
    char buf[11];
    int i = 0;

    if (n == 0) {
        uart_envia('0');
        return;
    }

    while (n > 0) {
        buf[i] = (char)('0' + (n % 10));
        n = n / 10;
        i++;
    }

    while (i > 0) {
        i--;
        uart_envia(buf[i]);
    }
}


void TIM2_IRQHandler(void)
{
    if (TIM2->SR & (1u << 1)) {
        ancho   = TIM2->CCR2;
        periodo = TIM2->CCR1;
    }
}


void USART1_IRQHandler(void)
{
    char c;

    if (USART1->ISR & (1u << 5)) {
        c = (char)USART1->RDR;

        if (periodo == 0) {
            uart_texto("No hay senal\r\n");
            return;
        }

        if (c == '0') {
            uart_texto("Periodo = ");
            uart_numero(periodo / 16);
            uart_texto(" us\r\n");
        }
        else if (c == '1') {
            uart_texto("Frecuencia = ");
            uart_numero(16000000u / periodo);
            uart_texto(" Hz\r\n");
        }
        else if (c == '2') {
            uart_texto("Ancho de pulso = ");
            uart_numero(ancho / 16);
            uart_texto(" us\r\n");
        }
        else if (c == '3') {
            uart_texto("Duty Cycle = ");
            uart_numero((ancho * 100u) / periodo);
            uart_texto("%\r\n");
        }
    }
}


int main(void)
{
    pwm_config();
    ic_config();
    uart_config();

    while (1) {
        __WFI();
    }
}
