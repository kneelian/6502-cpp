#include "6502.hpp"

#define _NR_OF_INSNS 0

int main()
{	
	CPU test;

	for(int i = 0; i < _NR_OF_INSNS; i++)
	{
		test.tick();
	}
	return 0;
}