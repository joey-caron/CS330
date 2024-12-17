///////////////////////////////////////////////////////////////////////////////
// scenemanager.cpp
// ============
// manage the preparing and rendering of 3D scenes - textures, materials, lighting
//
//  AUTHOR: Brian Battersby - SNHU Instructor / Computer Science
//	Created for CS-330-Computational Graphics and Visualization, Nov. 1st, 2023
///////////////////////////////////////////////////////////////////////////////

#include "SceneManager.h"

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#endif

#include <glm/gtx/transform.hpp>

// declaration of global variables
namespace
{
	const char* g_ModelName = "model";
	const char* g_ColorValueName = "objectColor";
	const char* g_TextureValueName = "objectTexture";
	const char* g_UseTextureName = "bUseTexture";
	const char* g_UseLightingName = "bUseLighting";
}

/***********************************************************
 *  SceneManager()
 *
 *  The constructor for the class
 ***********************************************************/
SceneManager::SceneManager(ShaderManager *pShaderManager)
{
	m_pShaderManager = pShaderManager;
	m_basicMeshes = new ShapeMeshes();

	// initialize the texture collection
	for (int i = 0; i < 16; i++)
	{
		m_textureIDs[i].tag = "/0";
		m_textureIDs[i].ID = -1;
	}
	m_loadedTextures = 0;
}

/***********************************************************
 *  ~SceneManager()
 *
 *  The destructor for the class
 ***********************************************************/
SceneManager::~SceneManager()
{
	// clear the allocated memory
	m_pShaderManager = NULL;
	delete m_basicMeshes;
	m_basicMeshes = NULL;
	// destroy the created OpenGL textures
	DestroyGLTextures();
}

/***********************************************************
 *  CreateGLTexture()
 *
 *  This method is used for loading textures from image files,
 *  configuring the texture mapping parameters in OpenGL,
 *  generating the mipmaps, and loading the read texture into
 *  the next available texture slot in memory.
 ***********************************************************/
bool SceneManager::CreateGLTexture(const char* filename, std::string tag)
{
	int width = 0;
	int height = 0;
	int colorChannels = 0;
	GLuint textureID = 0;

	// indicate to always flip images vertically when loaded
	stbi_set_flip_vertically_on_load(true);

	// try to parse the image data from the specified image file
	unsigned char* image = stbi_load(
		filename,
		&width,
		&height,
		&colorChannels,
		0);

	// if the image was successfully read from the image file
	if (image)
	{
		std::cout << "Successfully loaded image:" << filename << ", width:" << width << ", height:" << height << ", channels:" << colorChannels << std::endl;

		glGenTextures(1, &textureID);
		glBindTexture(GL_TEXTURE_2D, textureID);

		// set the texture wrapping parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		// set texture filtering parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		// if the loaded image is in RGB format
		if (colorChannels == 3)
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
		// if the loaded image is in RGBA format - it supports transparency
		else if (colorChannels == 4)
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
		else
		{
			std::cout << "Not implemented to handle image with " << colorChannels << " channels" << std::endl;
			return false;
		}

		// generate the texture mipmaps for mapping textures to lower resolutions
		glGenerateMipmap(GL_TEXTURE_2D);

		// free the image data from local memory
		stbi_image_free(image);
		glBindTexture(GL_TEXTURE_2D, 0); // Unbind the texture

		// register the loaded texture and associate it with the special tag string
		m_textureIDs[m_loadedTextures].ID = textureID;
		m_textureIDs[m_loadedTextures].tag = tag;
		m_loadedTextures++;

		return true;
	}

	std::cout << "Could not load image:" << filename << std::endl;

	// Error loading the image
	return false;
}

/***********************************************************
 *  BindGLTextures()
 *
 *  This method is used for binding the loaded textures to
 *  OpenGL texture memory slots.  There are up to 16 slots.
 ***********************************************************/
void SceneManager::BindGLTextures()
{
	for (int i = 0; i < m_loadedTextures; i++)
	{
		// bind textures on corresponding texture units
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, m_textureIDs[i].ID);
	}
}

/***********************************************************
 *  DestroyGLTextures()
 *
 *  This method is used for freeing the memory in all the
 *  used texture memory slots.
 ***********************************************************/
void SceneManager::DestroyGLTextures()
{
	for (int i = 0; i < m_loadedTextures; i++)
	{
		glGenTextures(1, &m_textureIDs[i].ID);
	}
}

/***********************************************************
 *  FindTextureID()
 *
 *  This method is used for getting an ID for the previously
 *  loaded texture bitmap associated with the passed in tag.
 ***********************************************************/
int SceneManager::FindTextureID(std::string tag)
{
	int textureID = -1;
	int index = 0;
	bool bFound = false;

	while ((index < m_loadedTextures) && (bFound == false))
	{
		if (m_textureIDs[index].tag.compare(tag) == 0)
		{
			textureID = m_textureIDs[index].ID;
			bFound = true;
		}
		else
			index++;
	}

	return(textureID);
}

/***********************************************************
 *  FindTextureSlot()
 *
 *  This method is used for getting a slot index for the previously
 *  loaded texture bitmap associated with the passed in tag.
 ***********************************************************/
int SceneManager::FindTextureSlot(std::string tag)
{
	int textureSlot = -1;
	int index = 0;
	bool bFound = false;

	while ((index < m_loadedTextures) && (bFound == false))
	{
		if (m_textureIDs[index].tag.compare(tag) == 0)
		{
			textureSlot = index;
			bFound = true;
		}
		else
			index++;
	}

	return(textureSlot);
}

/***********************************************************
 *  FindMaterial()
 *
 *  This method is used for getting a material from the previously
 *  defined materials list that is associated with the passed in tag.
 ***********************************************************/
bool SceneManager::FindMaterial(std::string tag, OBJECT_MATERIAL& material)
{
	if (m_objectMaterials.size() == 0)
	{
		return(false);
	}

	unsigned int index = 0;
	bool bFound = false;
	while ((index < m_objectMaterials.size()) && (bFound == false))
	{
		if (m_objectMaterials[index].tag.compare(tag) == 0)
		{
			bFound = true;
			material.diffuseColor = m_objectMaterials[index].diffuseColor;
			material.specularColor = m_objectMaterials[index].specularColor;
			material.shininess = m_objectMaterials[index].shininess;
		}
		else
		{
			index++;
		}
	}

	return(true);
}

/***********************************************************
 *  SetTransformations()
 *
 *  This method is used for setting the transform buffer
 *  using the passed in transformation values.
 ***********************************************************/
void SceneManager::SetTransformations(
	glm::vec3 scaleXYZ,
	float XrotationDegrees,
	float YrotationDegrees,
	float ZrotationDegrees,
	glm::vec3 positionXYZ)
{
	// variables for this method
	glm::mat4 modelView;
	glm::mat4 scale;
	glm::mat4 rotationX;
	glm::mat4 rotationY;
	glm::mat4 rotationZ;
	glm::mat4 translation;

	// set the scale value in the transform buffer
	scale = glm::scale(scaleXYZ);
	// set the rotation values in the transform buffer
	rotationX = glm::rotate(glm::radians(XrotationDegrees), glm::vec3(1.0f, 0.0f, 0.0f));
	rotationY = glm::rotate(glm::radians(YrotationDegrees), glm::vec3(0.0f, 1.0f, 0.0f));
	rotationZ = glm::rotate(glm::radians(ZrotationDegrees), glm::vec3(0.0f, 0.0f, 1.0f));
	// set the translation value in the transform buffer
	translation = glm::translate(positionXYZ);

	modelView = translation * rotationZ * rotationY * rotationX * scale;

	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setMat4Value(g_ModelName, modelView);
	}
}

/***********************************************************
 *  SetShaderColor()
 *
 *  This method is used for setting the passed in color
 *  into the shader for the next draw command
 ***********************************************************/
void SceneManager::SetShaderColor(
	float redColorValue,
	float greenColorValue,
	float blueColorValue,
	float alphaValue)
{
	// variables for this method
	glm::vec4 currentColor;

	currentColor.r = redColorValue;
	currentColor.g = greenColorValue;
	currentColor.b = blueColorValue;
	currentColor.a = alphaValue;

	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setIntValue(g_UseTextureName, false);
		m_pShaderManager->setVec4Value(g_ColorValueName, currentColor);
	}
}

/***********************************************************
 *  SetShaderTexture()
 *
 *  This method is used for setting the texture data
 *  associated with the passed in ID into the shader.
 ***********************************************************/
void SceneManager::SetShaderTexture(
	std::string textureTag)
{
	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setIntValue(g_UseTextureName, true);

		int textureID = -1;
		textureID = FindTextureSlot(textureTag);
		m_pShaderManager->setSampler2DValue(g_TextureValueName, textureID);
	}
}

/***********************************************************
 *  SetTextureUVScale()
 *
 *  This method is used for setting the texture UV scale
 *  values into the shader.
 ***********************************************************/
void SceneManager::SetTextureUVScale(float u, float v)
{
	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setVec2Value("UVscale", glm::vec2(u, v));
	}
}

/***********************************************************
 *  SetShaderMaterial()
 *
 *  This method is used for passing the material values
 *  into the shader.
 ***********************************************************/
void SceneManager::SetShaderMaterial(
	std::string materialTag)
{
	if (m_objectMaterials.size() > 0)
	{
		OBJECT_MATERIAL material;
		bool bReturn = false;

		bReturn = FindMaterial(materialTag, material);
		if (bReturn == true)
		{
			m_pShaderManager->setVec3Value("material.diffuseColor", material.diffuseColor);
			m_pShaderManager->setVec3Value("material.specularColor", material.specularColor);
			m_pShaderManager->setFloatValue("material.shininess", material.shininess);
			
		}
	}
}

/**************************************************************/
/*** STUDENTS CAN MODIFY the code in the methods BELOW for  ***/
/*** preparing and rendering their own 3D replicated scenes.***/
/*** Please refer to the code in the OpenGL sample project  ***/
/*** for assistance.                                        ***/
/**************************************************************/

/***********************************************************
  *  DefineObjectMaterials()
  *
  *  This method is used for configuring the various material
  *  settings for all of the objects within the 3D scene.
  ***********************************************************/
void SceneManager::DefineObjectMaterials()
{
	/*** STUDENTS - add the code BELOW for defining object materials. ***/
	/*** There is no limit to the number of object materials that can ***/
	/*** be defined. Refer to the code in the OpenGL Sample for help  ***/

	OBJECT_MATERIAL carbonMaterial;
	carbonMaterial.ambientColor = glm::vec3(0.01f, 0.01f, 0.01f);
	carbonMaterial.ambientStrength = 0.4f;
	carbonMaterial.diffuseColor = glm::vec3(0.05f, 0.05f, 0.05f);
	carbonMaterial.specularColor = glm::vec3(0.2f, 0.2f, 0.2f);
	carbonMaterial.shininess = 5.0f;
	carbonMaterial.tag = "carbon";
	m_objectMaterials.push_back(carbonMaterial);

	OBJECT_MATERIAL plasticMaterial;
	plasticMaterial.ambientColor = glm::vec3(0.1f, 0.1f, 0.1f);
	plasticMaterial.ambientStrength = 0.3f;
	plasticMaterial.diffuseColor = glm::vec3(0.1f, 0.1f, 0.1f);
	plasticMaterial.specularColor = glm::vec3(0.1f, 0.1f, 0.1f);
	plasticMaterial.shininess = 25.0f;
	plasticMaterial.tag = "plastic";
	m_objectMaterials.push_back(plasticMaterial);

	OBJECT_MATERIAL fabricMaterial;
	fabricMaterial.ambientColor = glm::vec3(0.01f, 0.01f, 0.01f);
	fabricMaterial.ambientStrength = 0.1f;
	fabricMaterial.diffuseColor = glm::vec3(0.05f, 0.05f, 0.05f);
	fabricMaterial.specularColor = glm::vec3(0.01f, 0.01f, 0.01f);
	fabricMaterial.shininess = 5.0f;
	fabricMaterial.tag = "fabric";
	m_objectMaterials.push_back(fabricMaterial);

	OBJECT_MATERIAL noteMaterial;
	noteMaterial.ambientColor = glm::vec3(0.01f, 0.01f, 0.2f);
	noteMaterial.ambientStrength = 0.4f;
	noteMaterial.diffuseColor = glm::vec3(0.4f, 0.4f, 0.4f);
	noteMaterial.specularColor = glm::vec3(0.5f, 0.5f, 0.5f);
	noteMaterial.shininess = 100.0f;
	noteMaterial.tag = "note";
	m_objectMaterials.push_back(noteMaterial);

	OBJECT_MATERIAL woodMaterial;
	woodMaterial.ambientColor = glm::vec3(0.1f, 0.1f, 0.05f);
	woodMaterial.ambientStrength = 0.1f;
	woodMaterial.diffuseColor = glm::vec3(0.2f, 0.2f, 0.15f);
	woodMaterial.specularColor = glm::vec3(0.1f, 0.1f, 0.05f);
	woodMaterial.shininess = 50.0f;
	woodMaterial.tag = "wood";
	m_objectMaterials.push_back(woodMaterial);

	OBJECT_MATERIAL pencilMaterial;
	pencilMaterial.ambientColor = glm::vec3(0.3f, 0.3f, 0.0f);
	pencilMaterial.ambientStrength = 0.4f;
	pencilMaterial.diffuseColor = glm::vec3(0.7f, 0.7f, 0.0f);
	pencilMaterial.specularColor = glm::vec3(0.9f, 0.9f, 0.0f);
	pencilMaterial.shininess = 10.0f;
	pencilMaterial.tag = "pencil";
	m_objectMaterials.push_back(pencilMaterial);

	OBJECT_MATERIAL metalMaterial;
	metalMaterial.ambientColor = glm::vec3(0.5f, 0.5f, 0.5f);
	metalMaterial.ambientStrength = 0.4f;
	metalMaterial.diffuseColor = glm::vec3(0.8f, 0.8f, 0.8f);
	metalMaterial.specularColor = glm::vec3(0.9f, 0.9f, 0.9f);
	metalMaterial.shininess = 100.0f;
	metalMaterial.tag = "metal";
	m_objectMaterials.push_back(metalMaterial);

	OBJECT_MATERIAL rubberMaterial;
	rubberMaterial.ambientColor = glm::vec3(0.5f, 0.37f, 0.4f);
	rubberMaterial.ambientStrength = 0.9f;
	rubberMaterial.diffuseColor = glm::vec3(0.5f, 0.37f, 0.4f);
	rubberMaterial.specularColor = glm::vec3(0.01f, 0.007f, 0.008f);
	rubberMaterial.shininess = 10.0f;
	rubberMaterial.tag = "rubber";
	m_objectMaterials.push_back(rubberMaterial);

}

/***********************************************************
* LoadSceneTextures()
*
* This method is used for preparing the 3D scene by loading
* the shapes, textures in memory to support the 3D scene
* rendering
***********************************************************/
void SceneManager::LoadSceneTextures()
{
	bool bReturn = false;

	
	bReturn = CreateGLTexture(
		"textures/desktop.jpg", "desk"
	);

	bReturn = CreateGLTexture(
		"textures/keyboard.jpg", "keyboard"
	);

	bReturn = CreateGLTexture(
		"textures/rest.jpg", "rest"
	);

	bReturn = CreateGLTexture(
		"textures/notebook.jpg", "notebook"
	);

	bReturn = CreateGLTexture(
		"textures/metal.jpg", "metal"
	);

	bReturn = CreateGLTexture(
		"textures/wood.jpg", "wood"
	);

	bReturn = CreateGLTexture(
		"textures/pencil.jpg", "pencil"
	);

	bReturn = CreateGLTexture(
		"textures/metal1.jpg", "metal1"
	);

	bReturn = CreateGLTexture(
		"textures/eraser.jpg", "eraser"
	);

	BindGLTextures();

}

/***********************************************************
 *  SetupSceneLights()
 *
 *  This method is called to add and configure the light
 *  sources for the 3D scene.  There are up to 4 light sources.
 ***********************************************************/
void SceneManager::SetupSceneLights()
{
	// this line of code is NEEDED for telling the shaders to render 
	// the 3D scene with custom lighting, if no light sources have
	// been added then the display window will be black - to use the 
	// default OpenGL lighting then comment out the following line
	//m_pShaderManager->setBoolValue(g_UseLightingName, true);

	/*** STUDENTS - add the code BELOW for setting up light sources ***/
	/*** Up to four light sources can be defined. Refer to the code ***/
	/*** in the OpenGL Sample for help                              ***/

	m_pShaderManager->setVec3Value("directionalLight.direction", -5.0f, -5.0f, -4.0f);
	m_pShaderManager->setVec3Value("directionalLight.ambient", 0.8f, 0.8f, 0.8f);
	m_pShaderManager->setVec3Value("directionalLight.diffuse", 0.8f, 0.8f, 0.8f);
	m_pShaderManager->setVec3Value("directionalLight.specular", 0.6f, 0.6f, 0.6f);
	m_pShaderManager->setBoolValue("directionalLight.bActive", true);

	m_pShaderManager->setVec3Value("pointLights[0].position", 0.0f, 8.0f, 1.0f);
	m_pShaderManager->setVec3Value("pointLights[0].ambient", 0.4f, 0.4f, 0.3f);
	m_pShaderManager->setVec3Value("pointLights[0].diffuse", 0.8f, 0.8f, 0.7f);
	m_pShaderManager->setVec3Value("pointLights[0].specular", 0.9f, 0.9f, 0.8f);
	m_pShaderManager->setBoolValue("pointLights[0].bActive", true);


	m_pShaderManager->setBoolValue("bUseLighting", true);
}

/***********************************************************
 *  PrepareScene()
 *
 *  This method is used for preparing the 3D scene by loading
 *  the shapes, textures in memory to support the 3D scene 
 *  rendering
 ***********************************************************/
void SceneManager::PrepareScene()
{
	DefineObjectMaterials();
	LoadSceneTextures();
	SetupSceneLights();
	// only one instance of a particular mesh needs to be
	// loaded in memory no matter how many times it is drawn
	// in the rendered 3D scene

	m_basicMeshes->LoadPlaneMesh();
	m_basicMeshes->LoadPrismMesh();
	m_basicMeshes->LoadBoxMesh();
	m_basicMeshes->LoadCylinderMesh();
	m_basicMeshes->LoadConeMesh();
	m_basicMeshes->LoadTorusMesh();
	m_basicMeshes->LoadBoxMesh2();


}

/***********************************************************
 *  RenderScene()
 *
 *  This method is used for rendering the 3D scene by 
 *  transforming and drawing the basic 3D shapes
 ***********************************************************/
void SceneManager::RenderScene()
{
	// declare the variables for the transformations
	glm::vec3 scaleXYZ;
	float XrotationDegrees = 0.0f;
	float YrotationDegrees = 0.0f;
	float ZrotationDegrees = 0.0f;
	glm::vec3 positionXYZ;

	/*** Set needed transformations before drawing the basic mesh.  ***/
	/*** This same ordering of code should be used for transforming ***/
	/*** and drawing all the basic 3D shapes.						***/
	/******************************************************************/
	// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(15.0f, 1.0f, 5.0f);

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(0.0f, 0.0f, 4.0f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	//SetShaderColor(1, 1, 1, 1);
	SetShaderMaterial("carbon");
	SetShaderTexture("desk");


	// draw the mesh with transformation values
	m_basicMeshes->DrawPlaneMesh();
	/******************************************************************/

	/*** Keyboard ***/
	/******************************************************************/
	// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(10.0f, 0.2f, 4.0f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = 1.8f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(0.0f, 0.05f, 4.0f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	//SetShaderColor(0.2f, 0.2f, 0.2f, 1);
	SetShaderTexture("keyboard");
	SetShaderMaterial("plastic");

	m_basicMeshes->DrawBoxMesh();


	// draw the mesh with transformation values


	/****************************************************************/

	/*** Coaster ***/
	/******************************************************************/
	// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(1.0f, 0.05f, 1.0f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(7.0f, 0.0f, 4.0f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	//SetShaderColor(0.2f, 0.2f, 0.2f, 1);
	SetShaderTexture("rest");
	SetShaderMaterial("fabric");

	m_basicMeshes->DrawCylinderMesh(); 


	// draw the mesh with transformation values


	/****************************************************************/

	/*** WristRest ***/
	/******************************************************************/
	// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(9.5f, 0.05f, 1.5f);

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(0.0f, 0.05f, 7.0f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	m_basicMeshes->DrawBoxMesh();

	// top part
	scaleXYZ = glm::vec3(9.4f, 0.15f, 1.4f);

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(0.0f, 0.1f, 7.0f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	m_basicMeshes->DrawBoxMesh();


	/****************************************************************/

	/*** Notebook ***/
	/******************************************************************/
	// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(4.8f, 0.1f, 6.0f);

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(-9.0f, 0.05f, 4.5f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	//SetShaderColor(0.2f, 0.2f, 0.2f, 1);
	SetShaderTexture("notebook");
	SetShaderMaterial("note");

	m_basicMeshes->DrawBoxMesh2();


	// draw the mesh with transformation values


	/****************************************************************/

	/*** Spirals ***/
	/******************************************************************/
	// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(0.1f, 0.1f, 0.05f);

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(-11.4f, 0.05f, 4.0f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	//SetShaderColor(0.2f, 0.2f, 0.2f, 1);
	SetShaderTexture("metal");
	SetShaderMaterial("metal");

	m_basicMeshes->DrawTorusMesh();

	// draw the mesh with transformation values

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(-11.4f, 0.05f, 4.2f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	m_basicMeshes->DrawTorusMesh();


	// draw the mesh with transformation values

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(-11.4f, 0.05f, 4.4f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	m_basicMeshes->DrawTorusMesh();


	// draw the mesh with transformation values

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(-11.4f, 0.05f, 4.6f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	m_basicMeshes->DrawTorusMesh();


	// draw the mesh with transformation values

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(-11.4f, 0.05f, 4.8f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	m_basicMeshes->DrawTorusMesh();


	// draw the mesh with transformation values

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(-11.4f, 0.05f, 5.0f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	m_basicMeshes->DrawTorusMesh();


	// draw the mesh with transformation values

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(-11.4f, 0.05f, 5.2f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	m_basicMeshes->DrawTorusMesh();


	// draw the mesh with transformation values

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(-11.4f, 0.05f, 5.4f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	m_basicMeshes->DrawTorusMesh();


	// draw the mesh with transformation values

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(-11.4f, 0.05f, 5.6f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	m_basicMeshes->DrawTorusMesh();


	// draw the mesh with transformation values

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(-11.4f, 0.05f, 5.8f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	m_basicMeshes->DrawTorusMesh();

	// draw the mesh with transformation values

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(-11.4f, 0.05f, 6.0f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	m_basicMeshes->DrawTorusMesh();

	// draw the mesh with transformation values

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(-11.4f, 0.05f, 6.2f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	m_basicMeshes->DrawTorusMesh();

	// draw the mesh with transformation values

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(-11.4f, 0.05f, 6.4f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	m_basicMeshes->DrawTorusMesh();

	// draw the mesh with transformation values

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(-11.4f, 0.05f, 6.6f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	m_basicMeshes->DrawTorusMesh();


	// draw the mesh with transformation values

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(-11.4f, 0.05f, 6.8f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	m_basicMeshes->DrawTorusMesh();


	// draw the mesh with transformation values

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(-11.4f, 0.05f, 7.0f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	m_basicMeshes->DrawTorusMesh();

	// draw the mesh with transformation values

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(-11.4f, 0.05f, 7.2f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	m_basicMeshes->DrawTorusMesh();

	// draw the mesh with transformation values

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(-11.4f, 0.05f, 7.4f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	m_basicMeshes->DrawTorusMesh();

	// draw the mesh with transformation values

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(-11.4f, 0.05f, 3.8f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	m_basicMeshes->DrawTorusMesh();

	// draw the mesh with transformation values

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(-11.4f, 0.05f, 3.6f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	m_basicMeshes->DrawTorusMesh();

	// draw the mesh with transformation values

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(-11.4f, 0.05f, 3.4f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	m_basicMeshes->DrawTorusMesh();

	// draw the mesh with transformation values

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(-11.4f, 0.05f, 3.2f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	m_basicMeshes->DrawTorusMesh();

	// draw the mesh with transformation values

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(-11.4f, 0.05f, 3.0f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	m_basicMeshes->DrawTorusMesh();

	// draw the mesh with transformation values

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(-11.4f, 0.05f, 2.8f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	m_basicMeshes->DrawTorusMesh();

	// draw the mesh with transformation values

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(-11.4f, 0.05f, 2.6f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	m_basicMeshes->DrawTorusMesh();

	// draw the mesh with transformation values

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(-11.4f, 0.05f, 2.4f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	m_basicMeshes->DrawTorusMesh();

	// draw the mesh with transformation values

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(-11.4f, 0.05f, 2.2f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	m_basicMeshes->DrawTorusMesh();

	// draw the mesh with transformation values

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(-11.4f, 0.05f, 2.0f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	m_basicMeshes->DrawTorusMesh();

	// draw the mesh with transformation values

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(-11.4f, 0.05f, 1.8f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	m_basicMeshes->DrawTorusMesh();

	// draw the mesh with transformation values

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(-11.4f, 0.05f, 1.6f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	m_basicMeshes->DrawTorusMesh();

	// draw the mesh with transformation values

	/****************************************************************/

	/*** Pencils ***/
	/******************************************************************/
	// set the XYZ scale for the pencil tips
	scaleXYZ = glm::vec3(0.1f, 0.3f, 0.1f);

	// set the XYZ rotation for the pencil tips
	XrotationDegrees = -90.0f;
	YrotationDegrees = 45.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(-9.8f, 0.2f, 2.2f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	//SetShaderColor(0.2f, 0.2f, 0.2f, 1);
	SetShaderMaterial("wood");
	SetShaderTexture("wood");

	m_basicMeshes->DrawConeMesh();
	// draw the mesh with transformation values

	positionXYZ = glm::vec3(-10.3f, 0.2f, 2.7f);

	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	m_basicMeshes->DrawConeMesh();

	// set the XYZ scale for the pencil barrels
	scaleXYZ = glm::vec3(0.1f, 3.253f, 0.1f);

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(-7.5f, 0.2f, 4.5f);

	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	SetShaderColor(0.2f, 0.2f, 0.2f, 1);
	SetShaderMaterial("pencil");
	SetShaderTexture("pencil");

	m_basicMeshes->DrawCylinderMesh();

	positionXYZ = glm::vec3(-8.0f, 0.2f, 5.0f);

	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	m_basicMeshes->DrawCylinderMesh();

	// set the XYZ scale for the metal connector
	scaleXYZ = glm::vec3(0.105f, 0.3f, 0.105f);

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(-7.3f, 0.2f, 4.7f);

	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	SetShaderColor(0.2f, 0.2f, 0.2f, 1);
	SetShaderMaterial("metal");
	SetShaderTexture("metal1");

	m_basicMeshes->DrawCylinderMesh();

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(-7.8f, 0.2f, 5.2f);

	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	m_basicMeshes->DrawCylinderMesh();

	// set the XYZ scale for the eraser
	scaleXYZ = glm::vec3(0.1f, 0.2f, 0.1f);

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(-7.2f, 0.2f, 4.8f);

	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	//SetShaderColor(0.2f, 0.2f, 0.2f, 1);
	SetShaderMaterial("rubber");
	SetShaderTexture("eraser");

	m_basicMeshes->DrawCylinderMesh();

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(-7.7f, 0.2f, 5.3f);

	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	m_basicMeshes->DrawCylinderMesh();

	/****************************************************************/

}