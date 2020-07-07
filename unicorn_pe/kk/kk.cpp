#include "kk.h"

#define ADDRESS 0x1000000
#define X86_CODE32 "\x41\x4a\x66\x0f\xef\xc1\xc3\x41\x4a\x66\x0f\xef\xc1\xc3\x41\x4a\x66\x0f\xef\xc1\xc3" // INC ecx; DEC edx; PXOR xmm0, xmm1

csh m_cs;

// callback for tracing basic blocks
static void hook_block(uc_engine* uc, uint64_t address, uint32_t size, void* user_data)
{
	printf(">>> Tracing basic block at 0x%llx, block size = 0x%x\n", address, size);
}

// callback for tracing instruction
static void hook_code(uc_engine* uc, uint64_t address, uint32_t size, void* user_data)
{
	//int eflags;
	//printf(">>> Tracing instruction at 0x%llx, instruction size = 0x%x\n", address, size);
	//uc_reg_read(uc, UC_X86_REG_EFLAGS, &eflags);
	//printf(">>> --- EFLAGS is 0x%x\n", eflags);

	// print it
	unsigned char codeBuffer[15];
	uc_mem_read(uc, address, codeBuffer, size);
	cs_insn insn;
	memset(&insn, 0, sizeof(insn));
	uint64_t virtualBase = address;
	uint8_t* code = codeBuffer;
	size_t codeSize = size;
	cs_disasm_iter(m_cs, (const uint8_t**)&code, &codeSize, &virtualBase, &insn);
	printf("%llx %s %s\n", address, insn.mnemonic, insn.op_str);
	// Uncomment below code to stop the emulation using uc_emu_stop()
	// if (address == 0x1000009)
	//    uc_emu_stop(uc);
}

void test_i386(void)
{
	uc_engine* uc;
	uc_err err;
	uint32_t tmp;
	uc_hook trace1, trace2;

	int r_ecx = 0x1234;     // ECX register
	int r_edx = 0x7890;     // EDX register
	// XMM0 and XMM1 registers, low qword then high qword
	uint64_t r_xmm0[2] = { 0x08090a0b0c0d0e0f, 0x0001020304050607 };
	uint64_t r_xmm1[2] = { 0x8090a0b0c0d0e0f0, 0x0010203040506070 };

	printf("Emulate i386 code\n");

	// Initialize emulator in X86-32bit mode
	err = uc_open(UC_ARCH_X86, UC_MODE_32, &uc);
	if (err) {
		printf("Failed on uc_open() with error returned: %u\n", err);
		return;
	}

	cs_err err2 = cs_open(CS_ARCH_X86, CS_MODE_32, &m_cs);
	if (err2) {
		printf("failed to open cs with error returned %x\n", err2);
		return;
	}
	// map 2MB memory for this emulation
	uc_mem_map(uc, ADDRESS, 2 * 1024 * 1024, UC_PROT_ALL);

	// write machine code to be emulated to memory
	if (uc_mem_write(uc, ADDRESS, X86_CODE32, sizeof(X86_CODE32) - 1)) {
		printf("Failed to write emulation code to memory, quit!\n");
		return;
	}

	// initialize machine registers
	uc_reg_write(uc, UC_X86_REG_ECX, &r_ecx);
	uc_reg_write(uc, UC_X86_REG_EDX, &r_edx);
	uc_reg_write(uc, UC_X86_REG_XMM0, &r_xmm0);
	uc_reg_write(uc, UC_X86_REG_XMM1, &r_xmm1);

	// tracing all basic blocks with customized callback
	uc_hook_add(uc, &trace1, UC_HOOK_BLOCK, hook_block, NULL, 1, 0);

	// tracing all instruction by having @begin > @end
	uc_hook_add(uc, &trace2, UC_HOOK_CODE, hook_code, NULL, 1, 0);

	// emulate machine code in infinite time
	err = uc_emu_start(uc, ADDRESS, ADDRESS + sizeof(X86_CODE32) - 1, 0, 0);
	if (err) {
		printf("Failed on uc_emu_start() with error returned %u: %s\n",
			err, uc_strerror(err));
	}

	// now print out some registers
	uc_reg_read(uc, UC_X86_REG_ECX, &r_ecx);
	uc_reg_read(uc, UC_X86_REG_EDX, &r_edx);
	uc_reg_read(uc, UC_X86_REG_XMM0, &r_xmm0);
	printf(">>> ECX = 0x%x\n", r_ecx);
	printf(">>> EDX = 0x%x\n", r_edx);
	printf(">>> XMM0 = 0x%.16llx%.16llx\n", r_xmm0[1], r_xmm0[0]);

	// read from memory
	if (!uc_mem_read(uc, ADDRESS, &tmp, sizeof(tmp)))
		printf(">>> Read 4 bytes from [0x%x] = 0x%x\n", ADDRESS, tmp);
	else
		printf(">>> Failed to read 4 bytes from [0x%x]\n", ADDRESS);

	uc_close(uc);
}

int kk_main(int argc, char** argv)
{
	printf("kk main called! \n");
	test_i386();
	system("pause");

	return 0;
}