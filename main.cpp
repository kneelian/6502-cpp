#include <iostream>
#include <array>

#define __DEBUG

#define _CARRY   1
#define _ZERO    2
#define _INT_DIS 4
#define _DECIM   8
#define _BREAK   16
#define _RESERV  32
#define _OVERFL  64
#define _NEGA    128

struct MEM_UNIT
{
	std::array<uint8_t, 65536> memory;
	MEM_UNIT(){}

	uint8_t seek(uint16_t address)
	{
		return memory[address];
	}
	void   write(uint16_t address, uint8_t value)
	{
		memory[address] = value;
		return;
	}
};

struct CPU
{
	static int number;
	uint16_t PC_reg = 0;
	uint8_t   A_reg = 0;
	uint8_t   X_reg = 0;
	uint8_t   Y_reg = 0;
	uint8_t   F_reg = 0;
	uint8_t  SP_reg = 0;

	MEM_UNIT* local_memunit;

	uint8_t OPCODE = 0;

	bool hook_memunit(MEM_UNIT* a)
	{
		if(a) { local_memunit = a; }
		return (local_memunit != nullptr);
	}

	CPU() 
	{
	#ifdef __DEBUG
		std::cerr << "CPU #" << number++ << " has been created" << std::endl;
	#endif
		local_memunit = nullptr;
		if(!hook_memunit(new MEM_UNIT)) { throw; }
	}

	uint8_t seek(uint16_t address)
	{ return local_memunit->seek(address); }
	void   write(uint16_t address, uint8_t value)
	{ local_memunit->write(address,value); }

	uint8_t  fetch1(uint16_t address)
	{ return seek(address); }

	uint16_t fetch2(uint16_t address) // little endian retrieval, byte by byte
	{ return (uint16_t(seek(address + 1)) << 8) + seek(address); }

	uint8_t pull()
	{
		return seek(256 + (SP_reg++));
	}

	void push(uint8_t value)
	{ 
		write(256 + SP_reg--, value);
	}

	uint8_t  read1_PC()
	{ return seek(PC_reg++); }

	uint16_t read2_PC()
	{
		uint16_t temp = fetch2(PC_reg);
		PC_reg += 2;
		return temp;
	}

	void update_F_Zero(uint8_t val)
	{
		if(val == 0)
		{
			F_reg |= _ZERO;
		}
		return;
	}

	void update_F_Nega(uint8_t val)
	{
		if(val & _NEGA)
		{
			F_reg |= _NEGA;
		}
		return;
	}

	void update_F_Carry(uint8_t val, uint8_t old_value)
	{
		if(old_value > val)
		{
			F_reg |= _CARRY;
		}
		return;
	}

	uint8_t rotate_left(uint8_t val)
	{
		uint8_t temp = (F_reg & _CARRY);
		if(val & 128) { F_reg |= 1; }
		val = (val << 1) + temp;
		return val;
	}

	uint8_t rotate_right(uint8_t val)
	{
		uint8_t temp = (F_reg & _CARRY);
		if(val & 1) { F_reg |= 1; }
		val = (val >> 1) + (temp << 7);
		return val;
	}

	uint8_t shift_left(uint8_t val)
	{
		if(val & 128) { F_reg |= _CARRY; }
		return val << 1;
	}

	uint8_t shift_right(uint8_t val)
	{
		F_reg |= (val & 1);
		return val >> 1;
	}

	void step();
	void decode(uint8_t);

};
int CPU::number = 0;

void CPU::step()
{
	OPCODE = fetch1(PC_reg);
	decode(OPCODE);
}

void CPU::decode(uint8_t op)
{

	uint8_t  temp8  = 0;
	uint16_t temp16 = 0;

	enum ADDRESSING_MODE
	{				// one byte
		implied,			// operand implied
		accumulator,		// reg A implied
					// two bytes
		immediate,			// unsigned uint8_t
		zeropage,			// address in zeropage
		zero_x,				// zpg + x
		zero_y,				// zpg + y
		x_indirect,			// (byte+x, byte+x+1)
		y_indirect,			// (byte, byte+1) + y + c
		relative,			// signed int8_t
					// three bytes
		absolute,			// 2b addr
		x_absolute,			// 2b addr + X + carry
		y_absolute,			// 2b addr + Y + carry
		hword_indirect		// 2b addr of 2b addr
	};
	/*	
		technically speaking implied == accumulator,
		but official docs use this terminology so w/e
	*/

	if(op & 3)              // illegal opcodes are in these 4 columns
	{ 
		return; 
	} 
	if( 
	  ((op %  0x10) & 2) && // and the whole 2 column except for A2
	   (op != 0xa2)         // is also an illegal opcode column
	) 
	{ 
		return; 
	}

	switch(op) 				// a giant switch table for ops since there's few opcodes
	{
		case 0xEA:			// NOP
			return;

		// increment and decrement

		case 0xE8:			// INX
			X_reg++;
			update_F_Nega(X_reg);
			update_F_Zero(X_reg);
			return;
		case 0xC8:			// INY
			Y_reg++;
			update_F_Nega(Y_reg);
			update_F_Zero(Y_reg);
			return;
		case 0xCA:			// DEX
			X_reg--;
			update_F_Nega(X_reg);
			update_F_Zero(X_reg);
			return;
		case 0x88:			// DEY
			Y_reg--;
			update_F_Nega(Y_reg);
			update_F_Zero(Y_reg);
			return;

		// transfers

		case 0x8A:			// TXA
			A_reg = X_reg;
			update_F_Nega(A_reg);
			update_F_Zero(A_reg);
			return;
		case 0xAA:			// TAX
			X_reg = A_reg;
			update_F_Nega(X_reg);
			update_F_Zero(X_reg);
			return;

		case 0x98:			// TYA
			A_reg = Y_reg;
			update_F_Nega(A_reg);
			update_F_Zero(A_reg);
			return;
		case 0xA8:			// TAY
			Y_reg = A_reg;
			update_F_Nega(Y_reg);
			update_F_Zero(Y_reg);
			return;

		case 0xBA:			// TSX
			X_reg = SP_reg;
			update_F_Nega(X_reg);
			update_F_Zero(X_reg);
			return;
		case 0x9A:			// TXS
			SP_reg = X_reg;
			update_F_Nega(SP_reg);
			update_F_Zero(SP_reg);
			return;

		// set and clear flags

		case 0x38:			// SEC
			F_reg |= _CARRY;
			return;
		case 0x78:			// SEI
			F_reg |= _INT_DIS;
			return;
		case 0xF8:			// SED
			F_reg |= _DECIM;
			return;

		case 0x18:			// CLC
			F_reg ^= _CARRY;
			return;
		case 0x58:			// CLI
			F_reg ^= _INT_DIS;
			return;
		case 0xB8:			// CLO
			F_reg ^= _OVERFL;
			return;
		case 0xD8:			// CLD
			F_reg ^= _DECIM;
			return;

		// pushing and pulling
		case 0x48:		// PHA
			push(A_reg);
			return;
		case 0x68:		// PLA
			A_reg = pull();
			update_F_Nega(A_reg);
			update_F_Zero(A_reg);
			return;
		case 0x08:		// PHP
			push(F_reg);
			return;
		case 0x28:		// PLP
			F_reg = pull();
			return;

		// and the special cases
		case 0x00:		// BRK
			PC_reg += 2;
			push(PC_reg & 0xff);
			push(PC_reg >> 8);
			F_reg |= _BREAK;
			push(F_reg);
			PC_reg = fetch2(0xfffe);
			return;
		case 0x40:		// RTI
			F_reg  = pull();
			PC_reg =(pull() << 8);
			PC_reg+= pull();
			return;
		case 0x60:		// RTS
			PC_reg =(pull() << 8);
			PC_reg+= pull();
			return;

		// with immediate -- two bytes

		case 0xC9:		// CMP imm
			temp8  = A_reg;
			temp8 -= read1_PC();
			update_F_Nega(temp8);
			update_F_Zero(temp8);
			update_F_Carry(A_reg, temp8);
			return;
		case 0xE0:		// CPX imm
			temp8  = X_reg;
			temp8 -= read1_PC();
			update_F_Nega(temp8);
			update_F_Zero(temp8);
			update_F_Carry(X_reg, temp8);
			return;
		case 0xC0:		// CPY imm
			temp8  = Y_reg;
			temp8 -= read1_PC();
			update_F_Nega(temp8);
			update_F_Zero(temp8);
			update_F_Carry(Y_reg, temp8);
			return;

		case 0xA9:		// LDA imm
			A_reg = read1_PC();
			update_F_Nega(A_reg);
			update_F_Zero(A_reg);
			return;
		case 0xA2:		// LDX imm
			X_reg = read1_PC();
			update_F_Nega(X_reg);
			update_F_Zero(X_reg);
			return;
		case 0xA0:		// LDY imm
			Y_reg = read1_PC();
			update_F_Nega(Y_reg);
			update_F_Zero(Y_reg);
			return;

		// arithmetic and logic ops

		case 0x69:		// ADC imm
			temp8  = A_reg;
			A_reg += read1_PC() + (F_reg & _CARRY);
			update_F_Carry(A_reg, temp8);
			update_F_Nega(A_reg);
			update_F_Zero(A_reg);
			return;
		case 0xE9:		// SBC imm
			temp8  = A_reg;
			A_reg -= read1_PC() - (F_reg & _CARRY);
			update_F_Carry(A_reg, temp8);
			update_F_Nega(A_reg);
			update_F_Zero(A_reg);
			return;
		case 0x29:		// AND imm
			A_reg &= read1_PC();
			update_F_Nega(A_reg);
			update_F_Zero(A_reg);
			return;
		case 0x49:		// EOR imm
			A_reg ^= read1_PC();
			update_F_Nega(A_reg);
			update_F_Zero(A_reg);
			return;
		case 0x09:		// ORA imm
			A_reg |= read1_PC();
			update_F_Nega(A_reg);
			update_F_Zero(A_reg);
			return;

		// accumulator ops
		case 0x2A:		// ROL A
			A_reg = rotate_left(A_reg);
			update_F_Nega(A_reg);
			update_F_Zero(A_reg);
			return;
		case 0x6A:		// ROR A
			A_reg = rotate_right(A_reg);
			update_F_Nega(A_reg);
			update_F_Zero(A_reg);
			return;
		case 0x0A:		// ASL A
			A_reg = shift_left(A_reg);
			update_F_Nega(A_reg);
			update_F_Zero(A_reg);
			return;
		case 0x4A:		// LSR A
			A_reg = shift_right(A_reg);
			update_F_Nega(A_reg);
			update_F_Zero(A_reg);
			return;

		default:
			return;
	}

}

int main()
{	
	std::cout << "Hello, world!"  << std::endl;
	std::cerr << "(ERROR OUTPUT)" << std::endl;
	CPU test;
	CPU test_2;
	CPU test_3;
	return 0;
}