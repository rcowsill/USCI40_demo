# USCI40_demo

## Overview

This is a simplified version of the firmware I wrote to characterise the MSP430G2553 USCI40 erratum. The original version was used to generate the series of logic analyser captures shown as animations in the following forum post:

https://e2e.ti.com/support/microcontrollers/msp430/f/166/t/363755

## Erratum Description

When clocked externally, the USCI peripheral doesn't have the option to delay transmission until it's ready. It needs to output the next bit when the leading edge of the next clock pulse arrives, even if its shift register is empty and TXBUF hasn't been updated.

Ideally the USCI would have one consistent behaviour in cases where TXBUF was loaded too late. For example, it could always retransmit the previous byte loaded into TXBUF. That might break any higher-level protocol being used, but would produce valid signal timings and keep the USCI in sync with the clock source.

Unfortunately the USCI shows multiple behaviours in this case, depending on how late TXBUF was loaded. This is particularly severe when the `UCCKPH` bit is set, which results in the five different error behaviours detailed below.

## Sample output

In the logic analyser captures shown below, TXBUF gets set to `0xA8` immediately after the falling edge on the delay output (bottom row). The highlighted region of each capture shows the range of delays that would give the same result.

### Correct output

![captureA](https://user-images.githubusercontent.com/42620235/149661197-a8edb8ae-c189-4462-8a17-0ccc815dd186.png)

If TXBUF is loaded early enough there is no problem. The new value is transferred into the internal transmit shift register of the USCI, and is used as the next byte to output. 

### Error 1: Leading bit glitch

![captureB](https://user-images.githubusercontent.com/42620235/149661199-cddcf5ad-5511-43ee-b0f6-2cef7c4348ac.png)

If TXBUF is loaded up to 0.5 SPI clock cycles late, then the data pin changes state when TXBUF is loaded. In the worst case it can change immediately before the trailing edge of the SPI clock (when the data line is supposed to be stable). This could result in the wrong value being received at the other end (eg `0x28` instead of `0xA8`).

### Error 2: Repeated last bit

![captureC](https://user-images.githubusercontent.com/42620235/149661200-1c531749-d8a1-4f2a-a25e-68021733c7b4.png)

If TXBUF is loaded from 0.5 up to 1 SPI clock cycles late, then the USCI becomes offset by one bit from the external clock. This transmit offset persists until the USCI is reset. It repeats the previous bit for one extra clock, then transfers TXBUF into the shift register and uses that value for the next eight bits.

### Error 3: Repeated last bit, next byte replaced by previous

![captureD](https://user-images.githubusercontent.com/42620235/149661201-09c9d16d-54fb-49c4-9223-10a9d1ebca4a.png)

If TXBUF is loaded from 1 up to 2 SPI clock cycles late, then the USCI becomes offset by one bit from the external clock. This transmit offset persists until the USCI is reset. It repeats the previous bit for one extra clock, then the current shift register value gets reused for the subsequent eight bits. The TXIFG bit is set again, so the value in TXBUF is overwritten without ever being sent. 

### Error 4: Repeated last bit and byte, another repeated bit

![captureE](https://user-images.githubusercontent.com/42620235/149661202-9727db4d-c056-4968-837f-10757eb61fca.png)

If TXBUF is loaded exactly 2 SPI clock cycles late, then the USCI becomes offset by two bits from the external clock. This transmit offset persists until the USCI is reset. It repeats the previous bit for one extra clock, then the current shift register value gets reused for the subsequent eight bits. The previous bit is repeated and then the value in TXIFG is copied to the shift register and output for the following eight bits.

### Error 5: Repeated last bit and byte

![captureF](https://user-images.githubusercontent.com/42620235/149661203-8f909958-1f0a-4aae-865a-2a7dd3014d32.png)

If TXBUF is loaded more than 2 SPI clock cycles late, then the USCI becomes offset by one bit from the external clock. This transmit offset persists until the USCI is reset. It repeats the previous bit for one extra clock, then the current shift register value gets reused for the subsequent eight bits. The value in TXIFG is copied to the shift register and output for the following eight bits.
