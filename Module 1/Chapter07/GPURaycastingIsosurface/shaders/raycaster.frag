#version 330 core

layout(location = 0) out vec4 vFragColor;	//fragment shader output

smooth in vec3 vUV;				//3D texture coordinates form vertex shader 
								//interpolated by rasterizer

//uniforms
uniform sampler3D	volume;			//volumie dataset
uniform vec3		camPos;			//camera position
uniform vec3		step_size;		//ray step size 
uniform sampler1D lut;		//transfer function (lookup table) texture

//constants
const int MAX_SAMPLES = 1000;		//total samples for each ray march step
const vec3 texMin = vec3(0);		//minimum texture access coordinate
const vec3 texMax = vec3(1);		//maximum texture access coordinate
const float DELTA = 0.01;			//the step size for gradient calculation
const float isoValue = 40/255.0;	//the isovalue for iso-surface detection

//function to give a more accurate position of where the given iso-value (iso) is found
//given the initial minimum limit (left) and maximum limit (right)
vec3 Bisection(vec3 left, vec3 right , float iso)
{ 
	//loop 4 times
	for(int i=0;i<4;i++)
	{ 
		//get the mid value between the left and right limit
		vec3 midpoint = (right + left) * 0.5;
		//sample the texture at the middle point
		float cM = texture(lut, texture(volume, midpoint).x ).a;
		//check if the value at the middle point is less than the given iso-value
		if(cM < iso)
			//if so change the left limit to the new middle point
			left = midpoint;
		else
			//otherwise change the right limit to the new middle point
			right = midpoint; 
	}
	//finally return the middle point between the left and right limit
	return vec3(right + left) * 0.5;
}

//function to calculate the gradient at the given location in the volumie dataset
//The function user center finite difference approximation to estimate the 
//gradient
vec3 GetGradient(vec3 uvw) 
{
	vec3 s1, s2;  

	//Using center finite difference 
	s1.x = texture(lut, texture(volume, uvw-vec3(DELTA,0.0,0.0)).x  ).a;
	s2.x = texture(lut, texture(volume, uvw+vec3(DELTA,0.0,0.0)).x  ).a;

	s1.y = texture(lut, texture(volume, uvw-vec3(0.0,DELTA,0.0)).x  ).a;
	s2.y = texture(lut, texture(volume, uvw+vec3(0.0,DELTA,0.0)).x  ).a;

	s1.z = texture(lut, texture(volume, uvw-vec3(0.0,0.0,DELTA)).x  ).a;
	s2.z = texture(lut, texture(volume, uvw+vec3(0.0,0.0,DELTA)).x  ).a;
	 
	return normalize((s1-s2)/2.0); 
}

//function to estimate the PhongLighting component given the light vector (L),
//the normal (N), the view vector (V), the specular power (specPower) and the
//given diffuse colour (diffuseColor). The diffuse component is first calculated
//Then, the half way vector is computed to obtain the specular component. Finally
//the diffuse and specular contributions are added together
vec4 PhongLighting(vec3 L, vec3 N, vec3 V, float specPower, vec3 diffuseColor)
{
	float diffuse = max(dot(L,N),0.0);
	vec3 halfVec = normalize(L+V);
	float specular = pow(max(0.00001,dot(halfVec,N)),specPower);	
	return vec4((diffuse*diffuseColor + specular),1.0);
}

void main()
{ 
	//get the 3D texture coordinates for lookup into the volumie dataset
	vec3 dataPos = vUV;	
    //initialize the vFragColor to vec4(0,0,0,0)
	vFragColor = vec4(0,0,0,0);

	//Gettting the ray marching direction:
	//get the object space position by subracting 0.5 from the
	//3D texture coordinates. Then subtraact it from camera position
	//and normalize to get the ray marching direction
	vec3 geomDir = normalize((vUV-vec3(0.5)) - camPos); 

	//multiply the raymarching direction with the step size to get the
	//sub-step size we need to take at each raymarching step
	vec3 dirStep = geomDir * step_size; 
	
	//flag to indicate if the raymarch loop should terminate
	bool stop = false; 
	
	//for all samples along the ray
	for (int i = 0; i < MAX_SAMPLES; i++) {
		// advance ray by dirstep
		dataPos = dataPos + dirStep;
		
		//The two constants texMin and texMax have a value of vec3(-1,-1,-1)
		//and vec3(1,1,1) respectively. To determine if the data value is 
		//outside the volumie data, we use the sign function. The sign function 
		//return -1 if the value is less than 0, 0 if the value is equal to 0 
		//and 1 if value is greater than 0. Hence, the sign function for the 
		//calculation (sign(dataPos-texMin) and sign (texMax-dataPos)) will 
		//give us vec3(1,1,1) at the possible minimum and maximum position. 
		//When we do a dot product between two vec3(1,1,1) we get the answer 3. 
		//So to be within the dataset limits, the dot product will return a 
		//value less than 3. If it is greater than 3, we are already out of 
		//the volumie dataset
		stop = dot(sign(dataPos-texMin),sign(texMax-dataPos)) < 3.0;

		//if the stopping condition is true we brek out of the ray marching loop		
		if (stop) 
			break;
		
		// data fetching from the red channel of volumie texture
		float sample = texture(lut, texture(volume, dataPos).r).a;			//current sample
		float sample2 = texture(lut, texture(volume, dataPos+dirStep).r).a;	//next sample

		//In case of iso-surface rendering, we do not use compositing. 
		//Instead, we find the zero crossing of the volumie dataset iso function 
		//by sampling two consecutive samples. 
		if( (sample -isoValue) < 0  && (sample2-isoValue) >= 0.0)  {
			//If there is a zero crossing, we refine the detected iso-surface 
			//location by using bisection based refinement.
			vec3 xN = dataPos;
			vec3 xF = dataPos+dirStep;	
			vec3 tc = Bisection(xN, xF, isoValue);	
	
			//This returns the first hit surface
			//vFragColor = make_float4(xN,1);
          	
			//To get the shaded iso-surface, we first estimate the normal
			//at the refined position
			vec3 N = GetGradient(tc);					

			//The view vector is simply opposite to the ray marching 
			//direction
			vec3 V = -geomDir;

			//We keep the view vector as the light vector to give us a head 
			//light
			vec3 L =  V;

			//Finally, we call PhongLighing function to get the final colour
			//with diffuse and specular components. Try changing this call to this
			//vFragColor =  PhongLighting(L,N,V,250,  tc); to get a multi colour
			//iso-surface
			vFragColor =  PhongLighting(L,N,V,250, vec3(0.5));	
			break;
		} 
	} 
}