/*
 *  Plug & Play 2.5 layer compatibility
 *  Copyright (c) by Jaroslav Kysela <perex@suse.cz>
 *
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/config.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 2, 0)
#error "This driver is designed only for Linux 2.2.0 and higher."
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 3, 11)
#define NEW_RESOURCE
#endif

#ifdef ALSA_BUILD
#if defined(CONFIG_MODVERSIONS) && !defined(__GENKSYMS__) && !defined(__DEPEND__)
#define MODVERSIONS
#include <linux/modversions.h>
#include "sndversions.h"
#endif
#endif

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/string.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <asm/io.h>
#include <asm/dma.h>
#include <asm/irq.h>
#include <linux/pci.h>
#include <linux/vmalloc.h>
#include <linux/poll.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#ifndef ALSA_BUILD
#include <linux/isapnp.h>
#include <linux/pnp.h>
#else
#include <linux/isapnp.h>
#undef CONFIG_PNP
#define CONFIG_PNP
#include "pnp.h"
#endif

#ifndef __init
#define __init
#endif

#ifndef __exit
#define __exit
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,3,0)
#define module_init(x)      int init_module(void) { return x(); }
#define module_exit(x)      void cleanup_module(void) { x(); }
#endif

MODULE_AUTHOR("Jaroslav Kysela <perex@suse.cz>");
MODULE_DESCRIPTION("Plug & Play 2.5 compatible layer");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif

#ifndef list_for_each_safe
#define list_for_each_safe(pos, n, head) \
	for (pos = (head)->next, n = pos->next; pos != (head); \
		pos = n, n = pos->next)
#endif

struct pnp_driver_instance {
	struct pnp_dev * dev;
	struct pnp_driver * driver;
	struct list_head list;
};

struct pnp_card_driver_instance {
	struct pnp_card_link link;
	struct pnp_dev * devs[PNP_MAX_DEVICES];
	struct list_head list;
};

static LIST_HEAD(pnp_drivers);
static LIST_HEAD(pnp_card_drivers);

static unsigned int from_hex(unsigned char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'A' && c <= 'F')
		return (c - 'A') + 10;
	if (c >= 'a' && c <= 'f')
		return (c - 'a') + 10;
	return 0x0f;
}

static int parse_id(const char * id, unsigned short * vendor, unsigned short * device)
{
	if (memcmp(id, "ANYDEVS", 7) == 0) {
		*vendor = ISAPNP_ANY_ID;
		*device = ISAPNP_ANY_ID;
	} else {
		*vendor = ISAPNP_VENDOR(id[0], id[1], id[2]);
		if (memcmp(id + 3, "XXXX", 4) == 0) {
			*device = ISAPNP_ANY_ID;
		} else if (strchr(id + 3, 'X') != NULL) {
			printk(KERN_ERR "cannot detect incomplete PnP ID definition '%s'\n", id);
			return -EINVAL;
		} else {
			*device = ISAPNP_DEVICE((from_hex(id[3]) << 12) |
						(from_hex(id[4]) << 8) |
						(from_hex(id[5]) << 4) |
						from_hex(id[6]));
		}
	}
	return 0;
}

struct pnp_dev * pnp_request_card_device(struct pnp_card_link *clink, const char * id, struct pnp_dev * from)
{
	unsigned short vendor, function;
	struct pnp_dev *dev;

	if (parse_id(id, &vendor, &function) < 0)
		return NULL;
	dev = (struct pnp_dev *)isapnp_find_dev((struct isapnp_card *)clink->card, vendor, function, (struct isapnp_dev *)from);
	return dev;
}

void pnp_release_card_device(struct pnp_dev * dev)
{
	if (!dev->p.active)
		return;
	dev->p.deactivate((struct isapnp_dev *)dev);
}

int pnp_register_card_driver(struct pnp_card_driver * drv)
{
	unsigned short vendor, device;
	unsigned int i, res = 0;
	const struct pnp_card_device_id *cid;
	struct pnp_card *card;
	struct pnp_dev *dev;
	struct pnp_card_driver_instance *ninst = NULL;
	
	for (cid = drv->id_table; cid->id[0] != '\0'; cid++) {
	      __next_card:
		card = NULL;
		do {
		      __next:
			if (parse_id(cid->id, &vendor, &device) < 0)
				break;
			card = (struct pnp_card *)isapnp_find_card(vendor, device, (struct isapnp_card *)card);
			if (card) {
				if (ninst == NULL) {
					ninst = kmalloc(sizeof(*ninst), GFP_KERNEL);
					if (ninst == NULL)
						return res > 0 ? (int)res : -ENOMEM;
					memset(ninst, 0, sizeof(*ninst));
					INIT_LIST_HEAD(&ninst->list);
				}
				for (i = 0; i < PNP_MAX_DEVICES; i++)
					ninst->devs[i] = NULL;
				for (i = 0; i < PNP_MAX_DEVICES && cid->devs[i].id[0] != '\0'; i++) {
					if (parse_id(cid->devs[i].id, &vendor, &device) < 0) {
						cid++;
						goto __next_card;
					}
					dev = ninst->devs[i] = (struct pnp_dev *)isapnp_find_dev((struct isapnp_card *)card, vendor, device, NULL);
					if (dev == NULL)
						goto __next;
					if (! dev->p.active) {
						if (! (drv->flags & PNP_DRIVER_RES_DO_NOT_CHANGE)) {
							pnp_activate_dev(dev);
						}
					} else {
						if ((drv->flags & PNP_DRIVER_RES_DISABLE) == PNP_DRIVER_RES_DISABLE) {
							pnp_disable_dev(dev);
						}
					}
				}
				ninst->link.card = card;
				ninst->link.driver = drv;
				ninst->link.driver_data = NULL;
				if (drv->probe(&ninst->link, cid) >= 0) {
					list_add_tail(&ninst->list, &pnp_card_drivers);
					ninst = NULL;
					res++;
				}
			}
		} while (card != NULL);
	}

	if (ninst != NULL)
		kfree(ninst);
 
	return res;
}

void pnp_unregister_card_driver(struct pnp_card_driver * drv)
{
	struct pnp_card_driver_instance *inst;
	struct list_head *p, *n;
	
	list_for_each_safe(p, n, &pnp_card_drivers) {
		inst = list_entry(p, struct pnp_card_driver_instance, list);
		if (inst->link.driver == drv) {
			list_del(p);
			drv->remove(&inst->link);
			kfree(inst);
		}
	}
}

int pnp_register_driver(struct pnp_driver *drv)
{
	unsigned short vendor, function;
	unsigned int res = 0;
	const struct pnp_device_id *did;
	struct pnp_dev *dev;
	struct pnp_driver_instance *ninst = NULL;
	
	for (did = drv->id_table; did->id[0] != '\0'; did++) {
		dev = NULL;
		if (ninst == NULL) {
			ninst = kmalloc(sizeof(*ninst), GFP_KERNEL);
			if (ninst == NULL)
				return res > 0 ? (int)res : -ENOMEM;
			memset(ninst, 0, sizeof(*ninst));
			INIT_LIST_HEAD(&ninst->list);
		}
		if (parse_id(did->id, &vendor, &function) < 0)
			continue;
		dev = ninst->dev = (struct pnp_dev *)isapnp_find_dev(NULL, vendor, function, (struct isapnp_dev *)dev);
		if (dev == NULL)
			continue;
		ninst->driver = drv;
		if (drv->probe(ninst->dev, did) >= 0) {
			list_add_tail(&ninst->list, &pnp_drivers);
			ninst = NULL;
			res++;
		}
	}
	return res;
}

void pnp_unregister_driver(struct pnp_driver *drv)
{
	struct pnp_driver_instance *inst;
	struct list_head *p, *n;
	
	list_for_each_safe(p, n, &pnp_drivers) {
		inst = list_entry(p, struct pnp_driver_instance, list);
		if (inst->driver == drv) {
			list_del(p);
			drv->remove(inst->dev);
			if (inst->dev->p.active)
				inst->dev->p.deactivate((struct isapnp_dev *)inst->dev);
			kfree(inst);
		}
	}
}

static void copy_resource(struct resource *dst, const struct resource *src)
{
	dst->name = src->name;
	dst->start = src->start;
	dst->end = src->end;
	dst->flags = (dst->flags & ~IORESOURCE_AUTO) |
		(dst->flags & src->flags & IORESOURCE_AUTO);
}

void pnp_init_resource_table(struct pnp_resource_table *table)
{
	unsigned int idx;

	for (idx = 0; idx < PNP_MAX_IRQ; idx++) {
		table->irq_resource[idx].name = NULL;
		table->irq_resource[idx].start = 0;
		table->irq_resource[idx].end = 0;
		table->irq_resource[idx].flags = IORESOURCE_AUTO;
	}
	for (idx = 0; idx < PNP_MAX_DMA; idx++) {
		table->dma_resource[idx].name = NULL;
		table->dma_resource[idx].start = 0;
		table->dma_resource[idx].end = 0;
		table->dma_resource[idx].flags = IORESOURCE_AUTO;
	}
	for (idx = 0; idx < PNP_MAX_PORT; idx++) {
		table->port_resource[idx].name = NULL;
		table->port_resource[idx].start = 0;
		table->port_resource[idx].end = 0;
		table->port_resource[idx].flags = IORESOURCE_AUTO;
	}
	for (idx = 0; idx < PNP_MAX_MEM; idx++) {
		table->mem_resource[idx].name = NULL;
		table->mem_resource[idx].start = 0;
		table->mem_resource[idx].end = 0;
		table->mem_resource[idx].flags = IORESOURCE_AUTO;
	}
}

/* FIXME: this function cannot be called many times.  the setting is cleared at each time */
int pnp_manual_config_dev(struct pnp_dev *dev, struct pnp_resource_table *res, int mode)
{
	unsigned int idx;
	int err;

	/* prepare the isapnp */
	err = dev->p.prepare((struct isapnp_dev *)dev);

	for (idx = 0; idx < PNP_MAX_IRQ; idx++)
		copy_resource(&dev->p.irq_resource[idx], &res->irq_resource[idx]);
	for (idx = 0; idx < PNP_MAX_DMA; idx++)
		copy_resource(&dev->p.dma_resource[idx], &res->dma_resource[idx]);
	for (idx = 0; idx < PNP_MAX_PORT; idx++)
		copy_resource(&dev->p.resource[idx], &res->port_resource[idx]);
	for (idx = 0; idx < PNP_MAX_MEM; idx++)
		copy_resource(&dev->p.resource[idx+8], &res->mem_resource[idx]);

	return 0;
}

int pnp_activate_dev(struct pnp_dev *dev)
{
	struct pnp_resource_table *tmp;
	unsigned int idx;

	if (dev->p.active)
		return 0; /* FIXME: should be -EBUSY but 2.6 pnp layer behaves like this */

	/* reserve the manual configuration */
	tmp = kmalloc(sizeof(*tmp), GFP_KERNEL);
	if (! tmp)
		return -ENOMEM;
	pnp_init_resource_table(tmp);
	for (idx = 0; idx < PNP_MAX_IRQ; idx++)
		copy_resource(&tmp->irq_resource[idx], &dev->p.irq_resource[idx]);
	for (idx = 0; idx < PNP_MAX_DMA; idx++)
		copy_resource(&tmp->dma_resource[idx], &dev->p.dma_resource[idx]);
	for (idx = 0; idx < PNP_MAX_PORT; idx++)
		copy_resource(&tmp->port_resource[idx], &dev->p.resource[idx]);
	for (idx = 0; idx < PNP_MAX_MEM; idx++)
		copy_resource(&tmp->mem_resource[idx], &dev->p.resource[idx+8]);

	/* restore the manual configuration again */
	pnp_manual_config_dev(dev, tmp, 0);
	kfree(tmp);

	return dev->p.activate((struct isapnp_dev *)dev);
}

int pnp_disable_dev(struct pnp_dev *dev)
{
	if (! dev->p.active)
		return 0;
	return dev->p.deactivate((struct isapnp_dev *)dev);
	/* FIXME: do we need clean up the resources again? */
}

static int __init pnp_init(void)
{
	return 0;
}

static void __exit pnp_exit(void)
{
}

module_init(pnp_init)
module_exit(pnp_exit)

EXPORT_SYMBOL(pnp_request_card_device);
EXPORT_SYMBOL(pnp_release_card_device);
EXPORT_SYMBOL(pnp_register_card_driver);
EXPORT_SYMBOL(pnp_unregister_card_driver);
EXPORT_SYMBOL(pnp_register_driver);
EXPORT_SYMBOL(pnp_unregister_driver);
EXPORT_SYMBOL(pnp_init_resource_table);
EXPORT_SYMBOL(pnp_manual_config_dev);
EXPORT_SYMBOL(pnp_activate_dev);
EXPORT_SYMBOL(pnp_disable_dev);
