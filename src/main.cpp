//	std lib
#include <iostream>
#include <complex>
#include <vector>
#include <string>
#include <cmath>

//	headers
#include "schro.hpp"







std::complex<float> calc_psi(uint x, uint y, uint x0, uint y0, float E0, float alpha, float sigma) {
	std::complex<float> i(0, 1);
	float hBar = 6.582119569e-16;			//	eV * s
	float electronMass = 5.685630111e-30;	//	eV / (nm/s)^2

	float dx = float(x) - float(x0);
	float dy = float(y) - float(y0);

	std::complex<float> wave = std::exp((i * std::sqrt(2 * electronMass * E0) / hBar) * (dx * std::cos(alpha) + dy * std::sin(alpha)));
	std::complex<float> envelope = std::exp(- (((dx*dx) + (dy*dy)) / ( 4 * sigma * sigma)));

	return wave * envelope;
}

int main(int argc, char* argv[]) {
	Schro2D schro(500, 500, 2);

	std::vector<std::vector<std::complex<float>>> psi(1000, std::vector<std::complex<float>>(1000, std::complex<float>(0, 0)));
	std::vector<std::vector<std::complex<float>>> v(1000, std::vector<std::complex<float>>(1000, std::complex<float>(0, 0)));

	uint x0 = 200;		//	nm
	uint y0 = 500;		//	nm
	float E0 = 1e-2;	//	eV
	float alpha = 0;	//	rad
	float sigma = 50;	//	nm

	//	free particle
	if (*argv[1] == '0') {
		std::cout << "Schro2D: 'Wave Packet in Infinite Square Well'\n";
	}

	//	barrier
	if (*argv[1] == '1') {
		std::cout << "Schro2D: 'Wave Packet with Barrier'\n";
		for (size_t i = 0; i < 1000; i++) {
			for (size_t j = 0; j < 1000; j++) {
				if (i >= 475 && i <= 525) v[j][i] = std::complex<float>(1e16, 0.0);
			}
		}
	}

	//	slit
	if (*argv[1] == '2') {
		std::cout << "Schro2D: 'Wave Packet with Double Slit'\n";
		for (size_t i = 0; i < 1000; i++) {
			for (size_t j = 0; j < 1000; j++) {
				if (i >= 475 && i <= 525) {
					if (!((j >= 450 && j <= 475) || (j >= 525 && j <= 550))) {
						v[j][i] = std::complex<float>(1e16, 0.0);  // close enough to infinite
					}
				}
			}
		}
	}

	//	create wave packet
	for (size_t i = 0; i < 1000; i++) {
		for (size_t j = 0; j < 1000; j++) {
			auto psiValue = calc_psi(i, j, x0, y0, E0, alpha, sigma);
			psi[j][i] = (std::abs(psiValue) > 0) ? psiValue : 0;
		}
	}

	float dt = 1e-33;
	schro.run(psi, v, dt);

	return 0;
}