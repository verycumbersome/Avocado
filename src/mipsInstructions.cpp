#include "mipsInstructions.h"
#include <string>
#include "utils/string.h"

PrimaryInstruction OpcodeTable[64] =
{
	{ 0, mipsInstructions::special, "special" }, // R type
	{ 1, mipsInstructions::branch, "branch" },
	{ 2, mipsInstructions::j, "j" },
	{ 3, mipsInstructions::jal, "jal" },
	{ 4, mipsInstructions::beq, "beq" },
	{ 5, mipsInstructions::bne, "bne" },
	{ 6, mipsInstructions::blez, "blez" },
	{ 7, mipsInstructions::bgtz, "bgtz" },

	{ 8, mipsInstructions::addi, "addi" },
	{ 9, mipsInstructions::addiu, "addiu" },
	{ 10, mipsInstructions::slti, "slti" },
	{ 11, mipsInstructions::sltiu, "sltiu" },
	{ 12, mipsInstructions::andi, "andi" },
	{ 13, mipsInstructions::ori, "ori" },
	{ 14, mipsInstructions::xori, "xori" },
	{ 15, mipsInstructions::lui, "lui" },

	{ 16, mipsInstructions::cop0, "cop0" },
	{ 17, mipsInstructions::notImplemented, "cop1" },
	{ 18, mipsInstructions::notImplemented, "cop2" },
	{ 19, mipsInstructions::notImplemented, "cop3" },
	{ 20, mipsInstructions::invalid, "INVALID" },
	{ 21, mipsInstructions::invalid, "INVALID" },
	{ 22, mipsInstructions::invalid, "INVALID" },
	{ 23, mipsInstructions::invalid, "INVALID" },

	{ 24, mipsInstructions::invalid, "INVALID" },
	{ 25, mipsInstructions::invalid, "INVALID" },
	{ 26, mipsInstructions::invalid, "INVALID" },
	{ 27, mipsInstructions::invalid, "INVALID" },
	{ 28, mipsInstructions::invalid, "INVALID" },
	{ 29, mipsInstructions::invalid, "INVALID" },
	{ 30, mipsInstructions::invalid, "INVALID" },
	{ 31, mipsInstructions::invalid, "INVALID" },

	{ 32, mipsInstructions::lb, "lb" },
	{ 33, mipsInstructions::lh, "lh" },
	{ 34, mipsInstructions::notImplemented, "lwl" },
	{ 35, mipsInstructions::lw, "lw" },
	{ 36, mipsInstructions::lbu, "lbu" },
	{ 37, mipsInstructions::lbu, "lhu" },
	{ 38, mipsInstructions::notImplemented, "lwr" },
	{ 39, mipsInstructions::invalid, "INVALID" },

	{ 40, mipsInstructions::sb, "sb" },
	{ 41, mipsInstructions::sh, "sh" },
	{ 42, mipsInstructions::notImplemented, "swl" },
	{ 43, mipsInstructions::sw, "sw" },
	{ 44, mipsInstructions::invalid, "INVALID" },
	{ 45, mipsInstructions::invalid, "INVALID" },
	{ 46, mipsInstructions::notImplemented, "swr" },
	{ 47, mipsInstructions::invalid, "INVALID" },

	{ 48, mipsInstructions::notImplemented, "lwc0" },
	{ 49, mipsInstructions::notImplemented, "lwc1" },
	{ 50, mipsInstructions::notImplemented, "lwc2" },
	{ 51, mipsInstructions::notImplemented, "lwc3" },
	{ 52, mipsInstructions::invalid, "INVALID" },
	{ 53, mipsInstructions::invalid, "INVALID" },
	{ 54, mipsInstructions::invalid, "INVALID" },
	{ 55, mipsInstructions::invalid, "INVALID" },

	{ 56, mipsInstructions::notImplemented, "swc0" },
	{ 57, mipsInstructions::notImplemented, "swc1" },
	{ 58, mipsInstructions::notImplemented, "swc2" },
	{ 59, mipsInstructions::notImplemented, "swc3" },
	{ 60, mipsInstructions::invalid, "INVALID" },
	{ 61, mipsInstructions::invalid, "INVALID" },
	{ 62, mipsInstructions::invalid, "INVALID" },
	{ 63, mipsInstructions::invalid, "INVALID" },
};

extern bool disassemblyEnabled;
extern bool IsC;
extern std::string _mnemonic;
extern std::string _disasm;
extern std::string _pseudo;

uint8_t readMemory8(uint32_t address);
uint16_t readMemory16(uint32_t address);
uint32_t readMemory32(uint32_t address);
void writeMemory8(uint32_t address, uint8_t data);
void writeMemory16(uint32_t address, uint16_t data);
void writeMemory32(uint32_t address, uint32_t data);

namespace mipsInstructions
{
	void invalid(CPU* cpu, Opcode i)
	{
		printf("Invalid opcode (%s) at 0x%08x: 0x%08x\n", OpcodeTable[i.op].mnemnic, cpu->PC - 4, i.opcode);
		cpu->halted = true;
	}

	void notImplemented(CPU* cpu, Opcode i)
	{
		printf("Opcode %s not implemented at 0x%08x: 0x%08x\n", OpcodeTable[i.op].mnemnic, cpu->PC - 4, i.opcode);
		cpu->halted = true;
	}

	void special(CPU* cpu, Opcode i)
	{
		// Jump Register
		// JR rs
		if (i.fun == 8) {
			mnemonic("JR");
			disasm("r%d", i.rs);
			cpu->shouldJump = true;
			cpu->isJumpCycle = false;
			cpu->jumpPC = cpu->reg[i.rs];
		}

		// Jump Register
		// JALR
		else if (i.fun == 9) {
			mnemonic("JALR");
			disasm("r%d r%d", i.rd, i.rs);
			cpu->shouldJump = true;
			cpu->isJumpCycle = false;
			cpu->jumpPC = cpu->reg[i.rs];
			cpu->reg[i.rd] = cpu->PC + 4;
		}

		// Add
		// add rd, rs, rt
		else if (i.fun == 32) {
			mnemonic("ADD");
			disasm("r%d, r%d, r%d", i.rd, i.rs, i.rt);
			cpu->reg[i.rd] = ((int32_t)cpu->reg[i.rs]) + ((int32_t)cpu->reg[i.rt]);
		}

		// Add unsigned
		// add rd, rs, rt
		else if (i.fun == 33) {
			mnemonic("ADDU");
			disasm("r%d, r%d, r%d", i.rd, i.rs, i.rt);
			cpu->reg[i.rd] = ((int32_t)cpu->reg[i.rs]) + ((int32_t)cpu->reg[i.rt]);
		}

		// And
		// and rd, rs, rt
		else if (i.fun == 36) {
			mnemonic("AND");
			disasm("r%d, r%d, r%d", i.rd, i.rs, i.rt);

			cpu->reg[i.rd] = cpu->reg[i.rs] & cpu->reg[i.rt];
		}


		// TODO
		// Multiply
		// mul rs, rt
		else if (i.fun == 24) {
			mnemonic("MULT");
			disasm("r%d, r%d", i.rs, i.rt);

			uint64_t temp = cpu->reg[i.rs] * cpu->reg[i.rt];

			cpu->lo = temp & 0xffffffff;
			cpu->hi = (temp & 0xffffffff00000000) >> 32;
		}


		// Nor
		// NOR rd, rs, rt
		else if (i.fun == 39) {
			mnemonic("OR");
			disasm("r%d, r%d, r%d", i.rd, i.rs, i.rt);

			cpu->reg[i.rd] = ~(cpu->reg[i.rs] | cpu->reg[i.rt]);
		}

		// Or
		// OR rd, rs, rt
		else if (i.fun == 37) {
			mnemonic("OR");
			disasm("r%d, r%d, r%d", i.rd, i.rs, i.rt);

			cpu->reg[i.rd] = cpu->reg[i.rs] | cpu->reg[i.rt];
		}

		// Shift Word Left Logical
		// SLL rd, rt, a
		else if (i.fun == 0) {
			if (i.rt == 0 && i.rd == 0 && i.sh == 0) {
				mnemonic("NOP");
				disasm(" ");
			}
			else {
				mnemonic("SLL");
				disasm("r%d, r%d, %d", i.rd, i.rt, i.sh);

				cpu->reg[i.rd] = cpu->reg[i.rt] << i.sh;
			}
		}

		// Shift Word Left Logical Variable
		// SLLV rd, rt, rs
		else if (i.fun == 4) {
			mnemonic("SLLV");
			disasm("r%d, r%d, r%d", i.rd, i.rt, i.rs);

			cpu->reg[i.rd] = cpu->reg[i.rt] << (cpu->reg[i.rs] & 0x1f);
		}

		// Shift Word Right Arithmetic
		// SRA rd, rt, a
		else if (i.fun == 3) {
			mnemonic("SRA");
			disasm("r%d, r%d, %d", i.rd, i.rt, i.sh);

			cpu->reg[i.rd] = ((int32_t)cpu->reg[i.rt]) >> i.sh;
		}

		// Shift Word Right Arithmetic Variable
		// SRAV rd, rt, rs
		else if (i.fun == 7) {
			mnemonic("SRAV");
			disasm("r%d, r%d, r%d", i.rd, i.rt, i.rs);

			cpu->reg[i.rd] = ((int32_t)cpu->reg[i.rt]) >> (cpu->reg[i.rs] & 0x1f);
		}

		// Shift Word Right Logical
		// SRL rd, rt, a
		else if (i.fun == 2) {
			mnemonic("SRL");
			disasm("r%d, r%d, %d", i.rd, i.rt, i.sh);

			cpu->reg[i.rd] = cpu->reg[i.rt] >> i.sh;
		}

		// Shift Word Right Logical Variable
		// SRLV rd, rt, a
		else if (i.fun == 6) {
			mnemonic("SRLV");
			disasm("r%d, r%d, %d", i.rd, i.rt, i.sh);

			cpu->reg[i.rd] = cpu->reg[i.rt] >> (cpu->reg[i.rs] & 0x1f);
		}

		// Xor
		// XOR rd, rs, rt
		else if (i.fun == 38) {
			mnemonic("XOR");
			disasm("r%d, r%d, r%d", i.rd, i.rs, i.rt);

			cpu->reg[i.rd] = cpu->reg[i.rs] ^ cpu->reg[i.rt];
		}

		// Subtract
		// sub rd, rs, rt
		else if (i.fun == 34) {
			mnemonic("SUB");
			disasm("r%d, r%d, r%d", i.rd, i.rs, i.rt);
			cpu->reg[i.rd] = ((int32_t)cpu->reg[i.rs]) - ((int32_t)cpu->reg[i.rt]);
		}

		// Subtract unsigned
		// subu rd, rs, rt
		else if (i.fun == 35) {
			mnemonic("SUBU");
			disasm("r%d, r%d, r%d", i.rd, i.rs, i.rt);
			cpu->reg[i.rd] = ((int32_t)cpu->reg[i.rs]) - ((int32_t)cpu->reg[i.rt]);
		}


		// Set On Less Than Signed
		// SLT rd, rs, rt
		else if (i.fun == 42) {
			mnemonic("SLTU");
			disasm("r%d, r%d, r%d", i.rd, i.rs, i.rt);

			if ((int32_t)cpu->reg[i.rs] < (int32_t)cpu->reg[i.rt]) cpu->reg[i.rd] = 1;
			else cpu->reg[i.rd] = 0;
		}

		// Set On Less Than Unsigned
		// SLTU rd, rs, rt
		else if (i.fun == 43) {
			mnemonic("SLTU");
			disasm("r%d, r%d, r%d", i.rd, i.rs, i.rt);

			if (cpu->reg[i.rs] < cpu->reg[i.rt]) cpu->reg[i.rd] = 1;
			else cpu->reg[i.rd] = 0;
		}

		// Divide
		// div rs, rt
		else if (i.fun == 26) {
			mnemonic("DIV");
			disasm("r%d, r%d", i.rs, i.rt);

			cpu->lo = (int32_t)cpu->reg[i.rs] / (int32_t)cpu->reg[i.rt];
			cpu->hi = (int32_t)cpu->reg[i.rs] % (int32_t)cpu->reg[i.rt];
		}

		// Divide Unsigned Word
		// divu rs, rt
		else if (i.fun == 27) {
			mnemonic("DIVU");
			disasm("r%d, r%d", i.rs, i.rt);

			cpu->lo = cpu->reg[i.rs] / cpu->reg[i.rt];
			cpu->hi = cpu->reg[i.rs] % cpu->reg[i.rt];
		}

		// Move From Hi
		// MFHI rd
		else if (i.fun == 16) {
			mnemonic("MFHI");
			disasm("r%d", i.rd);

			cpu->reg[i.rd] = cpu->hi;
		}

		// Move From Lo
		// MFLO rd
		else if (i.fun == 18) {
			mnemonic("MFLO");
			disasm("r%d", i.rd);

			cpu->reg[i.rd] = cpu->lo;
		}

		// Move To Lo
		// MTLO rd
		else if (i.fun == 19) {
			mnemonic("MTLO");
			disasm("r%d", i.rd);

			cpu->lo = cpu->reg[i.rd];
		}

		// Move To Hi
		// MTHI rd
		else if (i.fun == 17) {
			mnemonic("MTHI");
			disasm("r%d", i.rd);

			cpu->hi = cpu->reg[i.rd];
		}

		// Syscall
		// SYSCALL
		else if (i.fun == 12) {
			//__debugbreak();
			//printf("Syscall: r4: 0x%x\n", cpu->reg[4]);
			mnemonic("SYSCALL");
			disasm("");

			cpu->COP0[14] = cpu->PC;// EPC - retur address from trap
			cpu->COP0[13] = 8 << 2;// Cause, hardcoded SYSCALL
			cpu->PC = 0x80000080;
		}
		else {
			invalid(cpu, i);
		}
		// TODO: break
		// TODO: multu
	}

	void branch(CPU* cpu, Opcode i)
	{
		// Branch On Less Than Zero
		// BLTZ rs, offset
		if (i.rt == 0) {
			mnemonic("BLTZ");
			disasm("r%d, 0x%x", i.rs, i.imm);

			if (cpu->reg[i.rs] & 0x80000000) {
				cpu->shouldJump = true;
				cpu->isJumpCycle = false;
				cpu->jumpPC = (cpu->PC & 0xf0000000) | ((cpu->PC) + (i.offset << 2));
			}
		}

		// Branch On Greater Than Or Equal To Zero
		// BGEZ rs, offset
		else if (i.rt == 1) {
		 	mnemonic("BGEZ");
		 	disasm("r%d, 0x%x", i.rs, i.imm);

		 	if ((int32_t)cpu->reg[i.rs] >= 0) {
		 		cpu->shouldJump = true;
		 		cpu->isJumpCycle = false;
		 		cpu->jumpPC = (cpu->PC & 0xf0000000) | ((cpu->PC) + (i.offset << 2));
		 	}
		}

		// Branch On Less Than Zero And Link
		// bltzal rs, offset
		else if (i.rt == 16) {
			// TODO:
			invalid(cpu, i);
		}

		// Branch On Greater Than Or Equal To Zero And Link
		// BGEZAL rs, offset
		else if (i.rt == 17) {
		 	mnemonic("BGEZAL");
		 	disasm("r%d, 0x%x", i.rs, i.imm);

		 	cpu->reg[31] = cpu->PC + 4;
		 	if ((int32_t)cpu->reg[i.rs] >= 0) {
		 		cpu->shouldJump = true;
		 		cpu->isJumpCycle = false;
		 		cpu->jumpPC = (cpu->PC & 0xf0000000) | ((cpu->PC) + (i.offset << 2));
		 	}
		}

		else {
			invalid(cpu, i);
		}
	}

	// Jump
	// J target
	void j(CPU *cpu, Opcode i)
	{
		disasm("0x%x", i.target << 2);
		cpu->shouldJump = true;
		cpu->isJumpCycle = false;
		cpu->jumpPC = (cpu->PC & 0xf0000000) | (i.target << 2);
	}

	// Jump And Link
	// JAL target
	void jal(CPU *cpu, Opcode i)
	{
		disasm("0x%x", (i.target << 2));
		cpu->shouldJump = true;
		cpu->isJumpCycle = false;
		cpu->jumpPC = (cpu->PC & 0xf0000000) | (i.target << 2);
		cpu->reg[31] = cpu->PC + 4;
	}

	// Branch On Equal
	// BEQ rs, rt, offset
	void beq(CPU *cpu, Opcode i)
	{
		disasm("r%d, r%d, 0x%x", i.rs, i.rt, i.imm);

		if (cpu->reg[i.rt] == cpu->reg[i.rs]) {
			cpu->shouldJump = true;
			cpu->isJumpCycle = false;
			cpu->jumpPC = (cpu->PC & 0xf0000000) | ((cpu->PC) + (i.offset << 2));
		}
	}

	// Branch On Greater Than Zero
	// BGTZ rs, offset
	void bgtz(CPU *cpu, Opcode i)
	{
		disasm("r%d, 0x%x", i.rs, i.imm);

		if (cpu->reg[i.rs] > 0) {
			cpu->shouldJump = true;
			cpu->isJumpCycle = false;
			cpu->jumpPC = (cpu->PC & 0xf0000000) | ((cpu->PC) + (i.offset << 2));
		}
	}

	// Branch On Less Than Or Equal To Zero
	// BLEZ rs, offset
	void blez(CPU *cpu, Opcode i)
	{
		disasm("r%d, 0x%x", i.rs, i.imm);

		if (cpu->reg[i.rs] == 0 || (cpu->reg[i.rs] & 0x80000000)) {
			cpu->shouldJump = true;
			cpu->isJumpCycle = false;
			cpu->jumpPC = (cpu->PC & 0xf0000000) | ((cpu->PC) + (i.offset << 2));
		}
	}

	// Branch On Not Equal
	// BNE rs, offset
	void bne(CPU *cpu, Opcode i)
	{
		disasm("r%d, r%d, 0x%x", i.rs, i.rt, i.imm);

		if (cpu->reg[i.rt] != cpu->reg[i.rs]) {
			cpu->shouldJump = true;
			cpu->isJumpCycle = false;
			cpu->jumpPC = (cpu->PC & 0xf0000000) | ((cpu->PC) + (i.offset << 2));
		}
	}

	// Add Immediate Word
	// ADDI rt, rs, imm
	void addi(CPU *cpu, Opcode i)
	{
		disasm("r%d, r%d, 0x%x", i.rt, i.rs, i.offset);

		cpu->reg[i.rt] = cpu->reg[i.rs] + i.offset;
	}

	// Add Immediate Unsigned Word
	// ADDIU rt, rs, imm
	void addiu(CPU *cpu, Opcode i)
	{
		mnemonic("ADDIU");
		disasm("r%d, r%d, 0x%x", i.rt, i.rs, i.offset);
		pseudo("r%d = r%d + %d", i.rt, i.rs, i.offset);

		cpu->reg[i.rt] = cpu->reg[i.rs] + i.offset;
	}

	// Set On Less Than Immediate Unsigned
	// SLTI rd, rs, rt
	void slti(CPU *cpu, Opcode i)
	{
		disasm("r%d, r%d, 0x%x", i.rt, i.rs, i.imm);

		if ((int32_t)cpu->reg[i.rs] < i.offset) cpu->reg[i.rt] = 1;
		else cpu->reg[i.rt] = 0;
	}

	// Set On Less Than Immediate Unsigned
	// SLTIU rd, rs, rt
	void sltiu(CPU *cpu, Opcode i)
	{
		disasm("r%d, r%d, 0x%x", i.rt, i.rs, i.imm);

		if (cpu->reg[i.rs] < i.imm) cpu->reg[i.rt] = 1;
		else cpu->reg[i.rt] = 0;
	}

	// And Immediate
	// ANDI rt, rs, imm
	void andi(CPU *cpu, Opcode i)
	{
		disasm("r%d, r%d, 0x%x", i.rt, i.rs, i.imm);
		pseudo("r%d = r%d & 0x%x", i.rt, i.rs, i.imm);

		cpu->reg[i.rt] = cpu->reg[i.rs] & i.imm;
	}

	// Or Immediete
	// ORI rt, rs, imm
	void ori(CPU *cpu, Opcode i)
	{
		disasm("r%d, r%d, 0x%x", i.rt, i.rs, i.imm);
		pseudo("r%d = (r%d&0xffff0000) | 0x%x", i.rt, i.rs, i.imm);

		cpu->reg[i.rt] = (cpu->reg[i.rs] & 0xffff0000) | i.imm;
	}

	// Xor Immediate
	// XORI rt, rs, imm
	void xori(CPU *cpu, Opcode i)
	{
		disasm("r%d, r%d, 0x%x", i.rt, i.rs, i.imm);
		pseudo("r%d = r%d ^ 0x%x", i.rt, i.rs, i.imm);

		cpu->reg[i.rt] = cpu->reg[i.rs] ^ i.imm;
	}

	// Load Upper Immediate
	// LUI rt, imm
	void lui(CPU *cpu, Opcode i)
	{
		disasm("r%d, 0x%x", i.rt, i.imm);
		pseudo("r%d = 0x%x", i.rt, i.imm << 16);

		cpu->reg[i.rt] = i.imm << 16;
	}

	// Coprocessor zero
	void cop0(CPU *cpu, Opcode i)
	{
		// Move from co-processor zero
		// MFC0 rd, <nn>
		if (i.rs == 0)
		{
			mnemonic("MFC0");
			disasm("r%d, $%d", i.rt, i.rd);

			uint32_t tmp = cpu->COP0[i.rd];
			cpu->reg[i.rt] = tmp;
		}

		// Move to co-processor zero
		// MTC0 rs, <nn>
		else if (i.rs == 4)
		{
			mnemonic("MTC0");
			disasm("r%d, $%d", i.rt, i.rd);

			uint32_t tmp = cpu->reg[i.rt];
			cpu->COP0[i.rd] = tmp;
			if (i.rd == 12) IsC = (tmp & 0x10000) ? true : false;
		}

		// Restore from exception
		// RFE
		else if (i.rs == 16)
		{
			if (i.fun == 16) {
				mnemonic("RFE");
				disasm("");
				printf("RFE TODO\n");
			}
			else
			{
				invalid(cpu, i);
			}

		}
		else
		{
			invalid(cpu, i);
		}
	}

	// Load Byte
	// LB rt, offset(base)
	void lb(CPU *cpu, Opcode i)
	{
		disasm("r%d, %d(r%d)", i.rt, i.offset, i.rs);

		uint32_t addr = cpu->reg[i.rs] + i.offset;
		cpu->reg[i.rt] = ((int32_t)(readMemory8(addr) << 24)) >> 24;
	}

	// Load Halfword 
	// LH rt, offset(base)
	void lh(CPU *cpu, Opcode i)
	{
		disasm("r%d, %d(r%d)", i.rt, i.offset, i.rs);

		uint32_t addr = cpu->reg[i.rs] + i.offset;
		cpu->reg[i.rt] = ((int32_t)(readMemory16(addr) << 16)) >> 16;
	}

	// Load Word
	// LW rt, offset(base)
	void lw(CPU *cpu, Opcode i)
	{
		disasm("r%d, %d(r%d)", i.rt, i.offset, i.rs);

		uint32_t addr = cpu->reg[i.rs] + i.offset;
		cpu->reg[i.rt] = readMemory32(addr);
	}

	// Load Byte Unsigned
	// LBU rt, offset(base)
	void lbu(CPU *cpu, Opcode i)
	{
		disasm("r%d, %d(r%d)", i.rt, i.offset, i.rs);

		uint32_t addr = cpu->reg[i.rs] + i.offset;
		cpu->reg[i.rt] = readMemory8(addr);
	}

	// Load Halfword Unsigned
	// LHU rt, offset(base)
	void lhu(CPU *cpu, Opcode i)
	{
		disasm("r%d, %d(r%d)", i.rt, i.offset, i.rs);

		uint32_t addr = cpu->reg[i.rs] + i.offset;
		cpu->reg[i.rt] = readMemory16(addr);
	}

	// Store Byte
	// SB rt, offset(base)
	void sb(CPU *cpu, Opcode i)
	{
		disasm("r%d, %d(r%d)", i.rt, i.offset, i.rs);
		uint32_t addr = cpu->reg[i.rs] + i.offset;
		writeMemory8(addr, cpu->reg[i.rt]);
	}

	// Store Halfword
	// SH rt, offset(base)
	void sh(CPU *cpu, Opcode i)
	{
		disasm("r%d, %d(r%d)", i.rt, i.offset, i.rs);
		uint32_t addr = cpu->reg[i.rs] + i.offset;
		writeMemory16(addr, cpu->reg[i.rt]);
	}

	// Store Word
	// SW rt, offset(base)
	void sw(CPU *cpu, Opcode i)
	{
		disasm("r%d, %d(r%d)", i.rt, i.offset, i.rs);
		uint32_t addr = cpu->reg[i.rs] + i.offset;
		writeMemory32(addr, cpu->reg[i.rt]);
		pseudo("mem[r%d+0x%x] = r%d    mem[0x%x] = 0x%x", i.rs, i.offset, i.rt, addr, cpu->reg[i.rt]);
	}
};