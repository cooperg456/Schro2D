//	std lib
#include <iostream>
#include <string>

//	headers
#include "schro.hpp"



int main(int argc, char* argv[]) {
	Schro2DConfig cfg = { 
		static_cast<uint32_t>(std::stoi(argv[1])), 
		static_cast<uint32_t>(std::stoi(argv[2])), 
		2, 
		true,
		true 
	};
	
	Schro2D sim = Schro2D(cfg);
	sim.run();

	return 0;
}