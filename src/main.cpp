//	std lib
#include <iostream>
#include <string>

//	headers
#include "schro.hpp"







int main(int argc, char* argv[]) {
	Schro2D schro = Schro2D(
		static_cast<uint32_t>(std::stoi(argv[1])), 
		static_cast<uint32_t>(std::stoi(argv[2])), 
		static_cast<uint32_t>(std::stoi(argv[3]))
	);

	schro.run();

	return 0;
}