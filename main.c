/*******************************************************************************
*   MSP430G2553 USCI40 erratum demonstration
*
*   Performs a series of SPI transfers in loopback mode with an increasing delay
*   before the fourth byte is written to TXBUF. This shows how the SPI output
*   becomes offset by 1 bit if TXBUF is loaded slightly too late
*
*   Connect P1.5 to P2.0 with a jumper
*   Connect logic analyser to P1.4 (chip select), P1.6 (data) and P2.3 (clock)
*
*   DCO=16MHz, MCLK=DCO, SMCLK=DCO, LFXT1=N/A
*
*                    MSP430G2553
*                |                 |
*       Delay <--|P1.3             |
*                |                 |
* Chip Select <--|P1.4         P1.7|--> UCB0SIMO
*                |      UCLISTEN | |
*     UCB0CLK -->|P1.5         P1.6|<-- UCB0SOMI
* Ext. jumper |  |                 |
*   SPI clock <--|P2.0             |
*                |                 |
*                |                 |
*                |             P2.3|--> SPI clock
*                 -----------------
*
*   Built with CCS Version 11.0.0.00012
******************************************************************************/

#include <msp430.h>

#define COUNT_OF(array) (sizeof(array)/sizeof(array[0]))
#define DEBUG_BREAK() __op_code(0x4343)

typedef enum {
    spiMode_0 = UCCKPH,
    spiMode_1 = 0,
    spiMode_2 = UCCKPL | UCCKPH,
    spiMode_3 = UCCKPL
} SpiMode;

extern void delay_cycles_runtime(unsigned short cycles);
void init(SpiMode spiMode);
void spiSelfTest(const unsigned char *data, unsigned short count, unsigned short delay);

int main()
{
    WDTCTL = WDTPW | WDTHOLD; // Stop watchdog timer

    init(spiMode_0);

    static const unsigned char data[] = {0x56, 0x6E, 0x1C, 0xA8, 0xD3, 0xAD};
    unsigned short delay = 6 * 128;      // Minimum delay 6 SPI bits
    const unsigned short max = 12 * 128; // Maximum delay 12 SPI bits

    DEBUG_BREAK();

    while(delay <= max)
    {
        spiSelfTest(data, COUNT_OF(data), delay);
        delay += 4;

        __delay_cycles(320000); // Delay so the protocol analyser can keep up
    }

    DEBUG_BREAK();
}

void initClocks()
{
    // Set DCO to 16MHz
    DCOCTL  = 0;
    BCSCTL1 = CALBC1_16MHZ | XT2OFF;
    DCOCTL  = CALDCO_16MHZ;

    // MCLK=DCO, SMCLK=DCO, LFXT1=VLO
    BCSCTL2 = DIVS_0 | DIVM_0 | SELM_0;
    BCSCTL3 = LFXT1S_2;
}

void initGPIO()
{
    // Output low on all P1 pins
    P1OUT = 0x00;
    P1DIR = 0xFF;

    // Output low on all P2 pins
    P2SEL = 0x00;
    P2OUT = 0x00;
    P2DIR = 0xFF;

    // Output low on all P3 pins
    P3OUT = 0x00;
    P3DIR = 0xFF;
}

void initTimer()
{
    // TACLK=SMCLK/8, TA1.0 output low
    TA1CTL   = TASSEL_2 | ID_3 | MC_1 | TACLR;
    TA1CCR0  = 0;
    TA1CCTL0 = OUTMOD_5;

    // Configure P2.0 and P2.3 to output TA1.0 for the SPI clock
    P2SEL |= (BIT0 | BIT3);
}

void initUSCI(SpiMode spiMode)
{
    P1OUT |= BIT4; // CS high

    // 3-pin SPI loopback mode, MSB first
    UCB0CTL1 |= UCSWRST;
    UCB0CTL0  = UCSYNC | UCMODE_0 | UCMSB | (unsigned short)spiMode;
    UCB0STAT  = UCLISTEN;
}

void init(SpiMode spiMode)
{
    initClocks();
    initGPIO();
    initTimer();
    initUSCI(spiMode);
}

void spiSelfTest(const unsigned char *data, unsigned short count, unsigned short delay)
{
    const unsigned char spiPins = BIT5 | BIT6 | BIT7;
    unsigned short index = 0;

    // Enable SPI pins and start USCI
    P1SEL    |= spiPins;
    P1SEL2   |= spiPins;
    UCB0TXBUF = 0;
    UCB0CTL1 &= ~UCSWRST;

    // Load first TX byte
    while((UC0IFG & UCB0TXIFG) == 0);
    UCB0TXBUF = data[index];

    P1OUT &= ~BIT4;  // CS low

    // Start SPI clock (= TACLK/16 = SMCLK/128)
    TA1CTL |= TACLR;
    TA1CCTL0 = OUTMOD_4;
    TA1CCR0 = 8-1;

    while(++index < count)
    {
        // Insert variable delay before loading byte 4
        if(index == 3)
        {
            P1OUT |= BIT3;
            delay_cycles_runtime(delay);
            P1OUT &= ~BIT3;
        }

        // Load next TX byte (can happen as soon as first SPI clock cycle occurs)
        while((UC0IFG & UCB0TXIFG) == 0);
        UCB0TXBUF = data[index];

        // Read next RX byte
        while((UC0IFG & UCB0RXIFG) == 0);
        (void)UCB0RXBUF;
    }

    // Read last RX byte
    while((UC0IFG & UCB0RXIFG) == 0);
    (void)UCB0RXBUF;

    // Stop USCI and deselect IO pins
    UCB0CTL1 |= UCSWRST;
    P1SEL2 &= ~spiPins;
    P1SEL  &= ~spiPins;

    // Stop SPI clock
    TA1CCTL0 = OUTMOD_5;
    TA1CTL &= ~TAIFG;
    while((TA1CTL & TAIFG) == 0);
    TA1CCR0 = 0;

    P1OUT |= BIT4; // CS high
}
