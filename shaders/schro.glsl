#version 450



layout (local_size_x = 32, local_size_y = 32) in;

layout (binding = 0) uniform writeonly image2D displayImage;

layout (std430, binding = 1) readonly buffer psiReadBuffer { 
	float psi[]; 
};

layout (std430, binding = 2) buffer psiWriteBuffer { 
	float psi2[]; 
};

layout (std430, binding = 3) readonly buffer potentialBuffer { 
	float V[]; 
};

layout (push_constant) uniform consts {
	float dt; 
};



vec2 getV(ivec2 VCoord, ivec2 VSize) {
	if (VCoord.x >= VSize.x || VCoord.y >= VSize.y || VCoord.x < 0 || VCoord.y < 0) {
		return vec2(1 / 0, 0);
	}
	float real = V[2 * (VCoord.x + VSize.x * VCoord.y)];
	float imag = V[2 * (VCoord.x + VSize.x * VCoord.y) + 1];
	return vec2(real, imag);
}

vec2 getPsi(ivec2 psiCoord, ivec2 psiSize) {
	if (psiCoord.x >= psiSize.x || psiCoord.y >= psiSize.y || psiCoord.x < 0 || psiCoord.y < 0) {
		return vec2(0, 0);
	}
	float real = psi[2 * (psiCoord.x + psiSize.x * psiCoord.y)];
	float imag = psi[2 * (psiCoord.x + psiSize.x * psiCoord.y) + 1];
	return vec2(real, imag);
}

vec2 getPsi2(ivec2 psiCoord, ivec2 psiSize) {
	if (psiCoord.x >= psiSize.x || psiCoord.y >= psiSize.y || psiCoord.x < 0 || psiCoord.y < 0) {
		return vec2(0, 0);
	}
	float real = psi2[2 * (psiCoord.x + psiSize.x * psiCoord.y)];
	float imag = psi2[2 * (psiCoord.x + psiSize.x * psiCoord.y) + 1];
	return vec2(real, imag);
}

void setPsi2(ivec2 psiCoord, ivec2 psiSize, vec2 value) {
	psi2[2 * (psiCoord.x + psiSize.x * psiCoord.y)] = value.x;
	psi2[2 * (psiCoord.x + psiSize.x * psiCoord.y) + 1] = value.y;
}

vec4 colorMap(ivec2 cmapCoord, ivec2 cmapSize) {
	vec2 psiValue = getPsi(cmapCoord, cmapSize);
	//float prob = complexProduct(psiValue, complexConjugate(psiValue))
	if (psiValue.x >= 0) {
		return vec4(psiValue.x, 0, 0, 1);
	}
	if (psiValue.x < 0) {
		return vec4(0, 0, -psiValue.x, 1);
	}
}

vec2 complexConjugate(vec2 a) {
	return vec2(a.x, -a.y);
}

vec2 complexProduct(vec2 a, vec2 b) {
	return vec2(a.x * b.x - a.y * b.y, a.x * b.y + a.y * b.x);
}



void main() {
	ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
	ivec2 size = imageSize(displayImage);

	if (coord.x < size.x && coord.y < size.y) {
		vec2 i = vec2(0, 1);
		float hBar = 6.582119569e-16;			//	eV * s
		float electronMass = 5.685630111e-30;	//	eV / (nm/s)^2

		vec2 laplacian =
			getPsi(coord + ivec2(1, 0), size)
			+ getPsi(coord + ivec2(-1, 0), size)
			+ getPsi(coord + ivec2(0, 1), size)
			+ getPsi(coord + ivec2(0, -1), size)
			- 4.0 * getPsi(coord, size);

		vec2 dPsi_dt =
			complexProduct(i * (hBar*hBar / (2*electronMass)), laplacian)
			- complexProduct(i / hBar * getPsi(coord, size), getV(coord, size));

		setPsi2(coord, size, dPsi_dt * dt);		//	Euler method

		imageStore(displayImage, coord, colorMap(coord, size));
	}
}