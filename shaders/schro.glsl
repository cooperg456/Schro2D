#version 450



//	---------------------------------------------------
//	--	resource bindings:
//	---------------------------------------------------

//	framebuffer
layout (binding = 0) uniform writeonly image2D framebuffer;


//	current wave function values
layout (std430, binding = 1) readonly buffer psiReadBuffer { 
	vec2 psi[]; 
};

//	updated wave function values
layout (std430, binding = 2) buffer psiWriteBuffer { 
	vec2 psi2[]; 
};

//	potential values
layout (std430, binding = 3) readonly buffer potentialBuffer { 
	vec2 potential[]; 
};

//	half step wave function values
layout (std430, binding = 4) buffer psiHalfBuffer { 
	vec2 psiHalf[]; 
};

//	time stepsize
layout (push_constant) uniform consts {
	float dt; 
	uint stage; 
};



//	---------------------------------------------------
//	--	helper functions:
//	---------------------------------------------------

//	complex multiplication
vec2 cMult(vec2 a, vec2 b) {
	float re = a.x * b.x - a.y * b.y;
    float im = a.x * b.y + a.y * b.x;
    return vec2(re, im);
}

//	complex conjugate
vec2 conj(vec2 z) {
	return vec2(z.x, -z.y);
}

//	color map from values
vec4 colorMap(vec2 psiValue, vec2 potentialValue) {
	//	return vec4(cMult(psiValue, conj(psiValue)).x, 0, potentialValue.x, 1);
	vec4 well = (potentialValue.x > 0) ? vec4(0.05, 0.05, 0.05, 1) : vec4(0, 0, 0, 0);
	return ((psiValue.x >= 0) ? vec4(psiValue.x, 0, 0, 1) : vec4(0, 0, -psiValue.x, 1)) + well;
}

//	schrodinger step stage 1
vec2 dPsiDt(uint idx, ivec2 shape) {
	//	physical constants
	vec2 i = vec2(0, 1);
	float hBar = 6.582119569e-16;			//	eV * s
	float electronMass = 5.685630111e-30;	//	eV / (nm/s)^2

	vec2 laplacian = (
		psi[idx-shape.x-1] + 4 * psi[idx-shape.x] + psi[idx-shape.x+1]
		+ 4 * psi[idx-1] - 20 * psi[idx] + 4 * psi[idx+1]
		+ psi[idx+shape.x-1] + 4 * psi[idx+shape.x] + psi[idx+shape.x+1]
	) / 6 * 1e18;

	return cMult(-i / hBar, (- hBar * hBar / (2 * electronMass)) * laplacian + cMult(psi[idx], potential[idx]));
}

//	schrodinger step stage 2
vec2 dPsiDt2(uint idx, ivec2 shape) {
	//	physical constants
	vec2 i = vec2(0, 1);
	float hBar = 6.582119569e-16;			//	eV * s
	float electronMass = 5.685630111e-30;	//	eV / (nm/s)^2

	vec2 laplacian = (
		psiHalf[idx-shape.x-1] + 4 * psiHalf[idx-shape.x] + psiHalf[idx-shape.x+1]
		+ 4 * psiHalf[idx-1] - 20 * psiHalf[idx] + 4 * psiHalf[idx+1]
		+ psiHalf[idx+shape.x-1] + 4 * psiHalf[idx+shape.x] + psiHalf[idx+shape.x+1]
	) / 6 * 1e18;

	return cMult(-i / hBar, (- hBar * hBar / (2 * electronMass)) * laplacian + cMult(psiHalf[idx], potential[idx]));
}


			
//	---------------------------------------------------
//	--	entry point:
//	---------------------------------------------------

layout (local_size_x = 32, local_size_y = 32) in;

void main() {
	ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
	ivec2 shape = imageSize(framebuffer);

	//	if thread out of bounds, return 
	if (coord.x >= shape.x || coord.y >= shape.y) {
		return;
	}
	
	//	convert to flattened indexing
	uint idx = coord.x + shape.x * coord.y;

	//	boundary conditions
	if (coord.x == 0 || coord.y == 0 || coord.x == shape.x - 1 || coord.y == shape.y - 1) {
		psi2[idx] = vec2(0.0, 0.0);
		psiHalf[idx] = vec2(0.0, 0.0);
	}
	//	half step solver
	else if (stage == 0) {
		psiHalf[idx] = psi[idx] + dPsiDt(idx, shape) * dt;
	}
	else if (stage == 1) {
		psi2[idx] = psi[idx] + (dPsiDt(idx, shape) + dPsiDt2(idx, shape)) * dt / 2;
		imageStore(framebuffer, coord, colorMap(psi2[idx], potential[idx]));
	}
}