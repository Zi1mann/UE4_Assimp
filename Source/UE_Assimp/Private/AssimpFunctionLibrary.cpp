	// Fill out your copyright notice in the Description page of Project Settings.


#include "AssimpFunctionLibrary.h"

#include "AIBone.h"
#include "AIScene.h"
#include "AINode.h"
#include "AIMaterial.h"
#include "UE_Assimp.h"
#include "assimp/cimport.h"
#include "assimp/DefaultLogger.hpp"
#include "assimp/scene.h"


#if PLATFORM_WINDOWS
#include <Runtime\Core\Public\HAL\FileManager.h>
#include <Runtime\Core\Public\Misc\Paths.h>
#include <Runtime\Core\Public\Windows\COMPointer.h>
#endif

#if PLATFORM_MAC
// Access to Objective-C functions for C++.
#include <CoreFoundation/CoreFoundation.h>
#include <objc/objc.h>
#include <objc/objc-runtime.h>
#include <objc/message.h>

// Access to MainThread wrapper.
#include "Mac/CocoaThread.h"

// Helpers for calling objective-c routines at runtime.
// The objc_msgSend function needs purpose-built casting.
#define id_OBJC_MSGSEND ((id (*)(id, SEL))objc_msgSend)
#define void_OBJC_MSGSEND_bool ((void (*)(id, SEL, bool))objc_msgSend)
#define id_OBJC_MSGSEND_id ((id (*)(id, SEL, id))objc_msgSend)
#define void_OBJC_MSGSEND_id ((void (*)(id, SEL, id))objc_msgSend)
#define id_OBJC_MSGSEND_cstr ((id (*)(id, SEL, const char *))objc_msgSend)
#define id_OBJC_MSGSEND_int ((id (*)(id, SEL, int))objc_msgSend)
#define cstr_OBJC_MSGSEND ((const char *(*)(id, SEL))objc_msgSend)
#define int_OBJC_MSGSEND ((int (*)(id, SEL))objc_msgSend)

#endif

#define MAX_FILETYPES_STR 4096
#define MAX_FILENAME_STR 65536


void UAssimpFunctionLibrary::OpenFileDialogue(FString DialogTitle, FString DefaultPath, FString DefaultFile,
                                              const FString& FileTypes, uint8 Flags, TArray<FString>& OutFilenames,
                                              bool MultiSelect, bool& Success)
{
	const void* ParentWindowHandle = nullptr;
	int OutFilterIndex;
	Success = FileDialogShared(false, ParentWindowHandle, DialogTitle, DefaultPath, DefaultFile, FileTypes, Flags,
	                           OutFilenames, OutFilterIndex);
}


bool UAssimpFunctionLibrary::FileDialogShared(bool bSave, const void* ParentWindowHandle, const FString& DialogTitle,
                                              const FString& DefaultPath, const FString& DefaultFile,
                                              const FString& FileTypes, uint32 Flags, TArray<FString>& OutFilenames,
                                              int32& OutFilterIndex)
{
#pragma region Windows
	//FScopedSystemModalMode SystemModalScope;
#if PLATFORM_WINDOWS
	WCHAR Filename[MAX_FILENAME_STR];
	FCString::Strcpy(Filename, MAX_FILENAME_STR, *(DefaultFile.Replace(TEXT("/"), TEXT("\\"))));

	// Convert the forward slashes in the path name to backslashes, otherwise it'll be ignored as invalid and use whatever is cached in the registry
	WCHAR Pathname[MAX_FILENAME_STR];
	FCString::Strcpy(Pathname, MAX_FILENAME_STR,
	                 *(FPaths::ConvertRelativePathToFull(DefaultPath).Replace(TEXT("/"), TEXT("\\"))));

	// Convert the "|" delimited list of filetypes to NULL delimited then add a second NULL character to indicate the end of the list
	WCHAR FileTypeStr[MAX_FILETYPES_STR];
	const WCHAR* FileTypesPtr = nullptr;
	const int32 FileTypesLen = FileTypes.Len();

	// Nicely formatted file types for lookup later and suitable to append to filenames without extensions
	TArray<FString> CleanExtensionList;

	// The strings must be in pairs for windows.
	// It is formatted as follows: Pair1String1|Pair1String2|Pair2String1|Pair2String2
	// where the second string in the pair is the extension.  To get the clean extensions we only care about the second string in the pair
	TArray<FString> UnformattedExtensions;
	FileTypes.ParseIntoArray(UnformattedExtensions, TEXT("|"), true);
	for (int32 ExtensionIndex = 1; ExtensionIndex < UnformattedExtensions.Num(); ExtensionIndex += 2)
	{
		const FString& Extension = UnformattedExtensions[ExtensionIndex];
		// Assume the user typed in an extension or doesnt want one when using the *.* extension. We can't determine what extension they wan't in that case
		if (Extension != TEXT("*.*"))
		{
			// Add to the clean extension list, first removing the * wildcard from the extension
			const int32 WildCardIndex = Extension.Find(TEXT("*"));
			CleanExtensionList.Add(WildCardIndex != INDEX_NONE ? Extension.RightChop(WildCardIndex + 1) : Extension);
		}
	}

	if (FileTypesLen > 0 && FileTypesLen - 1 < MAX_FILETYPES_STR)
	{
		FileTypesPtr = FileTypeStr;
		FCString::Strcpy(FileTypeStr, MAX_FILETYPES_STR, *FileTypes);

		TCHAR* Pos = FileTypeStr;
		while (Pos[0] != 0)
		{
			if (Pos[0] == '|')
			{
				Pos[0] = 0;
			}

			Pos++;
		}

		// Add two trailing NULL characters to indicate the end of the list
		FileTypeStr[FileTypesLen] = 0;
		FileTypeStr[FileTypesLen + 1] = 0;
	}

	OPENFILENAME ofn;
	FMemory::Memzero(&ofn, sizeof(OPENFILENAME));

	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = (HWND)ParentWindowHandle;
	ofn.lpstrFilter = FileTypesPtr;
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = Filename;
	ofn.nMaxFile = MAX_FILENAME_STR;
	ofn.lpstrInitialDir = Pathname;
	ofn.lpstrTitle = *DialogTitle;
	if (FileTypesLen > 0)
	{
		ofn.lpstrDefExt = &FileTypeStr[0];
	}

	ofn.Flags = OFN_HIDEREADONLY | OFN_ENABLESIZING | OFN_EXPLORER;

	if (bSave)
	{
		ofn.Flags |= OFN_CREATEPROMPT | OFN_OVERWRITEPROMPT | OFN_NOVALIDATE;
	}
	else
	{
		ofn.Flags |= OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
	}

	if (Flags & 0x01)
	{
		ofn.Flags |= OFN_ALLOWMULTISELECT;
	}

	bool bSuccess;
	if (bSave)
	{
		bSuccess = !!::GetSaveFileName(&ofn);
	}
	else
	{
		bSuccess = !!::GetOpenFileName(&ofn);
	}

	if (bSuccess)
	{
		// GetOpenFileName/GetSaveFileName changes the CWD on success. Change it back immediately.
		//FPlatformProcess::SetCurrentWorkingDirectoryToBaseDir();

		if (Flags & 0x01)
		{
			// When selecting multiple files, the returned string is a NULL delimited list
			// where the first element is the directory and all remaining elements are filenames.
			// There is an extra NULL character to indicate the end of the list.
			FString DirectoryOrSingleFileName = FString(Filename);
			TCHAR* Pos = Filename + DirectoryOrSingleFileName.Len() + 1;

			if (Pos[0] == 0)
			{
				// One item selected. There was an extra trailing NULL character.
				OutFilenames.Add(DirectoryOrSingleFileName);
			}
			else
			{
				// Multiple items selected. Keep adding filenames until two NULL characters.
				FString SelectedFile;
				do
				{
					SelectedFile = FString(Pos);
					new(OutFilenames) FString(DirectoryOrSingleFileName / SelectedFile);
					Pos += SelectedFile.Len() + 1;
				}
				while (Pos[0] != 0);
			}
		}
		else
		{
			new(OutFilenames) FString(Filename);
		}

		// The index of the filter in OPENFILENAME starts at 1.
		OutFilterIndex = ofn.nFilterIndex - 1;

		// Get the extension to add to the filename (if one doesnt already exist)
		FString Extension = CleanExtensionList.IsValidIndex(OutFilterIndex)
			                    ? CleanExtensionList[OutFilterIndex]
			                    : TEXT("");

		// Make sure all filenames gathered have their paths normalized and proper extensions added
		for (auto OutFilenameIt = OutFilenames.CreateIterator(); OutFilenameIt; ++OutFilenameIt)
		{
			FString& OutFilename = *OutFilenameIt;

			OutFilename = IFileManager::Get().ConvertToRelativePath(*OutFilename);

			if (FPaths::GetExtension(OutFilename).IsEmpty() && !Extension.IsEmpty())
			{
				// filename does not have an extension. Add an extension based on the filter that the user chose in the dialog
				OutFilename += Extension;
			}

			FPaths::NormalizeFilename(OutFilename);
		}
	}
	else
	{
		uint32 Error = ::CommDlgExtendedError();
		if (Error != ERROR_SUCCESS)
		{
			//UE_LOG(LogDesktopPlatform, Warning, TEXT("Error reading results of file dialog. Error: 0x%04X"), Error);
		}
	}

	return bSuccess;
#endif
#pragma endregion

#pragma region LINUX
#if PLATFORM_LINUX
	return false;
#endif
#pragma endregion

#pragma region MAC
#if PLATFORM_MAC

    bool bSuccess = false;

    OutFilenames.Empty();

    // TODO: honor multi select flag.
    // TODO: set dialog title
    // TODO: set default file
    // TODO: enable file type filters.

    MainThreadCall(^{
        SCOPED_AUTORELEASE_POOL;
        id panel = id_OBJC_MSGSEND((id)objc_getClass("NSOpenPanel"), sel_getUid("openPanel"));
        void_OBJC_MSGSEND_bool(panel, sel_getUid("setCanChooseFiles:"), YES);
        void_OBJC_MSGSEND_bool(panel, sel_getUid("setCanChooseDirectories:"), NO);
        void_OBJC_MSGSEND_bool(panel, sel_getUid("setAllowsMultipleSelection:"), YES);
        void_OBJC_MSGSEND_id(panel, sel_getUid("setDirectoryURL:"),
            id_OBJC_MSGSEND_id((id)objc_getClass("NSURL"), sel_getUid("fileURLWithPath:"),
            id_OBJC_MSGSEND_cstr((id)objc_getClass("NSString"), sel_getUid("stringWithUTF8String:"), TCHAR_TO_UTF8(&DefaultPath))));
        int response = int_OBJC_MSGSEND(panel, sel_getUid("runModal"));

        if (response == NSModalResponseOK) {
            id URLs = id_OBJC_MSGSEND(panel, sel_getUid("URLs"));
            int num_URLs = int_OBJC_MSGSEND(URLs, sel_getUid("count"));
            for (int i = 0; i < num_URLs; ++i) {
                id fileURL = id_OBJC_MSGSEND_int(URLs, sel_getUid("objectAtIndex:"), i);
                id path = id_OBJC_MSGSEND(fileURL, sel_getUid("path"));
                const char *path_utf8 = cstr_OBJC_MSGSEND(path, sel_getUid("UTF8String"));
	        OutFilenames.Add(FString(path_utf8));
            }
        }
    }, UnrealShowEventMode, true);

    return OutFilenames.Num() > 0;

#endif
#pragma endregion
}


void UAssimpFunctionLibrary::ImportScenes(TArray<FString> InFilenames, UObject* ParentObject,
                                          TArray<UAIScene*>& Scenes, int Flags, bool DisableAutoSpaceChange)
{
	Assimp::DefaultLogger::set(new UEAssimpStream());

	for (FString FileName : InFilenames)
	{
		TArray <FVector> temp; 
		UAIScene* Object = UAssimpFunctionLibrary::ImportScene(FileName, ParentObject, Flags, DisableAutoSpaceChange);
		if (Object != nullptr)
                {
			Scenes.Add(Object);
                }
	}
}

/*void CopyNodesWithMeshes(UAINode node, UAIScene targetParent, aiMatrix4x4 accTransform)
{
	UAIScene parent;
	aiMatrix4x4 transform;
	// if node has meshes, create a new scene object for it
	bool hasMeshes; 
	TArray <UAIMesh*> meshes; 
	hasMeshes = node.GetNodeMeshes(meshes); 
	if (hasMeshes)
	{
		UAIScene newScene;
		//targetParent. addChild(newScene);
		// copy the meshes
		//CopyMeshes(node, newObject);
		// the new object is the parent for all child nodes
		// parent = newScene;
		// transform.SetUnity();
	}
	else
	{
		// if no meshes, skip the node, but keep its transformation
		parent = targetParent;
		//FTransform transform = node.GetNodeTransform()  UAssimpFunctionLibrary::aiMatToTransform(accTransform); 
	}
	// continue for all child nodes
	const TArray <UAINode*> children = node.GetChildNodes(); 

	for (int i = 0; i < children.Num(); i++) {
		CopyNodesWithMeshes(children[i], parent, transform);
	}
	for (all node.mChildren)
		CopyNodesWithMeshes(node.mChildren[a], parent, transform);
}*/


TArray <UAIScene*> UAssimpFunctionLibrary::ImportSceneSplitMeshes(FString fileName, UObject* parentObjext, int flags, bool disableAutoSpaceChange) {
	TArray <UAIScene*> aiScenes; 

	return aiScenes; 
}


UAIScene* UAssimpFunctionLibrary::ImportScene(FString FileName, UObject* ParentObject, int Flags, bool DisableAutoSpaceChange)
{
	Assimp::DefaultLogger::set(new UEAssimpStream());

	if (!DisableAutoSpaceChange) {
		Flags |= aiProcess_MakeLeftHanded | aiProcessPreset_TargetRealtime_Quality;
	}

	const struct aiScene* scene = aiImportFile(TCHAR_TO_UTF8(*FileName), (unsigned int)Flags);

	if (!scene)
	{
		UE_LOG(LogAssimp, Error, TEXT("Error importing scene in assimpfunction library "))
			return nullptr;
	}
	else
	{
		UAIScene* Object = UAIScene::InternalConstructNewScene(ParentObject, scene, DisableAutoSpaceChange);
		Object->FullFilePath = FileName;
		return Object;
	}
}


UAIScene* UAssimpFunctionLibrary::NewImportScene(FString FileName, UObject* ParentObject, int Flags, bool DisableAutoSpaceChange, TArray <FTransform>& nodeTransforms, TArray <FSTRUCT_NodeMeshHolder_CPP>& sceneMeshHolder, TArray <int32>& materialIndices)
{
	Assimp::DefaultLogger::set(new UEAssimpStream());

        if (!DisableAutoSpaceChange) {
           Flags |= aiProcess_MakeLeftHanded | aiProcessPreset_TargetRealtime_Quality;
        }

	const struct aiScene* scene = aiImportFile(TCHAR_TO_UTF8(*FileName), (unsigned int)Flags);

	if (!scene)
	{
		UE_LOG(LogAssimp, Error, TEXT("Error importing scene in assimpfunction library "))
		return nullptr;
	}
	else
	{
		UAIScene* Object = UAIScene::InternalConstructNewScene(ParentObject, scene, DisableAutoSpaceChange);
		Object->FullFilePath=FileName;

		UAINode* root = Object->GetRootNode(); 
		if (root) {
			const TArray <UAINode*> children = root->GetChildNodes();
			UE_LOG(LogAssimp, Warning, TEXT("Root node num of children: %d"), children.Num());

			TArray<UAIMesh*> meshes; 
			root->GetNodeMeshes(meshes);
			int32 num = meshes.Num(); 

			TArray<FSTRUCT_NodeMeshHolder_CPP> finMeshHolder; 
			TArray<int32> finMatIndexHolder;
			UE_LOG(LogAssimp, Warning, TEXT("root->GetNodeMeshes.Num(): %d"), num);
			// for each child node of the root node: 
			for (int i = 0; i < children.Num(); i++) {
				// retrieve meshes of node
				TArray<UAIMesh*> locMeshes; 
				children[i]->GetNodeMeshes(locMeshes);
				UE_LOG(LogAssimp, Warning, TEXT("Current node has meshes: %d"), locMeshes.Num());
				// assign child meshes to holder struct and to array of structs
				FSTRUCT_NodeMeshHolder_CPP holder; 
				holder.nodeMeshes = locMeshes; 
				finMeshHolder.Add(holder); 

				// get transforms of child nodes
				FTransform temp;
				children[i]->GetNodeTransform(temp);

				// retrieve material indices from each mesh
				for (int j = 0; j < locMeshes.Num(); j++) {
					//tempMatIdcs.Add(locMeshes[j]->GetMaterialIndex()); 
					finMatIndexHolder.Add(locMeshes[j]->GetMaterialIndex());
					UE_LOG(LogAssimp, Warning, TEXT("Material index: %d"), locMeshes[j]->GetMaterialIndex());
				}
				FVector meshLoc = temp.GetLocation();
				FVector meshScale = temp.GetScale3D();
				//relPosVectors.Add(meshLoc);
				//UE_LOG(LogAssimp, Warning, TEXT("loc: %s"), *meshLoc.ToString());
				//UE_LOG(LogAssimp, Warning, TEXT("scale: %s"), *meshScale.ToString());
				nodeTransforms.Add(temp);
			}
			materialIndices = finMatIndexHolder;
			sceneMeshHolder = finMeshHolder;  
		}
		 
		return Object;
	}
}



void UAssimpFunctionLibrary::ImportScenesAsync(FString InFilename,UObject* ParentObject, int Flags, bool DisableAutoSpaceChange,FOnProgressUpdated OnProgressUpdated,FOnImportSceneComplete OnImportSceneComplete)
	{

	//I'm a noob in realms of async if you find a better way to keep data do a pull request
	Assimp::DefaultLogger::set(new UEAssimpStream());

	if (!DisableAutoSpaceChange) {
		Flags |= aiProcess_MakeLeftHanded | aiProcessPreset_TargetRealtime_Quality;
	}
	
	static  int NumOfThreads=0;
	static int TotalThreads=0;
	NumOfThreads=TotalThreads=1;
	static TArray<UAIScene*> AIScenes;
	AIScenes.Empty();
	
	if(NumOfThreads==0)
	{
		return;
	}
		AsyncTask(ENamedThreads::AnyNormalThreadNormalTask,[&]()
	   { 
			const struct aiScene* scene = aiImportFile( TCHAR_TO_UTF8( *InFilename),Flags);

			if( !scene) {

				UE_LOG(LogAssimp,Error,TEXT("Error importing scene in assimpfunction library async"))
				
			}else
			{
		   AsyncTask(ENamedThreads::GameThread,[&]()
		   {
							
			   UAIScene* Object=	UAIScene::InternalConstructNewScene(ParentObject,scene,DisableAutoSpaceChange);
								
			   NumOfThreads=NumOfThreads-1;
			   AIScenes.Add(Object);
			   OnProgressUpdated.Execute(1-static_cast<float>(NumOfThreads)/TotalThreads,Object);
						
			   if(NumOfThreads==0)
			   {
			   const	float EndTime=ParentObject->GetWorld()->GetTimeSeconds();
			
				   OnImportSceneComplete.Execute(AIScenes);
				  
								
			   }
		   });
		}
});
}

FTransform UAssimpFunctionLibrary::aiMatToTransform(aiMatrix4x4 NodeTransform)
{
	FMatrix mtx;
	FTransform Transform;

	// Note that assimp matrix is transpose of Unreal matrix.
	// (The compiler will efficiently unroll these loops.)
	for (int j = 0; j < 4; ++j) {
		for (int i = 0; i < 4; ++i) {
			mtx.M[i][j] = NodeTransform[j][i];
		}
	}
	Transform = FTransform(mtx);

	return Transform;
}

aiMatrix4x4 UAssimpFunctionLibrary::TransformtoaiMat(FTransform nodeTransform)
{
	FMatrix temp = nodeTransform.ToMatrixWithScale();
	aiMatrix4x4 targetTransform;

	for (int i = 0; i < 4; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			targetTransform[i][j] = temp.M[j][i]; 
		}
	}
	return targetTransform; 
}


FString UAssimpFunctionLibrary::GetBoneName(FAIBone Bone)
{
	return UTF8_TO_TCHAR(Bone.Bone->mName.C_Str());
}
int UAssimpFunctionLibrary::GetNumOfWeights(FAIBone Bone)
{
	return Bone.Bone->mNumWeights;
}

FTransform UAssimpFunctionLibrary::GetBoneTransform(FAIBone Bone)
{
	return UAssimpFunctionLibrary::aiMatToTransform(Bone.Bone->mOffsetMatrix);
}

void UAssimpFunctionLibrary::GetBoneWeights(FAIBone Bone, TArray<FAIVertexWeight>& Weights)
{
	for (unsigned int i = 0; i < Bone.Bone->mNumWeights; i++)
	{
		FAIVertexWeight Weight = FAIVertexWeight(Bone.Bone->mWeights[i]);
		Weights.Add(Weight);
	}
}

void UAssimpFunctionLibrary::SetActorNameDebug(AActor* InActor, FString ActorName)
{


	//we intend to use this only for debugging inside editor
#if WITH_EDITOR  
	InActor->Rename(*ActorName);
	InActor->SetActorLabel(*ActorName);
#endif //  
}

void UAssimpFunctionLibrary::GetMaterialBaseColor(UAIMaterial* inputMaterial, FLinearColor& baseColor) {
	inputMaterial->GetMaterialBaseColor(baseColor); 
}


//void UAssimpFunctionLibrary::Cast

