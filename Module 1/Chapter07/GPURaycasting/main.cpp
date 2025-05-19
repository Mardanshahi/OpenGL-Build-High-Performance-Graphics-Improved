#include <GL/glew.h>
#include <GL/freeglut.h>
#include <iostream>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "..\src\GLSLShader.h"
#include <fstream>
#include <algorithm> // For std::fill

#define GL_CHECK_ERRORS assert(glGetError()== GL_NO_ERROR);


//#pragma comment(lib, "glew32.lib")

using namespace std;

//screen resolution
const int WIDTH  = 1280;
const int HEIGHT = 720;

//camera transform variables
int state = 0, oldX=0, oldY=0;
float rX=0, rY=0, dist = -1.5;


//modelview projection matrices
glm::mat4 MV,P;

//cube vertex array and vertex buffer object IDs
GLuint cubeVBOID;
GLuint cubeVAOID;
GLuint cubeIndicesID;

//ray casting shader
GLSLShader shader;

//background colour
glm::vec4 bg=glm::vec4(0.3,0.2,0.6,1);


//volume dataset filename  
const std::string volume_file = "../media/skull-monire.raw";
bool is16bit = true;

//dimensions of volume data
const int XDIM = 512;
const int YDIM = 512;
const int ZDIM = 438;

//volume texture ID
GLuint textureID;

//transfer function (lookup table) texture id
GLuint tfTexID;

//unit cube vertices 
glm::vec3 vertices[8] = { glm::vec3(-0.5f,-0.5f,-0.5f),
						glm::vec3(0.5f,-0.5f,-0.5f),
						glm::vec3(0.5f, 0.5f,-0.5f),
						glm::vec3(-0.5f, 0.5f,-0.5f),
						glm::vec3(-0.5f,-0.5f, 0.5f),
						glm::vec3(0.5f,-0.5f, 0.5f),
						glm::vec3(0.5f, 0.5f, 0.5f),
						glm::vec3(-0.5f, 0.5f, 0.5f) };

//unit cube indices
GLushort cubeIndices[36] = { 0,5,4,
						  5,0,1,
						  3,7,6,
						  3,6,2,
						  7,4,6,
						  6,4,5,
						  2,1,3,
						  3,1,0,
						  3,0,7,
						  7,0,4,
						  6,5,2,
						  2,5,1 };

//fill the colour values at the place where the colour should be after interpolation
// index must be below 256
int index0[] = { 133, 134, 135, 140 };
//transfer function (lookup table) colour values
const glm::vec4 jet_values[4] = {
						glm::vec4(0.0f, 0.0f, 0.0f, 0.0f),
						glm::vec4(0.3f, 0.3f, 0.2f, 0.5f),
						glm::vec4(1.0f, 0.9f, 0.9f, 0.75f),
						glm::vec4(1.0f, 1.0f, 1.0f, 0.79f),
};


//function that load a volume from the given raw data file and 
//generates an OpenGL 3D texture from it
bool LoadVolume() {
	std::ifstream infile(volume_file.c_str(), std::ios_base::binary);

	if(infile.good()) {
		//read the volume data file
		GLubyte* pData = new GLubyte[XDIM*YDIM*ZDIM];
		std::fill(pData, pData + (XDIM * YDIM * ZDIM), static_cast<GLushort>(1));

		infile.read(reinterpret_cast<char*>(pData), XDIM*YDIM*ZDIM*sizeof(GLubyte));
		infile.close();
		
		//generate OpenGL texture
		glGenTextures(1, &textureID);
		glBindTexture(GL_TEXTURE_3D, textureID);
		GL_CHECK_ERRORS
		// set the texture parameters
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

		//set the mipmap levels (base and max)
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL, 4);
				
		//allocate data with internal format and foramt as (GL_RED)	
		glTexImage3D(GL_TEXTURE_3D,0,GL_RED,XDIM,YDIM,ZDIM,0,GL_RED,GL_UNSIGNED_BYTE,pData);
		std::cout << glGetError() << std::endl;
		GL_CHECK_ERRORS

		//generate mipmaps
		glGenerateMipmap(GL_TEXTURE_3D);

		//delete the volume data allocated on heap
		delete [] pData;

		return true;
	} else {
		return false;
	}
}

bool LoadVolumeUShort() {
	std::ifstream infile(volume_file.c_str(), std::ios_base::binary);

	if (infile.good()) {
		//read the volume data file
		GLushort* pData = new GLushort[XDIM * YDIM * ZDIM];
		std::fill(pData, pData + (XDIM * YDIM * ZDIM), static_cast<GLushort>(100000));

		infile.read(reinterpret_cast<char*>(pData), XDIM * YDIM * ZDIM * sizeof(GLushort));
		infile.close();

		//generate OpenGL texture
		glGenTextures(1, &textureID);
		glBindTexture(GL_TEXTURE_3D, textureID);
		GL_CHECK_ERRORS
		// set the texture parameters
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

		//set the mipmap levels (base and max)
		//glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_BASE_LEVEL, 0);
		//glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL, 4);

		//allocate data with internal format and foramt as (GL_RED)	
		glTexImage3D(GL_TEXTURE_3D, 0, GL_RED, XDIM, YDIM, ZDIM, 0, GL_RED, GL_UNSIGNED_SHORT, pData);
		std::cout << glGetError() << std::endl;
		GL_CHECK_ERRORS

		//generate mipmaps
		//glGenerateMipmap(GL_TEXTURE_3D);

		//delete the volume data allocated on heap
		delete[] pData;

		return true;
	}
	else {
		return false;
	}
}

void LoadTransferFunction() {
	float pData[256][4];
	int indices[4];


	for (int i = 0; i < 4; i++)
	{
		pData[index0[i]][0] = jet_values[i].x;
		pData[index0[i]][1] = jet_values[i].y;
		pData[index0[i]][2] = jet_values[i].z;
		pData[index0[i]][3] = jet_values[i].w;
		indices[i] = index0[i];

	}


	//for each adjacent pair of colours, find the difference in the rgba values and then interpolate
	for (int j = 0; j < 4 - 1; j++)
	{
		float dDataR = (pData[indices[j + 1]][0] - pData[indices[j]][0]);
		float dDataG = (pData[indices[j + 1]][1] - pData[indices[j]][1]);
		float dDataB = (pData[indices[j + 1]][2] - pData[indices[j]][2]);
		float dDataA = (pData[indices[j + 1]][3] - pData[indices[j]][3]);
		int dIndex = indices[j + 1] - indices[j];

		float dDataIncR = dDataR / float(dIndex);
		float dDataIncG = dDataG / float(dIndex);
		float dDataIncB = dDataB / float(dIndex);
		float dDataIncA = dDataA / float(dIndex);
		for (int i = indices[j] + 1; i < indices[j + 1]; i++)
		{
			pData[i][0] = (pData[i - 1][0] + dDataIncR);
			pData[i][1] = (pData[i - 1][1] + dDataIncG);
			pData[i][2] = (pData[i - 1][2] + dDataIncB);
			pData[i][3] = (pData[i - 1][3] + dDataIncA);
		}
	}

	//generate the OpenGL texture
	glGenTextures(1, &tfTexID);
	//bind this texture to texture unit 1
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_1D, tfTexID);

	// set the texture parameters
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	//allocate the data to texture memory. Since pData is on stack, we donot delete it 
	glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA, 256, 0, GL_RGBA, GL_FLOAT, pData);

	GL_CHECK_ERRORS
}

//mouse down event handler
void OnMouseDown(int button, int s, int x, int y)
{
	if (s == GLUT_DOWN)
	{
		oldX = x;
		oldY = y;
	}

	if(button == GLUT_MIDDLE_BUTTON)
		state = 0;
	else
		state = 1;
}

//mouse move event handler
void OnMouseMove(int x, int y)
{
	if (state == 0) {
		dist += (y - oldY)/50.0f;
	} else {
		rX += (y - oldY)/50.0f;
		rY += (x - oldX)/50.0f;
	}
	oldX = x;
	oldY = y;

	glutPostRedisplay();
}

//OpenGL initialization
void OnInit() {


	//Load the raycasting shader
	shader.LoadFromFile(GL_VERTEX_SHADER, "shaders/raycaster.vert");
	shader.LoadFromFile(GL_FRAGMENT_SHADER, "shaders/raycaster.frag");

	//compile and link the shader
	shader.CreateAndLinkProgram();
	shader.Use();
	//add attributes and uniforms
	shader.AddAttribute("vVertex");
	shader.AddUniform("MVP");
	shader.AddUniform("volume");
	shader.AddUniform("camPos");
	shader.AddUniform("step_size");
	shader.AddUniform("lut");

	//pass constant uniforms at initialization
	glUniform3f(shader("step_size"), 1.0f/XDIM, 1.0f/YDIM, 1.0f/ZDIM);
	glUniform1i(shader("volume"),0);
	glUniform1i(shader("lut"), 1);

	shader.UnUse();

	GL_CHECK_ERRORS

	//load volume data
	if(is16bit ? LoadVolumeUShort() : LoadVolume()) {
		std::cout<<"Volume data loaded successfully."<<std::endl; 
	} else {
		std::cout<<"Cannot load volume data."<<std::endl;
		exit(EXIT_FAILURE);
	}

	LoadTransferFunction();

	//set background colour
	glClearColor(bg.r, bg.g, bg.b, bg.a);
	
	//setup unit cube vertex array and vertex buffer objects
	glGenVertexArrays(1, &cubeVAOID);
	glGenBuffers(1, &cubeVBOID);
	glGenBuffers(1, &cubeIndicesID);

	glBindVertexArray(cubeVAOID);
	glBindBuffer (GL_ARRAY_BUFFER, cubeVBOID);
	//pass cube vertices to buffer object memory
	glBufferData (GL_ARRAY_BUFFER, sizeof(vertices), &(vertices[0].x), GL_STATIC_DRAW);

	GL_CHECK_ERRORS
	//enable vertex attributre array for position
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,0,0);

	//pass indices to element array  buffer
	glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, cubeIndicesID);
	glBufferData (GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), &cubeIndices[0], GL_STATIC_DRAW);

	glBindVertexArray(0);

	//enable depth test
	glEnable(GL_DEPTH_TEST);
	glCullFace(GL_BACK);
	glEnable(GL_CULL_FACE);

	//set the over blending function
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	cout<<"Initialization successfull"<<endl;
}

//release all allocated resources
void OnShutdown() {
	shader.DeleteShaderProgram();

	glDeleteVertexArrays(1, &cubeVAOID);
	glDeleteBuffers(1, &cubeVBOID);
	glDeleteBuffers(1, &cubeIndicesID);
	glDeleteTextures(1, &tfTexID);

	glDeleteTextures(1, &textureID);
	cout<<"Shutdown successfull"<<endl;
}


//resize event handler
void OnResize(int w, int h) {
	//reset the viewport
	glViewport (0, 0, (GLsizei) w, (GLsizei) h);
	//setup the projection matrix
	P = glm::perspective(glm::quarter_pi<float >(), (float)w / h, 0.01f, 10000.0f);
}

//display callback function
void OnRender() {
	GL_CHECK_ERRORS

	//set the camera transform
	glm::mat4 Tr	= glm::translate(glm::mat4(1.0f),glm::vec3(0.0f, 0.0f, dist));
	glm::mat4 Rx	= glm::rotate(Tr,  rX, glm::vec3(1.0f, 0.0f, 0.0f));
	glm::mat4 MV    = glm::rotate(Rx, rY, glm::vec3(0.0f, 1.0f, 0.0f));

	//get the camera position
	glm::vec3 camPos = glm::vec3(glm::inverse(MV)*glm::vec4(0,0,0,1));

	//clear colour and depth buffer
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

	//get the combined modelview projection matrix
    glm::mat4 MVP	= P*MV;

	//render grid

	//enable blending and bind the cube vertex array object
	glEnable(GL_BLEND);
	glBindVertexArray(cubeVAOID);
	//bind the raycasting shader
	shader.Use();
	//pass shader uniforms
	glUniformMatrix4fv(shader("MVP"), 1, GL_FALSE, glm::value_ptr(MVP));
	glUniform3fv(shader("camPos"), 1, &(camPos.x));
	//render the cube
	glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, 0);
	//unbind the raycasting shader
	shader.UnUse();
	//disable blending
	glDisable(GL_BLEND);

	//swap front and back buffers to show the rendered result
	glutSwapBuffers();
}

int main(int argc, char** argv) {
	//freeglut initialization
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);
	glutInitContextVersion (3, 3);
	glutInitContextFlags (GLUT_CORE_PROFILE | GLUT_DEBUG);
	glutInitWindowSize(WIDTH, HEIGHT);
	glutCreateWindow("Volume Rendering using GPU Ray Casting - OpenGL 3.3");

	//glew initialization
	glewExperimental = GL_TRUE;
	GLenum err = glewInit();
	if (GLEW_OK != err)	{
		cerr<<"Error: "<<glewGetErrorString(err)<<endl;
	} else {
		if (GLEW_VERSION_3_3)
		{
			cout<<"Driver supports OpenGL 3.3\nDetails:"<<endl;
		}
	}
	err = glGetError(); //this is to ignore INVALID ENUM error 1282
	GL_CHECK_ERRORS

	//output hardware information
	cout<<"\tUsing GLEW "<<glewGetString(GLEW_VERSION)<<endl;
	cout<<"\tVendor: "<<glGetString (GL_VENDOR)<<endl;
	cout<<"\tRenderer: "<<glGetString (GL_RENDERER)<<endl;
	cout<<"\tVersion: "<<glGetString (GL_VERSION)<<endl;
	cout<<"\tGLSL: "<<glGetString (GL_SHADING_LANGUAGE_VERSION)<<endl;

	GL_CHECK_ERRORS

	//OpenGL initialization
	OnInit();

	//callback hooks
	glutCloseFunc(OnShutdown);
	glutDisplayFunc(OnRender);
	glutReshapeFunc(OnResize);
	glutMouseFunc(OnMouseDown);
	glutMotionFunc(OnMouseMove);

	//main loop call
	glutMainLoop();

	return 0;
}