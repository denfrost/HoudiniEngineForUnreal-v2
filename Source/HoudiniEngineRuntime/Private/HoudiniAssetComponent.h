/*
* Copyright (c) <2018> Side Effects Software Inc.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
*
* 2. The name of Side Effects Software may not be used to endorse or
*    promote products derived from this software without specific prior
*    written permission.
*
* THIS SOFTWARE IS PROVIDED BY SIDE EFFECTS SOFTWARE "AS IS" AND ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
* NO EVENT SHALL SIDE EFFECTS SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

#include "HoudiniEngineRuntimeUtils.h"
#include "HoudiniRuntimeSettings.h"
#include "HoudiniOutput.h"
#include "HoudiniInputTypes.h"
#include "HoudiniPluginSerializationVersion.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"

#include "HoudiniAssetComponent.generated.h"

class UHoudiniAsset;
class UHoudiniParameter;
class UHoudiniInput;
class UHoudiniOutput;
class UHoudiniHandleComponent;
class UHoudiniPDGAssetLink;
class UHoudiniAssetComponent_V1;

UENUM()
enum class EHoudiniAssetState : uint8
{
	// Loaded / Duplicated HDA,
	// Will need to be instantiated upon change/update
	NeedInstantiation,

	// Newly created HDA, needs to be instantiated immediately
	PreInstantiation,

	// Instantiating task in progress
	Instantiating,	

	// Instantiated HDA, needs to be cooked immediately
	PreCook,

	// Cooking task in progress
	Cooking,

	// Cooking has finished
	PostCook,

	// Cooked HDA, needs to be processed immediately
	PreProcess,

	// Processing task in progress
	Processing,

	// Processed / Updated HDA
	// Will need to be cooked upon change/update
	None,

	// Asset needs to be rebuilt (Deleted/Instantiated/Cooked)
	NeedRebuild,

	// Asset needs to be deleted
	NeedDelete,

	// Deleting
	Deleting,

	// Process component template. This is ticking has very limited
	// functionality, typically limited to checking for parameter updates
	// in order to trigger PostEditChange() to run construction scripts again.
	ProcessTemplate,
};

UENUM()
enum class EHoudiniAssetStateResult : uint8
{
	None,
	Working,
	Success,
	FinishedWithError,
	FinishedWithFatalError,
	Aborted
};

UENUM()
enum class EHoudiniStaticMeshMethod : uint8
{
	// Use the RawMesh method to build the UStaticMesh
	RawMesh,
	// Use the FMeshDescription method to build the UStaticMesh
	FMeshDescription,
	// Build a fast proxy mesh: UHoudiniStaticMesh
	UHoudiniStaticMesh,
};

#if WITH_EDITORONLY_DATA
UENUM()
enum class EHoudiniEngineBakeOption : uint8
{
	ToActor,
	ToBlueprint,
	ToFoliage,
	ToWorldOutliner,
};
#endif

class UHoudiniAssetComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FHoudiniAssetEvent, UHoudiniAsset*);
DECLARE_MULTICAST_DELEGATE_OneParam(FHoudiniAssetComponentEvent, UHoudiniAssetComponent*)

UCLASS(ClassGroup = (Rendering, Common), hidecategories = (Object, Activation, "Components|Activation"), ShowCategories = (Mobility), editinlinenew)
class HOUDINIENGINERUNTIME_API UHoudiniAssetComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

	// Declare translators as friend so they can easily directly modify
	// Inputs, outputs and parameters
	friend class FHoudiniEngineManager;
	friend struct FHoudiniOutputTranslator;
	friend struct FHoudiniInputTranslator;
	friend struct FHoudiniSplineTranslator;
	friend struct FHoudiniParameterTranslator;
	friend struct FHoudiniPDGManager;
	friend struct FHoudiniHandleTranslator;

#if WITH_EDITORONLY_DATA
	friend class FHoudiniAssetComponentDetails;
#endif
	
public:

	// Declare the delegate that is broadcast when RefineMeshesTimer fires
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnRefineMeshesTimerDelegate, UHoudiniAssetComponent*);
	DECLARE_DELEGATE_RetVal_OneParam(bool, FOnPostCookBakeDelegate, UHoudiniAssetComponent*);

	virtual ~UHoudiniAssetComponent();

	virtual void Serialize(FArchive & Ar) override;

	virtual bool ConvertLegacyData();

	// Called after the C++ constructor and after the properties have been initialized, including those loaded from config.
	// This is called before any serialization or other setup has happened.
	virtual void PostInitProperties() override;

	// Returns the Owner actor / HAC name
	FString	GetDisplayName() const;

	// Indicates if the HAC needs to be updated
	bool NeedUpdate() const;

	// Indicates if the HAC's transform needs to be updated
	bool NeedTransformUpdate() const { return (bHasComponentTransformChanged && bUploadTransformsToHoudiniEngine); };

	// Indicates if any of the HAC's output components needs to be updated (no recook needed)
	bool NeedOutputUpdate() const;

	// Check whether any inputs / outputs / parameters have made blueprint modifications.
	bool NeedBlueprintStructureUpdate() const;
	bool NeedBlueprintUpdate() const;

	// Try to find one of our parameter that matches another (name, type, size and enabled)
	UHoudiniParameter* FindMatchingParameter(UHoudiniParameter* InOtherParam);

	// Try to find one of our input that matches another one (name, isobjpath, index / parmId)
	UHoudiniInput* FindMatchingInput(UHoudiniInput* InOtherInput);

	// Try to find one of our handle that matches another one (name and handle type)
	UHoudiniHandleComponent* FindMatchingHandle(UHoudiniHandleComponent* InOtherHandle);

	// Finds a parameter by name
	UHoudiniParameter* FindParameterByName(const FString& InParamName);

	// Returns True if the component has at least one mesh output of class U
	template <class U>
	bool HasMeshOutputObjectOfClass() const;

	// Returns True if the component has at least one mesh output with a current proxy
	bool HasAnyCurrentProxyOutput() const;

	// Returns True if the component has at least one proxy mesh output (not necessarily current/displayed)
	bool HasAnyProxyOutput() const;

	// Returns True if the component has at least one non-proxy output component amongst its outputs
	bool HasAnyOutputComponent() const;

	// Returns true if the component has InOutputObjectToFind in its output object
	bool HasOutputObject(UObject* InOutputObjectToFind) const;

	//------------------------------------------------------------------------------------------------
	// Accessors
	//------------------------------------------------------------------------------------------------
	UHoudiniAsset * GetHoudiniAsset() const;
	int32 GetAssetId() const { return AssetId; };
	EHoudiniAssetState GetAssetState() const { return AssetState; };
	FString GetAssetStateAsString() const { return FHoudiniEngineRuntimeUtils::EnumToString(TEXT("EHoudiniAssetState"), GetAssetState()); };
	EHoudiniAssetStateResult GetAssetStateResult() const { return AssetStateResult; };
	FGuid GetHapiGUID() const { return HapiGUID; };
	FGuid GetComponentGUID() const { return ComponentGUID; };

	int32 GetNumInputs() const { return Inputs.Num(); };
	int32 GetNumOutputs() const { return Outputs.Num(); };
	int32 GetNumParameters() const { return Parameters.Num(); };
	int32 GetNumHandles() const { return HandleComponents.Num(); };

	UHoudiniInput* GetInputAt(const int32& Idx) { return Inputs.IsValidIndex(Idx) ? Inputs[Idx] : nullptr; };
	UHoudiniOutput* GetOutputAt(const int32& Idx) { return Outputs.IsValidIndex(Idx) ? Outputs[Idx] : nullptr;};
	UHoudiniParameter* GetParameterAt(const int32& Idx) { return Parameters.IsValidIndex(Idx) ? Parameters[Idx] : nullptr;};
	UHoudiniHandleComponent* GetHandleComponentAt(const int32& Idx) { return HandleComponents.IsValidIndex(Idx) ? HandleComponents[Idx] : nullptr; };

	void GetOutputs(TArray<UHoudiniOutput*>& OutOutputs) const;

	TArray<FHoudiniBakedOutput>& GetBakedOutputs() { return BakedOutputs; }
	const TArray<FHoudiniBakedOutput>& GetBakedOutputs() const { return BakedOutputs; }

	/*
	TArray<UHoudiniParameter*>& GetParameters() { return Parameters; };
	TArray<UHoudiniInput*>& GetInputs() { return Inputs; };
	TArray<UHoudiniOutput*>& GetOutputs() { return Outputs; };
	*/

	bool IsCookingEnabled() const { return bEnableCooking; };
	bool HasBeenLoaded() const { return bHasBeenLoaded; };
	bool HasBeenDuplicated() const { return bHasBeenDuplicated; };
	bool HasRecookBeenRequested() const { return bRecookRequested; };
	bool HasRebuildBeenRequested() const { return bRebuildRequested; };

	//bool GetEditorPropertiesNeedFullUpdate() const { return bEditorPropertiesNeedFullUpdate; };

	int32 GetAssetCookCount() const { return AssetCookCount; };

	bool IsFullyLoaded() const { return bFullyLoaded; };

	UHoudiniPDGAssetLink * GetPDGAssetLink() const { return PDGAssetLink; };

	virtual bool IsProxyStaticMeshEnabled() const;
	bool IsProxyStaticMeshRefinementByTimerEnabled() const;
	float GetProxyMeshAutoRefineTimeoutSeconds() const;
	bool IsProxyStaticMeshRefinementOnPreSaveWorldEnabled() const;
	bool IsProxyStaticMeshRefinementOnPreBeginPIEEnabled() const;
	// If true, then the next cook should not build proxy meshes, regardless of global or override settings,
	// but should instead directly build UStaticMesh
	bool HasNoProxyMeshNextCookBeenRequested() const { return bNoProxyMeshNextCookRequested; }
	// Returns true if the asset state indicates that it has been cooked in this session, false otherwise.
	bool IsHoudiniCookedDataAvailable(bool &bOutNeedsRebuildOrDelete, bool &bOutInvalidState) const;
	// Returns true if the asset should be bake after the next cook
	bool IsBakeAfterNextCookEnabled() const { return bBakeAfterNextCook; }

	FOnPostCookBakeDelegate& GetOnPostCookBakeDelegate() { return OnPostCookBakeDelegate; }

	// Derived blueprint based components will check whether the template
	// component contains updates that needs to processed.
	bool NeedUpdateParameters() const;
	bool NeedUpdateInputs() const;

	// Returns true if the component has any previous baked output recorded in its outputs
	bool HasPreviousBakeOutput() const;

	//------------------------------------------------------------------------------------------------
	// Mutators
	//------------------------------------------------------------------------------------------------
	//void SetAssetId(const int& InAssetId);
	//void SetAssetState(const EHoudiniAssetState& InAssetState) { AssetState = InAssetState; };
	//void SetAssetStateResult(const EHoudiniAssetStateResult& InAssetStateResult) { AssetStateResult = InAssetStateResult; };

	//void SetHapiGUID(const FGuid& InGUID) { HapiGUID = InGUID; };
	//void SetComponentGUID(const FGuid& InGUID) { ComponentGUID = InGUID; };

	//UFUNCTION(BlueprintSetter)
	virtual void SetHoudiniAsset(UHoudiniAsset * NewHoudiniAsset);

	void SetHasBeenLoaded(const bool& InLoaded) { bHasBeenLoaded = InLoaded; };

	void SetHasBeenDuplicated(const bool& InDuplicated) { bHasBeenDuplicated = InDuplicated; };

	//void SetEditorPropertiesNeedFullUpdate(const bool& InUpdate) { bEditorPropertiesNeedFullUpdate = InUpdate; };

	// Marks the assets as needing a recook
	void MarkAsNeedCook();
	// Marks the assets as needing a full rebuild
	void MarkAsNeedRebuild();
	// Marks the asset as needing to be instantiated
	void MarkAsNeedInstantiation();
	// The blueprint has been structurally modified
	void MarkAsBlueprintStructureModified();
	// The blueprint has been modified but not structurally changed.
	void MarkAsBlueprintModified();

	//
	void SetAssetCookCount(const int32& InCount) { AssetCookCount = InCount; };
	//
	void SetRecookRequested(const bool& InRecook) { bRecookRequested = InRecook; };
	//
	void SetRebuildRequested(const bool& InRebuild) { bRebuildRequested = InRebuild; };
	//
	void SetHasComponentTransformChanged(const bool& InHasChanged);

	// Set to True to force the next cook to not build a proxy mesh (regardless of global or override settings) and
	// instead build a UStaticMesh directly (if applicable for the output type).
	void SetNoProxyMeshNextCookRequested(bool bInNoProxyMeshNextCookRequested) { bNoProxyMeshNextCookRequested = bInNoProxyMeshNextCookRequested; }

	// Set to True to force the next cook to bake the asset after the cook completes.
	void SetBakeAfterNextCookEnabled(bool bInEnabled) { bBakeAfterNextCook = bInEnabled; }

	//
	void SetPDGAssetLink(UHoudiniPDGAssetLink* InPDGAssetLink);
	//
	virtual void OnHoudiniAssetChanged();

	//
	void AddDownstreamHoudiniAsset(UHoudiniAssetComponent* InDownstreamAsset) { DownstreamHoudiniAssets.Add(InDownstreamAsset); };
	//
	void RemoveDownstreamHoudiniAsset(UHoudiniAssetComponent* InRemoveDownstreamAsset) { DownstreamHoudiniAssets.Remove(InRemoveDownstreamAsset); };
	//
	void ClearDownstreamHoudiniAsset() { DownstreamHoudiniAssets.Empty(); };
	//
	bool NotifyCookedToDownstreamAssets();
	//
	bool NeedsToWaitForInputHoudiniAssets();

	// Clear/disable the RefineMeshesTimer.
	void ClearRefineMeshesTimer();

	// Re-set the RefineMeshesTimer to its default value.
	void SetRefineMeshesTimer();

	virtual void OnRefineMeshesTimerFired();
	
	// Called by RefineMeshesTimer when the timer is triggered.
	// Checks for any UHoudiniStaticMesh in Outputs and bakes UStaticMesh for them via FHoudiniMeshTranslator.	 
	FOnRefineMeshesTimerDelegate& GetOnRefineMeshesTimerDelegate() { return OnRefineMeshesTimerDelegate; }

	// Returns true if the asset is valid for cook/bake
	virtual bool IsComponentValid() const;
	// Return false if this component has no cooking or instantiation in progress.
	bool IsInstantiatingOrCooking() const;

	// HoudiniEngineTick will be called by HoudiniEngineManager::Tick()
	virtual void HoudiniEngineTick();

#if WITH_EDITOR
	// This alternate version of PostEditChange is called when properties inside structs are modified.  The property that was actually modified
	// is located at the tail of the list.  The head of the list of the FStructProperty member variable that contains the property that was modified.
	virtual void PostEditChangeProperty(FPropertyChangedEvent & PropertyChangedEvent) override;

	//Called after applying a transaction to the object.  Default implementation simply calls PostEditChange. 
	virtual void PostEditUndo() override;

	// Whether this component is currently open in a Blueprint editor. This
	// method is overridden by HoudiniAssetBlueprintComponent.
	virtual bool HasOpenEditor() const { return false; };

#endif

	virtual void RegisterHoudiniComponent(UHoudiniAssetComponent* InComponent);

	virtual void OnRegister() override;

	// USceneComponent methods.
	virtual FBoxSphereBounds CalcBounds(const FTransform & LocalToWorld) const override;
	virtual void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport) override;

	FBox GetAssetBounds(UHoudiniInput* IgnoreInput, const bool& bIgnoreGeneratedLandscape) const;

	// Set this component's input presets
	void SetInputPresets(const TMap<UObject*, int32>& InPresets);
	// Apply the preset input for HoudiniTools
	void ApplyInputPresets();

	// return the cached component template, if available.
	virtual UHoudiniAssetComponent* GetCachedTemplate() const { return nullptr; }

	//------------------------------------------------------------------------------------------------
	// Supported Features
	//------------------------------------------------------------------------------------------------

	// Whether or not this component should be able to delete the Houdini nodes
	// that correspond to the HoudiniAsset when being deregistered. 
	virtual bool CanDeleteHoudiniNodes() const { return true; }

	virtual bool IsInputTypeSupported(EHoudiniInputType InType) const;
	virtual bool IsOutputTypeSupported(EHoudiniOutputType InType) const;

	//------------------------------------------------------------------------------------------------5
	// Characteristics
	//------------------------------------------------------------------------------------------------

	// Try to determine whether this component belongs to a preview actor.
	// Preview / Template components need to sync their data for HDA cooks and output translations.
	bool IsPreview() const;

	virtual bool IsValidComponent() const;

	//------------------------------------------------------------------------------------------------
	// Notifications
	//------------------------------------------------------------------------------------------------

	// TODO: After the cook worfklow rework, most of these won't be needed anymore, so clean up!
	//FHoudiniAssetComponentEvent OnTemplateParametersChanged;
	//FHoudiniAssetComponentEvent OnPreAssetCook;
	//FHoudiniAssetComponentEvent OnCookCompleted;
	//FHoudiniAssetComponentEvent OnOutputProcessingCompleted;

	/*virtual void BroadcastParametersChanged();
	virtual void BroadcastPreAssetCook();
	virtual void BroadcastCookCompleted();*/

	virtual void OnPrePreCook() {};
	virtual void OnPostPreCook() {};
	virtual void OnPreOutputProcessing() {};
	virtual void OnPostOutputProcessing() {};
	virtual void OnPrePreInstantiation() {};


	virtual void NotifyHoudiniRegisterCompleted() {};
	virtual void NotifyHoudiniPreUnregister() {};
	virtual void NotifyHoudiniPostUnregister() {};

	virtual void OnFullyLoaded();

	// Component template parameters have been updated. 
	// Broadcast delegate, and let preview components take care of the rest.
	virtual void OnTemplateParametersChanged() { };
	virtual void OnBlueprintStructureModified() { };
	virtual void OnBlueprintModified() { };


protected:

	// UActorComponents Method
	virtual void OnComponentCreated() override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

	virtual void OnChildAttached(USceneComponent* ChildComponent) override;

	virtual void BeginDestroy() override;

	// Do any object - specific cleanup required immediately after loading an object.
	// This is not called for newly - created objects, and by default will always execute on the game thread.
	virtual void PostLoad() override;

	// Called after importing property values for this object (paste, duplicate or .t3d import)
	// Allow the object to perform any cleanup for properties which shouldn't be duplicated or
	// Are unsupported by the script serialization
	virtual void PostEditImport() override;
		
	//
	void OnActorMoved(AActor* Actor);

	// 
	void UpdatePostDuplicate();

	//
	//static void AddReferencedObjects(UObject * InThis, FReferenceCollector & Collector);

public:

	// Houdini Asset associated with this component.
	/*Category = HoudiniAsset, EditAnywhere, meta = (DisplayPriority=0)*/
	UPROPERTY(Category = HoudiniAsset, EditAnywhere)// BlueprintSetter = SetHoudiniAsset, BlueprintReadWrite, )
	UHoudiniAsset* HoudiniAsset;

	// Automatically cook when a parameter or input is changed
	UPROPERTY()
	bool bCookOnParameterChange;

	// Enables uploading of transformation changes back to Houdini Engine.
	UPROPERTY()
	bool bUploadTransformsToHoudiniEngine;

	// Transform changes automatically trigger cooks.
	UPROPERTY()
	bool bCookOnTransformChange;

	// Houdini materials will be converted to Unreal Materials.
	//UPROPERTY()
	//bool bUseNativeHoudiniMaterials;

	// This asset will cook when its asset input cook
	UPROPERTY()
	bool bCookOnAssetInputCook;

	// Enabling this will prevent the HDA from producing any output after cooking.
	UPROPERTY()
	bool bOutputless;

	// Enabling this will allow outputing the asset's templated geos
	UPROPERTY()
	bool bOutputTemplateGeos;

	// Temporary cook folder
	UPROPERTY()
	FDirectoryPath TemporaryCookFolder;

	// Folder used for baking this asset's outputs
	UPROPERTY()
	FDirectoryPath BakeFolder;

	// HoudiniGeneratedStaticMeshSettings
	/** If true, the physics triangle mesh will use double sided faces when doing scene queries. */
	UPROPERTY(EditAnywhere,
	Category = HoudiniGeneratedStaticMeshSettings,
	meta = (DisplayName = "Double Sided Geometry"))
	uint32 bGeneratedDoubleSidedGeometry : 1;
	UPROPERTY(EditAnywhere,
	Category = HoudiniGeneratedStaticMeshSettings,
	meta = (DisplayName = "Simple Collision Physical Material"))
	UPhysicalMaterial * GeneratedPhysMaterial;

	/** Default properties of the body instance, copied into objects on instantiation, was URB_BodyInstance */
	UPROPERTY(EditAnywhere, Category = HoudiniGeneratedStaticMeshSettings, meta = (FullyExpand = "true"))
	struct FBodyInstance DefaultBodyInstance;

	/** Collision Trace behavior - by default, it will keep simple(convex)/complex(per-poly) separate. */
	UPROPERTY(EditAnywhere,
	Category = HoudiniGeneratedStaticMeshSettings,
	meta = (DisplayName = "Collision Complexity"))
	TEnumAsByte< enum ECollisionTraceFlag > GeneratedCollisionTraceFlag;

	/** Resolution of lightmap. */
	UPROPERTY(EditAnywhere,
	Category = HoudiniGeneratedStaticMeshSettings,
	meta = (DisplayName = "Light Map Resolution", FixedIncrement = "4.0"))
	int32 GeneratedLightMapResolution;

	/** Bias multiplier for Light Propagation Volume lighting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly,
	Category = HoudiniGeneratedStaticMeshSettings,
	meta = (DisplayName = "Lpv Bias Multiplier", UIMin = "0.0", UIMax = "3.0"))
	float GeneratedLpvBiasMultiplier;

	/** Mesh distance field resolution, setting it to 0 will prevent the mesh distance field generation while editing the asset **/
	UPROPERTY(EditAnywhere,
	Category = HoudiniGeneratedStaticMeshSettings,
	meta = (DisplayName = "Distance Field Resolution Scale", UIMin = "0.0", UIMax = "100.0"))
	float GeneratedDistanceFieldResolutionScale;

	/** Custom walkable slope setting for generated mesh's body. */
	UPROPERTY(EditAnywhere, AdvancedDisplay,
		Category = HoudiniGeneratedStaticMeshSettings,
		meta = (DisplayName = "Walkable Slope Override"))
		FWalkableSlopeOverride GeneratedWalkableSlopeOverride;

	/** The light map coordinate index. */
	UPROPERTY(EditAnywhere, AdvancedDisplay,
		Category = HoudiniGeneratedStaticMeshSettings,
		meta = (DisplayName = "Light map coordinate index"))
		int32 GeneratedLightMapCoordinateIndex;

	/** True if mesh should use a less-conservative method of mip LOD texture factor computation. */
	UPROPERTY(EditAnywhere, AdvancedDisplay,
		Category = HoudiniGeneratedStaticMeshSettings,
		meta = (DisplayName = "Use Maximum Streaming Texel Ratio"))
		uint32 bGeneratedUseMaximumStreamingTexelRatio : 1;

	/** Allows artists to adjust the distance where textures using UV 0 are streamed in/out. */
	UPROPERTY(EditAnywhere, AdvancedDisplay,
		Category = HoudiniGeneratedStaticMeshSettings,
		meta = (DisplayName = "Streaming Distance Multiplier"))
		float GeneratedStreamingDistanceMultiplier;

	/** Default settings when using this mesh for instanced foliage. */
	/*
	UPROPERTY(EditAnywhere, AdvancedDisplay, Instanced,
		Category = HoudiniGeneratedStaticMeshSettings,
		meta = (DisplayName = "Foliage Default Settings"))
		UFoliageType_InstancedStaticMesh * GeneratedFoliageDefaultSettings;
	*/

	/** Array of user data stored with the asset. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Instanced,
		Category = HoudiniGeneratedStaticMeshSettings,
		meta = (DisplayName = "Asset User Data"))
		TArray<UAssetUserData*> GeneratedAssetUserData;
	
	// Override the global fast proxy mesh settings on this component?
	UPROPERTY(Category = "HoudiniAsset | Static Mesh", EditAnywhere, meta = (DisplayPriority = 0))
	bool bOverrideGlobalProxyStaticMeshSettings;

	// For StaticMesh outputs: should a fast proxy be created first?
	UPROPERTY(Category = "HoudiniAsset | Static Mesh", EditAnywhere, meta = (DisplayName="Enable Proxy Static Mesh", EditCondition="bOverrideGlobalProxyStaticMeshSettings", DisplayPriority = 0))
	bool bEnableProxyStaticMeshOverride;

	// If fast proxy meshes are being created, must it be baked as a StaticMesh after a period of no updates?
	UPROPERTY(Category = "HoudiniAsset | Static Mesh", EditAnywhere, meta = (DisplayName="Refine Proxy Static Meshes After a Timeout", EditCondition = "bOverrideGlobalProxyStaticMeshSettings && bEnableProxyStaticMeshOverride"))
	bool bEnableProxyStaticMeshRefinementByTimerOverride;
	
	// If the option to automatically refine the proxy mesh via a timer has been selected, this controls the timeout in seconds.
	UPROPERTY(Category = "HoudiniAsset | Static Mesh", EditAnywhere, meta = (DisplayName="Proxy Mesh Auto Refine Timeout Seconds", EditCondition = "bOverrideGlobalProxyStaticMeshSettings && bEnableProxyStaticMeshOverride && bEnableProxyStaticMeshRefinementByTimerOverride"))
	float ProxyMeshAutoRefineTimeoutSecondsOverride;

	// Automatically refine proxy meshes to UStaticMesh before the map is saved
	UPROPERTY(Category = "HoudiniAsset | Static Mesh", EditAnywhere, meta = (DisplayName="Refine Proxy Static Meshes When Saving a Map", EditCondition = "bOverrideGlobalProxyStaticMeshSettings && bEnableProxyStaticMeshOverride"))
	bool bEnableProxyStaticMeshRefinementOnPreSaveWorldOverride;

	// Automatically refine proxy meshes to UStaticMesh before starting a play in editor session
	UPROPERTY(Category = "HoudiniAsset | Static Mesh", EditAnywhere, meta = (DisplayName="Refine Proxy Static Meshes On PIE", EditCondition = "bOverrideGlobalProxyStaticMeshSettings && bEnableProxyStaticMeshOverride"))
	bool bEnableProxyStaticMeshRefinementOnPreBeginPIEOverride;

	// The method to use to create the mesh
	UPROPERTY(Category = "HoudiniAsset | Development", EditAnywhere, meta = (DisplayPriority = 0))
	EHoudiniStaticMeshMethod StaticMeshMethod;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	bool bGenerateMenuExpanded;

	UPROPERTY()
	bool bBakeMenuExpanded;

	UPROPERTY()
	bool bAssetOptionMenuExpanded;

	UPROPERTY()
	bool bHelpAndDebugMenuExpanded;

	UPROPERTY()
	EHoudiniEngineBakeOption HoudiniEngineBakeOption;

	// If true, then after a successful bake, the HACs outputs will be cleared and removed.
	UPROPERTY()
	bool bRemoveOutputAfterBake;

	// If true, recenter baked actors to their bounding box center after bake
	UPROPERTY()
	bool bRecenterBakedActors;

	// If true, replace the previously baked output (if any) instead of creating new objects
	UPROPERTY()
	bool bReplacePreviousBake;
#endif

protected:

	// Id of corresponding Houdini asset.
	UPROPERTY(DuplicateTransient)
	int32 AssetId;

	// List of dependent downstream HACs that have us as an asset input
	UPROPERTY(DuplicateTransient)
	TSet<UHoudiniAssetComponent*> DownstreamHoudiniAssets;

	// Unique GUID created by component.
	UPROPERTY(DuplicateTransient)
	FGuid ComponentGUID;

	// GUID used to track asynchronous cooking requests.
	UPROPERTY(DuplicateTransient)
	FGuid HapiGUID;

	// Current state of the asset
	UPROPERTY(DuplicateTransient)
	EHoudiniAssetState AssetState;

	// Last asset state logged.
	UPROPERTY(DuplicateTransient)
	mutable EHoudiniAssetState DebugLastAssetState;

	// Result of the current asset's state
	UPROPERTY(DuplicateTransient)
	EHoudiniAssetStateResult AssetStateResult;

	//// Contains the context for keeping track of shared 
	//// Houdini data.
	//UPROPERTY(DuplicateTransient)
	//UHoudiniAssetContext* AssetContext;

	// Subasset index
	UPROPERTY()
	uint32 SubAssetIndex;
	
	// Number of times this asset has been cooked.
	UPROPERTY(DuplicateTransient)
	int32 AssetCookCount;

	// 
	UPROPERTY(DuplicateTransient)
	bool bHasBeenLoaded;

	UPROPERTY(DuplicateTransient)
	bool bHasBeenDuplicated;

	UPROPERTY(DuplicateTransient)
	bool bPendingDelete;

	UPROPERTY(DuplicateTransient)
	bool bRecookRequested;

	UPROPERTY(DuplicateTransient)
	bool bRebuildRequested;

	UPROPERTY(DuplicateTransient)
	bool bEnableCooking;

	UPROPERTY(DuplicateTransient)
	bool bForceNeedUpdate;

	UPROPERTY(DuplicateTransient)
	bool bLastCookSuccess;

	UPROPERTY(DuplicateTransient)
	bool bBlueprintStructureModified;

	UPROPERTY(DuplicateTransient)
	bool bBlueprintModified;
	
	//UPROPERTY(DuplicateTransient)
	//bool bEditorPropertiesNeedFullUpdate;

	UPROPERTY(Instanced)
	TArray<UHoudiniParameter*> Parameters;

	UPROPERTY(Instanced)
	TArray<UHoudiniInput*> Inputs;
	
	UPROPERTY(Instanced)
	TArray<UHoudiniOutput*> Outputs;

	// The baked outputs from the last bake.
	UPROPERTY()
	TArray<FHoudiniBakedOutput> BakedOutputs;

	// Any actors that aren't explicitly
	// tracked by output objects should be registered
	// here so that they can be cleaned up.
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> UntrackedOutputs;

	UPROPERTY()
	TArray<UHoudiniHandleComponent*> HandleComponents;

	UPROPERTY(Transient, DuplicateTransient)
	bool bHasComponentTransformChanged;

	UPROPERTY(Transient, DuplicateTransient)
	bool bFullyLoaded;

	UPROPERTY()
	UHoudiniPDGAssetLink* PDGAssetLink;

	// Timer that is used to trigger creation of UStaticMesh for all mesh outputs
	// that still have UHoudiniStaticMeshes. The timer is cleared on PreCook and reset
	// at the end of the PostCook.
	UPROPERTY()
	FTimerHandle RefineMeshesTimer;

	// Delegate that is used to broadcast when RefineMeshesTimer fires
	FOnRefineMeshesTimerDelegate OnRefineMeshesTimerDelegate;

	// If true, don't build a proxy mesh next cook (regardless of global or override settings),
	// instead build the UStaticMesh directly (if applicable for the output types).
	UPROPERTY(DuplicateTransient)
	bool bNoProxyMeshNextCookRequested;

	// Maps a UObject to an Input number, used to preset the asset's inputs 
	UPROPERTY(Transient, DuplicateTransient)
	TMap<UObject*, int32> InputPresets;

	// If true, bake the asset after its next cook.
	UPROPERTY(DuplicateTransient)
	bool bBakeAfterNextCook;

	// Delegate to broadcast when baking after a cook.
	// Currently we cannot call the bake functions from here (Runtime module)
	// or from the HoudiniEngineManager (HoudiniEngine) module, so we use
	// a delegate.
	FOnPostCookBakeDelegate OnPostCookBakeDelegate;

	// Cached flag of whether this object is considered to be a 'preview' component or not.
	// This is typically useful in destructors when references to the World, for example, 
	// is no longer available.
	UPROPERTY(Transient, DuplicateTransient)
	bool bCachedIsPreview;

	USimpleConstructionScript* GetSCS() const;

	// Object used to convert V1 HAC to V2 HAC
	UHoudiniAssetComponent_V1* Version1CompatibilityHAC;
};
