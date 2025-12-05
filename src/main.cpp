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

int main() {
	Schro2D schro(800, 800, 2);

	std::vector<std::vector<std::complex<float>>> psi(1600, std::vector<std::complex<float>>(1600, std::complex<float>(0, 0)));
	std::vector<std::vector<std::complex<float>>> v(1600, std::vector<std::complex<float>>(1600, std::complex<float>(0, 0)));

	uint x0 = 800;		//	nm
	uint y0 = 800;		//	nm
	float E0 = 10e-4;	//	eV
	float alpha = 0;	//	rad
	float sigma = 50;	//	nm

	for (size_t i = 0; i < 1600; i++) {
		for (size_t j = 0; j < 1600; j++) {
			v[i][j] = std::complex<float>(0.0, 0.0);
			psi[i][j] = calc_psi(i, j, x0, y0, E0 - v[i][j].real(), alpha, sigma);
		}
	}

	float dt = 1e-16;	//	0.1 fs / frame
	schro.run(psi, v, dt);

	return 0;
}