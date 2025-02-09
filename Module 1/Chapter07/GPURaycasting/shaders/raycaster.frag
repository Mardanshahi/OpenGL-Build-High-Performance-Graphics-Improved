#version 330 core

layout(location = 0) out vec4 vFragColor;	//fragment shader output

in vec3 vUV;				//3D texture coordinates form vertex shader 
								//interpolated by rasterizer

//uniforms
uniform sampler3D	volume;		//volume dataset
uniform vec3		camPos;		//camera position
uniform vec3		step_size;	//ray step size 
uniform sampler1D lut;		//transfer function (lookup table) texture

//constants
const int MAX_SAMPLES = 1000;	//total samples for each ray march step
const vec3 texMin = vec3(0);	//minimum texture access coordinate
const vec3 texMax = vec3(1);	//maximum texture access coordinate

#define use_cubic_filt 0

float interpolate_cubic(sampler3D tex, vec3 coord,vec3 cell_size1)
{
	
	// transform the coordinate from [0,extent] to [-0.5, extent-0.5]
	vec3 coord_grid = coord/cell_size1-vec3(0.5);
	vec3 index = floor(coord_grid);
	vec3 fraction = coord_grid - index;
	vec3 one_frac = vec3(1.0) - fraction;
	vec3 one_frac2 = one_frac * one_frac;
	vec3 fraction2 = fraction * fraction;

	vec3 w0 = 1.0/6.0 * one_frac2 * one_frac;
	vec3 w1 = vec3(2.0/3.0) - 0.5 * fraction2 * (2.0-fraction);
	vec3 w2 = vec3(2.0/3.0) - 0.5 * one_frac2 * (2.0-one_frac);
	vec3 w3 = 1.0/6.0 * fraction2 * fraction;
	vec3 g0 = w0 + w1;
	vec3 g1 = w2 + w3;
	// h0 = w1/g0 - 1, move from [-0.5, extent-0.5] to [0, extent]
	vec3 h0 = (w1 / g0) - vec3(0.5) + index;
	vec3 h1 = (w3 / g1) + vec3(1.5) + index;
	h0*=cell_size1;
	h1*=cell_size1;

	// fetch the four linear interpolations
	
	float tex000 = texture(tex, h0).x;
	float tex100 = texture(tex, vec3(h1.x, h0.y,h0.z)).x;
	float tex010 = texture(tex, vec3(h0.x, h1.y,h0.z)).x;
	float tex110 = texture(tex, vec3(h1.x,h1.y,h0.z)).x;
						  
	float tex001 = texture(tex, vec3(h0.x,h0.y,h1.z)).x;
	float tex101 = texture(tex, vec3(h1.x,h0.y,h1.z)).x;
	float tex011 = texture(tex, vec3(h0.x,h1.y,h1.z)).x;
	float tex111 = texture(tex, h1).x;
	// weigh along the z-direction
	tex000 = mix(tex001, tex000, g0.z);
	tex100 = mix(tex101, tex100, g0.z);
	tex010 = mix(tex011, tex010, g0.z);
	tex110 = mix(tex111, tex110, g0.z);

	// weigh along the y-direction
	tex000 = mix(tex010, tex000, g0.y);
	tex100 = mix(tex110, tex100, g0.y);
	// weigh along the x-direction
	return mix(tex100, tex000, g0.x);
}

float Equ(vec3 arg, vec3 dirStep )
{
	float block = 128.0;
    vec3 cell_size_block = dirStep; //vec3(1/block, 1/block, 1/block);

	#if use_cubic_filt
		return interpolate_cubic(volume, arg,cell_size_block);
	#else
		return texture(volume, arg).r;
	#endif
}
float rand() {
    /* the internet **really** likes this one, still no source to be
     * found, probably Rey 1998, cited by TestU01 but nothing
     * downloadable */
    return fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453);
}
void main()
{ 
	//get the 3D texture coordinates for lookup into the volume dataset
	vec3 dataPos = vUV;

	//initialize the vFragColor to vec4(0,0,0,0)
	vFragColor = vec4(0,0,0,0);

	//Getting the ray marching direction:
	//get the object space position by subracting 0.5 from the
	//3D texture coordinates. Then subtraact it from camera position
	//and normalize to get the ray marching direction
	vec3 geomDir = normalize((vUV-vec3(0.5)) - camPos); 

	//multiply the raymarching direction with the step size to get the
	//sub-step size we need to take at each raymarching step
	vec3 dirStep = geomDir * step_size; 
	 
	//flag to indicate if the raymarch loop should terminate
	bool stop = false; 

	dataPos  = dataPos + dirStep * rand();

	//for all samples along the ray
	for (int i = 0; i < MAX_SAMPLES; i++) {
		// advance ray by dirstep
		dataPos = dataPos + dirStep;
		
		
		//The two constants texMin and texMax have a value of vec3(-1,-1,-1)
		//and vec3(1,1,1) respectively. To determine if the data value is 
		//outside the volume data, we use the sign function. The sign function 
		//return -1 if the value is less than 0, 0 if the value is equal to 0 
		//and 1 if value is greater than 0. Hence, the sign function for the 
		//calculation (sign(dataPos-texMin) and sign (texMax-dataPos)) will 
		//give us vec3(1,1,1) at the possible minimum and maximum position. 
		//When we do a dot product between two vec3(1,1,1) we get the answer 3. 
		//So to be within the dataset limits, the dot product will return a 
		//value less than 3. If it is greater than 3, we are already out of 
		//the volume dataset
		stop = dot(sign(dataPos-texMin),sign(texMax-dataPos)) < 3.0;

		//if the stopping condition is true we brek out of the ray marching loop
		if (stop) 
			break;
		
		// data fetching from the red channel of volume texture
		vec4 sample = texture(lut, Equ(dataPos, dirStep*4));	

		//Opacity calculation using compositing:
		//here we use front to back compositing scheme whereby the current sample
		//value is multiplied to the currently accumulated alpha and then this product
		//is subtracted from the sample value to get the alpha from the previous steps.
		//Next, this alpha is multiplied with the current sample colour and accumulated
		//to the composited colour. The alpha value from the previous steps is then 
		//accumulated to the composited colour alpha.
		float prev_alpha = sample.a - (sample.a * vFragColor.a);
		vFragColor.rgb = prev_alpha * sample.rgb + vFragColor.rgb; 
		vFragColor.a += prev_alpha; 
			
		//early ray termination
		//if the currently composited colour alpha is already fully saturated
		//we terminated the loop
		if( vFragColor.a > 0.9)
			break;
	} 
}