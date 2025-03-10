// Fill out your copyright notice in the Description page of Project Settings.



#include "AIScene.h"
#include "AICamera.h"
#include "AILight.h"
#include "AIMesh.h"
#include "AINode.h"
#include "AIMaterial.h"
#include "AssimpMesh.h"
#include "AssimpFunctionLibrary.h"
#include "assimp/cimport.h"
#include "CoreMinimal.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/BufferArchive.h"

using namespace Assimp; 

/*void CopyNodesWithMeshes(const aiScene* aiScene, aiNode node, aiMatrix4x4 accTransform)
{
	SceneObject parent;
	Matrix4x4 transform;
	// if node has meshes, create a new scene object for it
	if (node.mNumMeshes > 0)
	{
		SceneObjekt newObject = new SceneObject;
		targetParent.addChild(newObject);
		// copy the meshes
		CopyMeshes(node, newObject);
		// the new object is the parent for all child nodes
		parent = newObject;
		transform.SetUnity();
	}
	else
	{
		// if no meshes, skip the node, but keep its transformation
		parent = targetParent;
		transform = node.mTransformation * accTransform;
	}
	// continue for all child nodes
	for (all node.mChildren) {
		CopyNodesWithMeshes(node.mChildren[a], parent, transform);
	}
}*/

UAIScene* UAIScene::InternalConstructNewScene(UObject* Parent, const aiScene* Scene, const bool DisableAutoSpaceChange)
{
	//todo check if object is already created and skip creation and return object 
	UAIScene* SceneObject = NewObject<UAIScene>(Parent, UAIScene::StaticClass(), NAME_None, RF_Transient);
	SceneObject->scene = const_cast<aiScene*>(Scene);
	//Setup Meshes
	SceneObject->OwnedMeshes.Reset();
	SceneObject->OwnedMeshes.AddUninitialized(Scene->mNumMeshes);
	SceneObject->OwnedLights.AddUninitialized(Scene->mNumLights);
	SceneObject->OwnedCameras.AddUninitialized(Scene->mNumCameras);
	SceneObject->OwnedMaterials.AddUninitialized(Scene->mNumMaterials);
	//SceneObject->OwnedRootNode.AddUniitialiued(Scene->mRootNode); 

        //Add Meshes
	if (Scene->HasMeshes())
	{
		for (unsigned Index = 0; Index < Scene->mNumMeshes; Index++)
		{
			UAIMesh* Mesh = NewObject<UAIMesh>(SceneObject, UAIMesh::StaticClass(), NAME_None, RF_Transient);
			Mesh->Mesh = Scene->mMeshes[Index];
			bool hasPositions = Scene->mMeshes[Index]->HasPositions(); 
			if (hasPositions) {
			}
			SceneObject->OwnedMeshes[Index] = Mesh;
		}
	}

	//Add Cams
	if (Scene->HasCameras())
	{
		for (unsigned Index = 0; Index < Scene->mNumCameras; Index++)
		{
			UAICamera* Camera = NewObject<UAICamera>(SceneObject, UAICamera::StaticClass(), NAME_None, RF_Transient);
			Camera->camera = Scene->mCameras[Index];
			SceneObject->OwnedCameras[Index] = Camera;
		}
	}

	//Add Lights
	if (Scene->HasLights())
	{
		for (unsigned Index = 0; Index < Scene->mNumLights; Index++)
		{
			UAILight* Light = NewObject<UAILight>(SceneObject, UAILight::StaticClass(), NAME_None, RF_Transient);
			Light->Light = Scene->mLights[Index];
			SceneObject->OwnedLights[Index] = Light;
		}
	}

	//Add Materials
	if (Scene->HasMaterials())
	{
		UE_LOG(LogAssimp, Log, TEXT("numOfMaterials in scene: %d"), Scene->mNumMaterials); 
		for (unsigned Index = 0; Index < Scene->mNumMaterials; Index++)
		{
			UAIMaterial* Material = NewObject<UAIMaterial>(SceneObject, UAIMaterial::StaticClass(), NAME_None,
			                                               RF_Transient);
			Material->Material = Scene->mMaterials[Index];
			SceneObject->OwnedMaterials[Index] = Material;
			FLinearColor baseColor; 
		}
	}

        //Build Node Tree
	UAINode* RootNode = NewObject<UAINode>(SceneObject, UAINode::StaticClass(), NAME_None, RF_Transient);
	SceneObject->OwnedRootNode = RootNode;


        // If assimp scene does not have UnitScaleFactor in metadata, presume 1.0f
	bool success = SceneObject->scene->mMetaData->Get("UnitScaleFactor", SceneObject->SceneScale);
	
        if (!success) {
            SceneObject->SceneScale = 1.0f;
            UE_LOG(LogAssimp, Warning, TEXT("No UnitScaleFactor in metadata."));
        }
        if (SceneObject->SceneScale == 0.0f) {
            SceneObject->SceneScale = 1.0f;
            UE_LOG(LogAssimp, Warning, TEXT("Zero UnitScaleFactor replaced with 1.0."));
        }
        UE_LOG(LogAssimp, Warning, TEXT("UnitScaleFactor: %g"), SceneObject->SceneScale);

        // The "parent" transform of the root node is an identity matrix.
        // However, we optionally apply the UnitScaleFactor and an x-rotation to move from y-up to z-up.
        aiMatrix4x4t<float> AdjustmentXfm;
        if (!DisableAutoSpaceChange) {
            aiMatrix4x4t<float> tmpRot;
            aiMatrix4x4t<float> tmpScale;
            aiMatrix4x4t<float>::RotationX( PI/2., tmpRot);
            aiMatrix4x4t<float>::Scaling( aiVector3t<float>(SceneObject->SceneScale), tmpScale);
            AdjustmentXfm = tmpScale * tmpRot;
        }
		
		bool hasParent = true; 
		UAINode* parentNode = RootNode->GetParentNode(hasParent); 

		if (hasParent) {
			UE_LOG(LogAssimp, Warning, TEXT("hasParent: %g"), hasParent);
			FTransform tmpTransform; 
			parentNode->GetNodeTransform(tmpTransform);
			//aiMatrix4x4t<float>::Translation(); 
		}
		else {
			UE_LOG(LogAssimp, Warning, TEXT("hasParent: FALSE "), hasParent);
		}

	RootNode->Setup(Scene->mRootNode, SceneObject, AdjustmentXfm);

        // Changes root transformation to incorporate unitscalefactor and x-rotation.
        if (!DisableAutoSpaceChange) {
             Scene->mRootNode->mTransformation = AdjustmentXfm * Scene->mRootNode->mTransformation;
        }
	// walk along node tree and retrieve scene hierarchy+
	TArray<UAINode*> childNodes = RootNode->GetChildNodes();
	//for(int index = 0; i<RootNode->mChildren

	return SceneObject;
}


float UAIScene::GetUnitScaleFactor()
{
	return SceneScale;
}

TArray<UMeshComponent*> UAIScene::SpawnAllMeshes(FTransform Transform, TSubclassOf<AActor> ClassToSpawn)
{
	//TODO 
	TArray<UMeshComponent*> Meshes;
	if (scene)
	{
		for (const auto Mesh : OwnedMeshes)
		{
			//mesh data will be owned by the scene object
			AActor* SpawnedActor = GetWorld()->SpawnActor<AActor>(ClassToSpawn, Transform);
			UAssimpMesh* AssimpMesh = NewObject<UAssimpMesh>(SpawnedActor, UAssimpMesh::StaticClass());
			AssimpMesh->RegisterComponent();
			AssimpMesh->SetWorldLocation(Transform.GetLocation());
			AssimpMesh->SetWorldRotation(Transform.GetRotation());
			AssimpMesh->AttachToComponent(SpawnedActor->GetRootComponent(),
			                              FAttachmentTransformRules::KeepWorldTransform);
			AssimpMesh->SetupMesh(Mesh);
		}
	}
	else
	{
		UE_LOG(LogAssimp, Error, TEXT("Assimp scene is not valid "));
	}


	return Meshes;
}

const TArray<UAIMesh*>& UAIScene::GetAllMeshes() const
{
	return OwnedMeshes;
}


const TArray<UAIMaterial*>& UAIScene::GetAllMaterials() const
{
	return OwnedMaterials;
}

void UAIScene::BeginDestroy()
{
	aiReleaseImport(scene);
	scene = nullptr;

	Super::BeginDestroy();
}


UAINode* UAIScene::GetRootNode()
{
	return OwnedRootNode;
}

UAIMesh* UAIScene::GetMeshAtIndex(int Index)
{
	return OwnedMeshes[Index];
}

const TArray<UAICamera*>& UAIScene::GetAllCameras() const
{
	return OwnedCameras;
}

const TArray<UAILight*>& UAIScene::GetAllLights() const
{
	return OwnedLights;
}


UTexture2D* UAIScene::GetEmbeddedTexture(FString FilePath, bool bIsNormalMap)
{
	const auto EmbedTexture = scene->GetEmbeddedTexture(TCHAR_TO_UTF8(*FilePath));

	UTexture2D* Result;


	if (!EmbedTexture)
	{
		UE_LOG(LogAssimp, Log, TEXT("Embedded texture not found . Texture might be  external "));
		return nullptr;
	}

	const int TextureWidth = EmbedTexture->mWidth;
	const float TextureHeight = EmbedTexture->mHeight;
	const EPixelFormat PixelFormat = PF_B8G8R8A8;

	UE_LOG(LogAssimp, Log, TEXT("Importing embedded texture X: %d  Y: %f"), TextureWidth, TextureHeight);
	if (EmbedTexture->mHeight != 0)
	{
		Result = UTexture2D::CreateTransient(EmbedTexture->mWidth, EmbedTexture->mHeight, PixelFormat);


		if (Result)
		{
#if ENGINE_MAJOR_VERSION>4

			FTexturePlatformData* PlatformData = Result->GetPlatformData();
#else

			FTexturePlatformData* PlatformData = Result->PlatformData;
#endif
			Result->bNotOfflineProcessed = true;
			uint8* MipData = static_cast<uint8*>(PlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE));


			//an array for pixels
			uint8* Pixels = new uint8[TextureWidth * TextureHeight * 4];
			for (int32 y = 0; y < TextureHeight; y++)
			{
				for (int32 x = 0; x < TextureWidth; x++)
				{
					//Get the current pixel
					const int32 CurrentPixelIndex = ((y * TextureWidth) + x);
					//Get a random vector that will represent the RGB values for the current pixel
					const aiTexel CurrentPixel = EmbedTexture->pcData[CurrentPixelIndex];

					Pixels[4 * CurrentPixelIndex] = CurrentPixel.b; //b
					Pixels[4 * CurrentPixelIndex + 1] = CurrentPixel.g; //g
					Pixels[4 * CurrentPixelIndex + 2] = CurrentPixel.b; //r
					Pixels[4 * CurrentPixelIndex + 3] = CurrentPixel.a; //set A channel always to maximum
				}
			}
			FMemory::Memcpy(MipData, Pixels, PlatformData->Mips[0].BulkData.GetBulkDataSize());


		        PlatformData->Mips[0].BulkData.Unlock();

		        Result->UpdateResource();
		}
	}
	else
	{
		//Texture is compressed read it from memory
		const size_t size = EmbedTexture->mWidth;
		const auto BinaryData = reinterpret_cast<const unsigned char*>(EmbedTexture->pcData);
		const TArray<uint8> Buffer(BinaryData, size);


		Result = UKismetRenderingLibrary::ImportBufferAsTexture2D(GetWorld(), Buffer);
	}

	if (Result && bIsNormalMap)
	{
		Result->CompressionSettings = TC_Normalmap;
		Result->SRGB = false;
		Result->UpdateResource();
	}
	return Result;
}


EPixelFormat UAIScene::GetPixelFormat(const aiTexture* Texture)
{
	if (Texture->CheckFormat("rgba8888"))
	{
		return EPixelFormat::PF_R8G8B8A8;
	}
	else
	{
		UE_LOG(LogAssimp, Fatal, TEXT("Pixel format not implemented"));
	}
	return EPixelFormat::PF_Unknown;
}

 float UAIScene::GetSceneScale()
{
	return SceneScale;
}

 // NEEDS TO BE IMPLEMENTED
/* static void UAIScene::GetEmbeddedTexturePathAndType(FString& texturePath, EAiTextureType& textureType) {
	 FString path; 

 }*/

 void  UAIScene::ConstructAssimpSceneAndWriteToFBXFile(UObject* Parent, TArray<FSTRUCT_ExportProcMeshData_CPP> sceneMeshData, TArray <UMaterialInstanceDynamic*> dynamicMaterialInstances)
 {	
	 // init new aiScene
	 aiScene* scene = new aiScene(); 
	 scene->mNumMeshes = sceneMeshData.Num();
	 scene->mMeshes = new aiMesh * [scene->mNumMeshes];
	 scene->mMaterials = new aiMaterial * [scene->mNumMeshes];
	
	 // init transformation of root node
	 aiVector3D scale = aiVector3D(1, 1, 1); 
	 aiVector3D pos = aiVector3D(0, 0, 0); 
	 /* not sure about this*/
	 aiQuaternion rot = aiQuaternion(aiVector3D(0, 1, 1), 0); 
	 aiMatrix4x4 transf = aiMatrix4x4(scale, rot, pos); 
	 
	 // init new root node
	 scene->mRootNode = new aiNode();
	 scene->mRootNode->mName = aiString("RootNode");
	 //scene->mRootNode->mTransformation = transf;
	 //UE_LOG(LogAssimp, Warning, TEXT("name of root node: %s"), const(scene->mRootNode->mName)); 

	 // initialize child nodes according to number of meshes given in mesh data struct, each mesh will be associated with its own child node
	 // old: scene->mRootNode->mNumMeshes = sceneMeshData.Num(); 
	 scene->mRootNode->mNumMeshes = 0; 
	 scene->mRootNode->mMeshes = new unsigned int[1];
	 scene->mRootNode->mNumChildren = sceneMeshData.Num(); 
	 //scene->mRootNode->mChildren = aiNode[sceneMeshData.Num()]; 
	 for (int i = 0; i < sceneMeshData.Num(); i++) {
		 // retrieve data from input 
		 FSTRUCT_ExportProcMeshData_CPP item = sceneMeshData[i]; 
		 TArray<FVector> verts = item.vertices; 
		 TArray<int32> tris = item.triangles; 
		 TArray<FVector> norms = item.normals; 
		 TArray<FVector2D> uvs = item.UV0; 
		 TArray<FProcMeshTangent> tans = item.tangents; 
		 FTransform trans = item.nodeTransform; 
		 TArray<int32> matId = item.materialIdcs; 
		 
		 // initialize and set up child node
		 aiNode* child = new aiNode();
		 child->mParent = scene->mRootNode;
		 child->mNumMeshes = 1;
		 child->mMeshes = new unsigned int[1]; 
		 child->mMeshes[0] = i;
		
		 
		 aiMatrix4x4 childTrans = UAssimpFunctionLibrary::TransformtoaiMat(trans);
		 child->mTransformation = childTrans;

		 // using the geometry information provided in the input, construct mesh
		 aiMesh* mesh = new aiMesh();
		
		 mesh->mNumVertices = verts.Num(); 
		 mesh->mVertices = new aiVector3D[mesh->mNumVertices]; 
		 mesh->mNormals = new aiVector3D[mesh->mNumVertices]; 
		 mesh->mTangents = new aiVector3D[mesh->mNumVertices];
		 mesh->mBitangents = new aiVector3D[mesh->mNumVertices]; 
		 
		 // ????
		 mesh->mTextureCoords[0] = new aiVector3D[mesh->mNumVertices]; 
		 mesh->mNumUVComponents[0] = 2;
		
		 // now assign vertices etc
		/*
		 for (int j = 0; j < verts.Num();j++)
		 {	
			 //aiVector3D aiVert =  
			 mesh->mVertices[j] = aiVector3D(verts[j].X, verts[j].Y, verts[j].Z);
			 mesh->mNormals[j] = aiVector3D(norms[j].X, norms[j].Y, norms[j].Z);
			 //Maybe assimp postprocessing can be used here! 
			 //FProcMeshTangent tan = tans[j]; 
			 //FProcMeshTangent test = FProcMeshTangent(1, 0.5, 3); 
			 // 
			 //lets try without tangents, otherwise calculate tangents

		 } 
			*/
		 // Just for testing if this works so far
		 for (int j = 0; j < matId.Num(); j++)
		 {
			 UE_LOG(LogAssimp, Warning, TEXT("mat id = %d"), matId[j]); 
		 }
	 }
	 Exporter exporter;
	 FString dir = FPaths::ProjectDir();
	 dir += FString(TEXT("Export/Scenes/test.fbx"));
	 
	 aiScene* testscene = new aiScene();
	 exporter.Export(testscene, "fbx", "test.fbx");
 }


 void UAIScene::ExportSceneToFile(TArray <FSTRUCT_SceneNodeData_CPP> sceneData, int32 numIndependentNodes, FString targetFile, int32 numOfNodes)
 {
	 // Creating a test scene
	 //std::unique_ptr<aiScene> scene(new aiScene());
	 aiScene* scene = new aiScene(); 
	 scene->mNumMaterials = 1;
	 scene->mMaterials = new aiMaterial * [] { new aiMaterial() };
	 scene->mRootNode = new aiNode();
	 scene->mRootNode->mNumMeshes = 0;
	 //scene->mRootNote->mTransformation =
     //scene->mRootNode->mMeshes = new unsigned [] { 0 };
	 scene->mRootNode->mNumChildren = sceneData.Num(); 
	 unsigned mChildren = static_cast<unsigned>(numIndependentNodes);
	 scene->mRootNode->mChildren = new aiNode*[mChildren];
	 scene->mMetaData = new aiMetadata(); // workaround, issue #3781
	
	 scene->mNumMeshes = sceneData.Num();
	 scene->mMeshes = new aiMesh * [scene->mNumMeshes];

	 // Create and initialize new node, add to hierarchy, add all meshes associated with that Node to global mesh 
	 // container and set correct references afterwards. 
	 unsigned globalMeshIndex = 0; 
	 for (int elem = 0; elem < sceneData.Num(); elem++) {
		 //for each element in the sceneData, create node and populate that will be added as child node to root node with orientation as specifed.
		 FSTRUCT_SceneNodeData_CPP currentNodeData = sceneData[elem];
		 aiNode* currentNode = new aiNode(); 
		 currentNode->mName = std::string(TCHAR_TO_UTF8(&currentNodeData.nodeName));
		 //currentNode->mTransformation = UAssimpFunctionLibrary::TransformtoaiMat(currentNodeData.nodeTransform); 
		 //aiVector3D* test; 
		 //currentNode->mTransformation.a1 
		 currentNode->mMeshes = new unsigned[unsigned(currentNodeData.nodeNumOfMeshes)];
		 currentNode->mNumChildren = 0; 
		 
		 //indices of nodes meshes pointing to corresponding mesh in scene->mMeshes
		 //add all Meshes to scene->mMeshes
		 for (int meshIndex = 0; meshIndex < currentNodeData.nodeNumOfMeshes; meshIndex++) {
			 FSTRUCT_ExportProcMeshData_CPP currentMesh = currentNodeData.nodeMeshData[meshIndex];
			 unsigned int temp = unsigned(currentMesh.vertices.Num());
			 //UE_LOG(LogAssimp, Warning, TEXT("vertices: %d"), temp);
			 aiMesh* tempMesh = new aiMesh;
			 // take care of vertices
			 tempMesh->mNumVertices = currentMesh.vertices.Num();
			 tempMesh->mVertices = new aiVector3D[tempMesh->mNumVertices];
			 // UE_LOG(LogAssimp, Warning, TEXT("current mesh vertices: %d"), tempMesh->mNumVertices);

			 for (int k = 0; k < currentMesh.vertices.Num(); k++) {
				aiVector3D vert;
				vert.x = currentMesh.vertices[k][0];
				vert.y = currentMesh.vertices[k][1];
				vert.z = currentMesh.vertices[k][2];
				tempMesh->mVertices[k] = vert;
			 }

			 // take care of faces which are given by triangles
			 // bugged as of now!
			 tempMesh->mNumFaces = currentMesh.triangles.Num();
			 // UE_LOG(LogAssimp, Warning, TEXT("current mesh tris: %d"), tempMesh->mNumFaces);
			 tempMesh->mFaces = new aiFace[tempMesh->mNumFaces];
			 tempMesh->mPrimitiveTypes = aiPrimitiveType_TRIANGLE; // workaround, issue #3778#
			 unsigned k = 0;

			 // needs to be fixed
			 while (k < unsigned(currentMesh.triangles.Num())) {
				 tempMesh->mFaces[k].mNumIndices = 3;
				 tempMesh->mFaces[k].mIndices = new unsigned[] { 0,1,2};
				 //UE_LOG(LogAssimp, Warning, TEXT("current mesh tris iteration: %d"), k);
				 k++;
			 }

			 UE_LOG(LogAssimp, Warning, TEXT("Num of tris: %d"), currentMesh.triangles.Num());
			 UE_LOG(LogAssimp, Warning, TEXT("Num of verts: %d"), currentMesh.vertices.Num());
			
			 unsigned temp_index = meshIndex+globalMeshIndex;
			 scene->mMeshes[temp_index] = tempMesh;
			 UE_LOG(LogAssimp, Warning, TEXT("vertices in scene mesh: %d"), scene->mMeshes[temp_index]->mNumVertices);
			 UE_LOG(LogAssimp, Warning, TEXT("first vertex x coord: %f"), scene->mMeshes[temp_index]->mVertices[1].x);
			// UE_LOG(LogAssimp, Warning, TEXT("mesh vertices: %d"), scene->mMeshes[i]->mNumVertices);
		 }
		 // set references of meshes for currentNode to scene->mMeshes
		 for (unsigned loc = 0; loc < unsigned(currentNodeData.nodeNumOfMeshes); loc++) {
			 currentNode->mMeshes[loc] = loc + globalMeshIndex; 
			 UE_LOG(LogAssimp, Warning, TEXT("currentNode->mMeshes[loc] = loc + globalMeshIndex: %d"), loc + globalMeshIndex);
			 UE_LOG(LogAssimp, Warning, TEXT("scene->mMeshes[loc+globalMeshIndex]->mVertices[0].x = %f"), loc + globalMeshIndex);

		 }
		 globalMeshIndex += unsigned(currentNodeData.nodeNumOfMeshes); 
		 // add currentNode as child to scene->rootNode 
		 scene->mRootNode->mChildren[elem] = currentNode;
		 // set parent node of currentNode to be scene->mRootNode
		 currentNode->mParent = scene->mRootNode; 
	 }


	 //accessing now from the top level of the scene the nodes and meshes for testing purposes
	 UE_LOG(LogAssimp, Warning, TEXT("scene->mRootNode->mChildren[0]->mMeshes[0] = %d"), scene->mRootNode->mChildren[0]->mMeshes[0]);
	 UE_LOG(LogAssimp, Warning, TEXT("scene->mRootNode->mChildren[0]->mMeshes[0]->mVertices[0].x = %f"), scene->mMeshes[0]->mVertices[0].x);
	 UE_LOG(LogAssimp, Warning, TEXT("scene->mRootNode->mChildren[0]->mMeshes[0]->mVertices[0].y = %f"), scene->mMeshes[0]->mVertices[0].y);
	 UE_LOG(LogAssimp, Warning, TEXT("scene->mRootNode->mChildren[0]->mMeshes[0]->mVertices[0].z = %f"), scene->mMeshes[0]->mVertices[0].z);
	 UE_LOG(LogAssimp, Warning, TEXT("scene->mRootNode->mChildren[0]->mMeshes[0]->mVertices[5].x = %f"), scene->mMeshes[0]->mVertices[5].z);


	 // Creating an exporter
	 Assimp::Exporter exporter;
	
	 // Constructing the export path
	 FString ExportPath = FPaths::ProjectDir();
	 ExportPath = FPaths::Combine(ExportPath, "Export");
	 ExportPath = FPaths::ConvertRelativePathToFull(ExportPath);
	 if (!FPlatformFileManager::Get().GetPlatformFile().DirectoryExists(*ExportPath))
	 {
		 FPlatformFileManager::Get().GetPlatformFile().CreateDirectory(*ExportPath);
	 }
	 ExportPath = FPaths::Combine(ExportPath, targetFile);

	 // Exporting the scene
	 if (exporter.Export(scene, "fbx", TCHAR_TO_ANSI(*ExportPath)) != AI_SUCCESS)
	 {
		 UE_LOG(LogAssimp, Fatal, TEXT("Exporting scene to fbx failed: %s"), ANSI_TO_TCHAR(exporter.GetErrorString()));
	 }
	 
	// scene->mNumMeshes = 1;
	// UE_LOG(LogAssimp, Warning, TEXT(" %d"), scene->mNumMeshes);

	 //scene->mMeshes = new aiMesh * [] {mesh};
	 //scene->mNumMaterials = 1;
	 // scene->mMaterials = new aiMaterial * [] { new aiMaterial() };
	 //scene->mRootNode = new aiNode();
	// scene->mRootNode->mNumMeshes = 1;
	// scene->mRootNode->mMeshes = new unsigned [] { 0 };
	// scene->mMetaData = new aiMetadata(); // workaround, issue #3781
	
	 // Creating an exporter
	//Assimp::Exporter exporter;
	/*
	 // Constructing the export path
	 FString ExportPath = FPaths::ProjectDir();
	 ExportPath = FPaths::Combine(ExportPath, "Export");
	 ExportPath = FPaths::ConvertRelativePathToFull(ExportPath);
	 if (!FPlatformFileManager::Get().GetPlatformFile().DirectoryExists(*ExportPath))
	 {
		 FPlatformFileManager::Get().GetPlatformFile().CreateDirectory(*ExportPath);
	 }
	 ExportPath = FPaths::Combine(ExportPath, "Test.obj");

	 // Exporting the scene
	 if (exporter.Export(scene.get(), "obj", TCHAR_TO_ANSI(*ExportPath)) != AI_SUCCESS)
	 {
		 UE_LOG(LogAssimp, Fatal, TEXT("Exporting scene to fbx failed: %s"), ANSI_TO_TCHAR(exporter.GetErrorString()));
	 }
	*/
 }
 
void UAIScene::AddNumbersAsync(TArray<float> InData, const FAsyncDelegateExample& Result)
{
	AsyncTask(ENamedThreads::BackgroundThreadPriority, [InData = MoveTemp(InData), Result]() mutable
	{
		// Just for example
		for (float& InDataElement : InData)
		{
			InDataElement += 10;
		}

		aiScene* testscene = new aiScene();
		FString dir = FPaths::ProjectDir();
		dir += FString(TEXT("Export/Scenes/test.obj"));
		Exporter exporter;
		if (exporter.Export(testscene, "obj", "test.obj") != AI_SUCCESS)
		{
			UE_LOG(LogAssimp, Fatal, TEXT("Exporting scene to fbx failed."));
		}
		
		AsyncTask(ENamedThreads::AnyThread, [OutData = MoveTemp(InData), Result]() mutable
		{
			Result.ExecuteIfBound(OutData);
		});
	});
}


 /*void UAIScene::AddMeshAsNode(UAIScene* targetScene) {

 }*/