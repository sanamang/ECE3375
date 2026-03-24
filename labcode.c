#include <stdint.h>

#define LEDR_BASE       ((volatile uint32_t *) 0xFF200000)
#define HEX3_HEX0_BASE  ((volatile uint32_t *) 0xFF200020)
#define HEX5_HEX4_BASE  ((volatile uint32_t *) 0xFF200030)

#define ADC_BASE        ((volatile uint32_t *) 0xFF204000)

#define THRESHOLD_PUMP  1365    
#define THRESHOLD_WARN  1820    

#define POT_CHANNEL     0

#define LED_WARN        (1 << 0)    
#define LED_PUMP        (1 << 1)    

static const uint8_t DIGIT[10] = {
    0x3F, /* 0 */  0x06, /* 1 */  0x5B, /* 2 */  0x4F, /* 3 */
    0x66, /* 4 */  0x6D, /* 5 */  0x7D, /* 6 */  0x07, /* 7 */
    0x7F, /* 8 */  0x6F  /* 9 */
};

#define SEG_n     0x37      
#define SEG_o     0x3F      
#define SEG_d     0x5E     
#define SEG_r     0x50     
#define SEG_Y     0x6E     
#define SEG_P     0x73      
#define SEG_E     0x79      
#define SEG_BLANK 0x00      

static uint32_t adc_read(uint8_t channel)
{
    volatile uint32_t *adc = ADC_BASE;

    adc[0] = (uint32_t)channel;

    volatile int i;
    for (i = 0; i < 50; i++);

    return adc[1] & 0xFFF;
}

static int get_state(uint32_t moisture)
{
    if (moisture <= THRESHOLD_PUMP) return 2;   
    if (moisture <= THRESHOLD_WARN) return 1;   
    return 0;                                   
}

static void display_update(uint32_t moisture, int state)
{
    uint32_t hex3_0, hex5_4;

    if (state == 0) {
      
        if (moisture > 4095) moisture = 4095;

        uint8_t d0 = (uint8_t)( moisture         % 10);
        uint8_t d1 = (uint8_t)((moisture /   10) % 10);
        uint8_t d2 = (uint8_t)((moisture /  100) % 10);
        uint8_t d3 = (uint8_t)((moisture / 1000) % 10);

        hex3_0 = ((uint32_t)DIGIT[d3] << 24)
               | ((uint32_t)DIGIT[d2] << 16)
               | ((uint32_t)DIGIT[d1] <<  8)
               | ((uint32_t)DIGIT[d0]      );

        
        hex5_4 = ((uint32_t)SEG_n << 8) | (uint32_t)SEG_o;

    } else if (state == 1) {
        
        hex3_0 = ((uint32_t)SEG_BLANK << 24)
               | ((uint32_t)SEG_d     << 16)
               | ((uint32_t)SEG_r     <<  8)
               | ((uint32_t)SEG_Y          );
        hex5_4 = 0x00000000;

    } else {
        
        hex3_0 = ((uint32_t)SEG_P << 24)
               | ((uint32_t)SEG_E << 16)
               | ((uint32_t)SEG_n <<  8)
               | ((uint32_t)SEG_P      );
        hex5_4 = 0x00000000;
    }

    *HEX3_HEX0_BASE = hex3_0;
    *HEX5_HEX4_BASE = hex5_4;
}

static void led_update(int state)
{
    uint32_t leds = 0;
    if (state >= 1) leds |= LED_WARN;
    if (state >= 2) leds |= LED_PUMP;
    *LEDR_BASE = leds;
}

int main(void)
{
    while (1) {
        uint32_t moisture = adc_read(POT_CHANNEL);  
        int      state    = get_state(moisture);    
        display_update(moisture, state);            
        led_update(state);                          
    }

    return 0;
}
