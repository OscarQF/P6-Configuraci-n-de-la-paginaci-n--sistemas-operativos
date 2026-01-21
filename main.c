/* This file contains the main program of MINIX as well as its shutdown code.
   * The routine main() initializes the system and starts the ball rolling by
   * setting up the process table, interrupt vectors, and scheduling each task
   * to run to initialize itself.
   * The routine shutdown() does the opposite and brings down MINIX.
   *
   * The entries into this file are:
   *   main:	    	MINIX main program
   *   prepare_shutdown:	prepare to take MINIX down
   *
   * Changes:
   *   Nov 24, 2004   simplified main() with system image  (Jorrit N. Herder)
   *   Aug 20, 2004   new prepare_shutdown() and shutdown()  (Jorrit N. Herder)
   */
#include "kernel.h"
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <a.out.h>
#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/endpoint.h>
#include <stdio.h>
#include <stdint.h> /* Agregado para poder usar uint32_t */
#include <sys/vm.h> /* PAGE_SIZE, I386_VM_PRESENT, ... , I386_CR0_PG */
#include "proc.h"
#include "e9_system.h"

/* Prototype declarations for PRIVATE functions. */
FORWARD _PROTOTYPE(void announce, (void));
FORWARD _PROTOTYPE(void shutdown, (timer_t * tp));

/* Macros para paginación */
#define PAGE_SIZE 4096
#define PTE_SIZE 4
#define CEIL_DIV(x, y) (((x) + (y) - 1) / (y))
#define MEM_SIZE_MB 128
#define NUM_PAGS_MAPEADAS (MEM_SIZE_MB*1048576)/PAGE_SIZE
#define NUM_PAGS_PT CEIL_DIV(NUM_PAGS_MAPEADAS*PTE_SIZE,PAGE_SIZE)
#define NUM_PAGS_PD 1
#define ALIGN_UP(addr, size) (addr+(size-1))&(~(size-1))
#define OFFSET_SEG_TO_LINEAR(ptr) ((uint32_t)kinfo.data_base + (uint32_t)(ptr))
#define PAG_EXTRA 1


/* Tipos para paginación */
typedef uint32_t pte_t;
typedef uint32_t pde_t;

uint32_t rm[(NUM_PAGS_PT + NUM_PAGS_PD + PAG_EXTRA) * PAGE_SIZE / sizeof(pde_t)];
/*===========================================================================*
 *                            configurar_paginacion
 *===========================================================================*/


PRIVATE void configurar_paginacion(void)
{
    pte_t* pt;
    int* var;
    uint32_t i;
    uint32_t npm = NUM_PAGS_MAPEADAS -1 ;
    uint32_t marco_pt;
    uint32_t m;
    unsigned cr0;
    uint32_t pd_linear;

    pde_t* pd;
    e9_printf("MEM_SIZE_MB: %d\n", MEM_SIZE_MB);
    e9_printf("NUM_PAGS_MAPEADAS: %u\n", NUM_PAGS_MAPEADAS);
    e9_printf("NUM_PAGS_PT: %u\n", NUM_PAGS_PT);
    e9_printf("NUM_PAGS_PD: %u\n", NUM_PAGS_PD);
    e9_printf("rm:\ndesp. en el segmento: 0x%08X\n", rm);
    e9_printf("dir. lineal: 0x%08X\n", (uint32_t)OFFSET_SEG_TO_LINEAR(rm));

    pt = (pte_t*)(ALIGN_UP ((uint32_t)rm, PAGE_SIZE));
    e9_printf("pt:\ndesp. en el segmento (pt): 0x%08X\n", pt);
    e9_printf("dir. lineal (pt): 0x%08X\n\n", (uint32_t)OFFSET_SEG_TO_LINEAR(pt));

    for (i = 0; i < NUM_PAGS_MAPEADAS; i++)
    {
        pt[i] = (i << I386_VM_PT_ENT_SHIFT) | (I386_VM_PRESENT | I386_VM_WRITE | I386_VM_USER);
    }
    e9_printf("pt[0]: 0x%08X\n", pt[0]);
    e9_printf("pt[1]: 0x%08X\n", pt[1]);
    e9_printf("pt[2]: 0x%08X\n", pt[2]);
    e9_printf("pt[%d]: 0x%08X", npm, pt[npm]);

    marco_pt = (uint32_t)(OFFSET_SEG_TO_LINEAR(pt)) >> I386_VM_PT_ENT_SHIFT;
    e9_printf("\n\nmarco_pt 0x%08X", marco_pt);

    pd = (pde_t*) (((uint32_t) pt) + (NUM_PAGS_PT*PAGE_SIZE));
    e9_printf("\n\npd:\ndesp. en el segmento (pd): 0x%08X\n", pd);
    e9_printf("dir. lineal (pd): 0x%08X\n\n", (uint32_t)OFFSET_SEG_TO_LINEAR(pd));

    for (i = 0; i < NUM_PAGS_PT; i++)
        {
        m = marco_pt + i;
        pd[i] = (m << I386_VM_PT_ENT_SHIFT) |(I386_VM_PRESENT | I386_VM_WRITE | I386_VM_USER);
        }
    for (i = NUM_PAGS_PT; i < I386_VM_DIR_ENTRIES; i++) {
        pd[i] = 0x00000000;
        }
    for (i=0; i<9; i++)
        {
            e9_printf("pd[%d]: 0x%08X\n", i, pd[i]);
        }

    pd_linear = (uint32_t)(OFFSET_SEG_TO_LINEAR(pd));
    write_cr3(pd_linear);
    e9_printf("direccion escrita en cr3: 0x%08X\n", pd_linear);
    cr0 = read_cr0();
    cr0 = cr0 | I386_CR0_PG;
    write_cr0(cr0);

}