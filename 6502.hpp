#include <array>

#define _CARRY     0x01
#define _ZERO      0x02
#define _INTERRUPT 0x04
#define _DECIM     0x08
#define _BREAK     0x10
#define _RESERVED  0x20
#define _OVERFLOW  0x40
#define _NEGA      0x80

#define _DEBUG_LVL     1
#define _UNDOCUMENTED  FALSE
#define _BCD           FALSE

#define _USING_FMT

#define _STACK_BASE 0x100

#if defined(_USING_FMT)
	#include "fmt/core.h"
	#include "fmt/color.h"
#else
	#include <iostream>
#endif

#include "tables.hpp"

struct CPU
{
	uint8_t   A;
	uint8_t   X;
	uint8_t   Y;
	uint8_t   F;
	uint8_t   SP;
	uint16_t  PC;
	uint16_t  EA;

	uint8_t* _op_a;
	uint8_t* _op_b;

	uint16_t  TEMP_1;
	uint16_t  TEMP_2;

	uint8_t   _operation;
	uint8_t   _addrmode;
	uint8_t   _insn;

	uint64_t  _cycles;
	uint64_t  _instructions;
	uint8_t   _penalty;

	std::array<uint8_t, 65536>* memory;

	void _log_start();
	void _log_insns();
	void _log_fetch();
	void _log_decode();
	void _log_execute();

	uint8_t read(uint16_t address)
	{
		return memory->at(address);
	}

	void write(uint16_t address, uint8_t val)
	{
		memory->at(address) = val;
		return;
	}

	uint8_t pull_8()
	{
		SP++;
		return read(_STACK_BASE + SP);
	}
	void push_8(uint8_t val)
	{
		write(_STACK_BASE + SP, val);
		SP--;
	}

	uint16_t pull_16()
	{
		return (pull_8() << 8) | pull_8();
	}

	void push_16(uint16_t val)
	{
		push_8(val >> 8);
		push_8(val & 0xff);
		return;
	}

	CPU()
	{
		A  = 0;
		X  = 0;
		Y  = 0;
		F  = 0;
		SP = 0;
		PC = 0;
		EA = 0;

		TEMP_1 = 0;
		TEMP_2 = 0;

		_operation = 0;
		_addrmode = 0;
		_insn = 0;

		_cycles = 0;
		_instructions = 0;
		_penalty = 0;

		memory = new std::array<uint8_t, 65536>;
		memory->fill(0);

		if(_DEBUG_LVL) { _log_start(); }
	}

	~CPU()
	{ delete memory; }

	void set_carry() { F |=   _CARRY ; }
	void clr_carry() { F &= (~_CARRY); }

	void set_zero()  { F |=   _ZERO ; }
	void clr_zero()  { F &= (~_ZERO); }

	void set_inter() { F |=  _INTERRUPT ; }
	void clr_inter() { F &=(~_INTERRUPT); }

	void set_decim() { F |=  _DECIM ; }
	void clr_decim() { F &=(~_DECIM); }

	void set_ovfl()  { F |=   _OVERFLOW ; }
	void clr_ovfl()  { F &= (~_OVERFLOW); }

	void set_nega()  { F |=   _NEGA ; }
	void clr_nega()  { F &= (~_NEGA); }

	bool check_zero (uint16_t n)
	{ return (n & 0xff) == 0; }

	void upd_zero(uint16_t n)
	{
		if(check_zero(n)) { set_zero(); }
		else { clr_zero(); }
	}

	bool check_carry(uint16_t n)
	{ return (n > 0xff); }

	void upd_carry(uint16_t n)
	{
		if(check_carry(n)) { set_carry(); }
		else { clr_carry(); }
	}

	bool check_nega (uint16_t n)
	{ return (n & 0x80); }


	void upd_nega(uint16_t n)
	{
		if(check_nega(n)) { set_nega(); }
		else { clr_nega(); }
	}

	bool check_ovfl (uint16_t n, uint8_t a, uint8_t b)
	{ return (a ^ n) & (b ^ n) & 0x80; }

	void upd_ovfl(uint16_t temp, uint8_t operand)
	{
		if(check_ovfl(temp, A, operand)) { set_ovfl(); }
		else { clr_ovfl(); }
	}

	void prep_args(ADDRESSING_MODE addrm)
	{
		select(addrm)
		{
			case imp:
			case acc:
			default:
				return;
		}
	}

	void reset();
	void irq();
	void nmi();

	void fetch()
	{
		_insn = read(PC++);
		if(_DEBUG_LVL) { _log_fetch(); }
	}

	uint8_t read_pc()
	{
		TEMP_1 = read(PC++);
		if(_DEBUG_LVL) { _log_fetch(); }
		return TEMP_1;
	}

	void decode()
	{
		_operation   = op_table[_insn];
		_addrmode    = addr_table[_insn];
		if(_DEBUG_LVL) { _log_decode(); }
	}

	void execute()
	{
		if(_DEBUG_LVL) { _log_execute(); }
		prep_args(_addrmode);

	}

	void tick()
	{
		fetch();
		decode();
		execute();

		_instructions++;
	}
};

void CPU::_log_start()
{
	#if defined(_USING_FMT)
		fmt::print("Initialised 6502 CPU; implementation version {0}\nUsing {1} for output.\n",
		/* --> */ fmt::styled("0.01", fmt::fg(fmt::color::crimson) | fmt::emphasis::italic | fmt::emphasis::bold),
		          fmt::styled("FMT" , fmt::fg(fmt::color::medium_aquamarine) | fmt::emphasis::bold)
    	);
    	fmt::print("Memory unit address: 0x{0:x}\n",
    		fmt::styled(uint64_t(memory), fmt::fg(fmt::color::medium_aquamarine) | fmt::emphasis::bold));
	#else
		std::cout << "Initialised 6502 CPU; implementation version " << "0.01" << "\nUsing std::cout for output."<< std::endl;
		std::cout << "Memory unit address: 0x" << std::hex << memory << std::dec << std::endl;
	#endif
}

void CPU::_log_fetch()
{
	#if defined(_USING_FMT)
		fmt::print("Fetched data {0:x} from {1:x}\n",
		/* --> */ fmt::styled(_insn, fmt::fg(fmt::color::medium_aquamarine) | fmt::emphasis::bold),
		          fmt::styled(PC - 1,fmt::fg(fmt::color::medium_aquamarine) | fmt::emphasis::bold)
    	);
	#else
		std::cout << "Fetched data " << std::hex << _insn << " from " << PC-1 << std::dec << std::endl;
	#endif
}

void CPU::_log_decode() // currently placeholdered
{
	/* give us address, opcode, operation, addrmode */
}

void CPU::_log_execute() // currently placeholdered
{
	/* give us operands and result */
}
