/* Standard includes */
#include <stdio.h>

/* HiFive1/FE310 includes */
#include "sifive/devices/spi.h"
#include "platform.h"
#include "encoding.h"
#include "plic/plic_driver.h"

/* Hardcode to use device SPI1 */
#define SPI_REG(x) SPI1_REG(x)
#define RTC_FREQUENCY 32768

volatile uint8_t send_SPI = 0;

static const uint32_t SPI1_IOF_MASK =
    (1 << IOF_SPI1_SS0)  |
    (1 << IOF_SPI1_SCK)  |
    (1 << IOF_SPI1_MOSI) |
    (1 << IOF_SPI1_MISO);

static const uint32_t BLUE_LED_MASK = (0x1 << BLUE_LED_OFFSET);

void set_SPI_frame_length(uint8_t length)
{
    /* Set frame format register
     * Bit 0-1: proto - SPI protocol used. SPI_PROTO_S = single channel
     * Bit 2: endian - Endianess of frame. SPI_ENDIAN_MSB = transmit MSB first
     * Bit 3: dir - Allows RX during dual and quad modes
     * Bit 16-19: len - length of frame
     */
    SPI_REG(SPI_REG_FMT) = 0;
    SPI_REG(SPI_REG_FMT) =
        SPI_FMT_PROTO(SPI_PROTO_S)     |
        SPI_FMT_ENDIAN(SPI_ENDIAN_MSB) |
        SPI_FMT_DIR(SPI_DIR_TX)        |
        SPI_FMT_LEN(length); // 8 bit long packets
}

void write_SPI()
{
    /* Toggle an LED for visual feedback */
    GPIO_REG(GPIO_OUTPUT_VAL) ^=  BLUE_LED_MASK;
    set_SPI_frame_length(4);
    SPI_REG(SPI_REG_TXFIFO) = 0xF0;
    while (SPI_REG(SPI_REG_TXFIFO) & SPI_TXFIFO_FULL);

    set_SPI_frame_length(8);
    SPI_REG(SPI_REG_TXFIFO) = 0xAA;
    while (SPI_REG(SPI_REG_TXFIFO) & SPI_TXFIFO_FULL);
}

void handle_m_time_interrupt()
{
    /* Disable the machine and timer interrupts until setup is completed */
    clear_csr(mie, MIP_MTIP);

    /* Set the machine timer to go off in X seconds */
    volatile uint64_t * mtime    = (uint64_t*) (CLINT_CTRL_ADDR + CLINT_MTIME);
    volatile uint64_t * mtimecmp = (uint64_t*) (CLINT_CTRL_ADDR + CLINT_MTIMECMP);
    uint64_t now = *mtime;
    /* Send data every second */
    uint64_t then = now + RTC_FREQUENCY;
    *mtimecmp = then;

    /* Notify the main loop that it can now send SPI data */
    send_SPI = 1;

    /* Re-enable timer */
    set_csr(mie, MIP_MTIP);
}

void handle_m_ext_interrupt()
{
}

void init_SPI()
{
    /* Full documentation for the FE310 SPI module is at:
     * https://www.sifive.com/documentation/freedom-soc/freedom-e300-platform-reference-manual
     * Chapter 13
     */

    /* Select and enable SPI1 device pins */
    GPIO_REG(GPIO_IOF_SEL) &=  ~SPI1_IOF_MASK;
    GPIO_REG(GPIO_IOF_EN)  |=   SPI1_IOF_MASK;

    /* Set data mode
     * Bit 0: pha - Inactive state of SCK is logical 0
     * Bit 1: pol - Data is sampled on the leading edge of SCK
     */
    SPI_REG(SPI_REG_SCKMODE) = 0x0;

    set_SPI_frame_length(8);

    /* Set the delay between CS assertions to zero on consecutive frames */
    SPI_REG(SPI_REG_DINTERCS) = 0x00;

    /* Set CS mode auto
     * SPI_CSMODE_AUTO - Assert/de-assert CS at beginning and end of each frame
     */
    SPI_REG(SPI_REG_CSMODE) = SPI_CSMODE_AUTO;

    /* Clock divider
     * Original clock is coreclk (the main CPU clock) where the resulting SPI
     * clock speed is determined by:
     *
     * sck = coreclk / 2(divider + 1)
     *
     * Defaults to 0x003. E.g.,:
     *
     * 262MHz / 2(3+1) = 32.75MHz
     * 262MHz / 2(2+1) = 43.67MHz
     */
    SPI_REG(SPI_REG_SCKDIV) = 0x03;
}

void init_GPIO ()
{
    GPIO_REG(GPIO_OUTPUT_EN)   |=  BLUE_LED_MASK;
    GPIO_REG(GPIO_OUTPUT_VAL)  &=  ~BLUE_LED_MASK;
}

void init_timer()
{
    clear_csr(mie, MIP_MTIP);

    /* Set the machine timer to go off in X seconds */
    volatile uint64_t * mtime    = (uint64_t*) (CLINT_CTRL_ADDR + CLINT_MTIME);
    volatile uint64_t * mtimecmp = (uint64_t*) (CLINT_CTRL_ADDR + CLINT_MTIMECMP);
    uint64_t now = *mtime;
    /* Send every second */
    uint64_t then = now + RTC_FREQUENCY;
    *mtimecmp = then;

    /* Enable timer and interrupts */
    set_csr(mie, MIP_MTIP);
    set_csr(mstatus, MSTATUS_MIE);
}

int main()
{
    puts("Sending periodic SPI bursts ...");

    init_GPIO();
    init_timer();
    init_SPI();

    while (1) {
        if (send_SPI) {
            puts("Sending SPI data");
            write_SPI();
            send_SPI = 0;
        }
    };

    return 0;
}
