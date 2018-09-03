/*
* $Id: hw.h,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/
 
#ifndef	__HW_H__
#define	__HW_H__

/*
 * Atheros Hardware Access Layer (HAL) OS Dependent Definitions.
 */

/*
 * Starting with 2.6.4 the kernel supports a configuration option
 * to pass parameters in registers.  If this is enabled we must
 * mark all function interfaces in+out of the HAL to pass parameters
 * on the stack as this is the convention used internally (for
 * maximum portability).
 */
#ifdef CONFIG_REGPARM
#define	__ahdecl	__attribute__((regparm(0)))
#else
#define	__ahdecl
#endif
#ifndef __packed
#define	__packed	__attribute__((__packed__))
#endif

/*
 * When building the HAL proper we use no GPL-contaminated include
 * files and must define these types ourself.  Beware of these being
 * mismatched against the contents of <linux/types.h>
 */
#if  0 //ndef _LINUX_TYPES_H
/* NB: arm defaults to unsigned so be explicit */
typedef signed char int8_t;
typedef short int16_t;
typedef int int32_t;
typedef long long int64_t;

typedef unsigned char u_int8_t;
typedef unsigned short u_int16_t;
typedef unsigned int u_int32_t;
typedef unsigned long long u_int64_t;

typedef unsigned int size_t;
typedef unsigned int u_int;
typedef	void *va_list;
#endif

/*
 * Linux/BSD gcc compatibility shims.
 */
#define	__printflike(_a,_b) \
	__attribute__ ((__format__ (__printf__, _a, _b)))
#define	__va_list	va_list 
#define	OS_INLINE	__inline

typedef void* HAL_SOFTC;
typedef int HAL_BUS_TAG;
typedef void* HAL_BUS_HANDLE;
typedef u_int32_t HAL_BUS_ADDR;			/* XXX architecture dependent */

/*
 * Delay n microseconds.
 */
extern	void __ahdecl ath_hal_delay(int);
#define	OS_DELAY(_n)	ath_hal_delay(_n)

extern void __ahdecl ath_hal_memzero(void *, size_t);
#define	OS_MEMZERO(_a, _n)	ath_hal_memzero((_a), (_n))

extern void * __ahdecl ath_hal_memcpy(void *, const void *, size_t);
#define	OS_MEMCPY(_d, _s, _n)	ath_hal_memcpy(_d,_s,_n)

#ifndef abs
#define	abs(_a)		__builtin_abs(_a)
#endif

struct ath_hal;
#if 0
extern	u_int32_t __ahdecl ath_hal_getuptime(struct ath_hal *);
#define	OS_GETUPTIME(_ah)	ath_hal_getuptime(_ah)
#endif

/*
 * Byte order/swapping support.
 */
#define	AH_LITTLE_ENDIAN	1234
#define	AH_BIG_ENDIAN		4321

#if AH_BYTE_ORDER == AH_BIG_ENDIAN
/*
 * This could be optimized but since we only use it for
 * a few registers there's little reason to do so.
 */
static inline u_int32_t __bswap32(u_int32_t _x)
{
 	return ((u_int32_t)(
	      (((const u_int8_t *)(&_x))[0]    ) |
	      (((const u_int8_t *)(&_x))[1]<< 8) |
	      (((const u_int8_t *)(&_x))[2]<<16) |
	      (((const u_int8_t *)(&_x))[3]<<24))
	);
}
#else
#define __bswap32(_x)	(_x)
#endif

/*
 * Register read/write; we assume the registers will always
 * be memory-mapped.  Note that register accesses are done
 * using target-specific functions when debugging is enabled
 * (AH_DEBUG) or we are explicitly configured this way.  The
 * latter is used on some platforms where the full i/o space
 * cannot be directly mapped.
 *
 * The hardware registers are native little-endian byte order.
 * Big-endian hosts are handled by enabling hardware byte-swap
 * of register reads and writes at reset.  But the PCI clock
 * domain registers are not byte swapped!  Thus, on big-endian
 * platforms we have to byte-swap thoese registers specifically.
 * Most of this code is collapsed at compile time because the
 * register values are constants.
 */
#if AH_BYTE_ORDER == AH_BIG_ENDIAN

#if ( defined(CONFIG_ARCH_IXP425) || defined(CONFIG_ARCH_IXP4XX)) /* IXP4XX */

#define _OS_REG_WRITE(_ah, _reg, _val) do {                                 \
        if ( (_reg) >= 0x4000 && (_reg) < 0x5000)                           \
            writel((_val), (volatile unsigned long)((_ah)->ah_sh + (_reg)));\
        else                                                                \
            writel(__bswap32(_val), (volatile unsigned long)((_ah)->ah_sh + (_reg))); \
} while (0)
#define _OS_REG_READ(_ah, _reg) \
        (((_reg) >= 0x4000 && (_reg) < 0x5000) ?                            \
            readl((volatile unsigned long)((_ah)->ah_sh + (_reg))) :        \
            __bswap32(readl((volatile unsigned long)((_ah)->ah_sh + (_reg)))))

#else  /* normal case */

#define _OS_REG_WRITE(_ah, _reg, _val) do {                                 \
        if ( (_reg) >= 0x4000 && (_reg) < 0x5000)                           \
                *((volatile u_int32_t *)((_ah)->ah_sh + (_reg))) =          \
                        __bswap32((_val));                                  \
        else                                                                \
                *((volatile u_int32_t *)((_ah)->ah_sh + (_reg))) = (_val);  \
} while (0)
#define _OS_REG_READ(_ah, _reg) \
        (((_reg) >= 0x4000 && (_reg) < 0x5000) ? \
                __bswap32(*((volatile u_int32_t *)((_ah)->ah_sh + (_reg)))) : \
                *((volatile u_int32_t *)((_ah)->ah_sh + (_reg))))

#endif

#else /* AH_LITTLE_ENDIAN */
#define _OS_REG_WRITE(_ah, _reg, _val) do { \
	*((volatile u_int32_t *)((_ah)->ah_sh + (_reg))) = (_val); \
} while (0)
#define _OS_REG_READ(_ah, _reg) \
	*((volatile u_int32_t *)((_ah)->ah_sh + (_reg)))
#endif /* AH_BYTE_ORDER */

#if defined(AH_DEBUG) || defined(AH_REGOPS_FUNC) || defined(AH_DEBUG_ALQ)
/* use functions to do register operations */
#define	OS_REG_WRITE(_ah, _reg, _val)	ath_hal_reg_write(_ah, _reg, _val)
#define	OS_REG_READ(_ah, _reg)		ath_hal_reg_read(_ah, _reg)

extern	void __ahdecl ath_hal_reg_write(struct ath_hal *ah, u_int reg, u_int32_t val);
extern	u_int32_t __ahdecl ath_hal_reg_read(struct ath_hal *ah, u_int reg);
#else
/* inline register operations */
#define OS_REG_WRITE(_ah, _reg, _val)	_OS_REG_WRITE(_ah, _reg, _val)
#define OS_REG_READ(_ah, _reg)		_OS_REG_READ(_ah, _reg)
#endif /* AH_DEBUG || AH_REGFUNC || AH_DEBUG_ALQ */


/*
 * Linux-specific attach/detach methods needed for module reference counting.
 *
 * XXX We can't use HAL_STATUS because the type isn't defined at this
 *     point (circular dependency); we wack the type and patch things
 *     up in the function.
 *
 * NB: These are intentionally not marked __ahdecl since they are
 *     compiled with the default calling convetion and are not called
 *     from within the HAL.
 */
extern	struct ath_hal *hw_hal_attach(u_int16_t devid, HAL_SOFTC, HAL_BUS_TAG, HAL_BUS_HANDLE, void* status);
extern	void hw_hal_detach(struct ath_hal *);

#endif

