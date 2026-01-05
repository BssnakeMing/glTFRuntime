// Copyright 2021-2023, Roberto De Ioris.


#include "glTFRuntimeAssetActorAsync.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/LightComponent.h"
#include "Engine/StaticMeshSocket.h"


// Sets default values
AglTFRuntimeAssetActorAsync::AglTFRuntimeAssetActorAsync()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	AssetRoot = CreateDefaultSubobject<USceneComponent>(TEXT("AssetRoot"));
	RootComponent = AssetRoot;

	bShowWhileLoading = true;
	bStaticMeshesAsSkeletal = false;

	bAllowLights = true;
}

// Called when the game starts or when spawned
void AglTFRuntimeAssetActorAsync::BeginPlay()
{
	Super::BeginPlay();

	if (!Asset)
	{
		return;
	}
	
	StaticMeshAsyncDelegate.BindDynamic(this, &AglTFRuntimeAssetActorAsync::LoadStaticMeshAsync);
	SkeletalMeshAsyncDelegate.BindDynamic(this, &AglTFRuntimeAssetActorAsync::LoadSkeletalMeshAsync);

	LoadingStartTime = FPlatformTime::Seconds();

	TArray<FglTFRuntimeScene> Scenes = Asset->GetScenes();
	for (FglTFRuntimeScene& Scene : Scenes)
	{
		FName NodeName = *Scene.Name;
		USceneComponent* SceneComponent = NewObject<USceneComponent>(this,NodeName);// *FString::Printf(TEXT("Scene %d"), Scene.Index
		SceneComponent->SetupAttachment(RootComponent);
		SceneComponent->RegisterComponent();
		AddInstanceComponent(SceneComponent);
		for (int32 NodeIndex : Scene.RootNodesIndices)
		{
			FglTFRuntimeNode Node;
			if (!Asset->GetNode(NodeIndex, Node))
			{
				return;
			}
			ProcessNode(SceneComponent, NAME_None, Node);
		}
	}

	LoadNextMeshAsync();
}

void AglTFRuntimeAssetActorAsync::ProcessNode(USceneComponent* NodeParentComponent, const FName SocketName, FglTFRuntimeNode& Node)
{
	// skip bones/joints
	if (Asset->NodeIsBone(Node.Index))
	{
		for (int32 ChildIndex : Node.ChildrenIndices)
		{
			FglTFRuntimeNode Child;
			if (!Asset->GetNode(ChildIndex, Child))
			{
				return;
			}
			ProcessNode(NodeParentComponent, *Child.Name, Child);
		}
		return;
	}

	USceneComponent* NewComponent = nullptr;
	if (Node.MeshIndex < 0)
	{
		FName NodeName = *Node.Name;
		NewComponent = NewObject<USceneComponent>(this, NodeName);// GetSafeNodeName<USceneComponent>(Node)
		NewComponent->SetupAttachment(NodeParentComponent);
		NewComponent->RegisterComponent();
		NewComponent->SetRelativeTransform(Node.Transform);
		AddInstanceComponent(NewComponent);
	}
	else
	{
		if (Node.SkinIndex < 0 && !bStaticMeshesAsSkeletal)
		{
			FString NodeStr = Node.Name;
			if (NodeNames.Contains(NodeStr))
			{
				FString IntString = FString::FromInt(Node.MeshIndex);
				NodeStr = NodeStr + "_" + IntString;
			}
			else
			{
				NodeNames.Add(NodeStr);
			}
			FName NodeName = *NodeStr;
			UStaticMeshComponent* StaticMeshComponent = NewObject<UStaticMeshComponent>(this, NodeName);//GetSafeNodeName<UStaticMeshComponent>(Node)
			StaticMeshComponent->SetupAttachment(NodeParentComponent);
			StaticMeshComponent->RegisterComponent();
			StaticMeshComponent->SetRelativeTransform(Node.Transform);
			AddInstanceComponent(StaticMeshComponent);
			MeshesToLoad.Add(StaticMeshComponent, Node);
			AllMeshesLoad.Add(StaticMeshComponent, Node);
			NewComponent = StaticMeshComponent;
			//ReceiveOnStaticMeshComponentCreated(StaticMeshComponent, Node);

			bIsStaticMesh = true;
		}
		else
		{
			FString NodeStr = Node.Name;
			if (NodeNames.Contains(NodeStr))
			{
				FString IntString = FString::FromInt(Node.MeshIndex);
				NodeStr = NodeStr + "_" + IntString;
			}
			else
			{
				NodeNames.Add(NodeStr);
			}
			FName NodeName = *NodeStr;
			USkeletalMeshComponent* SkeletalMeshComponent = NewObject<USkeletalMeshComponent>(this, NodeName);// GetSafeNodeName<USkeletalMeshComponent>(Node)
			SkeletalMeshComponent->SetupAttachment(NodeParentComponent);
			
			//SkeletalMeshComponent->SetCollisionProfileName(TEXT("BlockAll"));
			
			SkeletalMeshComponent->RegisterComponent();
			SkeletalMeshComponent->SetRelativeTransform(Node.Transform);
			AddInstanceComponent(SkeletalMeshComponent);
			MeshesToLoad.Add(SkeletalMeshComponent, Node);
			AllMeshesLoad.Add(SkeletalMeshComponent, Node);
			NewComponent = SkeletalMeshComponent;
			//ReceiveOnSkeletalMeshComponentCreated(SkeletalMeshComponent, Node);
			
			bIsStaticMesh=false;
		}
	}
	
	// check for animations
	if (!NewComponent->IsA<USkeletalMeshComponent>())
	{
		if (bAllowNodeAnimations)
		{
			TArray<UglTFRuntimeAnimationCurve*> ComponentAnimationCurves = Asset->LoadAllNodeAnimationCurves(Node.Index);
			TMap<FString, UglTFRuntimeAnimationCurve*> ComponentAnimationCurvesMap;
			for (UglTFRuntimeAnimationCurve* ComponentAnimationCurve : ComponentAnimationCurves)
			{
				if (!CurveBasedAnimations.Contains(NewComponent))
				{
					CurveBasedAnimations.Add(NewComponent, ComponentAnimationCurve);
					CurveBasedAnimationsTimeTracker.Add(NewComponent, 0);
				}
				DiscoveredCurveAnimationsNames.Add(ComponentAnimationCurve->glTFCurveAnimationName);
				ComponentAnimationCurvesMap.Add(ComponentAnimationCurve->glTFCurveAnimationName, ComponentAnimationCurve);
			}
			DiscoveredCurveAnimations.Add(NewComponent, ComponentAnimationCurvesMap);
		}
	}
	else
	{
		USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(NewComponent);
		if (bAllowSkeletalAnimations)
		{
			UAnimSequence* SkeletalAnimation = nullptr;
			if (bLoadAllSkeletalAnimations)
			{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION > 0
				TMap<FString, UAnimSequence*> SkeletalAnimationsMap = Asset->LoadNodeSkeletalAnimationsMap(SkeletalMeshComponent->GetSkeletalMeshAsset(), Node.Index, SkeletalAnimationConfig);
#else
				TMap<FString, UAnimSequence*> SkeletalAnimationsMap = Asset->LoadNodeSkeletalAnimationsMap(SkeletalMeshComponent->SkeletalMesh, Node.Index, SkeletalAnimationConfig);
#endif
				if (SkeletalAnimationsMap.Num() > 0)
				{
					DiscoveredSkeletalAnimations.Add(SkeletalMeshComponent, SkeletalAnimationsMap);
					
					for (const TPair<FString, UAnimSequence*>& Pair : SkeletalAnimationsMap)
					{
						AllSkeletalAnimations.Add(Pair.Value);
						// set the first animation (TODO: allow this to be configurable)
						if (!SkeletalAnimation)
						{
							SkeletalAnimation = Pair.Value;
						}
					}
				}
			}
			else
			{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION > 0
				SkeletalAnimation = Asset->LoadNodeSkeletalAnimation(SkeletalMeshComponent->GetSkeletalMeshAsset(), Node.Index, SkeletalAnimationConfig);
#else
				SkeletalAnimation = Asset->LoadNodeSkeletalAnimation(SkeletalMeshComponent->SkeletalMesh, Node.Index, SkeletalAnimationConfig);
#endif
			}

			if (!SkeletalAnimation && bAllowPoseAnimations)
			{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION > 0
				SkeletalAnimation = Asset->CreateAnimationFromPose(SkeletalMeshComponent->GetSkeletalMeshAsset(), SkeletalAnimationConfig, Node.SkinIndex);
#else
				SkeletalAnimation = Asset->CreateAnimationFromPose(SkeletalMeshComponent->SkeletalMesh, SkeletalAnimationConfig, Node.SkinIndex);
#endif
			}

			if (SkeletalAnimation)
			{
				SkeletalMeshComponent->AnimationData.AnimToPlay = SkeletalAnimation;
				SkeletalMeshComponent->AnimationData.bSavedLooping = true;
				SkeletalMeshComponent->AnimationData.bSavedPlaying = bAutoPlayAnimations;
				SkeletalMeshComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			}
		}
	}
	
	if (bAllowLights)
	{
		int32 LightIndex;
		if (Asset->GetNodeExtensionIndex(Node.Index, "KHR_lights_punctual", "light", LightIndex))
		{
			ULightComponent* LightComponent = Asset->LoadPunctualLight(LightIndex, this, LightConfig);
			if (LightComponent)
			{
				LightComponent->SetupAttachment(NewComponent);
				LightComponent->RegisterComponent();
				LightComponent->SetRelativeTransform(FTransform::Identity);
				AddInstanceComponent(LightComponent);
			}
		}
	}

	ReceiveOnNodeProcessed(Node.Index, NewComponent);

	if (!NewComponent)
	{
		return;
	}
	else
	{
		NewComponent->ComponentTags.Add(*FString::Printf(TEXT("glTFRuntime:NodeName:%s"), *Node.Name));
		NewComponent->ComponentTags.Add(*FString::Printf(TEXT("glTFRuntime:NodeIndex:%d"), Node.Index));

		if (SocketName != NAME_None)
		{
			SocketMapping.Add(NewComponent, SocketName);
		}
	}

	for (int32 ChildIndex : Node.ChildrenIndices)
	{
		FglTFRuntimeNode Child;
		if (!Asset->GetNode(ChildIndex, Child))
		{
			return;
		}
		ProcessNode(NewComponent, NAME_None, Child);
	}
}

void AglTFRuntimeAssetActorAsync::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bPauseNodeAnimation)
	{
		return;
	}

	for (TPair<USceneComponent*, UglTFRuntimeAnimationCurve*>& Pair : CurveBasedAnimations)
	{
		// the curve could be null
		if (!Pair.Value)
		{
			continue;
		}
		float MinTime;
		float MaxTime;
		Pair.Value->GetTimeRange(MinTime, MaxTime);

		float CurrentTime = CurveBasedAnimationsTimeTracker[Pair.Key];
		if (CurrentTime > Pair.Value->glTFCurveAnimationDuration)
		{
			CurveBasedAnimationsTimeTracker[Pair.Key] = 0;
			CurrentTime = 0;
		}

		if (CurrentTime >= 0)
		{
			FTransform FrameTransform = Pair.Value->GetTransformValue(CurveBasedAnimationsTimeTracker[Pair.Key]);
			Pair.Key->SetRelativeTransform(FrameTransform);
		}
		CurveBasedAnimationsTimeTracker[Pair.Key] += DeltaTime;
	}
}

void AglTFRuntimeAssetActorAsync::LoadNextMeshAsync()
{
	while (MeshesToLoad.Num() > 0 && CurrentLoadingCount < MaxParallelLoads)
	{
		auto It = MeshesToLoad.CreateIterator();
		UPrimitiveComponent* Component = It->Key;
		FglTFRuntimeNode Node = It->Value;

		// ⚠️ 先 Remove，避免重复调度
		It.RemoveCurrent();

		CurrentLoadingCount++;
	
		if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Component))
		{
			//CurrentPrimitiveComponent = StaticMeshComponent;
			if (StaticMeshConfig.Outer == nullptr)
			{
				StaticMeshConfig.Outer = StaticMeshComponent;
			}
			
			Asset->LoadStaticMeshAsync(Node.MeshIndex, StaticMeshAsyncDelegate, StaticMeshConfig, StaticMeshComponent);
		}
		else if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(Component))
		{
			//CurrentPrimitiveComponent = SkeletalMeshComponent;
		
			Asset->LoadSkeletalMeshAsync(Node.MeshIndex, Node.SkinIndex, SkeletalMeshAsyncDelegate, SkeletalMeshConfig, SkeletalMeshComponent);
		}
	}
}

void AglTFRuntimeAssetActorAsync::LoadStaticMeshAsync(UStaticMesh* StaticMesh, UStaticMeshComponent* StaticMeshComponent)
{
	CurrentLoadingCount--;
	if (StaticMeshComponent)
	{
		FglTFRuntimeNode Node = AllMeshesLoad[StaticMeshComponent];
		
		DiscoveredStaticMeshComponents.Add(StaticMeshComponent, StaticMesh);
		if (bShowWhileLoading)
		{
			StaticMeshComponent->SetStaticMesh(StaticMesh);
		}

		if (StaticMesh && !StaticMeshConfig.ExportOriginalPivotToSocket.IsEmpty())
		{
			UStaticMeshSocket* DeltaSocket = StaticMesh->FindSocket(FName(StaticMeshConfig.ExportOriginalPivotToSocket));
			if (DeltaSocket)
			{
				FTransform NewTransform = StaticMeshComponent->GetRelativeTransform();
				FVector DeltaLocation = -DeltaSocket->RelativeLocation * NewTransform.GetScale3D();
				DeltaLocation = NewTransform.GetRotation().RotateVector(DeltaLocation);
				NewTransform.AddToTranslation(DeltaLocation);
				StaticMeshComponent->SetRelativeTransform(NewTransform);
			}
		}
		
		ReceiveOnStaticMeshComponentCreated(StaticMeshComponent, Node);
		//MeshesToLoad.Remove(StaticMeshComponent);
	}
	
	if (MeshesToLoad.Num() > 0)
	{
		LoadNextMeshAsync();
	}
	// trigger event
	else
	{
		if (CurrentLoadingCount == 0)
		{
			ScenesLoaded();
		}

	}
}

void AglTFRuntimeAssetActorAsync::LoadSkeletalMeshAsync(USkeletalMesh* SkeletalMesh, USkeletalMeshComponent* SkeletalMeshComponent)
{
	if (SkeletalMeshComponent)
	{
		const FglTFRuntimeNode* Node = AllMeshesLoad.Find(SkeletalMeshComponent);
		DiscoveredSkeletalMeshComponents.Add(SkeletalMeshComponent, SkeletalMesh);
		if (bShowWhileLoading)
		{
			SkeletalMeshComponent->SetSkeletalMesh(SkeletalMesh);
		}

		//USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(NewComponent);
		if (bAllowSkeletalAnimations)
		{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION > 0
			UAnimSequence* SkeletalAnimation = Asset->LoadNodeSkeletalAnimation(SkeletalMeshComponent->GetSkeletalMeshAsset(), Node->Index, SkeletalAnimationConfig);
			if (!SkeletalAnimation && bAllowPoseAnimations)
			{
				SkeletalAnimation = Asset->CreateAnimationFromPose(SkeletalMeshComponent->GetSkeletalMeshAsset(), SkeletalAnimationConfig, Node->SkinIndex);
			}
#else
			UAnimSequence* SkeletalAnimation = Asset->LoadNodeSkeletalAnimation(SkeletalMeshComponent->SkeletalMesh, Node->Index, SkeletalAnimationConfig);
			if (!SkeletalAnimation && bAllowPoseAnimations)
			{
				SkeletalAnimation = Asset->CreateAnimationFromPose(SkeletalMeshComponent->SkeletalMesh, SkeletalAnimationConfig, Node->SkinIndex);
			}
#endif
			if (SkeletalAnimation)
			{
				SkeletalMeshComponent->AnimationData.AnimToPlay = SkeletalAnimation;
				SkeletalMeshComponent->AnimationData.bSavedLooping = true;
				SkeletalMeshComponent->AnimationData.bSavedPlaying = true;
				SkeletalMeshComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			}
		}
		
		ReceiveOnSkeletalMeshComponentCreated(SkeletalMeshComponent, *Node);
	}

	//MeshesToLoad.Remove(CurrentPrimitiveComponent);
	if (MeshesToLoad.Num() > 0)
	{
		LoadNextMeshAsync();
	}
	// trigger event
	else
	{
		ScenesLoaded();
	}
}

void AglTFRuntimeAssetActorAsync::ScenesLoaded()
{
	if (!bShowWhileLoading)
	{
		for (const TPair<UStaticMeshComponent*, UStaticMesh*>& Pair : DiscoveredStaticMeshComponents)
		{
			Pair.Key->SetStaticMesh(Pair.Value);
		}

		for (const TPair<USkeletalMeshComponent*, USkeletalMesh*>& Pair : DiscoveredSkeletalMeshComponents)
		{
			Pair.Key->SetSkeletalMesh(Pair.Value);
		}
	}

	for (TPair<USceneComponent*, FName>& Pair : SocketMapping)
	{
		for (const TPair<USkeletalMeshComponent*, USkeletalMesh*>& MeshPair : DiscoveredSkeletalMeshComponents)
		{
			if (MeshPair.Key->DoesSocketExist(Pair.Value))
			{
				Pair.Key->AttachToComponent(MeshPair.Key, FAttachmentTransformRules::KeepRelativeTransform, Pair.Value);
				Pair.Key->SetRelativeTransform(FTransform::Identity);
				
				CurveBasedAnimations.Remove(Pair.Key);
				break;
			}
		}
	}
	
	if (!bIsStaticMesh)
	{
		SkeletalAnims = GetSkeletalAnimSequences();
	}

	UE_LOG(LogGLTFRuntime, Log, TEXT("Asset loaded asynchronously in %f seconds"),
	   FPlatformTime::Seconds() - LoadingStartTime);
	ReceiveOnScenesLoaded();

	for (TPair<USceneComponent*, FName>& Pair : SocketMapping)
	{
		for (auto mesh : DiscoveredSkeletalMeshComponents)
		{
			if (mesh.Key->DoesSocketExist(Pair.Value))
			{
				Pair.Key->AttachToComponent(mesh.Key, FAttachmentTransformRules::KeepRelativeTransform,
				                            Pair.Value);
				Pair.Key->SetRelativeTransform(FTransform::Identity);
				CurveBasedAnimations.Remove(Pair.Key);
				break;
			}
		}
	}
	NodeNames.Empty();
}

void AglTFRuntimeAssetActorAsync::Destroyed()
{
	AActor::Destroyed();
	if (Asset)
	{
		Asset->ClearCache();
		Asset = nullptr;
	}

	for (auto Element : CurveBasedAnimations)
	{
		Element.Key->DestroyComponent(true);
		Element.Value->MarkAsGarbage();
	}
	CurveBasedAnimations.Empty();

	for (auto Element : DiscoveredCurveAnimations)
	{
		Element.Key->DestroyComponent(true);
		for (auto Element1 : Element.Value)
		{
			Element1.Value->MarkAsGarbage();
		}
	}
	DiscoveredCurveAnimations.Empty();

	for (auto Element : CurveBasedAnimationsTimeTracker)
	{
		Element.Key->DestroyComponent(true);
	}
	CurveBasedAnimationsTimeTracker.Empty();

	for (auto Element : DiscoveredSkeletalAnimations)
	{
		Element.Key->DestroyComponent(true);
		for (auto Element1 : Element.Value)
		{
			Element1.Value->MarkAsGarbage();
		}
	}
	DiscoveredSkeletalAnimations.Empty();

	for (auto Element : AllSkeletalAnimations)
	{
		Element->MarkAsGarbage();
	}
	AllSkeletalAnimations.Empty();

	if (AssetRoot)
	{
		AssetRoot->DestroyComponent();
	}

	for (auto Element : MeshesToLoad)
	{
		Element.Key->DestroyComponent(true);
	}
	MeshesToLoad.Empty();

	for (auto Element : AllMeshesLoad)
	{
		Element.Key->DestroyComponent(true);
	}
	AllMeshesLoad.Empty();

	if (CurrentPrimitiveComponent)
	{
		CurrentPrimitiveComponent->DestroyComponent();
	}

	
	
}

void AglTFRuntimeAssetActorAsync::ReceiveOnScenesLoaded_Implementation()
{

}

void AglTFRuntimeAssetActorAsync::PostUnregisterAllComponents()
{
	if (Asset)
	{
		Asset->ClearCache();
		Asset = nullptr;
	}
	Super::PostUnregisterAllComponents();
}

void AglTFRuntimeAssetActorAsync::ResetNodeAnimation()
{
	for (TPair<USceneComponent*, UglTFRuntimeAnimationCurve*>& Pair : CurveBasedAnimations)
	{
		// the curve could be null
		UglTFRuntimeAnimationCurve* Curve = Pair.Value;
		if (!Curve)
		{
			// 跳出本次循环
			continue;
		}

		CurveBasedAnimationsTimeTracker[Pair.Key] = 0;
		NodeAnimaTime = 0;

		FTransform FrameTransform = Pair.Value->GetTransformValue(CurveBasedAnimationsTimeTracker[Pair.Key]);
		Pair.Key->SetRelativeTransform(FrameTransform);
	}
}

void AglTFRuntimeAssetActorAsync::GetNodeCurvaAnimationData(TArray<FString>& NodeAnimName, TArray<float>& NodeAnimTime)
{
	for (TPair<USceneComponent*, TMap<FString, UglTFRuntimeAnimationCurve*>> Pair : DiscoveredCurveAnimations)
	{
		for (TPair<FString, UglTFRuntimeAnimationCurve*> Value : Pair.Value)
		{
			if (NodeAnimName.Contains(Value.Key))
			{
				int32 index = NodeAnimName.Find(Value.Key);
				float oldCurveTime = NodeAnimTime[index];

				// 获取同名称动画最大的时长
				NodeAnimTime[index] = Value.Value->glTFCurveAnimationDuration > oldCurveTime ? Value.Value->glTFCurveAnimationDuration : oldCurveTime;
			}
			else
			{
				NodeAnimName.Add(Value.Key);
				NodeAnimTime.Add(Value.Value->glTFCurveAnimationDuration);
			}			
		}
	}
}

void AglTFRuntimeAssetActorAsync::SetPauseNodeAnimation(bool Pause)
{
	bPauseNodeAnimation = Pause;
}

void AglTFRuntimeAssetActorAsync::StopNodeAnimation()
{
	SetPauseNodeAnimation(true);
	ResetNodeAnimation();
}

float AglTFRuntimeAssetActorAsync::PlayCurveAnimationByName(const FString& CurveAnimationName, float AnimPosition)
{
	if (!DiscoveredCurveAnimationsNames.Contains(CurveAnimationName))
	{
		return 0.f;
	}

	float Second = 0.f;

	for (TPair<USceneComponent*, UglTFRuntimeAnimationCurve*>& Pair : CurveBasedAnimations)
	{

		TMap<FString, UglTFRuntimeAnimationCurve*> WantedCurveAnimationsMap = DiscoveredCurveAnimations[Pair.Key];
		if (WantedCurveAnimationsMap.Contains(CurveAnimationName))
		{
			Pair.Value = WantedCurveAnimationsMap[CurveAnimationName];
			CurveBasedAnimationsTimeTracker[Pair.Key] = AnimPosition;
			Second = Pair.Value->glTFCurveAnimationDuration > Second ? Pair.Value->glTFCurveAnimationDuration : Second;

			SetPauseNodeAnimation(false);
		}
		else
		{
			Second = 0.f;
			Pair.Value = nullptr;
		}
	}

	return Second;
}

float AglTFRuntimeAssetActorAsync::PlaySkeletalAnim_Index(int32 AnimIndex, float AnimPosition)
{
	if (SkeletalAnims.Num() == 0 || AnimIndex >= SkeletalAnims.Num())
	{
		return 0.f;
	}

	TArray<UAnimSequence*> AnimSequences;
	SkeletalAnims.GenerateValueArray(AnimSequences);

	UAnimSequence* Anim = AnimSequences[AnimIndex];
	PlaySkeletalAnim_Instance(Anim, AnimPosition);

	return Anim->GetPlayLength();
}

void AglTFRuntimeAssetActorAsync::PlaySkeletalAnim_Instance(UAnimSequence* AnimSequence, float AnimPosition)
{
	for (TPair<USkeletalMeshComponent*, USkeletalMesh*> Component : DiscoveredSkeletalMeshComponents)
	{
		Component.Key->PlayAnimation(AnimSequence, true);

		Component.Key->SetPosition(AnimPosition);
	}
}

void AglTFRuntimeAssetActorAsync::SetPauseSkeletalAnim(bool Pause)
{
	for (TPair<USkeletalMeshComponent*, USkeletalMesh*> Component : DiscoveredSkeletalMeshComponents)
	{
		if (Pause)
		{
			Component.Key->Stop();
		}
		else
		{
			Component.Key->Play(true);
		}
	}
}

void AglTFRuntimeAssetActorAsync::StopSkeletaAnim()
{
	for (TPair<USkeletalMeshComponent*, USkeletalMesh*> Component : DiscoveredSkeletalMeshComponents)
	{
		Component.Key->SetAnimation(nullptr);
	}
}

TMap<FString, UAnimSequence*> AglTFRuntimeAssetActorAsync::GetSkeletalAnimSequences()
{
	USkeletalMesh* SkeletalMesh = nullptr;
	TMap<FString,UAnimSequence*> AnimSequences;

	for (TPair<USkeletalMeshComponent*, USkeletalMesh*> Component : DiscoveredSkeletalMeshComponents)
	{
		SkeletalMesh = Component.Value;
		break;
	}

	FglTFRuntimeSkeletalAnimationConfig AnimationConfig = FglTFRuntimeSkeletalAnimationConfig();

	int32 AnimNums = Asset->GetNumAnimations();
	for (int i = 0; i < AnimNums; i++)
	{
		TSharedPtr<FJsonObject> JsonAnimationObject = Asset->GetParser()->GetJsonObjectFromRootIndex("animations", i);

		if (!JsonAnimationObject)
		{
			Asset->GetParser()->AddError("LoadNodeSkeletalAnimation()", FString::Printf(TEXT("Unable to find animation %d"), i));
			break;
		}
		FString Name = Asset->GetParser()->GetJsonObjectString(JsonAnimationObject.ToSharedRef(), "name", "");

		UAnimSequence* Anim = Asset->LoadSkeletalAnimation(SkeletalMesh, i, AnimationConfig);

		AnimSequences.Add(Name, Anim);
	}

	return AnimSequences;
}

void AglTFRuntimeAssetActorAsync::GetSkeletalAnimData(TArray<FString>& AnimSequenceName,
	TArray<float>& AnimSequenceTime)
{
	for (TPair<FString, UAnimSequence*> Pair : SkeletalAnims)
	{
		AnimSequenceName.Add(Pair.Key);
		AnimSequenceTime.Add(Pair.Value->GetPlayLength());
	}
}

void AglTFRuntimeAssetActorAsync::ReceiveOnNodeProcessed_Implementation(const int32 NodeIndex, USceneComponent* NodeSceneComponent)
{

}

FglTFRuntimeStaticMeshConfig AglTFRuntimeAssetActorAsync::OverrideStaticMeshConfig_Implementation(const int32 NodeIndex, UStaticMeshComponent* NodeStaticMeshComponent)
{
	return StaticMeshConfig;
}

void AglTFRuntimeAssetActorAsync::ReceiveOnStaticMeshComponentCreated_Implementation(UStaticMeshComponent* StaticMeshComponent, const FglTFRuntimeNode& Node)
{

}

void AglTFRuntimeAssetActorAsync::ReceiveOnSkeletalMeshComponentCreated_Implementation(USkeletalMeshComponent* SkeletalMeshComponent, const FglTFRuntimeNode& Node)
{

}