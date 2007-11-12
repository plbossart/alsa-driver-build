#define __NO_VERSION__
/*
 * C-Media CMI8788 driver - helper functions
 *
 * Copyright (c) Clemens Ladisch <clemens@ladisch.de>
 *
 *
 *  This driver is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License, version 2.
 *
 *  This driver is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this driver; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include <sound/driver.h>
#include <linux/delay.h>
#include <sound/core.h>
#include <asm/io.h>
#include "oxygen.h"

u8 oxygen_read8(struct oxygen *chip, unsigned int reg)
{
	return inb(chip->addr + reg);
}
EXPORT_SYMBOL(oxygen_read8);

u16 oxygen_read16(struct oxygen *chip, unsigned int reg)
{
	return inw(chip->addr + reg);
}
EXPORT_SYMBOL(oxygen_read16);

u32 oxygen_read32(struct oxygen *chip, unsigned int reg)
{
	return inl(chip->addr + reg);
}
EXPORT_SYMBOL(oxygen_read32);

void oxygen_write8(struct oxygen *chip, unsigned int reg, u8 value)
{
	outb(value, chip->addr + reg);
}
EXPORT_SYMBOL(oxygen_write8);

void oxygen_write16(struct oxygen *chip, unsigned int reg, u16 value)
{
	outw(value, chip->addr + reg);
}
EXPORT_SYMBOL(oxygen_write16);

void oxygen_write32(struct oxygen *chip, unsigned int reg, u32 value)
{
	outl(value, chip->addr + reg);
}
EXPORT_SYMBOL(oxygen_write32);

void oxygen_write8_masked(struct oxygen *chip, unsigned int reg,
			  u8 value, u8 mask)
{
	u8 tmp = inb(chip->addr + reg);
	outb((tmp & ~mask) | (value & mask), chip->addr + reg);
}
EXPORT_SYMBOL(oxygen_write8_masked);

void oxygen_write16_masked(struct oxygen *chip, unsigned int reg,
			   u16 value, u16 mask)
{
	u16 tmp = inw(chip->addr + reg);
	outw((tmp & ~mask) | (value & mask), chip->addr + reg);
}
EXPORT_SYMBOL(oxygen_write16_masked);

void oxygen_write32_masked(struct oxygen *chip, unsigned int reg,
			   u32 value, u32 mask)
{
	u32 tmp = inl(chip->addr + reg);
	outl((tmp & ~mask) | (value & mask), chip->addr + reg);
}
EXPORT_SYMBOL(oxygen_write32_masked);

void oxygen_write_ac97(struct oxygen *chip, unsigned int codec,
		       unsigned int index, u16 data)
{
	unsigned int tries, count;
	u32 reg;

	reg = data;
	reg |= index << OXYGEN_AC97_REG_ADDR_SHIFT;
	reg |= OXYGEN_AC97_REG_DIR_WRITE;
	reg |= codec << OXYGEN_AC97_REG_CODEC_SHIFT;
	/*
	 * AC'97 writes are extremely unreliable with this controller, about
	 * 10% of them do not complete.  We better try multiple times.
	 */
	for (tries = 3; tries > 0; --tries) {
		oxygen_write32(chip, OXYGEN_AC97_REGS, reg);
		/* should not need more than 20.8 us (1 / 48 kHz) */
		count = 50;
		for (;;) {
			if (oxygen_read8(chip, OXYGEN_AC97_INTERRUPT_STATUS) &
			    OXYGEN_AC97_WRITE_COMPLETE)
				return;
			if (count == 0)
				break;
			udelay(1);
			--count;
		}
	}
	snd_printk(KERN_ERR "AC'97 write timeout\n");
}
EXPORT_SYMBOL(oxygen_write_ac97);

u16 oxygen_read_ac97(struct oxygen *chip, unsigned int codec,
		     unsigned int index)
{
	unsigned int tries, count;
	u32 reg;

	reg = index << OXYGEN_AC97_REG_ADDR_SHIFT;
	reg |= OXYGEN_AC97_REG_DIR_READ;
	reg |= codec << OXYGEN_AC97_REG_CODEC_SHIFT;
	/* AC'97 reads are just as unreliable */
	for (tries = 3; tries > 0; --tries) {
		oxygen_write32(chip, OXYGEN_AC97_REGS, reg);
		/* should not need more than 41.7 us (2 / 48 kHz) */
		udelay(30);
		count = 50;
		for (;;) {
			if (oxygen_read8(chip, OXYGEN_AC97_INTERRUPT_STATUS) &
			    OXYGEN_AC97_READ_COMPLETE)
				return oxygen_read16(chip, OXYGEN_AC97_REGS);
			if (count == 0)
				break;
			udelay(1);
			--count;
		}
	}
	snd_printk(KERN_ERR "AC'97 read timeout on codec %u\n", codec);
	return 0;
}
EXPORT_SYMBOL(oxygen_read_ac97);

void oxygen_write_spi(struct oxygen *chip, u8 control, unsigned int data)
{
	unsigned int count;

	/* should not need more than 3.84 us (24 * 160 ns) */
	count = 10;
	while ((oxygen_read8(chip, OXYGEN_SPI_CONTROL) & OXYGEN_SPI_BUSY)
	       && count > 0) {
		udelay(1);
		--count;
	}

	spin_lock_irq(&chip->reg_lock);
	oxygen_write8(chip, OXYGEN_SPI_DATA1, data);
	oxygen_write8(chip, OXYGEN_SPI_DATA2, data >> 8);
	if (control & OXYGEN_SPI_DATA_LENGTH_3)
		oxygen_write8(chip, OXYGEN_SPI_DATA3, data >> 16);
	oxygen_write8(chip, OXYGEN_SPI_CONTROL, control);
	spin_unlock_irq(&chip->reg_lock);
}
EXPORT_SYMBOL(oxygen_write_spi);
