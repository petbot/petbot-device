/*
 * Allwinner sunxi resistive touchscreen controller driver
 *
 * Copyright (C) 2013 - 2014 Hans de Goede <hdegoede@redhat.com>
 *
 * The hwmon parts are based on work by Corentin LABBE which is:
 * Copyright (C) 2013 Corentin LABBE <clabbe.montjoie@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/*
 * The sun4i-ts controller is capable of detecting a second touch, but when a
 * second touch is present then the accuracy becomes so bad the reported touch
 * location is not useable.
 *
 * The original android driver contains some complicated heuristics using the
 * aprox. distance between the 2 touches to see if the user is making a pinch
 * open / close movement, and then reports emulated multi-touch events around
 * the last touch coordinate (as the dual-touch coordinates are worthless).
 *
 * These kinds of heuristics are just asking for trouble (and don't belong
 * in the kernel). So this driver offers straight forward, reliable single
 * touch functionality only.
 *
 * s.a. A20 User Manual "1.15 TP" (Documentation/arm/sunxi/README)
 * (looks like the description in the A20 User Manual v1.3 is better
 * than the one in the A10 User Manual v.1.5)
 */


#include <stdlib.h>
#include <stdint.h>

static inline void writeb(uint8_t val, void *addr)
{
        *((volatile uint8_t *)addr) = val;
}

static inline void writel(uint32_t val, void *addr)
{
        *((volatile uint32_t *)addr) = val;
}

static inline uint32_t readl(void *addr)
{
        return *((volatile uint32_t *) addr);
}

#define TP_BASE 0x01C25000

#define TP_CTRL0		0x00
#define TP_CTRL1		0x04
#define TP_CTRL2		0x08
#define TP_CTRL3		0x0c
#define TP_INT_FIFOC		0x10
#define TP_INT_FIFOS		0x14
#define TP_TPR			0x18
#define TP_CDAT			0x1c
#define TEMP_DATA		0x20
#define TP_DATA			0x24

/* TP_CTRL0 bits */
#define ADC_FIRST_DLY(x)	((x) << 24) /* 8 bits */
#define ADC_FIRST_DLY_MODE(x)	((x) << 23)
#define ADC_CLK_SEL(x)		((x) << 22)
#define ADC_CLK_DIV(x)		((x) << 20) /* 3 bits */
#define FS_DIV(x)		((x) << 16) /* 4 bits */
#define T_ACQ(x)		((x) << 0) /* 16 bits */

/* TP_CTRL1 bits */
#define STYLUS_UP_DEBOUN(x)	((x) << 12) /* 8 bits */
#define STYLUS_UP_DEBOUN_EN(x)	((x) << 9)
#define TOUCH_PAN_CALI_EN(x)	((x) << 6)
#define TP_DUAL_EN(x)		((x) << 5)
#define TP_MODE_EN(x)		((x) << 4)
#define TP_ADC_SELECT(x)	((x) << 3)
#define ADC_CHAN_SELECT(x)	((x) << 0)  /* 3 bits */

/* on sun6i, bits 3~6 are left shifted by 1 to 4~7 */
#define SUN6I_TP_MODE_EN(x)	((x) << 5)

/* TP_CTRL2 bits */
#define TP_SENSITIVE_ADJUST(x)	((x) << 28) /* 4 bits */
#define TP_MODE_SELECT(x)	((x) << 26) /* 2 bits */
#define PRE_MEA_EN(x)		((x) << 24)
#define PRE_MEA_THRE_CNT(x)	((x) << 0) /* 24 bits */

/* TP_CTRL3 bits */
#define FILTER_EN(x)		((x) << 2)
#define FILTER_TYPE(x)		((x) << 0)  /* 2 bits */

/* TP_INT_FIFOC irq and fifo mask / control bits */
#define TEMP_IRQ_EN(x)		((x) << 18)
#define OVERRUN_IRQ_EN(x)	((x) << 17)
#define DATA_IRQ_EN(x)		((x) << 16)
#define TP_DATA_XY_CHANGE(x)	((x) << 13)
#define FIFO_TRIG(x)		((x) << 8)  /* 5 bits */
#define DATA_DRQ_EN(x)		((x) << 7)
#define FIFO_FLUSH(x)		((x) << 4)
#define TP_UP_IRQ_EN(x)		((x) << 1)
#define TP_DOWN_IRQ_EN(x)	((x) << 0)

/* TP_INT_FIFOS irq and fifo status bits */
#define TEMP_DATA_PENDING	BIT(18)
#define FIFO_OVERRUN_PENDING	BIT(17)
#define FIFO_DATA_PENDING	BIT(16)
#define TP_IDLE_FLG		BIT(2)
#define TP_UP_PENDING		BIT(1)
#define TP_DOWN_PENDING		BIT(0)

/* TP_TPR bits */
#define TEMP_ENABLE(x)		((x) << 16)
#define TEMP_PERIOD(x)		((x) << 0)  /* t = x * 256 * 16 / clkin */




int main(int argc, char * argv) {
	/*
	 * Select HOSC clk, clkin = clk / 6, adc samplefreq = clkin / 8192,
	 * t_acq = clkin / (16 * 64)
	 */
	writel(ADC_CLK_SEL(0) | ADC_CLK_DIV(2) | FS_DIV(7) | T_ACQ(63),
	       TP_BASE + TP_CTRL0);

	uint32_t reg;
	reg |= TP_MODE_EN(1);
	writel(reg, TP_BASE + TP_CTRL1);

	/*
	 * The thermal core does not register hwmon devices for DT-based
	 * thermal zone sensors, such as this one.
	 */
	return 0;
}
