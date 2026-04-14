# **Phase 1: Hardware Initialization & Environment Setup**

The Samsung S5PV210 System-on-Chip (SoC), built upon the ARM Cortex-A8 architecture, presents a highly capable but intricately complex environment for bare-metal application development. Executing a sophisticated graphical user interface (GUI) framework such as the Light and Versatile Graphics Library (LVGL) in the absolute absence of an operating system requires the meticulous initialization of the underlying hardware subsystems. Because hardware acceleration via the integrated PowerVR SGX535/540 GPU is strictly excluded from this implementation paradigm, the system must rely entirely on CPU-driven software rendering.1 Consequently, the initialization of the core clock frequencies, memory controllers, Memory Management Unit (MMU), and cache subsystems must be aggressively optimized to prevent rendering bottlenecks and ensure a fluid user experience.

### **The Bare-Metal Boot Sequence**

Understanding the execution flow prior to the instantiation of the C runtime environment is critical. The S5PV210 boot architecture relies on a multi-stage process dictated by the hardware's internal Read-Only Memory (iROM).3 Upon power-on reset, the Cortex-A8 core fetches its first instruction from the 64 KB iROM (designated as Bootloader 0, or BL0).3 The primary responsibilities of BL0 include disabling the hardware watchdog timer, initializing the instruction cache, establishing a rudimentary stack within the 96 KB internal SRAM (iRAM), and interrogating the Operating Mode (OM) hardware pins to determine the primary boot medium (e.g., NAND flash, SD/MMC, or UART).4

Once the boot medium is identified, BL0 loads a small user-provided primary bootloader (BL1, limited to 16 KB) into the iRAM and branches execution to it.7 In a bare-metal paradigm, the BL1 payload must perform the critical initialization of the system clocks and the external Dynamic Random Access Memory (DRAM) controller. Only after the external SDRAM is stable can the complete bare-metal application payload (analogous to BL2) be loaded into the vast SDRAM space for execution.5

### **System Clock Configuration (APLL, MPLL, EPLL, and VPLL)**

The S5PV210 clock generation logic utilizes multiple Phase-Locked Loops (PLLs) to synthesize high-frequency operational clocks from a standard 24 MHz external crystal oscillator (![][image1]).8 The core subsystems rely heavily on the Advanced PLL (APLL) and the Main PLL (MPLL).9 The APLL primarily drives the ARM Cortex-A8 core (MSYS domain), while the MPLL drives the system bus, memory controller, and display subsystems (DSYS and PSYS domains).8

The mathematical synthesis of the PLL output frequency (![][image2]) is defined by the following equation:

![][image3]  
Where ![][image4], ![][image5], and ![][image6] represent the multiplier, divider, and scaler configuration values respectively, programmed into the associated PLL control registers.

To support the heavy computational load of software rendering for LVGL, the ARM Cortex-A8 core must operate at its maximum stable frequency of 1 GHz, while the memory bus must be driven at 200 MHz.1 The bare-metal clock initialization sequence dictates that the PLLs must be bypassed before modification to prevent transient clock instability from halting the core. The initialization sequence is executed via direct memory-mapped register manipulation:

1. **Bypass PLLs**: The CLK\_SRC0 register (located at 0xE010\_0200) is modified to route the raw 24 MHz oscillator signal directly to the system components, bypassing the APLL and MPLL during reconfiguration.8  
2. **Configure Lock Times**: The APLL\_LOCK and MPLL\_LOCK registers define the delay required for the analog PLL circuitry to stabilize after frequency modifications. These are set to their maximum hexadecimal value (0x0000FFFF) to ensure absolute stability before the clock switch is finalized.8  
3. **Set Division Ratios**: The CLK\_DIV0 register manages the ratios between the core clock (ARMCLK), the high-speed bus (HCLK), and the peripheral bus (PCLK). For a 1 GHz core and 200 MHz HCLK, the register is programmed with the hexadecimal value 0x14131440.8 This establishes the necessary synchronous dividers across the MSYS, DSYS, and PSYS domains.  
4. **Program PLL Multipliers**: The APLL\_CON0 register (0xE010\_0100) is programmed to achieve 1 GHz. Using the formula above with a 24 MHz input, the values ![][image7], ![][image8], and ![][image9] yield exactly 1000 MHz. The MPLL\_CON register (0xE010\_0108) is similarly programmed to achieve 667 MHz, which is subsequently divided down to yield the 200 MHz bus clocks.11  
5. **Re-engage PLLs**: Following the lock time delay, the CLK\_SRC0 register is updated to 0x10001111 to finalize the switch from the bypass oscillator source to the newly configured high-frequency PLL outputs.8

| Clock Domain | Target Frequency | Hardware Subsystems Driven |
| :---- | :---- | :---- |
| **ARMCLK (MSYS)** | 1000 MHz | Cortex-A8 Core, NEON SIMD Coprocessor, L1/L2 Caches |
| **HCLK\_MSYS** | 200 MHz | Internal High-Speed Matrix, System Memory |
| **HCLK\_DSYS** | 166 MHz | FIMD (LCD Controller), Display DMA Engines |
| **PCLK\_PSYS** | 66 MHz | PWM Timers, UART, I2C, SPI, GPIO |

*Table 1: Optimal Clock Domain Frequencies for Bare-Metal LVGL Execution.*

### **Dynamic Random Access Memory (DRAM) Controller Initialization**

The study210 development board features 512 MB of DDR2 SDRAM.1 Because the LVGL framework requires substantial dynamic memory for partial draw buffers, widget state management, and the massive primary physical framebuffer required by the FIMD display controller, the DRAM controller (DMC0 and DMC1) must be fully initialized prior to executing any C-language environment setup code.

The physical address space for the SDRAM on the S5PV210 architecture begins at 0x2000\_0000.13 Bare-metal initialization of the DMC requires establishing the physical memory mapping, defining the Column Address Strobe (CAS) latency, setting the burst length, and executing the strictly timed DDR2-specific power-up sequence. This sequence involves issuing a series of NOP (No Operation), Precharge All, Auto Refresh, and Load Mode Register (LMR) commands directly to the SDRAM chips via the memory controller's command registers. Once the memory controller confirms the SDRAM is accessible and stable, the C-language execution environment—comprising the stack pointer initialization and the zeroing of the .bss segment—can be safely established in the external memory.

### **Memory Management Unit (MMU) and Cache Coherency**

The single most critical factor in achieving acceptable software rendering frame rates on an ARM Cortex-A8 processor is the enablement of the L1 (32 KB Instruction, 32 KB Data) and L2 (512 KB) caches.1 By default, the Cortex-A8 boots with all caches disabled. Operating LVGL in a cache-disabled state forces the processor to fetch and write every single pixel directly to the DDR2 memory, resulting in rendering times measured in seconds per frame due to the extreme latency of raw memory access during complex alpha-blending operations.

Enabling the caches requires the initialization of the Memory Management Unit (MMU).14 In a bare-metal paradigm, where virtual memory abstraction is unnecessary, a flat translation table (identity mapping) is implemented. In this model, the virtual address requested by the CPU directly maps to the identical physical address on the hardware bus.

A Level 1 Translation Table, occupying 16 KB of contiguous, 16 KB-aligned physical memory, must be constructed in the SDRAM. The 32-bit address space is divided into 4096 sections of 1 MB each. The memory attributes for each 1 MB section are defined using a Section Descriptor format, which dictates the cacheability and bufferability of that specific memory region.

| Memory Region | Physical Address Range | Cacheable (C) | Bufferable (B) | Execute Never (XN) |
| :---- | :---- | :---- | :---- | :---- |
| **Internal SRAM** | 0xD002\_0000 \- 0xD003\_7FFF | 1 | 1 | 0 |
| **SFR (Peripherals)** | 0xE000\_0000 \- 0xFFFF\_FFFF | 0 | 0 | 1 |
| **SDRAM (General)** | 0x2000\_0000 \- 0x3FFF\_FFFF | 1 | 1 | 0 |
| **Framebuffer** | Configurable (e.g., 0x3E00\_0000) | 0 | 1 | 1 |

*Table 2: Optimal MMU Section Descriptor Attributes for the S5PV210 Architecture.*

As demonstrated in Table 2, the Special Function Register (SFR) region—which houses the control registers for the FIMD, PWM Timers, and VIC—must be strictly defined as Non-Cacheable and Non-Bufferable.9 This ensures that hardware register modifications executed by the CPU are immediate, strongly ordered, and immune to cache line evictions.

Conversely, the general SDRAM region—where the LVGL internal draw buffers (lv\_disp\_draw\_buf\_t), the application code, and the memory heap reside—must be strictly defined as Cacheable and Bufferable. This configuration allows the CPU to execute rapid pixel-level blending operations entirely within the ultra-fast L2 cache, vastly accelerating software rendering.

The physical memory allocated for the final FIMD display controller framebuffer presents a unique cache coherency challenge in a bare-metal environment. If the framebuffer is mapped as Cacheable, pixels written by the CPU during a flush operation will remain trapped in the L1 or L2 cache. The FIMD hardware controller, which reads directly from the physical SDRAM via Direct Memory Access (DMA), will fetch stale, unrendered data, resulting in severe visual artifacts or a persistently blank screen.15 While manual cache-flushing instructions (using Coprocessor 15 operations) can be issued at the end of every LVGL rendering pass to force the data to RAM, the optimal bare-metal solution is to map the specific memory boundary containing the framebuffer as Non-Cacheable but Bufferable (often referred to as Write-Combining memory). This specific configuration eliminates the need for manual cache-clean overhead while utilizing the processor's write buffers to efficiently burst sequential pixel data out to the SDRAM.

The MMU is subsequently engaged by writing the base physical address of the newly constructed translation table to the Translation Table Base Register 0 (TTBR0), configuring the Domain Access Control Register (DACR) to enable client access, and finally enabling the MMU, Data Cache, and Instruction Cache bits in the System Control Register (SCTLR) via inline assembly CP15 instructions.

# **Phase 2: LVGL Source Tree Integration**

With the physical hardware primed, the PLLs locked at maximum frequency, and the C execution environment running within a cache-enabled SDRAM space, the integration of the LVGL source tree constitutes the next architectural phase.

LVGL is fundamentally operating-system-agnostic and is explicitly designed to support bare-metal deployments on embedded microprocessors.15 The strict separation between the hardware abstraction layer (HAL) and the LVGL API relies on the principle of inversion of control; the bare-metal application registers hardware-specific function pointers, which the LVGL core invokes only when it requires physical display updates or asynchronous input data.

### **Build System Integration and Compiler Directives**

The standard LVGL repository includes a highly structured directory hierarchy. For a bare-metal build targeting the Cortex-A8, the src/ directory—containing the core framework, widgets, styling engines, and drawing algorithms—must be compiled without any reliance on operating system dependencies.16 This means avoiding POSIX headers, threading libraries, or standard I/O streams. All .c files within the src/ hierarchy must be recursively added to the bare-metal Makefile or CMakeLists.txt. Directories such as env\_support/ or high-level examples/ should be excluded initially to minimize the compilation footprint and prevent missing-dependency failures during the linking phase.

The compiler flags utilized by the ARM GCC toolchain must be meticulously tuned to leverage the specific architectural capabilities of the S5PV210. The compilation flags must include \-mcpu=cortex-a8, \-mfpu=neon, and \-mfloat-abi=hard. These flags instruct the compiler to generate machine code tailored for the Cortex-A8 pipeline and authorize the use of hardware floating-point registers, which heavily influences the performance of complex GUI calculations.

### **Core Framework Configuration (lv\_conf.h)**

The configuration file lv\_conf.h acts as the master control nexus, governing the memory architecture, feature set, and rendering engine behavior of the entire library.17 The following configurations are mandatory for an optimized bare-metal port relying on software rendering:

1. **Color Depth (LV\_COLOR\_DEPTH)**: The S5PV210 FIMD controller supports various bit depths, ranging from 8-bit palletized to 24-bit Non-Palletized Color TFT modes.10 However, 16 (RGB565) or 32 (ARGB8888) are the most practical choices for modern GUIs.17 For software rendering on a 1 GHz CPU, configuring LV\_COLOR\_DEPTH to 16 is highly recommended. Utilizing the 16-bit RGB565 format halves the memory bandwidth required per frame compared to 32-bit color. Given that a single 800x480 frame requires 768 KB of memory in 16-bit mode versus 1.53 MB in 32-bit mode, this reduction significantly alleviates the burden on the SDRAM controller and demonstrably increases the overall software rendering frame rate.18  
2. **Memory Management (LV\_MEM\_CUSTOM)**: Because a strict bare-metal environment lacks a standard C library (stdlib) implementation of dynamic memory allocation (malloc() and free()), the most robust approach is to configure LVGL to utilize its internal TLSF (Two-Level Segregated Fit) memory allocator.17 Setting LV\_MEM\_CUSTOM to 0 and defining LV\_MEM\_SIZE to a suitably large value (e.g., (1024 \* 1024 \* 8\) for 8 MB) directs LVGL to instantiate a massive static array within the .bss section of the compiled binary. LVGL will internally manage heap fragmentation, object allocation, and memory freeing within this bounded array, preventing catastrophic memory leaks or the requirement to engineer a custom, thread-safe sbrk() system call.  
3. **Software Rendering Enforcement**: The S5PV210's PowerVR SGX540/535 GPU relies on complex, proprietary user-space binaries and kernel-space drivers that are impossible to execute in a pure bare-metal environment.1 Therefore, the LVGL drawing engine must be explicitly instructed to bypass any GPU abstraction. The macro LV\_USE\_DRAW\_SW must be enabled, ensuring that all vector graphics, font rendering, and image decoding are routed through the CPU matrix.

### **ARM NEON SIMD Optimization**

A crucial architectural advantage of the Cortex-A8 core is its integration of the Advanced SIMD (Single Instruction, Multiple Data) extension, commercially branded as NEON.1 The NEON unit provides a comprehensive bank of 128-bit wide vector registers capable of executing parallel mathematical operations across multiple data points simultaneously.

Software rendering involves exceptionally intensive mathematical workloads: matrix transformations, alpha channel blending, and massive memory copying operations. LVGL natively supports NEON assembly routines to dramatically accelerate these specific bottlenecks.19 By defining LV\_USE\_DRAW\_SW\_ASM\_NEON to 1 in lv\_conf.h, LVGL will bypass standard C-based loops for image blending and pixel filling, substituting them with highly optimized, handwritten inline ARM assembly targeting the NEON coprocessor.

This single configuration parameter bridges the performance gap caused by the absence of the PowerVR GPU. For example, blending a transparent bounding box over a background image can be executed on 8 pixels simultaneously using a single NEON instruction, rather than processing each pixel sequentially in a standard C for loop. This vectorization enables smooth 60 FPS transition animations and rapid widget updates even on high-resolution display panels.

# **Phase 3: Display and Input Driver Registration**

The physical abstraction boundary between the Samsung hardware and the LVGL logical environment is established during the driver registration phase. This requires directly configuring the Fully Interactive Mobile Display (FIMD) controller and mapping its physical memory requirements to the LVGL lv\_disp\_drv\_t structures.

### **Hardware Abstraction Layer (HAL): FIMD Initialization**

The S5PV210 FIMD peripheral is memory-mapped to the base address 0xF900\_0000.10 Initialization of the LCD controller strictly bypasses Linux /dev/fb0 abstractions or DRM/KMS subsystem drivers 15, requiring direct manipulation of the Special Function Registers (SFRs) to establish the necessary video timing parameters.

Assuming the integration of a standard 7-inch TFT LCD panel with an 800x480 resolution 18, the hardware initialization follows a precise, sequential methodology:

1. **GPIO Pin Multiplexing**: The physical output pins corresponding to the 24-bit RGB interface (VD\[23:0\]) and the critical control signals (VSYNC, HSYNC, VDEN, VCLK) must be routed from the SoC core to the LCD controller. This is achieved by modifying the Port Configuration Registers (GPF0CON, GPF1CON, GPF2CON, GPF3CON), configuring each pin to its specific "LCD Output" alternate function mode.  
2. **Display Control (VIDCON0, VIDCON1)**: The VIDCON0 register configures the primary video clock (VCLK). It calculates the necessary clock division ratio based on the HCLK\_DSYS input to match the target TFT panel's pixel clock requirement (typically around 33 MHz for an 800x480 panel). VIDCON1 defines the polarity of the synchronization signals. Depending on the specific panel datasheet, the VSYNC, HSYNC, and VDEN signals may need to be inverted to match active-high or active-low requirements.  
3. **Timing Configuration (VIDTCON0, VIDTCON1, VIDTCON2)**: These registers encapsulate the critical display timing parameters. VIDTCON0 holds the Vertical Back Porch (VBP), Vertical Front Porch (VFP), and Vertical Sync Pulse Width (VSPW). VIDTCON1 holds the Horizontal Back Porch (HBP), Horizontal Front Porch (HFP), and Horizontal Sync Pulse Width (HSPW). VIDTCON2 defines the active resolution (Vertical and Horizontal Output Size). Incorrect configuration of any of these fields results in an immediate loss of visual synchronization, manifesting as screen tearing, rolling, or a complete failure of the display controller to output a valid signal.  
4. **Window Configuration (WINCON0, VIDOSD0A, VIDOSD0B)**: The FIMD supports up to five hardware overlay windows.10 For a bare-metal LVGL deployment, a single window (Window 0\) is sufficient. WINCON0 is configured to define the color format (matching LV\_COLOR\_DEPTH, typically 16-bit RGB565). VIDOSD0A and VIDOSD0B define the spatial coordinate geometry of the window, typically configured from (0,0) to (800, 480).  
5. **Framebuffer Address Mapping (VIDW00ADD0B0)**: This critical register points the hardware's internal DMA engine to the physical start address of the framebuffer in the SDRAM. As discussed in Phase 1, this specific memory block must be aligned to a 1 MB boundary and configured as Non-Cacheable within the MMU page tables to guarantee pixel coherency.

| FIMD Register | Offset Address | Function Description | Bare-Metal Configuration Data |
| :---- | :---- | :---- | :---- |
| **VIDCON0** | 0xF900\_0000 | Video Control 0 | VCLK Divider, ENVID Enable |
| **VIDCON1** | 0xF900\_0004 | Video Control 1 | VSYNC/HSYNC/VDEN Polarity Inversion |
| **VIDTCON0** | 0xF900\_0010 | Video Timing 0 | VBP, VFP, VSPW |
| **VIDTCON1** | 0xF900\_0014 | Video Timing 1 | HBP, HFP, HSPW |
| **VIDTCON2** | 0xF900\_0018 | Video Timing 2 | Horizontal/Vertical Active Size (800x480) |
| **WINCON0** | 0xF900\_0020 | Window 0 Control | BPP Mode (16-bit RGB565), Window Enable |
| **VIDW00ADD0B0** | 0xF900\_00A0 | Window 0 Buffer Start | Physical SDRAM Address (e.g., 0x3E00\_0000) |

*Table 3: Core FIMD Register Map and Required Configurations for Display Initialization.*

### **LVGL API Configuration: Framebuffer and flush\_cb**

With the FIMD hardware emitting a stable pixel clock and continuously scanning the physical framebuffer via DMA, the LVGL display driver must be formally registered to the framework.

To maximize rendering performance and minimize absolute memory overhead, LVGL does not fundamentally require the user to allocate a secondary full-screen double buffer. Instead, LVGL utilizes partial draw buffers (lv\_disp\_draw\_buf\_t).22 The framework renders complex GUI elements—including shadows, gradients, and anti-aliased fonts—into these small, highly cacheable partial buffers. Because these buffers fit comfortably within the Cortex-A8's 512 KB L2 cache, memory access latency is virtually eliminated during the rendering pass. Once a specific rectangular region is fully rendered, LVGL invokes a hardware-specific flush callback to transfer the finished pixels to the actual physical framebuffer monitored by the FIMD.

C

/\* LVGL API Configuration Logic: Display Driver Registration \*/

/\* 1\. Allocate partial draw buffers (e.g., 1/10th of the screen size) \*/  
\#**define** DRAW\_BUF\_SIZE (800 \* 480 / 10\)  
/\* Allocated in the BSS segment, residing in Cacheable SDRAM \*/  
static lv\_color\_t buf\_1;  
static lv\_color\_t buf\_2; /\* Optional: Enables rendering while DMA transfers \*/

static lv\_disp\_draw\_buf\_t draw\_buf\_dsc;  
lv\_disp\_draw\_buf\_init(\&draw\_buf\_dsc, buf\_1, buf\_2, DRAW\_BUF\_SIZE);

/\* 2\. Initialize and register the display driver \*/  
static lv\_disp\_drv\_t disp\_drv;  
lv\_disp\_drv\_init(\&disp\_drv);

disp\_drv.hor\_res \= 800;  
disp\_drv.ver\_res \= 480;  
disp\_drv.flush\_cb \= s5pv210\_fimd\_flush\_cb; /\* The direct HAL binding \*/  
disp\_drv.draw\_buf \= \&draw\_buf\_dsc;

lv\_disp\_drv\_register(\&disp\_drv);

The critical link between the LVGL logical software rendering engine and the physical hardware resides entirely within the implementation of the s5pv210\_fimd\_flush\_cb function.23

C

/\* Hardware Abstraction Layer (HAL) Binding: Flush Callback \*/  
void s5pv210\_fimd\_flush\_cb(lv\_disp\_drv\_t \* disp\_drv, const lv\_area\_t \* area, lv\_color\_t \* color\_p)  
{  
    /\* Calculate physical dimensions of the invalidated area \*/  
    int32\_t y;  
    int32\_t width \= area-\>x2 \- area-\>x1 \+ 1;  
      
    /\* Pointer to the physical SDRAM address monitored by the FIMD \*/  
    /\* This address must match the value programmed into VIDW00ADD0B0 \*/  
    uint16\_t \* fb \= (uint16\_t \*)0x3E000000; 

    /\* Transfer the rendered partial buffer to the FIMD Framebuffer \*/  
    for(y \= area-\>y1; y \<= area-\>y2; y++) {  
        /\* Calculate row offset within the physical framebuffer \*/  
        uint16\_t \* dst \= fb \+ (y \* 800) \+ area-\>x1;  
          
        /\* Fast memory copy (utilizing NEON if optimized via custom assembly) \*/  
        memcpy(dst, color\_p, width \* sizeof(lv\_color\_t));  
          
        /\* Advance the partial buffer pointer to the next row \*/  
        color\_p \+= width;  
    }

    /\* Notify LVGL that the memory transfer is complete \*/  
    lv\_disp\_flush\_ready(disp\_drv);  
}

Because the physical framebuffer (0x3E00\_0000) is configured as Non-Cacheable (or Write-Combining) via the MMU, the memcpy operation bursts the pixel data directly to the SDRAM matrix. When the FIMD DMA engine subsequently scans this address space, it fetches the immediately updated pixel data, ensuring a tear-free graphical output without the requisite overhead of issuing CP15 cache clean operations.

### **Input Driver Integration**

For touch screen input, a similar abstraction architecture is required. The lv\_indev\_drv\_t structure is initialized, and a read\_cb function is registered. In a bare-metal environment, the touch controller—typically an I2C device like the FT5x06 for capacitive screens, or an onboard ADC for resistive screens—must be polled.22

The read\_cb interrogates the bare-metal I2C/ADC HAL. If a touch interrupt or a valid coordinate set is detected, the physical X and Y coordinates are written to the lv\_indev\_data\_t struct, and the state is explicitly set to LV\_INDEV\_STATE\_PR (Pressed). If no touch is detected during the polling interval, the state is returned as LV\_INDEV\_STATE\_REL (Released). This asynchronous polling seamlessly integrates into the LVGL event loop.

# **Phase 4: System Tick Configuration**

LVGL relies on a highly accurate, monotonic internal timebase to manage animation interpolations, widget state transitions, task scheduling, and input polling frequencies.22 This timebase is advanced by invoking the lv\_tick\_inc(uint32\_t tick\_period) function periodically.

In RTOS environments like FreeRTOS, this is easily achieved by tying the function to an OS thread or a software timer. However, in a strict bare-metal environment, this mechanism must be manually constructed using a hardware Pulse Width Modulation (PWM) Timer and the Vectored Interrupt Controller (VIC). The architecture requires configuring a specific hardware timer to overflow at exactly 1-millisecond intervals and vectoring the CPU exception execution flow to an Interrupt Service Routine (ISR) that subsequently calls the LVGL tick function.

### **Hardware Abstraction Layer (HAL): Timer 0 Initialization**

The S5PV210 contains five 32-bit internal timers. Timers 0 through 3 support both PWM output and interrupt generation, while Timer 4 is an internal timer without output pins.9 For the system tick, Timer 0 is utilized.

The initialization sequence requires routing the peripheral clock (PCLK\_PSYS) through an 8-bit prescaler and a secondary multiplexer (divider) to achieve a 1 kHz (1 millisecond) decrement rate.24 The registers associated with Timer 0 are located within the base memory space at 0xE250\_0000 and are structured as follows:

| Timer Register | Offset Address | Function Description | Bare-Metal Configuration |
| :---- | :---- | :---- | :---- |
| **TCFG0** | 0xE250\_0000 | Timer Configuration 0 | Prescaler 0 (Defines 8-bit divider for Timer 0/1) |
| **TCFG1** | 0xE250\_0004 | Timer Configuration 1 | Multiplexer (Defines secondary divider for Timer 0\) |
| **TCNTB0** | 0xE250\_000C | Timer 0 Count Buffer | Auto-reload value (e.g., 1000 for 1ms) |
| **TCMPB0** | 0xE250\_0010 | Timer 0 Compare Buffer | PWM Duty Cycle (Unused for pure interrupt generation) |
| **TCON** | 0xE250\_0008 | Timer Control | Auto-reload enable, Manual Update, Start/Stop |

*Table 4: S5PV210 Timer 0 Register Map for 1ms System Tick Generation.*

Assuming a PCLK of 66 MHz (derived from the MPLL configuration established in Phase 1), the mathematical configuration for a 1-millisecond interrupt is calculated as follows:

1. **Prescaler**: The TCFG0 register is set to divide by 66\. The intermediate clock frequency becomes 1 MHz (1,000,000 ticks per second).  
2. **Multiplexer**: The TCFG1 register is set to divide by 1 (0b0000 for Timer 0). The operational timer clock remains 1 MHz.  
3. **Count Buffer**: The TCNTB0 register is loaded with the value 1000\. As the 1 MHz clock decrements this register, it will reach zero exactly every 1 millisecond (![][image10] Hz).  
4. **Control Initialization**: The TCON register is manipulated in a two-step process. First, the auto-reload feature is enabled, and the manual update bit is asserted to actively load the 1000 value into the active internal counter. Second, the manual update bit is cleared, and the start bit is asserted, commencing the countdown.24

### **Hardware Abstraction Layer (HAL): Vectored Interrupt Controller (VIC)**

The S5PV210 incorporates an advanced exception model utilizing four cascaded Vectored Interrupt Controllers (VIC0 to VIC3) to manage the routing of asynchronous hardware events to the Cortex-A8 core.10 Unlike legacy architectures that rely on a single, monolithic IRQ handler that must manually poll status registers to determine the interrupt source, the VIC architecture allows the programmer to assign specific memory addresses to specific interrupt vectors. When an interrupt fires, the hardware automatically loads the Program Counter (PC) with the predefined address, drastically reducing interrupt latency.26

Timer 0 is mapped as Interrupt Source 21 within the VIC0 domain (0xF200\_0000).10 The bare-metal configuration entails:

1. **Select Interrupt Type**: Ensure the Timer 0 interrupt is routed as a standard IRQ rather than a Fast Interrupt (FIQ) by writing 0 to the 21st bit of the VIC0INTSELECT register.  
2. **Assign Vector Address**: The absolute memory address of the C-language ISR (e.g., Timer0\_ISR) is written to the specific vector address register associated with Timer 0 (VIC0VECTADDR21).  
3. **Enable Interrupt**: The 21st bit in the VIC0INTENABLE register is set to 1 to unmask the interrupt source at the controller level.26

Finally, the global IRQ mask bit (the 'I' bit) in the Current Program Status Register (CPSR) of the Cortex-A8 core must be cleared via an inline assembly instruction (cpsie i) to permit the CPU to recognize and service the incoming hardware signals.29

### **LVGL API Configuration: The Interrupt Service Routine**

When Timer 0 decrements to zero, the VIC hardware interrupts the main program execution and branches instantly to the Timer0\_ISR. In a bare-metal environment, the developer is strictly responsible for state preservation. The registers used by the ISR (R0-R12, LR) must be pushed to the IRQ stack, and upon completion, popped back to restore the exact state of the interrupted code.29 Modern C compilers targeting the ARM architecture (e.g., GCC utilizing the \_\_attribute\_\_((interrupt("IRQ"))) directive) handle this prologue and epilogue assembly generation automatically.

The ISR architecture strictly bounds the logic to HAL clearance and the LVGL API call:

C

/\* Bare-Metal Interrupt Service Routine \*/  
\_\_attribute\_\_((interrupt("IRQ"))) void Timer0\_ISR(void)  
{  
    /\* 1\. HAL: Clear the hardware interrupt pending status in the Timer \*/  
    uint32\_t \* TINT\_CSTAT \= (uint32\_t \*)0xE2500044;  
    \*TINT\_CSTAT |= (1 \<\< 5); /\* Writing 1 to bit 5 clears Timer 0 interrupt flag \*/

    /\* 2\. LVGL API: Advance the framework timebase by 1 millisecond \*/  
    lv\_tick\_inc(1);

    /\* 3\. HAL: Acknowledge the end of interrupt processing to the VIC \*/  
    /\* Writing to the VICADDRESS register signals the end of the ISR \*/  
    uint32\_t \* VIC0ADDRESS \= (uint32\_t \*)0xF2000F00;  
    \*VIC0ADDRESS \= 0;  
}

### **The Main Execution Loop**

With the display driver bound to the FIMD, the memory allocator initialized within SDRAM, the software rendering optimized via ARM NEON SIMD instructions, and the timebase accurately ticking via the hardware PWM timer, the LVGL framework is fully operational.

The final architectural requirement is the implementation of the main bare-metal execution loop. The core application logic must continuously invoke lv\_timer\_handler(). This critical function evaluates the time elapsed (based on the lv\_tick\_inc() counter) and executes all pending framework tasks: rendering invalidated regions to the partial buffers, triggering the flush\_cb, reading the touchscreen via the read\_cb, and advancing animation states.22

C

int main(void)  
{  
    /\* Phase 1: Hardware Initialization \*/  
    s5pv210\_clock\_init();    /\* Bypasses, configures, and locks APLL/MPLL \*/  
    s5pv210\_sdram\_init();    /\* Initializes DMC0/DMC1 for DDR2 access \*/  
    s5pv210\_mmu\_init();      /\* Establishes flat mapping, enables L1/L2 caches \*/

    /\* Phase 2 & 3: LVGL Initialization \*/  
    lv\_init();  
    s5pv210\_fimd\_init();     /\* Configures LCD timing and non-cacheable framebuffer \*/  
    lvgl\_display\_driver\_init(); /\* Registers flush\_cb and partial draw buffers \*/  
    lvgl\_touch\_driver\_init();   /\* Registers read\_cb for I2C/ADC polling \*/

    /\* Phase 4: Tick Initialization \*/  
    s5pv210\_timer0\_init();   /\* Configures PWM Timer 0 and VIC0 vectors \*/

    /\* Generate GUI Elements using LVGL API \*/  
    lv\_obj\_t \* btn \= lv\_btn\_create(lv\_scr\_act());  
    lv\_obj\_t \* label \= lv\_label\_create(btn);  
    lv\_label\_set\_text(label, "Bare-Metal LVGL Active");  
    lv\_obj\_center(btn);

    /\* Main Execution Super-Loop \*/  
    while(1) {  
        /\* Process all LVGL rendering, input, and animation tasks \*/  
        lv\_timer\_handler();  
          
        /\* Power Management: Execute WFI (Wait For Interrupt)   
           The Cortex-A8 enters a low-power standby state, halting execution   
           until the next 1ms Timer 0 interrupt awakens the core \*/  
        \_\_asm\_\_ volatile ("wfi");  
    }

    return 0;  
}

The incorporation of the Wait For Interrupt (wfi) inline assembly instruction is a crucial optimization for mobile or embedded deployments. It ensures the Cortex-A8 core enters a low-power standby state when no GUI updates or processing tasks are actively required, awakening instantly upon the next 1 ms hardware timer tick.10

# **Recommended Reference Materials**

To successfully navigate the immense complexities of porting a modern GUI framework to a bare-metal ARMv7-A architecture without the safety net of an operating system, the following technical documentation and architectural reference materials are highly recommended for the cross-verification of memory mapping, register definition, and assembly constraints:

1. **Samsung S5PV210 RISC Microprocessor User's Manual (Revision 1.10)**  
   * This document is the absolute baseline for bare-metal engineering on the study210 board. It is essential for deriving the exact register offsets, bit-field definitions, and complex hardware initialization sequences for the FIMD (Section: Display Controller), PWM Timers (Section: Timer), APLL/MPLL Clock synthesis (Section: Clock Controller), and the Vectored Interrupt Controller (Section: VIC).10  
2. **ARM Cortex-A8 Technical Reference Manual & ARMv7-A Architecture Reference Manual**  
   * These manuals are critical for understanding the CP15 coprocessor instructions required to manually configure the Memory Management Unit (MMU). They provide the bit-level definitions necessary for establishing Section Descriptors for Cacheable versus Non-Cacheable memory spaces, which is strictly required for preventing FIMD framebuffer tearing. Furthermore, they outline the IRQ/FIQ exception vector tables and stack preservation requirements necessary for bare-metal ISR development.9  
3. **Neon Programmer's Guide for ARM Cortex-A Series**  
   * Provides the necessary context and syntax for the Advanced SIMD instruction set. Because hardware GPU acceleration is excluded, this guide is invaluable for understanding how LVGL's software rendering backend (LV\_USE\_DRAW\_SW\_ASM\_NEON) leverages 128-bit vector registers to accelerate complex matrix operations and alpha blending.1  
4. **LVGL Official Documentation: Porting Guide (v8.x/v9.x)**  
   * The official documentation defines the precise structural requirements for the lv\_conf.h file, the lifecycle of the flush\_cb partial draw buffer logic, and the rigid execution rules concerning lv\_tick\_inc() and lv\_timer\_handler() in non-RTOS environments. It serves as the definitive reference for the software abstraction layer.22  
5. **U-Boot Source Tree for Samsung S5PV210 (arch/arm/mach-s5pv210)**  
   * While specifically an open-source bootloader, the source code repositories (e.g., clock.c, memory.c) offer practical, community-vetted C-language implementations of the complex mathematical formulas required to configure the S5PV210 PLLs, the DMC SDRAM timings, and the physical base addresses (0xF900\_0000 for FIMD, 0xF200\_0000 for VIC). Reviewing these files provides an excellent reference implementation for bare-metal hardware initialization.10

#### **Works cited**

1. Single Board Computers \- Electronics Datasheets, accessed April 13, 2026, [https://www.electronicsdatasheets.com/datasheet/MYS-S5PV210%20datasheet.pdf](https://www.electronicsdatasheets.com/datasheet/MYS-S5PV210%20datasheet.pdf)  
2. Porting and Hands-On Guide for LVGL 8.2 on the OK3568-C Platform under Linux 4.19 Buildroot \- DEV Community, accessed April 13, 2026, [https://dev.to/ronnie\_r\_152dc2151d9449c6/porting-and-hands-on-guide-for-lvgl-82-on-the-ok3568-c-platform-under-linux-419-buildroot-fpc](https://dev.to/ronnie_r_152dc2151d9449c6/porting-and-hands-on-guide-for-lvgl-82-on-the-ok3568-c-platform-under-linux-419-buildroot-fpc)  
3. Processor boot process \- EEWorld, accessed April 13, 2026, [https://en.eeworld.com.cn/news/mcu/eic689650.html](https://en.eeworld.com.cn/news/mcu/eic689650.html)  
4. Zhu's ARM bare metal learning notes (IV): Detailed explanation of the S5PV210 boot process \- EEWORLD, accessed April 13, 2026, [https://en.eeworld.com.cn/news/mcu/eic361871.html](https://en.eeworld.com.cn/news/mcu/eic361871.html)  
5. FriendlyARM mini210 u-boot bringup \- Polprog's, accessed April 13, 2026, [https://polprog.net/blog/mini210/](https://polprog.net/blog/mini210/)  
6. \[project X\] tiny210 (s5pv210) power-on startup process (BL0-BL2) \- EEWORLD, accessed April 13, 2026, [https://en.eeworld.com.cn/news/mcu/eic687698.html](https://en.eeworld.com.cn/news/mcu/eic687698.html)  
7. S5PV210 bare board driver: Start \- EEWORLD, accessed April 13, 2026, [https://en.eeworld.com.cn/news/mcu/eic687201.html](https://en.eeworld.com.cn/news/mcu/eic687201.html)  
8. S5PV210 initializes system clock \- EEWORLD, accessed April 13, 2026, [https://en.eeworld.com.cn/news/mcu/eic685258.html](https://en.eeworld.com.cn/news/mcu/eic685258.html)  
9. S5PV210 overview \- EEWORLD, accessed April 13, 2026, [https://en.eeworld.com.cn/news/mcu/eic685364.html](https://en.eeworld.com.cn/news/mcu/eic685364.html)  
10. S5PV210 \- Samsung Confidential, accessed April 13, 2026, [https://onelec.ru/uploads/product\_pdf/64410/S5PV210.pdf](https://onelec.ru/uploads/product_pdf/64410/S5PV210.pdf)  
11. \[PATCH v2 2/6\] ARM: S5PV210: Fix wrong EPLL rate getting on setup clocks \- Mailing Lists, accessed April 13, 2026, [http://lists.infradead.org/pipermail/linux-arm-kernel/2010-October/028819.html](http://lists.infradead.org/pipermail/linux-arm-kernel/2010-October/028819.html)  
12. S5PV210 Evaluation Kit Price US$329.00 Samsung S5PV210 Application Processor based on 1GHz ARM Cortex-A8 core  
    \- Onboard 512MByte DDR2, 512MByte NAND Flash, accessed April 13, 2026, [https://www.quickembed.com/Tools/Shop/ARM/201107/177.html](https://www.quickembed.com/Tools/Shop/ARM/201107/177.html)  
13. \[PATCH 3/6\] S5PV210 added register mappings \- Mailing Lists, accessed April 13, 2026, [http://lists.infradead.org/pipermail/linux-arm-kernel/2010-June/017961.html](http://lists.infradead.org/pipermail/linux-arm-kernel/2010-June/017961.html)  
14. Problems enabling MMU on ARM Cortex-A8. CPU is S5PV210 \- Stack Overflow, accessed April 13, 2026, [https://stackoverflow.com/questions/27379166/problems-enabling-mmu-on-arm-cortex-a8-cpu-is-s5pv210](https://stackoverflow.com/questions/27379166/problems-enabling-mmu-on-arm-cortex-a8-cpu-is-s5pv210)  
15. How to run LGVL on iMX using framebuffer \- NXP Community, accessed April 13, 2026, [https://community.nxp.com/t5/i-MX-Processors-Knowledge-Base/How-to-run-LGVL-on-iMX-using-framebuffer/ta-p/1853768](https://community.nxp.com/t5/i-MX-Processors-Knowledge-Base/How-to-run-LGVL-on-iMX-using-framebuffer/ta-p/1853768)  
16. Porting — LVGL documentation, accessed April 13, 2026, [https://docs.lvgl.io/9.1/porting/index.html](https://docs.lvgl.io/9.1/porting/index.html)  
17. Porting and Hands-On Guide for LVGL 8.2 on the OK3568-C Platform under Linux 4.19 Buildroot | by Ronnieroy-Forlinx | Medium, accessed April 13, 2026, [https://medium.com/@ronnieroy118/porting-and-hands-on-guide-for-lvgl-8-2-on-the-ok3568-c-platform-under-linux-4-19-buildroot-fdd45679d61b](https://medium.com/@ronnieroy118/porting-and-hands-on-guide-for-lvgl-8-2-on-the-ok3568-c-platform-under-linux-4-19-buildroot-fdd45679d61b)  
18. LCD Board \- User's Guide \- Mouser Electronics, accessed April 13, 2026, [https://www.mouser.com/datasheet/2/302/LCD\_Board\_Users\_Guide-594699.pdf](https://www.mouser.com/datasheet/2/302/LCD_Board_Users_Guide-594699.pdf)  
19. Arm \- LVGL 9.3 documentation, accessed April 13, 2026, [https://docs.lvgl.io/9.3/details/integration/chip/arm.html](https://docs.lvgl.io/9.3/details/integration/chip/arm.html)  
20. Create Embedded GUIs with Linux Frame Buffer and LVGL | Blog, accessed April 13, 2026, [https://lvgl.io/blog/tutorial-linux-framebuffer](https://lvgl.io/blog/tutorial-linux-framebuffer)  
21. Diff \- 11e410cc6d1483328839cd3df8b554299e86aede^2..11e410cc6d1483328839cd3df8b554299e86aede \- kernel/samsung \- Git at Google \- Android GoogleSource, accessed April 13, 2026, [https://android.googlesource.com/kernel/samsung/+/11e410cc6d1483328839cd3df8b554299e86aede%5E2..11e410cc6d1483328839cd3df8b554299e86aede/](https://android.googlesource.com/kernel/samsung/+/11e410cc6d1483328839cd3df8b554299e86aede%5E2..11e410cc6d1483328839cd3df8b554299e86aede/)  
22. Porting — LVGL documentation, accessed April 13, 2026, [https://docs.lvgl.io/9.0/porting/index.html](https://docs.lvgl.io/9.0/porting/index.html)  
23. What is drawn when you follow the porting guide? \- Get started \- LVGL Forum, accessed April 13, 2026, [https://forum.lvgl.io/t/what-is-drawn-when-you-follow-the-porting-guide/11337](https://forum.lvgl.io/t/what-is-drawn-when-you-follow-the-porting-guide/11337)  
24. \[ARM bare board\] Timer interrupt example and analysis \- EEWorld, accessed April 13, 2026, [https://en.eeworld.com.cn/news/mcu/eic490733.html](https://en.eeworld.com.cn/news/mcu/eic490733.html)  
25. Vectored Interrupt Controller (VIC) and NVIC \- EmbeTronicX, accessed April 13, 2026, [https://embetronicx.com/tutorials/microcontrollers/stm32/vectored-interrupt-controller-nested-vectored-interrupt-controller-vic-nvic/](https://embetronicx.com/tutorials/microcontrollers/stm32/vectored-interrupt-controller-nested-vectored-interrupt-controller-vic-nvic/)  
26. Vector Interrupt Controller \- Wikimedia Commons, accessed April 13, 2026, [https://upload.wikimedia.org/wikiversity/en/f/fa/ARM.2ASM.VIC.20220725.pdf](https://upload.wikimedia.org/wikiversity/en/f/fa/ARM.2ASM.VIC.20220725.pdf)  
27. Vectored Interrupt Programming \- Wikimedia Commons, accessed April 13, 2026, [https://upload.wikimedia.org/wikiversity/en/2/29/ARM.2ASM.VectorInt.20230610.pdf](https://upload.wikimedia.org/wikiversity/en/2/29/ARM.2ASM.VectorInt.20230610.pdf)  
28. External interrupts of s5pv210 \- EEWorld, accessed April 13, 2026, [https://en.eeworld.com.cn/news/mcu/eic686991.html](https://en.eeworld.com.cn/news/mcu/eic686991.html)  
29. S5PV210 interrupt system and key control external interrupt \- EEWorld, accessed April 13, 2026, [https://en.eeworld.com.cn/news/mcu/eic550991.html](https://en.eeworld.com.cn/news/mcu/eic550991.html)  
30. A Practical guide to ARM Cortex-M Exception Handling \- Memfault Interrupt, accessed April 13, 2026, [https://interrupt.memfault.com/blog/arm-cortex-m-exceptions-and-nvic](https://interrupt.memfault.com/blog/arm-cortex-m-exceptions-and-nvic)  
31. Porting — LVGL documentation, accessed April 13, 2026, [https://docs.lvgl.io/8.2/porting/index.html](https://docs.lvgl.io/8.2/porting/index.html)  
32. arch/arm/mach-s5pv210/clock.c · main · Mancomun / Kernel de Linux del Proxecto Abalar, accessed April 13, 2026, [https://eira.mancomun.gal/mancomun/kernel-abalar/-/blob/main/arch/arm/mach-s5pv210/clock.c?ref\_type=heads](https://eira.mancomun.gal/mancomun/kernel-abalar/-/blob/main/arch/arm/mach-s5pv210/clock.c?ref_type=heads)

[image1]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACEAAAAZCAYAAAC/zUevAAABoElEQVR4Xu2VPyhFcRTHjzIoJBIDm0kpgwxkeItiIMlGRimDsiiWN7DJgMWfoiRGwn7LaFUsBqWUkjJYJL5f5/fzzvu5r/devVuG+6lP7/3Oue+98/udc+8TSUlJKY0ZuFOiiZGBU/AOfsEzOGFchQ8ulyjd8A0+wvYgR+rgaRisNJOiO72ENUGOsIjDMFhpNkSL4NF7FkWLIy3wIJeqPPXwSrSIYRfjzs9hr78oaTrhC/wQ3T2HkcN5A5vNdYniW8FXzxFcMGtPRvRW/YTLbk2q3ft10e/acvEmOCc68JsuFksk+sFxE9uFA2ZtYbHXsDFMgCV4IvqjHp7mmsQP/C9Pou1gWzx9sNasPZyVCO7DqvzUT24F9sNXE++Bs2YdC0+h0M5C/Pz4u8bCHFvI1hzDBheflsKn+guL2AuDBRgV3SUfbiEjTjLk5GllYZuL55ERHS4WYL2HrbnL/hBJfMG+FXz1vMN5KaEV5VKsFZZb0Xkr2opy4f8LBy2EbRoLYlkpfrJl0SH62GbLIjhocheirX2W/FPqgtuig5ryv/kG3kpWcPNKNj8AAAAASUVORK5CYII=>

[image2]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAC0AAAAZCAYAAACl8achAAACRUlEQVR4Xu2WP0hVcRTHT6RQ5KJIEQaKDuIQCpEgGDg4KEGDBA5CBSFBYEMNLg1BBAkOaYMiouhg2BbR1NCDhoyGWpKgIRpSFCSKEhyivl/O7+c797z7QkzxBvcDH979nfe77/7+nN+5TyQnJydnL+iFUztwDDboLQfPaXgRPoG/4efQjg7ARfgTngn3ZIJq+EZ00BxkGs9F+2WGFrgBv8NW911kAR7xwYOEq8tV5mrH1awVTZkToT0aPjPDuOigp02sBz6CFSaWGWw+T4oevtvwGxw2/TJFWmqche9gXewUOAxvwBewDTbDJXgLHgp9OOm3olXoCqwXPTMz8Ivo/fzda3AdzomW1E9wNly/ggVYJWVgSnDQvCE+uBPOSzI1KuGE6EOOmXiH6MP5SeLOMeUsfB8MmvZDKdZ9HvBnUnx+IxwJ16mklTquBOu3hav5UUpfMCdFV/VmaLOWswpd2O6hMNVsnR8y1xzkqmmzgnEnysIH7OTFsQLv+KAUB83zQDh5DoADiXAl70qyzrM6RbrhV9OugcdNuwSfz+XYFM11DyfLScdDyzQrSDIfOYH7Utx+zz343gc9zL8t0QF7mXseHsCClB4Mthl/Co+GGHeuL3YI+NSwxNT4azrslrTd6IdrsN3EWCr9ALmScVIeLtIPSd/Ff4YV4pyLsUydd7EPotUnwqpjJ+WJqWFzfM+4BJfhZdFBPYCnEj2U6/Cl6IG8Ch8nv96GOf4a/gryel/+2/BHuWpdUprfFn7HiTWJnoec/4Y/nDlzydqKCsQAAAAASUVORK5CYII=>

[image3]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAmwAAAAxCAYAAABnGvUlAAAEyUlEQVR4Xu3dTainUxwH8COmFPIaecsQSSnKy2SihAWKhAWNnTKEDRqyGEqKhkhEQynFAhtNbCiyoZSXhY2yICULRJlEg/P1PMc997n/e+eOmTFj7udTv+4553nu//9f/jovv1MKAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA7LUOqnFrjZ9qfDV5Fqtr/FHj9hrXzH+0U9bW+KjGnzVuqHF9jZtrvF1m/w4AgBVtfY1NZUjMpjaWIanaHb6t8ft0sHp9OgAAsNK9VOPqsjAxO7LG6TW2TsanVk0HqgOnAzMkQfxyOlg9NR0AAFjpttQ4uCxM2B6vsa7Gy5PxqQ01Du36J9S4ruvPkoQu33fX2L+jewYAQGe/Msyuxfc1jhrbSdQi+8zOG9tLSdKW/WfLSdbishrbanxT4+syLI82+U3HjpF2JKFMv8nsX/8cAGCflWQsyVC8V+OcGo/UWDOOZRZsuUnR6hofTAcX8U6Ne7v+w107six6+GTs+RoXjO3M0LXkEgBgn5b9a02SpAdqHD32kxBtb/9a81kZZtaS3L05eTZLEsHjp4OdzPb18lsOKMOM3MllmKEDAFgR3u3a2U/2a9fPbNvnXX8xJ5X5y6DZj9bvaZslCVsSsMX8OOnnt0TG8/kpRQIAsF2pG7Z5kTise29v1Db9J5KUZQYrSVGSr2PKcHqzPc/y5VJm1We7bTowSs219rmJn+c//luWaF+YjD04/s2SaGb9Wh8AYLuy/NfvxYpHy9y+MHZcDkG0GbXmzq6dpO+srg8AsKTUEsuMVO+qSR8AgD2o34t1RhmuespSKQAAe4FTyjDDlnswc8Ky37D/X8j3LhWXzr0KALAypXZYu/fy2jLUMZu6qcbdXT+b/VPwNcuoKYHRCsDuX4aDCq1gbIvpcisAAMuU2bVU579w7Gc5NCcsm7fK/JOMqebf/NK1I/8b95QhiUuNsXa7wPRAw65w5T4aAADzZHZtsVpiF5XhJGOvJWl5v83KRUvWMt5uFMjJ09Y+bfwLAMAOyh2bSdhWTR9UT5fhTs1e3o1U9+9nzW7s2k1/ryYAALvBh5P+iTWeHNu5TaAlc7kr84mx3VvO7QL7uiTC95XhGqo3intDAYBdLAcNTh3bWdr8onuWpdJ2qfmzZeEVTtkbt34ytqdcUYaroJ4pcydPP6nxWv/STjiixqtluB80M5ApidL0tyu0/XwAALtUbjq4uAynQqeOK8Pp0P+DJFItwezH+uTq3/quDPeCxpllKJFy+djP3aT5nvfLwuVlAAA6be9dc8g4Nuuwxaz9fJH/mSWf0w5jtM9NPbsmM5UZe7EbAwCgk+XZ6eXsD5WhZMli+uXfJHCPjX9nySxde5Y9aknOciAj39vkNG6fxAEA0Emy9FsZasj9UGNbjTXz3pgtSVsSsefGv8uRpc8sg8bGGltq3FLjkn/eAABggb4w8I5IkrZ1OriEDWUoFgwAwA7KEmUr4LtcSdY2j+2Pa5zfPZtlbZk7Ubuuxv3dMwAAlpBTrtMDB8uRZK0tg55dhqRtMUnmPi1z96a+UoakDQCA7Uii1sdynVtm71nbNB0YTb/n+zJ3VRcAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAr21/Pi8DGPsR8KgAAAABJRU5ErkJggg==>

[image4]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABUAAAAZCAYAAADe1WXtAAABT0lEQVR4Xu2SvUpDQRCFj6BgIzaSIBoQX0AkSBqttBFJExR8A1u1Ee0F2yTEVqxEtLGwsZBga22nICLYC9oI6jlOlsyQ3OAD3A++Ipm5u/OzQE5OYpOe0yf6Qq9iODBEt2F5sklLIaNDha7TO/pDv2I4ME8fYXn3dJmOhAzHLL2hu7AP+rFBd2ibftKFEO3DCj2ha8g+tEXn6Bt9oBMx3MsB3aJl+kGHY/hvlkt0FXbpJXpzAqP0gi7SafpKx0KGzVKHHMIO1ZgGMgWbZxHWklqbdHEtog67/Bq2SBUwEN26536fovtRgR7Tcdjcv/GP1sUZ7IOEWqzB5rgPm6XQxWrdF5DJLWyWiVS5lnaEblWqUJX6AjLRkjSvRBVWveY44/5/hi3RF5CJ2vWoQlWkJ+bRgrQoX0BAj/gdNqOk3qDQC0hzTK/B58lGJ56TQ34B+h1C45PzeZQAAAAASUVORK5CYII=>

[image5]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAA8AAAAZCAYAAADuWXTMAAAA00lEQVR4Xu3SMQ4BQRTG8RE6GuEEolEpRMcBRChE5wBKp3ABUQmlzhEUW9NoRE1BTaGg4P92jOw+EVSa/ZJfsjvfvs1Mdo2J8r/UMMIEK+we17Lm9J5PqxTQxhQ3bB/3zgAX1N2ATgqescOdcOWnZGwnu3xJDgecUFSdpIUrKrqQNI198xJp1cUwxAIZ1fnpGzs81gXJY4+GLiTvzhtHFRscA+uhuC2vkVXdx7gtz5BQ3cd4xg531fpXkU90RlkX3+Tn88qPID+EDAbNkQw8F+WvuQM4Ty7kHwfxrQAAAABJRU5ErkJggg==>

[image6]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAA0AAAAaCAYAAABsONZfAAABDElEQVR4Xu3TMS8EQRjG8VeOy4lziEtUGrlGLSIhGgndXeNUCqVGJzqfQlQKnaiuo9CI5HQSnWgVKgkaFJLj/q+ZsPvsFj7APskv2Xnfmc3s7qxZkX9lHFMYiuNy4jqVAazgHi94xCemcYKZv6khvmAHz2jFsaeGL3QxGmu/WcYr1rVBnnCgRc8pbjChDXKFTS163qMFbZA2hrXoucM3euhgA2OpGTnZtbBIXSQnaUoWXoI/ly7M3ZqmgkWcW1g0m26HVLUQM4lbzGmjjmstxvjr9+1mtjePNy3GrFr4DJlsW9h3Q+p+swccSv3nfB1j38LBvMQRzvCBPcs52YNYig3/DZrYwhpGEvOKaPpTHjFImqMTUQAAAABJRU5ErkJggg==>

[image7]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEwAAAAZCAYAAACb1MhvAAAC0UlEQVR4Xu2Xz6uMURjHH0Upv6Mk5CaUX1EWIqRQLGz8KLLxB4iyIErdBQslC1lIWVigUFhYm2JpYUFkhcRCrNj4/f3cZ8687zkz887r1p076Xzq2507z5nznvOc58d5zTKZTCaTydRji3RdeiW9k57F5jYmSeesGHtZGioP6DMTpCXplyVmSpukpdLkxAbT0y/M52SfHVkk7ZMuSn+aqmK/9MV83AFpm1VMPoawqRXSA+lNbGpxW/okvZd+ma95SjTCbLf0XLomXZXuS1+lqeVBKXj+jnRY+ijNiawOCzwrHTRf4OfI2l/mmm9onfRNehubR9gpHbH4MFdJv5u2AA67ZO6sE9Iy871WskBqSGukF9LiyOpsNncYf39Ij2PzuFDlsJPmNsYEpplHGQ4K4LB/Zrt0S5pl7rjyQwKcAI5lIelDx4sqh+2QXpqvOUBUsnZqdmBUDjtt7ghgsk6TULsmSnfNH7onNo8LVQ6DtLZSalh72Cuw13nSGfOgOCbNLtnbCPWLTgJ0wOOFeYSV5g+fb0X9Wl4eUMF686ZSV6v9Z7Xo5bAUatcHi7sqDntkXmropDfNm1pXQjoSPcAEN6wofETW0ebnQUpHqOswDpvo+Z4aukCWDadfBsrpCCyiYZ7v5D8PmmEeiQ9tcNIR6jiMg6f7ETUERx3IMvba6d4WpSPQIVnAQumCeajCaNJxrKnjsL3Sa2lt83/2QZkA7qBPzWtYGQKoYV3uYg3zSQIURpxCHTtvRWpyOtxhuE7QnutyxYoLcR2d8p/VopfDNkpPLK5Zh8yzCig/PHNry+qQkjS3UKYi7lkcetyEmYTbb7klD1r9gl4O49Vtg3kEBVHUcRpg4xWv3E1D6eEtpgUpRRSVT7bsCOpWmGSXtUcBSjtpP8FB6XpQw4o0CgecKi0pBAVZw555zfpp/kqVqYBIJeq41gxZjVejTCaTyfyn/AVcFaz8qqgQRQAAAABJRU5ErkJggg==>

[image8]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAADMAAAAZCAYAAACclhZ6AAABqElEQVR4Xu2WvytGURjHH6EIyY9IkpJFKYMMiomBRMJmMBqVgb/AaJGFGAwWyabewfBmUZQYZDIwMIpBfgx8n/u853bOc98fDO576XzqU+99nve89557nnOel8jj8Xj+I6NwE27DC3iX+cwx40L47fgpgQ2wBTbBUjft0gVn4C78hLeZa+MafIdjZkDMnMMzkhfMz/ZCBSZUDdMkk5l1UwG9JDlexTgZh4+w24rtwCNYZcUcOuADfIY9KsdMwQ84oBO/zDB8g/1WjCeTJlmArEyQvHlezjqV45pdh6cktRsnfO9O67oSpuCqFYuwQjKZLZ0g+bF7kiUvNoMkK9WuE4Zc+4U3GQ++hk9WPBet5B4chRyC5cHI/NTDaZJT9RUeuGkXU2JXsFHlkgZPfg8ukZRgBFNi+7BM5ZIIH0b6UAhJk0xmXsWTADdKXS2mTSyqeAAfydyI+nTih4yQ3OS7HsOaYGR2+OS8hIewworzc/L4ZSsWktT90gxvKDqZSJlxY+QGqd9W3s5aBGpJ/gFskJTVCclzcr/5k7TBSThH0uvibtwejyepfAHvtV5lQogdVgAAAABJRU5ErkJggg==>

[image9]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAADAAAAAaCAYAAADxNd/XAAABfElEQVR4Xu2WvSuHURTHv/KelyiiZBGDWVJk+RVZSGEy2FhsVn+FTAZlkMnEwCLFRNkwWxUWJOXle5wH9zk/Pb+7uI90P/UZnnPurXvu23OBSCTyr2milc53lfn+k5TRAr2kt/SZrtBOukm7vpsGp5a22qCLDH6R3tCJ5LsROvBXekQbvlqHo5wO0hO6YXIphukdnTLxHnoNXYnQXEAn75G+oUQBW/SUNpt4PT2ksyYegjZaQ8fhUcBD4oCJV9Np6B7MC68CzqGNXug2nUmnc8WrgCVoI9d92u42ymASulK+dmg3L7wKkNMuB1jOgVvEDvy2T+4FuMjBkatLOsm12ptOB6dkAXLT/MQZ9GD32URgMgtoocc2mCDbaQ9+W+gJxWcoyzHt5kVmAf303gYTZPZHbDAHMgtYgCa7TVwKW6UVJh4auVzmoWOUCyWFvHfW6TL04XZA1+gu9Ped9+vzCsVbT5QV+UBmdwg6UPltS2KOjtK6z0aRSCTyK7wDKm5lFQTi1+YAAAAASUVORK5CYII=>

[image10]: <data:image/png;base64,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>