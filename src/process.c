#include "header/process/process.h"
#include "header/memory/paging.h"
#include "header/stdlib/string.h"
#include "header/cpu/gdt.h"

struct ProcessControlBlock _process_list[PROCESS_COUNT_MAX];

static uint32_t next_pid = 1;

static struct ProcessManagerState process_manager_state = {
    .process_frame_map = {
        [0 ... PROCESS_COUNT_MAX-1] = false
    },
    .active_process_count = 0
};

int ceil_div(int a, int b) {
    return (a + b - 1) / b;
}

int32_t process_list_get_inactive_index() {
    for (int i = 0; i < PROCESS_COUNT_MAX; i++) {
        if (!process_manager_state.process_frame_map[i]) {
            return i;
        }
    }
    return -1;
}

uint32_t process_generate_new_pid() {
    return next_pid++;
}

bool process_destroy(uint32_t pid) {
    struct ProcessControlBlock* process = NULL;
    for (uint32_t i = 0; i < PROCESS_COUNT_MAX; i++) {
        if (_process_list[i].metadata.pid == pid) {
            process = &_process_list[i];
            break;
        }
    }
    
    if (process == NULL) {
        return false;
    }

    for (uint32_t i = 0; i < process->memory.page_frame_used_count; i++) {
        void* virtual_addr = process->memory.virtual_addr_used[i];
        paging_free_user_page_frame((struct PageDirectory*) process->context.page_directory_virtual_addr, virtual_addr);
    }

    process->memory.page_frame_used_count = 0;

    for (uint32_t i = 0; i < PROCESS_COUNT_MAX; i++) {
        if (_process_list[i].metadata.pid == pid) {
            _process_list[i].metadata.pid = 0;
            _process_list[i].metadata.state = READY;
            _process_list[i].context.page_directory_virtual_addr = 0;
            break;
        }
    }

    for (uint32_t i = 0; i < PROCESS_COUNT_MAX; i++) {
        if (_process_list[i].metadata.pid == 0) {
            process_manager_state.process_frame_map[i] = false;
        }
    }
    
    process_manager_state.active_process_count--;

    return true;
}


int32_t process_create_user_process(struct FAT32DriverRequest request) {
    int32_t retcode = PROCESS_CREATE_SUCCESS; 
    if (process_manager_state.active_process_count >= PROCESS_COUNT_MAX) {
        retcode = PROCESS_CREATE_FAIL_MAX_PROCESS_EXCEEDED;
        goto exit_cleanup;
    }

    // Ensure entrypoint is not located at kernel's section at higher half
    if ((uint32_t) request.buf >= KERNEL_VIRTUAL_ADDRESS_BASE) {
        retcode = PROCESS_CREATE_FAIL_INVALID_ENTRYPOINT;
        goto exit_cleanup;
    }

    // Check whether memory is enough for the executable and additional frame for user stack
    uint32_t page_frame_count_needed = ceil_div(request.buffer_size + PAGE_FRAME_SIZE, PAGE_FRAME_SIZE);
    if (!paging_allocate_check(page_frame_count_needed) || page_frame_count_needed > PROCESS_PAGE_FRAME_COUNT_MAX) {
        retcode = PROCESS_CREATE_FAIL_NOT_ENOUGH_MEMORY;
        goto exit_cleanup;
    }

    // Process PCB 

    // virtual address space
    int32_t p_index = process_list_get_inactive_index();
    struct ProcessControlBlock *new_pcb = &(_process_list[p_index]);

    new_pcb->metadata.pid = process_generate_new_pid();
    new_pcb->metadata.state = READY;

    new_pcb->context.page_directory_virtual_addr = paging_create_new_page_directory();

    void* temp = paging_get_current_page_directory_addr();
    
    paging_allocate_user_page_frame(new_pcb->context.page_directory_virtual_addr, request.buf);

    paging_use_page_directory(new_pcb->context.page_directory_virtual_addr);
    // paging_use_page_directory((struct PageDirectory*)new_pcb->context.page_directory_virtual_addr);

    // struct PageDirectory* temp = paging_get_current_page_directory_addr();

    // // load executable  
    // new_pcb->context.page_directory_virtual_addr = (uint32_t) paging_create_new_page_directory();
    // paging_use_page_directory((struct PageDirectory*) new_pcb->context.page_directory_virtual_addr);

    int8_t read_retval = read(request); 

    if (read_retval != 0) {
        paging_use_page_directory(temp);
        process_destroy(new_pcb->metadata.pid);
        retcode = PROCESS_CREATE_FAIL_FS_READ_FAILURE;
        goto exit_cleanup;  
    }

    // context initialization
    memset(&new_pcb->context.cpu, 0, sizeof(new_pcb->context.cpu));
    // new_pcb->context.eip = (uint32_t)request.buf;
    new_pcb->context.eflags = CPU_EFLAGS_BASE_FLAG | CPU_EFLAGS_FLAG_INTERRUPT_ENABLE;

    // new_pcb->context.cpu.segment.ds = GDT_USER_DATA_SEGMENT_SELECTOR | 3;
    // new_pcb->context.cpu.segment.es = GDT_USER_DATA_SEGMENT_SELECTOR | 3;
    // new_pcb->context.cpu.segment.fs = GDT_USER_DATA_SEGMENT_SELECTOR | 3;
    // new_pcb->context.cpu.segment.gs = GDT_USER_DATA_SEGMENT_SELECTOR | 3;
    // new_pcb->context.cpu.stack.ebp = 0xBFFFFFFC;
    new_pcb->context.cpu.stack.esp = 0xBFFFFFFC;

    process_manager_state.process_frame_map[p_index] = true;
    process_manager_state.active_process_count++;    

    return retcode;

exit_cleanup:
    return retcode;
}