/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - cp0.c                                                   *
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
#include <string.h>

#include "cp0_private.h"
#include "exception.h"
#include "new_dynarec/new_dynarec.h"
#include "r4300.h"
#include "recomp.h"

#ifdef DBG
#include "debugger/dbg_debugger.h"
#include "debugger/dbg_types.h"
#endif

/* global variable */
#if NEW_DYNAREC != NEW_DYNAREC_ARM
/* ARM backend requires a different memory layout
 * and therefore manually allocate that variable */
uint32_t g_cp0_regs[CP0_REGS_COUNT];
#endif

void init_cp0(unsigned int _count_per_op)
{
#if 0
   count_per_op = _count_per_op;
#endif
}

void poweron_cp0(void)
{
   memset(g_cp0_regs, 0, CP0_REGS_COUNT * sizeof(g_cp0_regs[0]));
   g_cp0_regs[CP0_RANDOM_REG] = UINT32_C(31);
   g_cp0_regs[CP0_STATUS_REG]= UINT32_C(0x34000000);
   set_fpr_pointers(g_cp0_regs[CP0_STATUS_REG]);
   g_cp0_regs[CP0_CONFIG_REG]= UINT32_C(0x6e463);
   g_cp0_regs[CP0_PREVID_REG] = UINT32_C(0xb00);
   g_cp0_regs[CP0_COUNT_REG] = UINT32_C(0x5000);
   g_cp0_regs[CP0_CAUSE_REG] = UINT32_C(0x5C);
   g_cp0_regs[CP0_CONTEXT_REG] = UINT32_C(0x7FFFF0);
   g_cp0_regs[CP0_EPC_REG] = UINT32_C(0xFFFFFFFF);
   g_cp0_regs[CP0_BADVADDR_REG] = UINT32_C(0xFFFFFFFF);
   g_cp0_regs[CP0_ERROREPC_REG] = UINT32_C(0xFFFFFFFF);

   poweron_tlb();
}

/* global functions */
uint32_t* r4300_cp0_regs(void)
{
    return g_cp0_regs;
}

int check_cop1_unusable(void)
{
   if (!(g_cp0_regs[CP0_STATUS_REG] & CP0_STATUS_CU1))
   {
      g_cp0_regs[CP0_CAUSE_REG] = CP0_CAUSE_EXCCODE_CPU | CP0_CAUSE_CE1;
      exception_general();
      return 1;
   }
   return 0;
}

void cp0_update_count(void)
{
#ifdef NEW_DYNAREC
   if (r4300emu != CORE_DYNAREC)
   {
#endif
      g_cp0_regs[CP0_COUNT_REG] += ((PC->addr - last_addr) >> 2) * count_per_op;
      last_addr = PC->addr;
#ifdef NEW_DYNAREC
   }
#endif
}
