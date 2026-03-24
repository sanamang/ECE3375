/*
 * Soil Moisture Monitor
 *
 * Reads a moisture level (simulated by a potentiometer) via the ADC,
 * then drives 7-segment displays and LEDs on a DE-series FPGA board
 * to indicate whether the plant needs watering.
 *
 * States:
 *   0 – OK    : moisture is fine, display shows raw ADC value + "no" on HEX5-4
 *   1 – WARN  : soil is getting dry, display shows "drY", warning LED on
 *   2 – PUMP  : soil is very dry,   display shows "PEnP", both LEDs on
 */

#include <stdint.h>

/* ── Memory-mapped peripheral base addresses ────────────────────────────── */
#define LEDR_BASE       ((volatile uint32_t *) 0xFF200000)  /* Red LED register */
#define HEX3_HEX0_BASE  ((volatile uint32_t *) 0xFF200020)  /* 7-seg digits 0-3 */
#define HEX5_HEX4_BASE  ((volatile uint32_t *) 0xFF200030)  /* 7-seg digits 4-5 */

#define ADC_BASE        ((volatile uint32_t *) 0xFF204000)  /* ADC controller */

/* ── Moisture thresholds (12-bit ADC range: 0–4095) ─────────────────────── */
#define THRESHOLD_PUMP  1365    /* Below this → pump needed  (~33 % of full scale) */
#define THRESHOLD_WARN  1820    /* Below this → warning zone (~44 % of full scale) */

/* ── ADC channel wired to the potentiometer / moisture sensor ───────────── */
#define POT_CHANNEL     0

/* ── LED bit positions ───────────────────────────────────────────────────── */
#define LED_WARN        (1 << 0)    /* LED0: warn (dry) */
#define LED_PUMP        (1 << 1)    /* LED1: pump needed (very dry) */

/* ── 7-segment encoding for digits 0-9 (active-high segments a-g) ────────
 *
 *    Segment layout:
 *        _
 *       |_|   a=bit0 … g=bit6
 *       |_|
 */
static const uint8_t DIGIT[10] = {
    0x3F, /* 0 */  0x06, /* 1 */  0x5B, /* 2 */  0x4F, /* 3 */
    0x66, /* 4 */  0x6D, /* 5 */  0x7D, /* 6 */  0x07, /* 7 */
    0x7F, /* 8 */  0x6F  /* 9 */
};

/* ── 7-segment encodings for individual letters ─────────────────────────── */
#define SEG_n     0x37      /* lowercase 'n' */
#define SEG_o     0x3F      /* lowercase 'o' (same shape as 0) */
#define SEG_d     0x5E      /* lowercase 'd' */
#define SEG_r     0x50      /* lowercase 'r' */
#define SEG_Y     0x6E      /* uppercase 'Y' */
#define SEG_P     0x73      /* uppercase 'P' */
#define SEG_E     0x79      /* uppercase 'E' */
#define SEG_BLANK 0x00      /* all segments off */

/*
 * adc_read – trigger a conversion on the given channel and return the result.
 *
 * The ADC peripheral is memory-mapped:
 *   adc[0] (write) – channel select / start conversion
 *   adc[1] (read)  – conversion result (bits 11:0 = 12-bit value)
 *
 * A short busy-wait loop gives the ADC time to complete the conversion
 * before the result is read back.
 */
static uint32_t adc_read(uint8_t channel)
{
    volatile uint32_t *adc = ADC_BASE;

    adc[0] = (uint32_t)channel;     /* select channel and start conversion */

    /* Busy-wait ~50 cycles for the ADC to settle */
    volatile int i;
    for (i = 0; i < 50; i++);

    return adc[1] & 0xFFF;          /* mask to 12 bits (0–4095) */
}

/*
 * get_state – classify the raw moisture reading into one of three states.
 *
 * Returns:
 *   2 – very dry, pump needed  (moisture <= THRESHOLD_PUMP)
 *   1 – dry, warning           (moisture <= THRESHOLD_WARN)
 *   0 – OK                     (moisture >  THRESHOLD_WARN)
 */
static int get_state(uint32_t moisture)
{
    if (moisture <= THRESHOLD_PUMP) return 2;   /* very dry – pump! */
    if (moisture <= THRESHOLD_WARN) return 1;   /* getting dry – warn */
    return 0;                                   /* moisture is fine */
}

/*
 * display_update – write the appropriate message to the 7-segment displays.
 *
 * State 0 (OK):   HEX3-HEX0 show the 4-digit decimal moisture value,
 *                 HEX5-HEX4 show "no" (no action needed).
 *
 * State 1 (WARN): HEX3-HEX0 show " drY" (leading blank + d-r-Y).
 *
 * State 2 (PUMP): HEX3-HEX0 show "PEnP" (pump needed).
 *
 * Each HEX register holds four 8-bit segment patterns packed into 32 bits,
 * with the most-significant byte controlling the higher-numbered digit.
 */
static void display_update(uint32_t moisture, int state)
{
    uint32_t hex3_0, hex5_4;

    if (state == 0) {
        /* Clamp reading to valid 12-bit range just in case */
        if (moisture > 4095) moisture = 4095;

        /* Extract each decimal digit of the 4-digit moisture value */
        uint8_t d0 = (uint8_t)( moisture         % 10);   /* ones */
        uint8_t d1 = (uint8_t)((moisture /   10) % 10);   /* tens */
        uint8_t d2 = (uint8_t)((moisture /  100) % 10);   /* hundreds */
        uint8_t d3 = (uint8_t)((moisture / 1000) % 10);   /* thousands */

        /* Pack segment patterns into the 32-bit HEX3-HEX0 register */
        hex3_0 = ((uint32_t)DIGIT[d3] << 24)
               | ((uint32_t)DIGIT[d2] << 16)
               | ((uint32_t)DIGIT[d1] <<  8)
               | ((uint32_t)DIGIT[d0]      );

        /* HEX5-HEX4: display "no" (moisture is OK, no action needed) */
        hex5_4 = ((uint32_t)SEG_n << 8) | (uint32_t)SEG_o;

    } else if (state == 1) {
        /* Display " drY" – soil is dry, user should water soon */
        hex3_0 = ((uint32_t)SEG_BLANK << 24)
               | ((uint32_t)SEG_d     << 16)
               | ((uint32_t)SEG_r     <<  8)
               | ((uint32_t)SEG_Y          );
        hex5_4 = 0x00000000;   /* HEX5-HEX4 blank */

    } else {
        /* Display "PEnP" – pump is needed immediately */
        hex3_0 = ((uint32_t)SEG_P << 24)
               | ((uint32_t)SEG_E << 16)
               | ((uint32_t)SEG_n <<  8)
               | ((uint32_t)SEG_P      );
        hex5_4 = 0x00000000;   /* HEX5-HEX4 blank */
    }

    /* Write both display registers at once */
    *HEX3_HEX0_BASE = hex3_0;
    *HEX5_HEX4_BASE = hex5_4;
}

/*
 * led_update – turn on the warning and/or pump LEDs based on the state.
 *
 *   State 0: both LEDs off
 *   State 1: LED_WARN on           (soil dry)
 *   State 2: LED_WARN + LED_PUMP on (soil very dry – pump needed)
 */
static void led_update(int state)
{
    uint32_t leds = 0;
    if (state >= 1) leds |= LED_WARN;  /* warn LED on for state 1 and 2 */
    if (state >= 2) leds |= LED_PUMP;  /* pump LED on only for state 2   */
    *LEDR_BASE = leds;
}

/*
 * main – continuous polling loop.
 *
 * Every iteration:
 *   1. Read the ADC (potentiometer acting as moisture sensor).
 *   2. Classify the reading into a state (OK / WARN / PUMP).
 *   3. Update the 7-segment displays.
 *   4. Update the LEDs.
 *
 * The loop never exits (bare-metal embedded system with no OS).
 */
int main(void)
{
    while (1) {
        uint32_t moisture = adc_read(POT_CHANNEL);  /* read moisture level */
        int      state    = get_state(moisture);    /* classify the reading */
        display_update(moisture, state);            /* update 7-seg display */
        led_update(state);                          /* update LEDs          */
    }

    return 0;
}
