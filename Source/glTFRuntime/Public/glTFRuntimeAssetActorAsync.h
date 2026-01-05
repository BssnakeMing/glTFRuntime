// Copyright 2021, Roberto De Ioris.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "glTFRuntimeAsset.h"
#include "glTFRuntimeAssetActorAsync.generated.h"

UCLASS()
class GLTFRUNTIME_API AglTFRuntimeAssetActorAsync : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AglTFRuntimeAssetActorAsync();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void ProcessNode(USceneComponent* NodeParentComponent, const FName SocketName, FglTFRuntimeNode& Node);

	virtual void Tick(float DeltaTime) override;

	template<typename T>
	FName GetSafeNodeName(const FglTFRuntimeNode& Node)
	{
		return MakeUniqueObjectName(this, T::StaticClass(), *Node.Name);
	}

	TMap<USceneComponent*, FName> SocketMapping;
	TMap<USkeletalMeshComponent*, USkeletalMesh*> DiscoveredSkeletalMeshComponents;
	TMap<UStaticMeshComponent*, UStaticMesh*> DiscoveredStaticMeshComponents;

	void ScenesLoaded();

	virtual void Destroyed() override;

public:	

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Meta = (ExposeOnSpawn = true), Category = "glTFRuntime")
	UglTFRuntimeAsset* Asset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Meta = (ExposeOnSpawn = true), Category = "glTFRuntime")
	FglTFRuntimeStaticMeshConfig StaticMeshConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Meta = (ExposeOnSpawn = true), Category = "glTFRuntime")
	FglTFRuntimeSkeletalMeshConfig SkeletalMeshConfig;

	UFUNCTION(BlueprintNativeEvent, Category = "glTFRuntime", meta = (DisplayName = "On Scenes Loaded"))
	void ReceiveOnScenesLoaded();

	UFUNCTION(BlueprintNativeEvent, Category = "glTFRuntime", meta = (DisplayName = "On Node Processed"))
	void ReceiveOnNodeProcessed(const int32 NodeIndex, USceneComponent* NodeSceneComponent);

	UFUNCTION(BlueprintNativeEvent, Category = "glTFRuntime", meta = (DisplayName = "On StaticMeshComponent Created"))
	void ReceiveOnStaticMeshComponentCreated(UStaticMeshComponent* StaticMeshComponent, const FglTFRuntimeNode& Node);

	UFUNCTION(BlueprintNativeEvent, Category = "glTFRuntime", meta = (DisplayName = "On SkeletalMeshComponent Created"))
	void ReceiveOnSkeletalMeshComponentCreated(USkeletalMeshComponent* SkeletalMeshComponent, const FglTFRuntimeNode& Node);

	UFUNCTION(BlueprintNativeEvent, Category = "glTFRuntime", meta = (DisplayName = "Override StaticMeshConfig"))
	FglTFRuntimeStaticMeshConfig OverrideStaticMeshConfig(const int32 NodeIndex, UStaticMeshComponent* NodeStaticMeshComponent);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Meta = (ExposeOnSpawn = true), Category = "glTFRuntime")
	bool bShowWhileLoading;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Meta = (ExposeOnSpawn = true), Category = "glTFRuntime")
	bool bStaticMeshesAsSkeletal;

	virtual void PostUnregisterAllComponents() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Meta = (ExposeOnSpawn = true), Category = "glTFRuntime")
	bool bAllowLights;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Meta = (ExposeOnSpawn = true), Category = "glTFRuntime")
	FglTFRuntimeLightConfig LightConfig;

public:
	// 加载的模型是静态网格还是骨骼网格
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "glTFRuntime")
	bool bIsStaticMesh;

	// Animation
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Meta = (ExposeOnSpawn = true), Category = "glTFRuntime")
	bool bAllowNodeAnimations = true;

	UPROPERTY()
	TMap<USceneComponent*, UglTFRuntimeAnimationCurve*> CurveBasedAnimations;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Meta = (ExposeOnSpawn = true), Category = "glTFRuntime")
	bool bAllowSkeletalAnimations = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Meta = (ExposeOnSpawn = true), Category = "glTFRuntime")
	FglTFRuntimeSkeletalAnimationConfig SkeletalAnimationConfig;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Meta = (ExposeOnSpawn = true), Category = "glTFRuntime")
	bool bAllowPoseAnimations = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Meta = (ExposeOnSpawn = true), Category = "glTFRuntime")
	bool bAutoPlayAnimations;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Meta = (ExposeOnSpawn = true), Category = "glTFRuntime")
	bool bLoadAllSkeletalAnimations;

	// Node Curve动画名称集
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "glTFRuntime")
	TSet<FString> DiscoveredCurveAnimationsNames;

	// 重置Node动画
	//UFUNCTION(BlueprintCallable, Category = "glTFRuntime")
	void ResetNodeAnimation();

	UFUNCTION(BlueprintCallable, Category = "glTFRuntime")
	void GetNodeCurvaAnimationData(TArray<FString>& NodeAnimName, TArray<float>& NodeAnimTime);

	// 播放或继续播放Node动画
	UFUNCTION(BlueprintCallable, Category = "glTFRuntime")
	void SetPauseNodeAnimation(bool Pause);

	UFUNCTION(BlueprintCallable, Category = "glTFRuntime")
	void StopNodeAnimation();

	UFUNCTION(BlueprintCallable, Category = "glTFRuntime")
	float PlayCurveAnimationByName(const FString& CurveAnimationName, float AnimPosition = 0.f);

	UFUNCTION(BlueprintCallable, Category = "glTFRuntime")
	float PlaySkeletalAnim_Index(int32 AnimIndex, float AnimPosition = 0.f);

	//UFUNCTION(BlueprintCallable, Category = "glTFRuntime")
	void PlaySkeletalAnim_Instance(UAnimSequence* AnimSequence, float AnimPosition = 0.f);

	UFUNCTION(BlueprintCallable, Category = "glTFRuntime")
	void SetPauseSkeletalAnim(bool Pause);

	UFUNCTION(BlueprintCallable, Category = "glTFRuntime")
	void StopSkeletaAnim();

	//UFUNCTION(BlueprintCallable, Category = "glTFRuntime")
	TMap<FString,UAnimSequence*> GetSkeletalAnimSequences();

	UFUNCTION(BlueprintCallable, Category = "glTFRuntime")
	void GetSkeletalAnimData(TArray<FString>& AnimSequenceName, TArray<float>& AnimSequenceTime);



protected:
	// 当前播放动画的时间位置
	float NodeAnimaTime;
	
	// 骨骼动画实例和名称
	UPROPERTY()
	TMap<FString, UAnimSequence*> SkeletalAnims;

	// 场景组件对应的多个动画名称和Curve数据
	TMap<USceneComponent*, TMap<FString, UglTFRuntimeAnimationCurve*>> DiscoveredCurveAnimations;

	// 正在播放的node Curve动画
	TMap<USceneComponent*, float>  CurveBasedAnimationsTimeTracker;
	
	TMap<USkeletalMeshComponent*, TMap<FString, UAnimSequence*>> DiscoveredSkeletalAnimations;

	bool bPauseNodeAnimation = true;
	
	UPROPERTY()
	TArray<UAnimSequence*> AllSkeletalAnimations;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"), Category="glTFRuntime")
	USceneComponent* AssetRoot;

	TMap<UPrimitiveComponent*, FglTFRuntimeNode> MeshesToLoad;
	TMap<UPrimitiveComponent*, FglTFRuntimeNode> AllMeshesLoad;

	void LoadNextMeshAsync();

	UFUNCTION()
	void LoadStaticMeshAsync(UStaticMesh* StaticMesh, UStaticMeshComponent* StaticMeshComponent);

	UFUNCTION()
	void LoadSkeletalMeshAsync(USkeletalMesh* SkeletalMesh, USkeletalMeshComponent* SkeletalMeshComponent);

	// this is safe to share between game and async threads because everything is sequential
	UPrimitiveComponent* CurrentPrimitiveComponent;

	double LoadingStartTime;
	FglTFRuntimeStaticMeshAsync StaticMeshAsyncDelegate;
	FglTFRuntimeSkeletalMeshAsync SkeletalMeshAsyncDelegate;

	// 多步加载
	int32 MaxParallelLoads = 4;
	int32 CurrentLoadingCount = 0;

	TArray<FString> NodeNames;
};
