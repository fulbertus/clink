// Copyright (c) 2012 Martin Ridgers
// License: http://opensource.org/licenses/MIT

#include "pch.h"
#include "hook.h"
#include "vm.h"
#include "pe.h"

#include <core/base.h>
#include <core/log.h>

//------------------------------------------------------------------------------
struct repair_iat_node
{
    repair_iat_node* m_next;
    hookptr_t* m_iat;
    hookptr_t m_trampoline;
};

//------------------------------------------------------------------------------
static void write_addr(hookptr_t* where, hookptr_t to_write)
{
    vm vm;
    vm::region region = { vm.get_page(where), 1 };
    unsigned int prev_access = vm.get_access(region);
    vm.set_access(region, vm::access_write);

    if (!vm.write(where, &to_write, sizeof(to_write)))
        LOG("VM write to %p failed (err = %d)", where, GetLastError());

    vm.set_access(region, prev_access);
}

//------------------------------------------------------------------------------
static hookptr_t get_proc_addr(const char* dll, const char* func_name)
{
    if (void* base = LoadLibraryA(dll))
        return (hookptr_t)pe_info(base).get_export(func_name);

    LOG("Failed to load library '%s'", dll);
    return nullptr;
}

//------------------------------------------------------------------------------
hookptr_t hook_iat(
    void* base,
    const char* dll,
    const char* func_name,
    hookptr_t hook,
    int find_by_name
)
{
    LOG("Attempting to hook IAT for module %p.", base);
    if (find_by_name)
        LOG("Target is %s (by name).", func_name);
    else
        LOG("Target is %s in %s (by address).", func_name, dll);

    hookptr_t* import;

    // Find entry and replace it.
    pe_info pe(base);
    if (find_by_name)
        import = (hookptr_t*)pe.get_import_by_name(nullptr, func_name);
    else
    {
        // Get the address of the function we're going to hook.
        hookptr_t func_addr = get_proc_addr(dll, func_name);
        if (func_addr == nullptr)
        {
            LOG("Failed to find %s in %s.", func_name, dll);
            return nullptr;
        }

        LOG("Looking up import by address %p.", func_addr);
        import = (hookptr_t*)pe.get_import_by_addr(nullptr, (pe_info::funcptr_t)func_addr);
    }

    if (import == nullptr)
    {
        LOG("Unable to find import in IAT.");
        return nullptr;
    }

    LOG("Found import at %p (value is %p).", import, *import);

    hookptr_t prev_addr = *import;
    write_addr(import, hook);

    vm().flush_icache();
    return prev_addr;
}

//------------------------------------------------------------------------------
static char* alloc_trampoline(void* hint)
{
    size_t alloc_granularity = vm::get_block_granularity();
    size_t page_size = vm::get_page_size();

    vm vm;
    hookptr_t trampoline = nullptr;
    while (trampoline == nullptr)
    {
        void* vm_alloc_base = vm.get_alloc_base(hint);
        vm_alloc_base = vm_alloc_base ? vm_alloc_base : hint;

        char* tramp_page = (char*)vm_alloc_base - alloc_granularity;

        trampoline = hookptr_t(VirtualAlloc(tramp_page, page_size,
            MEM_COMMIT|MEM_RESERVE, PAGE_EXECUTE_READWRITE));

        hint = tramp_page;
    }

    return (char*)trampoline;
}

//------------------------------------------------------------------------------
static int get_mask_size(unsigned mask)
{
    mask &= 0x01010101;
    mask += mask >> 16;
    mask += mask >> 8;
    return mask & 0x0f;
}

//------------------------------------------------------------------------------
static char* write_rel_jmp(char* write, const void* dest)
{
    struct {
        char a;
        char b[4];
    } buffer;

    // jmp <displacement>
    intptr_t disp = (intptr_t)dest;
    disp -= (intptr_t)write;
    disp -= sizeof(buffer);

    buffer.a = (unsigned char)0xe9;
    *(int*)buffer.b = (int)disp;

    if (!vm().write(write, &buffer, sizeof(buffer)))
    {
        LOG("VM write to %p failed (err = %d)", write, GetLastError());
        return nullptr;
    }

    return write + sizeof(buffer);
}

//------------------------------------------------------------------------------
static char* write_trampoline_out(void* const dest, void* const to_hook, hookptr_t const hook)
{
    const int rel_jmp_size = 5;
    int offset = 0;
    char* write = (char*)dest;
    char* patch = (char*)to_hook;

    // Scan backwards for a nop slide or int3 block to patch into.
    int viable_bytes = 0;
    while (viable_bytes < rel_jmp_size)
    {
        patch--;
        offset++;

        unsigned char c = *patch;
        if (offset > 127){
            LOG("No nop slide or int3 block detected nearby prior to hook target, checked %d prior bytes", offset-1);
            LOG("Now checking bytes after hook target");
            // reset for checking forwards
            viable_bytes = 0;
            offset = 0;
            patch = (char*)to_hook;
            break;
        } else if (c != 0x90 && c != 0xcc)
            viable_bytes = 0;
        else
            viable_bytes++;
    }

    while (viable_bytes < rel_jmp_size)
    {
        patch++;
        offset--;

        if (offset < -131)
        {
            LOG("No nop slide or int3 block detected nearby after hook target, checked %d later bytes", (-1 * (offset+1)));
            return nullptr;
        }
        unsigned char c = *patch;
        if (c != 0x90 && c != 0xcc)
            viable_bytes = 0;
        else
            viable_bytes++;
    }

    // If we are patching after rather than before
    if (offset < 0){
        // Offset is currently on the fifth byte of our patch area, so back up
        offset += 4;
        patch -= 4;
    }
    LOG("Patching at offset %d", -1 * (offset));

    // Patch the API.
    patch = write_rel_jmp(patch, write);
    unsigned char temp[2] = { 0xeb, 0xfe };
    temp[1] -= offset;
    if (!vm().write(to_hook, &temp, sizeof(temp)))
    {
        LOG("VM write to %p failed (err = %d)", patch, GetLastError());
        return nullptr;
    }

    // Long jmp.
    struct {
        char a[2];
        char b[4];
        char c[sizeof(hookptr_t)];
    } inst;

    *(short*)inst.a = 0x25ff;

    unsigned rel_addr = 0;
#ifdef _M_IX86
    rel_addr = (intptr_t)write + 6;
#endif

    *(int*)inst.b = rel_addr;
    *(hookptr_t*)inst.c = hook;

    if (!vm().write(write, &inst, sizeof(inst)))
    {
        LOG("VM write to %p failed (err = %d)", write, GetLastError());
        return nullptr;
    }

    return write + sizeof(inst);
}

//------------------------------------------------------------------------------
static void* write_trampoline_in(void* dest, void* to_hook, int n)
{
    char* write = (char*)dest;

    for (int i = 0; i < n; ++i)
    {
        if (!vm().write(write, (char*)to_hook + i, 1))
        {
            LOG("VM write to %p failed (err = %d)", write, GetLastError());
            return nullptr;
        }

        ++write;
    }

    // If the moved instruction is JMP (e9) then the displacement is relative
    // to its original location. As we have relocated the jump the displacement
    // needs adjusting.
    if (*(unsigned char*)to_hook == 0xe9)
    {
        int displacement = *(int*)(write - 4);
        intptr_t old_ip = (intptr_t)to_hook + n;
        intptr_t new_ip = (intptr_t)write;

        *(int*)(write - 4) = (int)(displacement + old_ip - new_ip);
    }

    return write_rel_jmp(write, (char*)to_hook + n);
}

//------------------------------------------------------------------------------
static int get_instruction_length(const void* addr)
{
    static const struct {
        unsigned expected;
        unsigned mask;
    } asm_tags[] = {
#ifdef _M_X64
        { 0x38ec8348, 0xffffffff },  // sub rsp,38h
        { 0x0000f3ff, 0x0000ffff },  // push rbx
        { 0x00005340, 0x0000ffff },  // push rbx
        { 0x00dc8b4c, 0x00ffffff },  // mov r11, rsp
        { 0x0000b848, 0x0000f8ff },  // mov reg64, imm64  = 10-byte length
#elif defined _M_IX86
        { 0x0000ff8b, 0x0000ffff },  // mov edi,edi
#endif
        { 0x000000e9, 0x000000ff },  // jmp addr32        = 5-byte length
    };

    unsigned prolog = *(unsigned*)(addr);
    for (int i = 0; i < sizeof_array(asm_tags); ++i)
    {
        unsigned expected = asm_tags[i].expected;
        unsigned mask = asm_tags[i].mask;

        if (expected != (prolog & mask))
            continue;

        int length = get_mask_size(mask);

        // Checks for instructions that "expected" only partially matches.
        if (expected == 0x0000b848)
            length = 10;
        else if (expected == 0xe9)
            length = 5; // jmp [imm32]

        LOG("Matched prolog %08X (mask = %08X)", prolog, mask);
        return length;
    }

    return 0;
}

//------------------------------------------------------------------------------
void* follow_jump(void* addr)
{
    unsigned char* t = (unsigned char*)addr;

    // Check the opcode.
    if ((t[0] & 0xf0) == 0x40) // REX prefix.
        ++t;

    if (t[0] != 0xff)
        return addr;

    // Check the opcode extension from the modr/m byte.
    if ((t[1] & 070) != 040)
        return addr;

    int* imm = (int*)(t + 2);

    void* dest = addr;
    switch (t[1] & 007)
    {
    case 5:
#ifdef _M_X64
        // dest = [rip + disp32]
        dest = *(void**)(t + 6 + *imm);
#elif defined _M_IX86
        // dest = disp32
        dest = (void*)(intptr_t)(*imm);
#endif
    }

    LOG("Following jump to %p", dest);
    return dest;
}

//------------------------------------------------------------------------------
bool add_repair_iat_node(
    repair_iat_node*& list,
    void* base,
    const char* dll,
    const char* func_name,
    hookptr_t trampoline,
    bool find_by_name
)
{
    LOG("Attempting to hook IAT for module %p (repair).", base);

    hookptr_t* import;

    // Find entry and replace it.
    pe_info pe(base);
    if (find_by_name)
    {
        LOG("Target is %s (by name).", func_name);
        import = (hookptr_t*)pe.get_import_by_name(nullptr, func_name);
    }
    else
    {
        LOG("Target is %s in %s (by address).", func_name, dll);

        // Get the address of the function we're going to hook.
        hookptr_t func_addr = get_proc_addr(dll, func_name);
        if (func_addr == nullptr)
        {
            LOG("Failed to find %s in %s.", func_name, dll);
            return false;
        }

        LOG("Looking up import by address %p.", func_addr);
        import = (hookptr_t*)pe.get_import_by_addr(nullptr, (pe_info::funcptr_t)func_addr);
    }

    if (import == nullptr)
    {
        LOG("Unable to find import in IAT.");
        return false;
    }

    LOG("Found import at %p (value is %p).", import, *import);

    repair_iat_node* r = new repair_iat_node;
    r->m_next = list;
    r->m_iat = import;
    r->m_trampoline = trampoline;
    list = r;
    return true;
}

void apply_repair_iat_list(repair_iat_node*& list)
{
    vm vm;

    while (list)
    {
        repair_iat_node* r = list;
        list = list->m_next;

        // TODO: need to somehow preserve prev_addr in order for detach to work correctly.
        hookptr_t prev_addr = *r->m_iat;
        write_addr(r->m_iat, r->m_trampoline);

        delete r;
    }

    vm.flush_icache();
}

void free_repair_iat_list(repair_iat_node*& list)
{
    while (list)
    {
        repair_iat_node* r = list;
        list = list->m_next;
        delete r;
    }
}
