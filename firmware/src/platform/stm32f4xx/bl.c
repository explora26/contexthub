/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define SECURITY_API_IN_BOOTLOADER
#include <plat/inc/cmsis.h>
#include <plat/inc/gpio.h>
#include <plat/inc/pwr.h>
#include <plat/inc/bl.h>
#include <variant/inc/variant.h>
#include <alloca.h>
#include <aes.h>
#include <sha2.h>
#include <rsa.h>
#include <variant/inc/variant.h>


struct StmCrc
{
    volatile uint32_t DR;
    volatile uint32_t IDR;
    volatile uint32_t CR;
};

struct StmFlash
{
    volatile uint32_t ACR;
    volatile uint32_t KEYR;
    volatile uint32_t OPTKEYR;
    volatile uint32_t SR;
    volatile uint32_t CR;
    volatile uint32_t OPTCR;
};

struct StmRcc {
    volatile uint32_t CR;
    volatile uint32_t PLLCFGR;
    volatile uint32_t CFGR;
    volatile uint32_t CIR;
    volatile uint32_t AHB1RSTR;
    volatile uint32_t AHB2RSTR;
    volatile uint32_t AHB3RSTR;
    uint8_t unused0[4];
    volatile uint32_t APB1RSTR;
    volatile uint32_t APB2RSTR;
    uint8_t unused1[8];
    volatile uint32_t AHB1ENR;
    volatile uint32_t AHB2ENR;
    volatile uint32_t AHB3ENR;
    uint8_t unused2[4];
    volatile uint32_t APB1ENR;
    volatile uint32_t APB2ENR;
    uint8_t unused3[8];
    volatile uint32_t AHB1LPENR;
    volatile uint32_t AHB2LPENR;
    volatile uint32_t AHB3LPENR;
    uint8_t unused4[4];
    volatile uint32_t APB1LPENR;
    volatile uint32_t APB2LPENR;
    uint8_t unused5[8];
    volatile uint32_t BDCR;
    volatile uint32_t CSR;
    uint8_t unused6[8];
    volatile uint32_t SSCGR;
    volatile uint32_t PLLI2SCFGR;
};

struct StmUdid
{
    volatile uint32_t U_ID[3];
};

struct StmSpi {
    volatile uint32_t CR1;
    volatile uint32_t CR2;
    volatile uint32_t SR;
    volatile uint32_t DR;
    volatile uint32_t CRCPR;
    volatile uint32_t RXCRCR;
    volatile uint32_t TXCRCR;
    volatile uint32_t I2SCFGR;
    volatile uint32_t I2SPR;
};

struct StmGpio {
    volatile uint32_t MODER;
    volatile uint32_t OTYPER;
    volatile uint32_t OSPEEDR;
    volatile uint32_t PUPDR;
    volatile uint32_t IDR;
    volatile uint32_t ODR;
    volatile uint32_t BSRR;
    volatile uint32_t LCKR;
    volatile uint32_t AFR[2];
};

//stm defines
#define BL_MAX_FLASH_CODE   1024


#define FLASH_ACR_LAT(x)    ((x) & FLASH_ACR_LAT_MASK)
#define FLASH_ACR_LAT_MASK  0x0F
#define FLASH_ACR_PRFTEN    0x00000100
#define FLASH_ACR_ICEN      0x00000200
#define FLASH_ACR_DCEN      0x00000400
#define FLASH_ACR_ICRST     0x00000800
#define FLASH_ACR_DCRST     0x00001000

#define FLASH_SR_EOP        0x00000001
#define FLASH_SR_OPERR      0x00000002
#define FLASH_SR_WRPERR     0x00000010
#define FLASH_SR_PGAERR     0x00000020
#define FLASH_SR_PGPERR     0x00000040
#define FLASH_SR_PGSERR     0x00000080
#define FLASH_SR_RDERR      0x00000100
#define FLASH_SR_BSY        0x00010000

#define FLASH_CR_PG         0x00000001
#define FLASH_CR_SER        0x00000002
#define FLASH_CR_MER        0x00000004
#define FLASH_CR_SNB(x)     (((x) << FLASH_CR_SNB_SHIFT) & FLASH_CR_SNB_MASK)
#define FLASH_CR_SNB_MASK   0x00000078
#define FLASH_CR_SNB_SHIFT  3
#define FLASH_CR_PSIZE(x)   (((x) << FLASH_CR_PSIZE_SHIFT) & FLASH_CR_PSIZE_MASK)
#define FLASH_CR_PSIZE_MASK 0x00000300
#define FLASH_CR_PSIZE_SHIFT 8
#define FLASH_CR_PSIZE_8    0x0
#define FLASH_CR_PSIZE_16   0x1
#define FLASH_CR_PSIZE_32   0x2
#define FLASH_CR_PSIZE_64   0x3
#define FLASH_CR_STRT       0x00010000
#define FLASH_CR_EOPIE      0x01000000
#define FLASH_CR_ERRIE      0x02000000
#define FLASH_CR_LOCK       0x80000000

//for comms protocol
#define BL_SYNC_IN                      0x5A
#define BL_ACK                          0x79
#define BL_NAK                          0x1F

#define BL_CMD_GET                      0x00
#define BL_CMD_READ_MEM                 0x11
#define BL_CMD_WRITE_MEM                0x31
#define BL_CMD_ERASE                    0x44
#define BL_CMD_GET_SIZES                0xEE /* our own command. reports: {u32 osSz, u32 sharedSz, u32 eeSz} all in big endian */
#define BL_CMD_UPDATE_FINISHED          0xEF /* our own command. attempts to verify the update -> ACK/NAK. MUST be called after upload to mark it as completed */

#define BL_SHARED_AREA_FAKE_ERASE_BLK   0xFFF0
#define BL_SHARED_AREA_FAKE_ADDR        0x50000000


typedef void (*FlashEraseF)(volatile uint32_t *, uint32_t, volatile uint32_t *);
typedef void (*FlashWriteF)(volatile uint8_t *, uint8_t, volatile uint32_t *);

//linker provides these
extern uint32_t __pubkeys_start[];
extern uint32_t __pubkeys_end[];
extern uint8_t __stack_top[];
extern uint8_t __ram_start[];
extern uint8_t __ram_end[];
extern uint8_t __eedata_start[];
extern uint8_t __eedata_end[];
extern uint8_t __code_start[];
extern uint8_t __code_end[];
extern uint8_t __shared_start[];
extern uint8_t __shared_end[];
extern void __VECTORS();

//make GCC happy
BOOTLOADER void __blEntry(void);

enum BlFlashType
{
    BL_FLASH_BL,
    BL_FLASH_EEDATA,
    BL_FLASH_KERNEL,
    BL_FLASH_SHARED
};

BOOTLOADER_RO
static const struct blFlashTable   // For erase code, we need to know which page a given memory address is in
{
    uint8_t *address;
    uint32_t length;
    uint32_t type;
} mBlFlashTable[] =
#ifndef BL_FLASH_TABLE
{
    { (uint8_t *)(&BL),                      0x04000, BL_FLASH_BL     },
    { (uint8_t *)(__eedata_start),           0x04000, BL_FLASH_EEDATA },
    { (uint8_t *)(__eedata_start + 0x04000), 0x04000, BL_FLASH_EEDATA },
    { (uint8_t *)(__code_start),             0x04000, BL_FLASH_KERNEL },
    { (uint8_t *)(__code_start + 0x04000),   0x10000, BL_FLASH_KERNEL },
    { (uint8_t *)(__shared_start),           0x20000, BL_FLASH_SHARED },
    { (uint8_t *)(__shared_start + 0x20000), 0x20000, BL_FLASH_SHARED },
    { (uint8_t *)(__shared_start + 0x40000), 0x20000, BL_FLASH_SHARED },
};
#else
BL_FLASH_TABLE;
#endif

static const char BOOTLOADER_RO mOsUpdateMagic[] = OS_UPDT_MAGIC;



BOOTLOADER
static inline uint32_t blDisableInts(void)
{
    uint32_t state;

    asm volatile (
        "mrs %0, PRIMASK    \n"
        "cpsid i            \n"
        :"=r"(state)
    );

    return state;
}

BOOTLOADER
static inline void blRestoreInts(uint32_t state)
{
    asm volatile(
        "msr PRIMASK, %0   \n"
        ::"r"((uint32_t)state)
    );
}

BOOTLOADER
static uint32_t blExtApiGetVersion(void)
{
    return BL_VERSION_CUR;
}

BOOTLOADER
static void blExtApiReboot(void)
{
    SCB->AIRCR = 0x05FA0004;
    //we never get here
    while(1);
}

BOOTLOADER
static void blExtApiGetSnum(uint32_t *snum, uint32_t length)
{
    struct StmUdid *reg = (struct StmUdid *)UDID_BASE;
    uint32_t i;

    if (length > 3)
        length = 3;

    for (i = 0; i < length; i++)
        snum[i] = reg->U_ID[i];
}

/*
 * Return the address of the erase code and the length of the code
 *
 * This code needs to run out of ram and not flash since accessing flash
 * while erasing is undefined (best case the processor stalls, worst case
 * it starts executing garbage)
 *
 * This function is used to get a pointer to the actual code that does the
 * erase and polls for completion (so we can copy it to ram) as well as the
 * length of the code (so we know how much space to allocate for it)
 *
 * void FlashEraseF(volatile uint32_t *addr, uint32_t value, volatile uint32_t *status)
 * {
 *     *addr = value;
 *     while (*status & FLASH_SR_BSY) ;
 * }
 */
BOOTLOADER
static void __attribute__((naked)) blGetFlashEraseCode(uint16_t **addr, uint32_t *size)
{
    asm volatile (
        "  push {lr}          \n"
        "  bl   9f            \n"
        "  str  r1, [r0, #0]  \n" // *addr = value
        "1:                   \n"
        "  ldr  r3, [r2, #0]  \n" // r3 = *status
        "  lsls r3, #15       \n" // r3 <<= 15
        "  bmi  1b            \n" // if (r3 < 0) goto 1
        "  bx   lr            \n" // return
        "9:                   \n"
        "  bic  lr, #0x1      \n"
        "  adr  r3, 9b        \n"
        "  sub  r3, lr        \n"
        "  str  lr, [r0]      \n"
        "  str  r3, [r1]      \n"
        "  pop {pc}           \n"
    );
}

/*
 * Return the address of the write code and the length of the code
 *
 * This code needs to run out of ram and not flash since accessing flash
 * while writing to flash is undefined (best case the processor stalls, worst
 * case it starts executing garbage)
 *
 * This function is used to get a pointer to the actual code that does the
 * write and polls for completion (so we can copy it to ram) as well as the
 * length of the code (so we know how much space to allocate for it)
 *
 * void FlashWriteF(volatile uint8_t *addr, uint8_t value, volatile uint32_t *status)
 * {
 *     *addr = value;
 *     while (*status & FLASH_SR_BSY) ;
 * }
 */
BOOTLOADER
static void __attribute__((naked)) blGetFlashWriteCode(uint16_t **addr, uint32_t *size)
{
    asm volatile (
        "  push {lr}          \n"
        "  bl   9f            \n"
        "  strb r1, [r0, #0]  \n" // *addr = value
        "1:                   \n"
        "  ldr  r3, [r2, #0]  \n" // r3 = *status
        "  lsls r3, #15       \n" // r3 <<= 15
        "  bmi  1b            \n" // if (r3 < 0) goto 1
        "  bx   lr            \n" // return
        "9:                   \n"
        "  bic  lr, #0x1      \n"
        "  adr  r3, 9b        \n"
        "  sub  r3, lr        \n"
        "  str  lr, [r0]      \n"
        "  str  r3, [r1]      \n"
        "  pop {pc}           \n"
    );
}

BOOTLOADER
static bool blCompareBytes(const uint8_t *dst, const uint8_t *src, uint32_t length)
{
    while(length--)
        if (*src++ != *dst++)
            return false;

    return true;
}

BOOTLOADER
static void blEraseSectors(uint32_t sector_cnt, uint8_t *erase_mask)
{
    struct StmFlash *flash = (struct StmFlash *)FLASH_BASE;
    uint16_t *code_src, *code;
    uint32_t i, code_length;
    FlashEraseF func;

    blGetFlashEraseCode(&code_src, &code_length);

    if (code_length < BL_MAX_FLASH_CODE) {
        code = (uint16_t *)(((uint32_t)alloca(code_length + 1) + 1) & ~0x1);
        func = (FlashEraseF)((uint8_t *)code+1);

        for (i = 0; i < code_length / sizeof(uint16_t); i++)
            code[i] = code_src[i];

        for (i = 0; i < sector_cnt; i++) {
            if (erase_mask[i]) {
                flash->CR = (flash->CR & ~(FLASH_CR_SNB_MASK)) |
                    FLASH_CR_SNB(i) | FLASH_CR_SER;
                func(&flash->CR, flash->CR | FLASH_CR_STRT, &flash->SR);
                flash->CR &= ~(FLASH_CR_SNB_MASK | FLASH_CR_SER);
            }
        }
    }
}

BOOTLOADER
static void blWriteBytes(uint8_t *dst, const uint8_t *src, uint32_t length)
{
    struct StmFlash *flash = (struct StmFlash *)FLASH_BASE;
    uint16_t *code_src, *code;
    uint32_t i, code_length;
    FlashWriteF func;

    blGetFlashWriteCode(&code_src, &code_length);

    if (code_length < BL_MAX_FLASH_CODE) {
        code = (uint16_t *)(((uint32_t)alloca(code_length+1) + 1) & ~0x1);
        func = (FlashWriteF)((uint8_t *)code+1);

        for (i = 0; i < code_length / sizeof(uint16_t); i++)
            code[i] = code_src[i];

        flash->CR |= FLASH_CR_PG;

        for (i = 0; i < length; i++) {
            if (dst[i] != src[i])
                func(&dst[i], src[i], &flash->SR);
        }

        flash->CR &= ~FLASH_CR_PG;
    }
}

BOOTLOADER
static bool blProgramFlash(uint8_t *dst, const uint8_t *src, uint32_t length, uint32_t key1, uint32_t key2)
{
    struct StmFlash *flash = (struct StmFlash *)FLASH_BASE;
    const uint32_t sector_cnt = sizeof(mBlFlashTable) / sizeof(struct blFlashTable);
    uint32_t acr_cache, cr_cache, offset, i, j = 0, int_state = 0, erase_cnt = 0;
    uint8_t erase_mask[sector_cnt];
    uint8_t *ptr;

    if (((length == 0)) ||
        ((0xFFFFFFFF - (uint32_t)dst) < (length - 1)) ||
        ((dst < mBlFlashTable[0].address)) ||
        ((dst + length) > (mBlFlashTable[sector_cnt-1].address +
                           mBlFlashTable[sector_cnt-1].length))) {
        return false;
    }

    // disable interrupts
    // otherwise an interrupt during flash write/erase will stall the processor
    // until the write/erase completes
    int_state = blDisableInts();

    // figure out which (if any) blocks we have to erase
    for (i = 0; i < sector_cnt; i++)
        erase_mask[i] = 0;

    // compute which flash block we are starting from
    for (i = 0; i < sector_cnt; i++) {
        if (dst >= mBlFlashTable[i].address &&
            dst < (mBlFlashTable[i].address + mBlFlashTable[i].length)) {
            break;
        }
    }

    // now loop through all the flash blocks and see if we have to do any
    // 0 -> 1 transitions of a bit. If so, we must erase that block
    // 1 -> 0 transitions of a bit do not require an erase
    offset = (uint32_t)(dst - mBlFlashTable[i].address);
    ptr = mBlFlashTable[i].address;
    while (j < length && i < sector_cnt) {
        if (offset == mBlFlashTable[i].length) {
            i++;
            offset = 0;
            ptr = mBlFlashTable[i].address;
        }

        if ((ptr[offset] & src[j]) != src[j]) {
            erase_mask[i] = 1;
            erase_cnt++;
            j += mBlFlashTable[i].length - offset;
            offset = mBlFlashTable[i].length;
        } else {
            j++;
            offset++;
        }
    }

    // wait for flash to not be busy (should never be set at this point)
    while (flash->SR & FLASH_SR_BSY);

    cr_cache = flash->CR;

    if (flash->CR & FLASH_CR_LOCK) {
        // unlock flash
        flash->KEYR = key1;
        flash->KEYR = key2;
    }

    if (flash->CR & FLASH_CR_LOCK) {
        // unlock failed, restore interrupts
        blRestoreInts(int_state);

        return false;
    }

    flash->CR = FLASH_CR_PSIZE(FLASH_CR_PSIZE_8);

    acr_cache = flash->ACR;

    // disable and flush data and instruction caches
    flash->ACR &= ~(FLASH_ACR_DCEN | FLASH_ACR_ICEN);
    flash->ACR |= (FLASH_ACR_DCRST | FLASH_ACR_ICRST);

    if (erase_cnt)
        blEraseSectors(sector_cnt, erase_mask);

    blWriteBytes(dst, src, length);

    flash->ACR = acr_cache;
    flash->CR = cr_cache;

    blRestoreInts(int_state);

    return blCompareBytes(dst, src, length);
}

BOOTLOADER
static bool blExtApiProgramTypedArea(uint8_t *dst, const uint8_t *src, uint32_t length, uint32_t key1, uint32_t key2, uint32_t type)
{
    const uint32_t sector_cnt = sizeof(mBlFlashTable) / sizeof(struct blFlashTable);
    uint32_t i;

    for (i = 0; i < sector_cnt; i++) {

        if ((dst >= mBlFlashTable[i].address &&
             dst < (mBlFlashTable[i].address + mBlFlashTable[i].length)) ||
            (dst < mBlFlashTable[i].address &&
             (dst + length > mBlFlashTable[i].address))) {
            if (mBlFlashTable[i].type != type)
                return false;
        }
    }

    return blProgramFlash(dst, src, length, key1, key2);
}

BOOTLOADER
static bool blExtApiProgramSharedArea(uint8_t *dst, const uint8_t *src, uint32_t length, uint32_t key1, uint32_t key2)
{
    return blExtApiProgramTypedArea(dst, src, length, key1, key2, BL_FLASH_SHARED);
}

BOOTLOADER
static bool blExtApiProgramEe(uint8_t *dst, const uint8_t *src, uint32_t length, uint32_t key1, uint32_t key2)
{
    return blExtApiProgramTypedArea(dst, src, length, key1, key2, BL_FLASH_EEDATA);
}

BOOTLOADER
static bool blExtApiEraseSharedArea(uint32_t key1, uint32_t key2)
{
    struct StmFlash *flash = (struct StmFlash *)FLASH_BASE;
    const uint32_t sector_cnt = sizeof(mBlFlashTable) / sizeof(struct blFlashTable);
    uint32_t i, acr_cache, cr_cache, erase_cnt = 0, int_state = 0;
    uint8_t erase_mask[sector_cnt];

    for (i = 0; i < sector_cnt; i++) {
        if (mBlFlashTable[i].type == BL_FLASH_SHARED) {
            erase_mask[i] = 1;
            erase_cnt++;
        } else {
            erase_mask[i] = 0;
        }
    }

    // disable interrupts
    // otherwise an interrupt during flash write/erase will stall the processor
    // until the write/erase completes
    int_state = blDisableInts();

    // wait for flash to not be busy (should never be set at this point)
    while (flash->SR & FLASH_SR_BSY);

    cr_cache = flash->CR;

    if (flash->CR & FLASH_CR_LOCK) {
        // unlock flash
        flash->KEYR = key1;
        flash->KEYR = key2;
    }

    if (flash->CR & FLASH_CR_LOCK) {
        // unlock failed, restore interrupts
        blRestoreInts(int_state);

        return false;
    }

    flash->CR = FLASH_CR_PSIZE(FLASH_CR_PSIZE_8);

    acr_cache = flash->ACR;

    // disable and flush data and instruction caches
    flash->ACR &= ~(FLASH_ACR_DCEN | FLASH_ACR_ICEN);
    flash->ACR |= (FLASH_ACR_DCRST | FLASH_ACR_ICRST);

    if (erase_cnt)
        blEraseSectors(sector_cnt, erase_mask);

    flash->ACR = acr_cache;
    flash->CR = cr_cache;

    // restore interrupts
    blRestoreInts(int_state);

    return true; //we assume erase worked
}

BOOTLOADER
static void blSupirousIntHandler(void)
{
    //BAD!
    blExtApiReboot();
}

BOOTLOADER
static const uint32_t *blExtApiGetRsaKeyInfo(uint32_t *numKeys)
{
    uint32_t numWords = __pubkeys_end - __pubkeys_start;

    if (numWords % RSA_WORDS) // something is wrong
        return NULL;

    *numKeys = numWords / RSA_WORDS;
    return __pubkeys_start;
}

BOOTLOADER
static const uint32_t* blExtApiSigPaddingVerify(const uint32_t *rsaResult)
{
    uint32_t i;

    //all but first and last word of padding MUST have no zero bytes
    for (i = SHA2_HASH_WORDS + 1; i < RSA_WORDS - 1; i++) {
        if (!(uint8_t)(rsaResult[i] >>  0))
            return NULL;
        if (!(uint8_t)(rsaResult[i] >>  8))
            return NULL;
        if (!(uint8_t)(rsaResult[i] >> 16))
            return NULL;
        if (!(uint8_t)(rsaResult[i] >> 24))
            return NULL;
    }

    //first padding word must have all nonzero bytes except low byte
    if ((rsaResult[SHA2_HASH_WORDS] & 0xff) || !(rsaResult[SHA2_HASH_WORDS] & 0xff00) || !(rsaResult[SHA2_HASH_WORDS] & 0xff0000) || !(rsaResult[SHA2_HASH_WORDS] & 0xff000000))
        return NULL;

    //last padding word must have 0x0002 in top 16 bits and nonzero random bytes in lower bytes
    if ((rsaResult[RSA_WORDS - 1] >> 16) != 2)
        return NULL;
    if (!(rsaResult[RSA_WORDS - 1] & 0xff00) || !(rsaResult[RSA_WORDS - 1] & 0xff))
        return NULL;

    return rsaResult;
}

const struct BlVecTable __attribute__((section(".blvec"))) __BL_VECTORS =
{
    /* cortex */
    .blStackTop = (uint32_t)&__stack_top,
    .blEntry = &__blEntry,
    .blNmiHandler = &blSupirousIntHandler,
    .blMmuFaultHandler = &blSupirousIntHandler,
    .blBusFaultHandler = &blSupirousIntHandler,
    .blUsageFaultHandler = &blSupirousIntHandler,

    /* api */
    .blGetVersion = &blExtApiGetVersion,
    .blReboot = &blExtApiReboot,
    .blGetSnum = &blExtApiGetSnum,
    .blProgramShared = &blExtApiProgramSharedArea,
    .blEraseShared = &blExtApiEraseSharedArea,
    .blProgramEe = &blExtApiProgramEe,
    .blGetPubKeysInfo = &blExtApiGetRsaKeyInfo,
    .blRsaPubOpIterative = &_rsaPubOpIterative,
    .blSha2init = &_sha2init,
    .blSha2processBytes = &_sha2processBytes,
    .blSha2finish = &_sha2finish,
    .blAesInitForEncr = &_aesInitForEncr,
    .blAesInitForDecr = &_aesInitForDecr,
    .blAesEncr = &_aesEncr,
    .blAesDecr = &_aesDecr,
    .blAesCbcInitForEncr = &_aesCbcInitForEncr,
    .blAesCbcInitForDecr = &_aesCbcInitForDecr,
    .blAesCbcEncr = &_aesCbcEncr,
    .blAesCbcDecr = &_aesCbcDecr,
    .blSigPaddingVerify = &blExtApiSigPaddingVerify,
};

BOOTLOADER
static void blApplyVerifiedUpdate(void) //only called if an update has been found to exist and be valid, signed, etc!
{
    const struct OsUpdateHdr *updt = (const struct OsUpdateHdr*)__shared_start;

    //copy shared to code, and if successful, erase shared area
    if (blProgramFlash(__code_start, (const uint8_t*)updt + 1, updt->size, BL_FLASH_KEY1, BL_FLASH_KEY2))
        (void)blExtApiEraseSharedArea(BL_FLASH_KEY1, BL_FLASH_KEY2);
}

BOOTLOADER
static void blUpdateMark(uint32_t from, uint32_t to)
{
    struct OsUpdateHdr *hdr = (struct OsUpdateHdr*)__shared_start;
    uint8_t dstVal = to;

    if (hdr->marker != from)
        return;

    (void)blProgramFlash(&hdr->marker, &dstVal, 1, BL_FLASH_KEY1, BL_FLASH_KEY2);
}

BOOTLOADER
static bool blUpdateVerify(void)
{
    const uint32_t *rsaKey, *osSigHash, *osSigPubkey, *ourHash, *rsaResult, *expectedHash = NULL;
    const struct OsUpdateHdr *hdr = (const struct OsUpdateHdr*)__shared_start;
    uint32_t i, j, numRsaKeys = 0, rsaStateVar1, rsaStateVar2, rsaStep = 0;
    const uint8_t *updateBinaryData;
    bool isValid = false;
    struct Sha2state sha;
    struct RsaState rsa;


    //some basic sanity checking
    for (i = 0; i < sizeof(hdr->magic); i++) {
        if (hdr->magic[i] != mOsUpdateMagic[i])
            break;
    }
    if (i != sizeof(hdr->magic)) {
        //magic value is wrong -> DO NOTHING (shared area might contain something that is not an update but is useful & valid)!
        return false;
    }

    if (hdr->marker == OS_UPDT_MARKER_INVALID) {
        //it's already been checked and found invalid
        return false;
    }

    if (hdr->marker == OS_UPDT_MARKER_VERIFIED) {
        //it's been verified already
        return true;
    }

    if (hdr->marker != OS_UPDT_MARKER_DOWNLOADED) {
        //it's not a fully downloaded update or is not an update
        goto fail;
    }

    if (hdr->size & 3) {
        //updates are always multiples of 4 bytes in size
        goto fail;
    }

    if (hdr->size > __shared_end - __shared_start) {
        //udpate would not fit in shared area if it were real
        goto fail;
    }

    if (hdr->size > __code_end - __code_start) {
        //udpate would not fit in code area
        goto fail;
    }

    if (__shared_end - __shared_start - hdr->size < sizeof(struct OsUpdateHdr) + 2 * RSA_WORDS) {
        //udpate + header + sig would not fit in shared area if it were real
        goto fail;
    }

    //get pointers
    updateBinaryData = (const uint8_t*)(hdr + 1);
    osSigHash = (const uint32_t*)updateBinaryData + hdr->size;
    osSigPubkey = osSigHash + RSA_WORDS;

    //hash the update
    _sha2init(&sha);
    _sha2processBytes(&sha, hdr, sizeof(const struct OsUpdateHdr) + hdr->size);
    ourHash = _sha2finish(&sha);

    //make sure the pub key is known
    for (i = 0, rsaKey = blExtApiGetRsaKeyInfo(&numRsaKeys); i < numRsaKeys; i++, rsaKey += RSA_WORDS) {
        for (j = 0; j < RSA_WORDS; j++) {
            if (rsaKey[j] != osSigPubkey[j])
                break;
        }
        if (j == RSA_WORDS) //it matches
            break;
    }

    if (i != numRsaKeys) {
        //signed with an unknown key -> fail
        goto fail;
    }

    //decode sig using pubkey
    do {
        rsaResult = _rsaPubOpIterative(&rsa, osSigHash, osSigPubkey, &rsaStateVar1, &rsaStateVar2, &rsaStep);
    } while (rsaStep);

    if (!rsaResult) {
        //decode fails -> invalid sig
        goto fail;
    }

    //verify padding
    expectedHash = blExtApiSigPaddingVerify(rsaResult);

    if (!expectedHash) {
        //padding check fails -> invalid sig
        goto fail;
    }

    //verify hash match
    for (i = 0; i < SHA2_HASH_WORDS; i++) {
        if (expectedHash[i] != ourHash[i])
            break;
    }

    if (i != SHA2_HASH_WORDS) {
        //hash does not match -> data tampered with
        goto fail;
    }

    //it is valid
    isValid = true;

fail:
    //mark it appropriately
    blUpdateMark(OS_UPDT_MARKER_DOWNLOADED, isValid ? OS_UPDT_MARKER_VERIFIED : OS_UPDT_MARKER_INVALID);
    return isValid;
}

BOOTLOADER
static uint8_t blSpiLoaderRxByte(struct StmSpi *spi)
{
    while (!(spi->SR & 1));
    return spi->DR;
}

BOOTLOADER
static void blSpiLoaderTxByte(struct StmSpi *spi, uint32_t val)
{
    while (!(spi->SR & 2));
    spi->DR = val;
}

BOOTLOADER
static void blSpiLoaderTxBytes(struct StmSpi *spi, const void *data, uint32_t len)
{
    const uint8_t *buf = (const uint8_t*)data;

    blSpiLoaderTxByte(spi, len - 1);
    while (len--)
        blSpiLoaderTxByte(spi, *buf++);
}

BOOTLOADER
static bool blSpiLoaderSendAck(struct StmSpi *spi, bool ack)
{
    blSpiLoaderTxByte(spi, 0);
    blSpiLoaderTxByte(spi, ack ? BL_ACK : BL_NAK);
    return blSpiLoaderRxByte(spi) == BL_ACK;
}

BOOTLOADER
static void blSpiLoader(void)
{
    const uint32_t intInPin = SH_INT_WAKEUP - GPIO_PA(0);
    struct StmGpio *gpioa = (struct StmGpio*)GPIOA_BASE;
    struct StmSpi *spi = (struct StmSpi*)SPI1_BASE;
    struct StmRcc *rcc = (struct StmRcc*)RCC_BASE;
    uint32_t oldApb2State, oldAhb1State, nRetries;
    bool seenErase = false;

    if (SH_INT_WAKEUP < GPIO_PA(0) || SH_INT_WAKEUP > GPIO_PA(15)) {

        //link time assert :)
        extern void ThisIsAnError_BlIntPinNotInGpioA(void);
        ThisIsAnError_BlIntPinNotInGpioA();
    }

    //SPI & GPIOA on
    oldApb2State = rcc->APB2ENR;
    oldAhb1State = rcc->AHB1ENR;
    rcc->APB2ENR |= PERIPH_APB2_SPI1;
    rcc->AHB1ENR |= PERIPH_AHB1_GPIOA;

    //reset units
    rcc->APB2RSTR |= PERIPH_APB2_SPI1;
    rcc->AHB1RSTR |= PERIPH_AHB1_GPIOA;
    rcc->APB2RSTR &=~ PERIPH_APB2_SPI1;
    rcc->AHB1RSTR &=~ PERIPH_AHB1_GPIOA;

    //configure GPIOA for SPI A4..A7 for SPI use (function 5), int pin as not func, high speed, no pullups, not open drain, proper directions
    gpioa->AFR[0] = (gpioa->AFR[0] & 0x0000ffff & ~(0x0f << (intInPin * 4))) | 0x55550000;
    gpioa->OSPEEDR |= 0x0000ff00 | (3 << (intInPin * 2));
    gpioa->PUPDR &=~ (0x0000ff00 | (3 << (intInPin * 2)));
    gpioa->OTYPER &=~ (0x00f0 | (1 << intInPin));
    gpioa->MODER = (gpioa->MODER & 0xffff00ff & ~(0x03 << (intInPin * 2))) | 0x0000aa00;

    //if int pin is not low, do not bother any further
    if (gpioa->IDR & (1 << intInPin)) {

        //config SPI
        spi->CR1 = 0x00000040; //spi is on, configured same as bootloader would
        spi->CR2 = 0x00000000; //spi is on, configured same as bootloader would

        //wait for sync
        for (nRetries = 10000; nRetries; nRetries--) {
            if (spi->SR & 1) {
                if (spi->DR == BL_SYNC_IN)
                    break;
                (void)spi->SR; //re-read to clear overlfow condition (if any)
            }
        }

        //if we saw a sync, do the bootloader thing
        if (nRetries) {
            static const uint8_t BOOTLOADER_RO supportedCmds[] = {BL_CMD_GET, BL_CMD_READ_MEM, BL_CMD_WRITE_MEM, BL_CMD_ERASE, BL_CMD_GET_SIZES, BL_CMD_UPDATE_FINISHED};
            uint32_t allSizes[] = {__builtin_bswap32(__code_end - __code_start), __builtin_bswap32(__shared_end - __shared_start), __builtin_bswap32(__eedata_end - __eedata_start)};
            bool ack = true;  //we ack the sync

            //loop forever listening to commands
            while (1) {
                uint32_t sync, cmd, cmdNot, addr = 0, len, checksum = 0, i;
                uint8_t data[256];

                //send ack or NAK for last thing
                if (!blSpiLoaderSendAck(spi, ack))
                    goto out;

                while ((sync = blSpiLoaderRxByte(spi)) != BL_SYNC_IN);
                cmd = blSpiLoaderRxByte(spi);
                cmdNot = blSpiLoaderRxByte(spi);

                ack = false;
                if (sync == BL_SYNC_IN && (cmd ^ cmdNot) == 0xff) switch (cmd) {
                case BL_CMD_GET:

                    //ACK the command
                    (void)blSpiLoaderSendAck(spi, true);

                    blSpiLoaderTxBytes(spi, supportedCmds, sizeof(supportedCmds));
                    ack = true;
                    break;

                case BL_CMD_READ_MEM:
                    if (!seenErase)  //no reading till we erase the shared area (this way we do not leak encrypted apps' plaintexts)
                        break;

                    //ACK the command
                    (void)blSpiLoaderSendAck(spi, true);

                    //get address
                    for (i = 0; i < 4; i++) {
                        uint32_t byte = blSpiLoaderRxByte(spi);
                        checksum ^= byte;
                        addr = (addr << 8) + byte;
                    }

                    //reject addresses outside of our fake area or on invalid checksum
                    if (blSpiLoaderRxByte(spi) != checksum || addr < BL_SHARED_AREA_FAKE_ADDR || addr - BL_SHARED_AREA_FAKE_ADDR > __shared_end - __shared_start)
                       break;

                    //ack the address
                    (void)blSpiLoaderSendAck(spi, true);

                    //get the length
                    len = blSpiLoaderRxByte(spi);

                    //reject invalid checksum
                    if (blSpiLoaderRxByte(spi) != (uint8_t)~len || addr + len - BL_SHARED_AREA_FAKE_ADDR > __shared_end - __shared_start)
                       break;

                    len++;

                    //reject reads past the end of the shared area
                    if (addr + len - BL_SHARED_AREA_FAKE_ADDR > __shared_end - __shared_start)
                       break;

                    //ack the length
                    (void)blSpiLoaderSendAck(spi, true);

                    //read the data & send it
                    blSpiLoaderTxBytes(spi, __shared_start + addr - BL_SHARED_AREA_FAKE_ADDR, len);
                    ack = true;
                    break;

                case BL_CMD_WRITE_MEM:
                    if (!seenErase)  //no writing till we erase the shared area (this way we do not purposefully modify encrypted apps' plaintexts in a nefarious fashion)
                        break;

                    //ACK the command
                    (void)blSpiLoaderSendAck(spi, true);

                    //get address
                    for (i = 0; i < 4; i++) {
                        uint32_t byte = blSpiLoaderRxByte(spi);
                        checksum ^= byte;
                        addr = (addr << 8) + byte;
                    }

                    //reject addresses outside of our fake area or on invalid checksum
                    if (blSpiLoaderRxByte(spi) != checksum || addr < BL_SHARED_AREA_FAKE_ADDR || addr - BL_SHARED_AREA_FAKE_ADDR > __shared_end - __shared_start)
                       break;

                    //ack the address
                    (void)blSpiLoaderSendAck(spi, true);

                    //get the length
                    checksum = len = blSpiLoaderRxByte(spi);
                    len++;

                    //get bytes
                    for (i = 0; i < len; i++) {
                        uint32_t byte = blSpiLoaderRxByte(spi);
                        checksum ^= byte;
                        data[i] = byte;
                    }

                    //reject writes that takes out outside fo shared area or invalid checksums
                    if (blSpiLoaderRxByte(spi) != checksum || addr + len - BL_SHARED_AREA_FAKE_ADDR > __shared_end - __shared_start)
                       break;

                    //a write anywhere where the OS header will be must start at 0
                    if (addr && addr < sizeof(struct OsUpdateHdr))
                        break;

                    //a write startnig at zero must be big enough to contain a full OS update header
                    if (!addr) {
                        const struct OsUpdateHdr *hdr = (const struct OsUpdateHdr*)data;

                        //verify it is at least as big as the header
                        if (len < sizeof(struct OsUpdateHdr))
                            break;

                        //check for magic
                        for (i = 0; i < sizeof(hdr->magic) && hdr->magic[i] == mOsUpdateMagic[i]; i++);

                        //verify magic check passed & marker is properly set to inprogress
                        if (i != sizeof(hdr->magic) || hdr->marker != OS_UPDT_MARKER_INPROGRESS)
                            break;
                    }

                    //do it
                    ack = blProgramFlash(__shared_start + addr - BL_SHARED_AREA_FAKE_ADDR, data, len, BL_FLASH_KEY1, BL_FLASH_KEY2);
                    break;

                case BL_CMD_ERASE:

                    //ACK the command
                    (void)blSpiLoaderSendAck(spi, true);

                    //get address
                    for (i = 0; i < 2; i++) {
                        uint32_t byte = blSpiLoaderRxByte(spi);
                        checksum ^= byte;
                        addr = (addr << 8) + byte;
                    }

                    //reject addresses that are not our magic address or on invalid checksum
                    if (blSpiLoaderRxByte(spi) != checksum || addr != BL_SHARED_AREA_FAKE_ERASE_BLK)
                        break;

                    //do it
                    ack = !blExtApiEraseSharedArea(BL_FLASH_KEY1, BL_FLASH_KEY2);
                    if (ack)
                        seenErase = true;
                    break;

                case BL_CMD_GET_SIZES:

                    //ACK the command
                    (void)blSpiLoaderSendAck(spi, true);

                    blSpiLoaderTxBytes(spi, allSizes, sizeof(allSizes));
                    break;

                case BL_CMD_UPDATE_FINISHED:

                    blUpdateMark(OS_UPDT_MARKER_INPROGRESS, OS_UPDT_MARKER_DOWNLOADED);
                    ack = blUpdateVerify();
                    break;
                }
            }
        }
    }

out:
    //reset units & return APB2 & AHB1 to initial state
    rcc->APB2RSTR |= PERIPH_APB2_SPI1;
    rcc->AHB1RSTR |= PERIPH_AHB1_GPIOA;
    rcc->APB2RSTR &=~ PERIPH_APB2_SPI1;
    rcc->AHB1RSTR &=~ PERIPH_AHB1_GPIOA;
    rcc->APB2ENR = oldApb2State;
    rcc->AHB1ENR = oldAhb1State;
}

BOOTLOADER
void __blEntry(void)
{
    uint32_t appBase = ((uint32_t)&__VECTORS) & ~1;

    //make sure we're the vector table and no ints happen (BL does not use them)
    blDisableInts();
    SCB->VTOR = (uint32_t)&BL;

    //enter SPI loader if requested
    blSpiLoader();

    //try to run main app
    if (blUpdateVerify())
        blApplyVerifiedUpdate();

    //call main app with ints off
    blDisableInts();
    SCB->VTOR = appBase;
    asm volatile(
        "LDR SP, [%0, #0]    \n"
        "LDR PC, [%0, #4]    \n"
        :
        :"r"(appBase)
        :"memory", "cc"
    );

    //we should never return here
    while(1);
}














