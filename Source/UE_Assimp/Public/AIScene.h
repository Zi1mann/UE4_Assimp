// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "assimp/Exporter.hpp"
#include "UObject/NoExportTypes.h"
#include "ProceduralMeshComponent.h"
#include "AIScene.generated.h"


USTRUCT(BlueprintType)
struct FSTRUCT_ExportProcMeshData_CPP
{
	GENERATED_BODY()
		UPROPERTY(EditAnywhere, BlueprintReadWrite)
		TArray <FVector> vertices;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		TArray <int32> triangles;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		TArray <FVector> normals;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		TArray <FVector2D> UV0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		TArray <FProcMeshTangent> tangents;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		FTransform nodeTransform;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		TArray<int32> materialIdcs;

	UPROPERTY()
		UObject* SafeObjectPointer;
};

/**
 * 
 */
class UAIMesh;
class UAINode;
class UAICamera;
class UAILight;
class UAIMaterial;

DECLARE_DYNAMIC_DELEGATE_OneParam(FAsyncDelegateExample, const TArray<float>&, OutData);

//wrapper for scene
UCLASS(BlueprintType, DefaultToInstanced)
class UE_ASSIMP_API UAIScene : public UObject
{
	GENERATED_BODY()

	//TODO Get Meta data 
public:
	static UAIScene* InternalConstructNewScene(UObject* Parent, const aiScene* Scene, const bool DisableAutoSpaceChange);

	UFUNCTION(BlueprintCallable)
	static void ConstructAssimpSceneAndWriteToFBXFile(UObject* Parent, TArray<FSTRUCT_ExportProcMeshData_CPP> sceneMeshData, TArray <UMaterialInstanceDynamic*> dynamicMaterialInstances);
	UFUNCTION(BlueprintCallable)
	static void EmptySceneTestExport(); 

	UFUNCTION(BlueprintCallable)
	static void AddNumbersAsync(TArray<float> InData, const FAsyncDelegateExample& OutData);

	//UFUNCTION(BlueprintCallable)
	//static void Async_EmptySceneTestExport(const aiScene* scene, const FAsyncExportScene& result);

	/*WIP Function:
	Will spawn all meshes in most optimised fashion 
	*/
	UFUNCTION(BlueprintCallable, Category="Assimp|Scene")
	TArray<UMeshComponent*> SpawnAllMeshes(FTransform Transform, TSubclassOf<AActor> ClassToSpawn);
	/*Get All meshes stored in this scene
	* note that each material section is considered a  separate mesh 
	*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Assimp|Scene")
	const TArray<UAIMesh*>& GetAllMeshes() const;


	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Assimp|Scene")
	const TArray<UAIMaterial*>& GetAllMaterials() const;
	/** The root node of the hierarchy.
	*
	* There will always be at least the root node if the import
	* was successful (and no special flags have been set).
	* Presence of further nodes depends on the format and content
	* of the imported file.
	*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Assimp|Scene")
	UAINode* GetRootNode();
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Assimp|Scene")
	UAIMesh* GetMeshAtIndex(int Index);
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Assimp|Scene")
	const TArray<UAICamera*>& GetAllCameras() const;
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Assimp|Scene")
	const TArray<UAILight*>& GetAllLights() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Assimp|Scene")
	float GetUnitScaleFactor();

	//Texture
	//! Returns an embedded texture. if null then check path or texture is not embedded and must be imported using unreal default import texture function
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Assimp|Scene")
	UTexture2D* GetEmbeddedTexture(FString FilePath, bool bIsNormalMap);
	/*NEEDS TO BE IMPLEMENTED*/
	//Retrieves relative filepath to the embedded texture and the texture type to load it directly from this path using Unreal
	//UFUNCTION(BlueprintCallable, BlueprintPure)
	//const void GetEmbeddedTexturePathAndType(FString& texturePath)//, EAiTextureType& textureType);
	UPROPERTY(BlueprintReadOnly)
	FString FullFilePath;
	UFUNCTION(BlueprintCallable,BlueprintPure)
	float GetSceneScale();

	//UFUNCTION(BlueprintCallable, BlueprintPure)
	
	//UFUNCTION(BlueprintCallable, BlueprintPure)
	//UFUNCTION(BlueprintCallable)
		//static void AddMeshAsNode(UAIScene* targetScene, FSTRUCT_ExportProcMeshData_CPP meshData);

	static EPixelFormat GetPixelFormat(const aiTexture* Texture);

private:
	//For Object Creation
	UPROPERTY(Transient)
	TArray<UAIMesh*> OwnedMeshes;
	UPROPERTY(Transient)
	UAINode* OwnedRootNode;
	UPROPERTY(Transient)
	TArray<UAICamera*> OwnedCameras;
	UPROPERTY(Transient)
	TArray<UAILight*> OwnedLights;
	UPROPERTY(Transient)
	TArray<UAIMaterial*> OwnedMaterials;
	aiScene* scene;
	virtual void BeginDestroy() override;
	float SceneScale;
};


