#include "header/cpu/gdt.h"

/**
 * global_descriptor_table, predefined GDT.
 * Initial SegmentDescriptor already set properly according to Intel Manual & OSDev.
 * Table entry : [{Null Descriptor}, {Kernel Code}, {Kernel Data (variable, etc)}, ...].
 */
struct GlobalDescriptorTable global_descriptor_table = {
    .table = {
        {
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        },
        {
            0xFFFF, 0, 0, 0xA, 1, 0, 1, 0xF, 0, 0, 1, 1, 0
        },
        {
            0xFFFF, 0, 0, 0x2, 1, 0, 1, 0xF, 0, 0, 1, 1, 0
        }
    }
};

/**
 * _gdt_gdtr, predefined system GDTR. 
 * GDT pointed by this variable is already set to point global_descriptor_table above.
 * From: https://wiki.osdev.org/Global_Descriptor_Table, GDTR.size is GDT size minus 1.
 */
struct GDTR _gdt_gdtr = {
    .size = sizeof(struct GlobalDescriptorTable) - 1,
    .address = &global_descriptor_table
};