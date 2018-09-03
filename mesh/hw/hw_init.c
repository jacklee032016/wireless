/*
 * $Id: hw_init.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
 */

#include "mesh.h"

#include <linux/delay.h>
#include <asm/io.h>

#include "opt_ah.h"
#include "hal.h"

#ifdef AH_DEBUG
static	int hw_hal_debug = 0;
#endif

void __ahdecl hw_hal_vprintf(struct ath_hal *ah, const char* fmt, va_list ap)
{
	char buf[1024];					/* XXX */
	vsnprintf(buf, sizeof(buf), fmt, ap);
	printk("%s", buf);
}

/* this function is defined here and used by HAL */
void __ahdecl ath_hal_printf(struct ath_hal *ah, const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	hw_hal_vprintf(ah, fmt, ap);
	va_end(ap);
}
EXPORT_SYMBOL(ath_hal_printf);

const char* __ahdecl hw_hal_ether_sprintf(const u_int8_t *mac)
{
	static char etherbuf[18];
	snprintf(etherbuf, sizeof(etherbuf), "%02x:%02x:%02x:%02x:%02x:%02x",	mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	return etherbuf;
}

#ifdef AH_ASSERT
void __ahdecl hw_hal_assert_failed(const char* filename, int lineno, const char *msg)
{
	printk("Atheros HAL assertion failure: %s: line %u: %s\n", filename, lineno, msg);
	panic("hw_hal_assert");
}
#endif /* AH_ASSERT */

#if defined(AH_DEBUG) || defined(AH_REGOPS_FUNC)
/*
 * Memory-mapped device register read/write.  These are here
 * as routines when debugging support is enabled and/or when
 * explicitly configured to use function calls.  The latter is
 * for architectures that might need to do something before
 * referencing memory (e.g. remap an i/o window).
 *
 * NB: see the comments in ah_osdep.h about byte-swapping register
 *     reads and writes to understand what's going on below.
 */
void __ahdecl ath_hal_reg_write(struct ath_hal *ah, u_int reg, u_int32_t val)
{
#ifdef AH_DEBUG
	if (hw_hal_debug > 1)
		ath_hal_printf(ah, "WRITE 0x%x <= 0x%x\n", reg, val);
#endif
	_OS_REG_WRITE(ah, reg, val);
}
EXPORT_SYMBOL(ath_hal_reg_write);

u_int32_t __ahdecl ath_hal_reg_read(struct ath_hal *ah, u_int reg)
{
 	u_int32_t val;

	val = _OS_REG_READ(ah, reg);
#ifdef AH_DEBUG
	if (hw_hal_debug > 1)
		hw_hal_printf(ah, "READ 0x%x => 0x%x\n", reg, val);
#endif
	return val;
}
EXPORT_SYMBOL(ath_hal_reg_read);
#endif /* AH_DEBUG || AH_REGOPS_FUNC */

#ifdef AH_DEBUG
void __ahdecl HALDEBUG(struct ath_hal *ah, const char* fmt, ...)
{
	if (hw_hal_debug)
	{
		__va_list ap;
		va_start(ap, fmt);
		hw_hal_vprintf(ah, fmt, ap);
		va_end(ap);
	}
}

void __ahdecl HALDEBUGn(struct ath_hal *ah, u_int level, const char* fmt, ...)
{
	if (hw_hal_debug >= level)
	{
		__va_list ap;
		va_start(ap, fmt);
		hw_hal_vprintf(ah, fmt, ap);
		va_end(ap);
	}
}
#endif /* AH_DEBUG */

#if 0
u_int32_t __ahdecl ath_hal_getuptime(struct ath_hal *ah)
{
	return ((jiffies / HZ) * 1000) + (jiffies % HZ) * (1000 / HZ);
}
EXPORT_SYMBOL(ath_hal_getuptime);
#endif

/* following functions are defined here and used by HAL */
void __ahdecl ath_hal_delay(int n)
{
	udelay(n);
}

void * __ahdecl ath_hal_malloc(size_t size)
{
	void *p;
	p = kmalloc(size, GFP_KERNEL);
	if (p)
		OS_MEMZERO(p, size);
	return p;
}

void __ahdecl ath_hal_free(void* p)
{
	kfree(p);
}

void __ahdecl ath_hal_memzero(void *dst, size_t n)
{
	memset(dst, 0, n);
}
EXPORT_SYMBOL(ath_hal_memzero);

void * __ahdecl ath_hal_memcpy(void *dst, const void *src, size_t n)
{
	return memcpy(dst, src, n);
}
EXPORT_SYMBOL(ath_hal_memcpy);


struct ath_hal *hw_hal_attach(u_int16_t devid, HAL_SOFTC sc,	HAL_BUS_TAG t, HAL_BUS_HANDLE h, void* s)
{
	HAL_STATUS status;
	struct ath_hal *ah = ath_hal_attach(devid, sc, t, h, &status);

	*(HAL_STATUS *)s = status;
	if (ah)
		MOD_INC_USE_COUNT;
	
	return ah;
}

void hw_hal_detach(struct ath_hal *ah)
{
	(*ah->ah_detach)(ah);
	MOD_DEC_USE_COUNT;
}

static char *hwDevInfo = "HW-HAL";

MODULE_AUTHOR("Errno Consulting, Sam Leffler");
MODULE_DESCRIPTION("Atheros Hardware Access Layer (HAL)");
MODULE_SUPPORTED_DEVICE("Atheros WLAN devices");
#ifdef MODULE_LICENSE
MODULE_LICENSE("Proprietary");
#endif

/* following symbols are defined in binary HAL, declared in hal.h and used by PHY */
EXPORT_SYMBOL(ath_hal_probe);
EXPORT_SYMBOL(ath_hal_init_channels);
EXPORT_SYMBOL(ath_hal_getwirelessmodes);
EXPORT_SYMBOL(ath_hal_computetxtime);
EXPORT_SYMBOL(ath_hal_mhz2ieee);

EXPORT_SYMBOL(ath_hal_ieee2mhz);


EXPORT_SYMBOL(hw_hal_attach);
EXPORT_SYMBOL(hw_hal_detach);

void _hw_sysctl_register(void);
void _hw_sysctl_unregister(void);

static int __init hw_init(void)
{
	const char *sep;
	int i;

	printk(KERN_INFO MESH_VENDOR"(%s): %s (", hwDevInfo, ath_hal_version);
	sep = "";
	for (i = 0; ath_hal_buildopts[i] != NULL; i++)
	{
		printk("%s%s", sep, ath_hal_buildopts[i]);
		sep = ", ";
	}
	printk(")\n");
	
#ifdef CONFIG_SYSCTL
	_hw_sysctl_register();
#endif
	return (0);
}

static void __exit hw_exit(void)
{
#ifdef CONFIG_SYSCTL
	_hw_sysctl_unregister();
#endif
	printk(KERN_INFO "%s: driver unloaded\n", hwDevInfo);
}
module_init(hw_init);
module_exit(hw_exit);

