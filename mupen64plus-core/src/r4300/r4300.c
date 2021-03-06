/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - r4300.c                                                 *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2002 Hacktarux                                          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ai/ai_controller.h"
#include "api/callbacks.h"
#include "api/debugger.h"
#include "api/m64p_types.h"
#include "cached_interp.h"
#include "cp0_private.h"
#include "cp1_private.h"
#include "interrupt.h"
#include "main/main.h"
#include "main/device.h"
#include "main/rom.h"
#include "memory/memory.h"
#include "mi_controller.h"
#include "new_dynarec/new_dynarec.h"
#include "ops.h"
#include "pi/pi_controller.h"
#include "pure_interp.h"
#include "r4300.h"
#include "r4300_core.h"
#include "recomp.h"
#include "recomph.h"
#include "rsp/rsp_core.h"
#include "si/si_controller.h"
#include "tlb.h"
#include "vi/vi_controller.h"

#ifdef DBG
#include "debugger/dbg_debugger.h"
#include "debugger/dbg_types.h"
#endif

unsigned int r4300emu = 0;
unsigned int count_per_op = COUNT_PER_OP_DEFAULT;
unsigned int llbit;
#if NEW_DYNAREC != NEW_DYNAREC_ARM
int stop;
int64_t reg[32], hi, lo;
uint32_t next_interrupt;
struct precomp_instr *PC;
#endif
long long int local_rs;
uint32_t skip_jump = 0;
unsigned int dyna_interp = 0;
uint32_t last_addr;

cpu_instruction_table current_instruction_table;

void generic_jump_to(uint32_t address)
{
   if (r4300emu == CORE_PURE_INTERPRETER)
      PC->addr = address;
   else {
#ifdef NEW_DYNAREC
      if (r4300emu == CORE_DYNAREC)
         last_addr = pcaddr;
      else
         jump_to(address);
#else
      jump_to(address);
#endif
   }
}

static unsigned int get_tv_type(void)
{
    switch(ROM_PARAMS.systemtype)
    {
       default:
       case SYSTEM_NTSC: return 1;
       case SYSTEM_PAL: return 0;
       case SYSTEM_MPAL: return 2;
    }
}

/* Simulates end result of PIFBootROM execution */
void r4300_reset_soft(void)
{
    unsigned int rom_type = 0;              /* 0:Cart, 1:DD */
    unsigned int reset_type = 0;            /* 0:ColdReset, 1:NMI */
    unsigned int s7 = 0;                    /* ??? */
    unsigned int tv_type = get_tv_type();   /* 0:PAL, 1:NTSC, 2:MPAL */
    
    uint32_t bsd_dom1_config;
    
    if ((g_ddrom != NULL) && (g_ddrom_size != 0) && (g_dev.pi.cart_rom.rom == NULL) && (g_dev.pi.cart_rom.rom_size == 0))
    {
      //64DD IPL
      bsd_dom1_config = *(uint32_t*)g_ddrom;
      rom_type = 1;
    }
    else
    {
      //N64 ROM
      bsd_dom1_config = *(uint32_t*)g_dev.pi.cart_rom.rom;
    }

    g_cp0_regs[CP0_STATUS_REG] = 0x34000000;
    g_cp0_regs[CP0_CONFIG_REG] = 0x0006e463;

    g_dev.sp.regs[SP_STATUS_REG] = 1;
    g_dev.sp.regs2[SP_PC_REG] = 0;

    g_dev.pi.regs[PI_BSD_DOM1_LAT_REG] = (bsd_dom1_config      ) & 0xff;
    g_dev.pi.regs[PI_BSD_DOM1_PWD_REG] = (bsd_dom1_config >>  8) & 0xff;
    g_dev.pi.regs[PI_BSD_DOM1_PGS_REG] = (bsd_dom1_config >> 16) & 0x0f;
    g_dev.pi.regs[PI_BSD_DOM1_RLS_REG] = (bsd_dom1_config >> 20) & 0x03;
    g_dev.pi.regs[PI_STATUS_REG] = 0;

    g_dev.ai.regs[AI_DRAM_ADDR_REG] = 0;
    g_dev.ai.regs[AI_LEN_REG] = 0;

    g_dev.vi.regs[VI_V_INTR_REG] = 1023;
    g_dev.vi.regs[VI_CURRENT_REG] = 0;
    g_dev.vi.regs[VI_H_START_REG] = 0;

    g_dev.r4300.mi.regs[MI_INTR_REG] &= ~(MI_INTR_PI | MI_INTR_VI | MI_INTR_AI | MI_INTR_SP);

    if ((g_ddrom != NULL) && (g_ddrom_size != 0) && (g_dev.pi.cart_rom.rom == NULL) && (g_dev.pi.cart_rom.rom_size == 0))
    {
      //64DD IPL
      memcpy((unsigned char*)g_dev.sp.mem+0x40, g_ddrom+0x40, 0xfc0);
    }
    else
    {
      //N64 ROM
      memcpy((unsigned char*)g_dev.sp.mem+0x40, g_dev.pi.cart_rom.rom+0x40, 0xfc0);
    }

    reg[19] = rom_type;     /* s3 */
    reg[20] = tv_type;      /* s4 */
    reg[21] = reset_type;   /* s5 */
    reg[22] = g_dev.si.pif.cic.seed;/* s6 */
    reg[23] = s7;           /* s7 */

    /* required by CIC x105 */
    g_dev.sp.mem[0x1000/4] = 0x3c0dbfc0;
    g_dev.sp.mem[0x1004/4] = 0x8da807fc;
    g_dev.sp.mem[0x1008/4] = 0x25ad07c0;
    g_dev.sp.mem[0x100c/4] = 0x31080080;
    g_dev.sp.mem[0x1010/4] = 0x5500fffc;
    g_dev.sp.mem[0x1014/4] = 0x3c0dbfc0;
    g_dev.sp.mem[0x1018/4] = 0x8da80024;
    g_dev.sp.mem[0x101c/4] = 0x3c0bb000;

    /* required by CIC x105 */
    reg[11] = INT64_C(0xffffffffa4000040); /* t3 */
    reg[29] = INT64_C(0xffffffffa4001ff0); /* sp */
    reg[31] = INT64_C(0xffffffffa4001550); /* ra */

    /* ready to execute IPL3 */
}

#if !defined(NO_ASM)
static void dynarec_setup_code(void)
{
   // The dynarec jumps here after we call dyna_start and it prepares
   // Here we need to prepare the initial code block and jump to it
   jump_to(UINT32_C(0xa4000040));

   // Prevent segfault on failed jump_to
   if (!actual->block || !actual->code)
      dyna_stop();
}
#endif

void r4300_execute(void)
{
    current_instruction_table = cached_interpreter_table;

    stop = 0;

    last_addr = 0xa4000040;
    next_interrupt = 624999;
    init_interrupt();

    if (r4300emu == CORE_PURE_INTERPRETER)
    {
        DebugMessage(M64MSG_INFO, "Starting R4300 emulator: Pure Interpreter");
        r4300emu = CORE_PURE_INTERPRETER;
        pure_interpreter();
    }
#if defined(DYNAREC)
    else if (r4300emu >= 2)
    {
        DebugMessage(M64MSG_INFO, "Starting R4300 emulator: Dynamic Recompiler");
        r4300emu = CORE_DYNAREC;
        init_blocks();

#ifdef NEW_DYNAREC
        new_dynarec_init();
        new_dyna_start();
        new_dynarec_cleanup();
#else
        dyna_start(dynarec_setup_code);
        PC++;
#endif
        free_blocks();
    }
#endif
    else /* if (r4300emu == CORE_INTERPRETER) */
    {
        DebugMessage(M64MSG_INFO, "Starting R4300 emulator: Cached Interpreter");
        r4300emu = CORE_INTERPRETER;
        init_blocks();
        jump_to(UINT32_C(0xa4000040));

        /* Prevent segfault on failed jump_to */
        if (!actual->block)
            return;

        last_addr = PC->addr;

        r4300_step();

        free_blocks();
    }

    DebugMessage(M64MSG_INFO, "R4300 emulator finished.");
}

void r4300_step(void)
{
   while (!stop)
      PC->ops();
}
