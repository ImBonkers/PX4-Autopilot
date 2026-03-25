/****************************************************************************
 * STM32N6 FSBL (First Stage Boot Loader) for PX4
 * 
 * This FSBL initializes the STM32N6 hardware and loads PX4 into SRAM
 * for maximum performance execution.
 ****************************************************************************/

#include <stdint.h>
#include <string.h>
#include "../src/stm32n6_config.h"

/* FSBL Configuration - matches STM32 reference */
#define FSBL_SRAM_BASE      0x34180400  /* AXISRAM2 + offset */
#define FSBL_SIZE           0x3FC00     /* 255KB for FSBL */
#define PX4_SRAM_BASE       0x341C0000  /* PX4 in next SRAM region */
#define PX4_FLASH_BASE      0x90020000  /* PX4 in external flash */
#define PX4_MAX_SIZE        0x200000    /* 2MB max PX4 size */

/* FSBL Header Structure (required by Boot ROM) */
typedef struct {
    uint32_t magic;         /* 0x53544D32 = "STM2" */
    uint32_t version;       /* Version 1 */
    uint32_t size;          /* FSBL size */
    uint32_t entry_offset;  /* Entry point offset */
    uint32_t load_address;  /* Where to load in SRAM */
    uint32_t flags;         /* 0 = unsigned for dev */
    uint32_t reserved[10];  /* Reserved for future use */
} __attribute__((packed)) fsbl_header_t;

/* FSBL Header - Must be at offset 0 */
__attribute__((section(".fsbl_header")))
const fsbl_header_t fsbl_header = {
    .magic = 0x53544D32,        /* "STM2" */
    .version = 1,
    .size = FSBL_SIZE,
    .entry_offset = 0x200,       /* Entry after header */
    .load_address = FSBL_SRAM_BASE,
    .flags = 0x00000000,         /* Unsigned */
};

/* RCC Register Definitions */
#define RCC_CR          (*(volatile uint32_t*)(STM32N6_RCC_BASE + 0x00))
#define RCC_CFGR        (*(volatile uint32_t*)(STM32N6_RCC_BASE + 0x10))
#define RCC_PLL1CFGR    (*(volatile uint32_t*)(STM32N6_RCC_BASE + 0x28))
#define RCC_AHB1ENR     (*(volatile uint32_t*)(STM32N6_RCC_BASE + 0xD8))
#define RCC_AHB2ENR     (*(volatile uint32_t*)(STM32N6_RCC_BASE + 0xDC))
#define RCC_AHB3ENR     (*(volatile uint32_t*)(STM32N6_RCC_BASE + 0xE0))

/* SCB Register Definitions */
#define SCB_CPACR       (*(volatile uint32_t*)0xE000ED88)
#define SCB_VTOR        (*(volatile uint32_t*)0xE000ED08)

/* XSPI Register Definitions */
typedef struct {
    volatile uint32_t CR;
    volatile uint32_t DCR;
    volatile uint32_t SR;
    volatile uint32_t FCR;
    volatile uint32_t DLR;
    volatile uint32_t CCR;
    volatile uint32_t AR;
    volatile uint32_t ABR;
    volatile uint32_t DR;
} XSPI_TypeDef;

#define XSPI1 ((XSPI_TypeDef*)STM32N6_XSPI1_BASE)

/* Helper Functions */
static void configure_system_clocks(void) {
    /* Enable HSE (external crystal) */
    RCC_CR |= (1 << 16);  /* HSEON */
    while (!(RCC_CR & (1 << 17)));  /* Wait for HSERDY */
    
    /* Configure PLL1 for 480MHz */
    /* Assuming HSE = 25MHz: 25MHz / 5 * 96 / 1 = 480MHz */
    RCC_PLL1CFGR = (5 << 0) |      /* DIVM1 = 5 */
                   (96 << 8) |      /* DIVN1 = 96 */
                   (1 << 16) |      /* DIVP1 = 1 */
                   (1 << 24);       /* DIVQ1 = 1 */
    
    /* Enable PLL1 */
    RCC_CR |= (1 << 24);  /* PLL1ON */
    while (!(RCC_CR & (1 << 25)));  /* Wait for PLL1RDY */
    
    /* Switch system clock to PLL1 */
    RCC_CFGR = (RCC_CFGR & ~0x07) | 0x03;  /* SW = PLL1 */
    while ((RCC_CFGR & 0x38) != 0x18);  /* Wait for SWS = PLL1 */
}

static void enable_all_sram(void) {
    /* Enable all AXISRAM banks */
    RCC_AHB1ENR |= (1 << 0) |   /* AXISRAM1EN */
                   (1 << 1) |   /* AXISRAM2EN */
                   (1 << 2) |   /* AXISRAM3EN */
                   (1 << 3) |   /* AXISRAM4EN */
                   (1 << 4) |   /* AXISRAM5EN */
                   (1 << 5);    /* AXISRAM6EN */
    
    /* Enable TCM memories */
    RCC_AHB1ENR |= (1 << 8) |   /* DTCM1EN */
                   (1 << 9);    /* DTCM2EN */
    
    /* Small delay for SRAM to stabilize */
    for (volatile int i = 0; i < 100; i++);
}

static void init_xspi_memory_mapped(void) {
    /* Enable XSPI1 clock */
    RCC_AHB3ENR |= (1 << 14);  /* XSPI1EN */
    
    /* Reset XSPI1 */
    XSPI1->CR = 0;
    
    /* Configure for 32MB external flash */
    XSPI1->DCR = (25 << 16);  /* FSIZE = 25 (2^26 = 64MB) */
    
    /* Configure for Quad SPI memory-mapped mode */
    XSPI1->CCR = (3 << 24) |   /* FMODE = Memory-mapped */
                 (3 << 8) |    /* DMODE = 4-line data */
                 (3 << 10) |   /* ADMODE = 4-line address */
                 (2 << 12);    /* ADSIZE = 24-bit address */
}

/* Peripheral Address Translation Table */
typedef struct {
    uint32_t stm32h7_addr;    /* Address PX4 expects (STM32H7) */
    uint32_t stm32n6_addr;    /* Actual STM32N6 address */
    uint32_t size;            /* Size of peripheral region */
} peripheral_mapping_t;

/* Translation table for critical peripherals */
static const peripheral_mapping_t peripheral_map[] = {
    /* GPIO Ports - Major difference! */
    {0x58020000, 0x46020000, 0x400},  /* GPIOA */
    {0x58020400, 0x46020400, 0x400},  /* GPIOB */
    {0x58020800, 0x46020800, 0x400},  /* GPIOC */
    {0x58020C00, 0x46020C00, 0x400},  /* GPIOD */
    {0x58021000, 0x46021000, 0x400},  /* GPIOE */
    {0x58021400, 0x46021400, 0x400},  /* GPIOF */
    {0x58021800, 0x46021800, 0x400},  /* GPIOG */
    {0x58021C00, 0x46021C00, 0x400},  /* GPIOH */

    /* System peripherals */
    {0x58024400, 0x46028000, 0x400},  /* RCC */
    {0x58024800, 0x46024800, 0x400},  /* PWR */

    /* APB2 peripherals */
    {0x40011000, 0x42001000, 0x400},  /* USART1 */
    {0x40013000, 0x42003000, 0x400},  /* SPI1 */
    {0x40010000, 0x42000000, 0x400},  /* TIM1 */

    /* End marker */
    {0, 0, 0}
};

/* ARM Cortex-M55 MPU Register Definitions */
#define MPU_TYPE        (*(volatile uint32_t*)0xE000ED90)
#define MPU_CTRL        (*(volatile uint32_t*)0xE000ED94)
#define MPU_RNR         (*(volatile uint32_t*)0xE000ED98)
#define MPU_RBAR        (*(volatile uint32_t*)0xE000ED9C)
#define MPU_RLAR        (*(volatile uint32_t*)0xE000EDA0)
#define MPU_MAIR0       (*(volatile uint32_t*)0xE000EDC0)
#define MPU_MAIR1       (*(volatile uint32_t*)0xE000EDC4)

static void setup_mpu_protection(void) {
    /* CRITICAL DISCOVERY: ARM Cortex-M55 MPU cannot perform address translation!
     * MPU only provides:
     * 1. Access control (read/write/execute permissions)
     * 2. Memory attributes (cacheable, shareable, etc.)
     * 3. Region-based protection
     *
     * MPU CANNOT redirect 0x58020000 -> 0x46020000 like we need.
     * We need a different approach for peripheral address compatibility.
     */

    /* For now, configure MPU for basic memory protection */
    uint32_t mpu_type = MPU_TYPE;
    uint32_t num_regions = (mpu_type >> 8) & 0xFF;

    if (num_regions == 0) {
        /* No MPU available */
        return;
    }

    /* Enable MPU with default memory map as background */
    MPU_CTRL = (1 << 2) |  /* PRIVDEFENA - Enable default memory map for privileged access */
               (1 << 0);   /* ENABLE - Enable MPU */

    /* Configure memory attributes for different regions */
    MPU_MAIR0 = (0x00 << 0) |   /* Attr0: Device memory */
                (0x04 << 8) |   /* Attr1: Normal memory, non-cacheable */
                (0xAA << 16) |  /* Attr2: Normal memory, write-through cacheable */
                (0xFF << 24);   /* Attr3: Normal memory, write-back cacheable */
}

static void setup_peripheral_compatibility(void) {
    /* Since MPU cannot do address translation, we need an alternative approach.
     *
     * OPTIONS:
     * 1. Patch PX4 binary in memory to use N6 addresses (runtime patching)
     * 2. Use linker script tricks to redirect peripheral symbols
     * 3. Create wrapper functions that redirect peripheral access
     * 4. Modify NuttX headers to use N6 addresses directly
     *
     * For now, we'll document this as requiring a different approach.
     * The current FSBL will boot PX4, but peripheral access may fail.
     */

    /* TODO: Implement one of the alternative approaches above */
}

static void copy_px4_to_sram(void) {
    uint32_t *src = (uint32_t*)PX4_FLASH_BASE;
    uint32_t *dst = (uint32_t*)PX4_SRAM_BASE;

    /* Simple copy - in production use DMA for speed */
    /* First, read the vector table to determine actual size */
    /* For now, copy 2MB (TODO: read actual size from ELF header) */
    uint32_t copy_size = PX4_MAX_SIZE / 4;  /* Copy as 32-bit words */

    for (uint32_t i = 0; i < copy_size; i++) {
        dst[i] = src[i];
    }
}

/* FSBL Entry Point */
__attribute__((section(".text.fsbl_main")))
__attribute__((naked))
void fsbl_main(void) {
    /* Enable FPU for PX4 */
    SCB_CPACR |= ((3UL << 20) | (3UL << 22));
    __asm volatile("dsb");
    __asm volatile("isb");
    
    /* Configure system clocks to 480MHz */
    configure_system_clocks();
    
    /* Enable all SRAM banks */
    enable_all_sram();
    
    /* Initialize external memory controller */
    init_xspi_memory_mapped();
    
    /* Copy PX4 from external flash to SRAM */
    copy_px4_to_sram();

    /* Setup MPU for memory protection (cannot do address translation) */
    setup_mpu_protection();

    /* CRITICAL: Setup peripheral compatibility workaround */
    setup_peripheral_compatibility();

    /* Set vector table to PX4 location */
    SCB_VTOR = PX4_SRAM_BASE;
    
    /* Get PX4's initial stack pointer and reset handler */
    uint32_t *px4_vectors = (uint32_t*)PX4_SRAM_BASE;
    uint32_t px4_sp = px4_vectors[0];
    uint32_t px4_entry = px4_vectors[1];
    
    /* Jump to PX4 */
    __asm volatile(
        "msr msp, %0\n"
        "bx %1\n"
        :
        : "r"(px4_sp), "r"(px4_entry)
    );
}