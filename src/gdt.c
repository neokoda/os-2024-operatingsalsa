#include "header/cpu/gdt.h"

/**
 * global_descriptor_table, predefined GDT.
 * Initial SegmentDescriptor already set properly according to Intel Manual & OSDev.
 * Table entry : [{Null Descriptor}, {Kernel Code}, {Kernel Data (variable, etc)}, ...].
 */
static struct GlobalDescriptorTable global_descriptor_table = {
    .table = {
        {0}, // Null Descriptor
        {0xFFFF, 0, 0, 0xA, 1, 0, 1, 0xF, 0, 0, 1, 1, 0}, // Kernel Code Descriptor
        {0xFFFF, 0, 0, 0x2, 1, 0, 1, 0xF, 0, 0, 1, 1, 0}, // Kernel Data Descriptor
        // User Code Descriptor
        {
            .segment_high      = 0xF,
            .segment_low       = 0xFFFF,
            .base_high         = 0,
            .base_mid          = 0,
            .base_low          = 0,
            .non_system        = 1,    // S bit
            .type_bit          = 0xA,  // Code segment, read/execute, accessed
            .privilege         = 0x3,  // DPL 3 for user space
            .valid_bit         = 1,    // Present
            .opr_32_bit        = 1,    // 32-bit operation
            .long_mode         = 1,    // Long mode
            .granularity       = 1,    // Granularity, 4 KB
        },
        // User Data Descriptor
        {
            .segment_high      = 0xF,
            .segment_low       = 0xFFFF,
            .base_high         = 0,
            .base_mid          = 0,
            .base_low          = 0,
            .non_system        = 1,    // S bit
            .type_bit          = 0x2,  // Data segment, read/write, accessed
            .privilege         = 0x3,  // DPL 3 for user space
            .valid_bit         = 1,    // Present
            .opr_32_bit        = 1,    // 32-bit operation
            .long_mode         = 0,    // Not applicable for data segments
            .granularity       = 1,    // Granularity, 4 KB
        },
        // TSS Descriptor
        {
            .segment_high      = (sizeof(struct TSSEntry) & (0xF << 16)) >> 16,
            .segment_low       = sizeof(struct TSSEntry),
            .base_high         = 0,
            .base_mid          = 0,
            .base_low          = 0,
            .non_system        = 0,    // S bit
            .type_bit          = 0x9,
            .privilege         = 0,    // DPL
            .valid_bit         = 1,    // P bit
            .opr_32_bit        = 1,    // D/B bit
            .long_mode         = 0,    // L bit
            .granularity       = 0,    // G bit
        },
        // End of Table
        {0}
    }
};

void gdt_install_tss(void) {
    uint32_t base = (uint32_t) &_interrupt_tss_entry;
    global_descriptor_table.table[5].base_high = (base & (0xFF << 24)) >> 24;
    global_descriptor_table.table[5].base_mid  = (base & (0xFF << 16)) >> 16;
    global_descriptor_table.table[5].base_low  = base & 0xFFFF;
}


/**
 * _gdt_gdtr, predefined system GDTR. 
 * GDT pointed by this variable is already set to point global_descriptor_table above.
 * From: https://wiki.osdev.org/Global_Descriptor_Table, GDTR.size is GDT size minus 1.
 */
struct GDTR _gdt_gdtr = {
    .size = sizeof(struct GlobalDescriptorTable) - 1,
    .address = &global_descriptor_table
};