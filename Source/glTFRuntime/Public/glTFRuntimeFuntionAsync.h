#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"

#include "glTFRuntimeFuntionAsync.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAsyncGLTFGetComponentDelegate,const FString, ComponentName, const USceneComponent*, Component);


UCLASS()
class GLTFRUNTIME_API UAsyncGlTFRuntimeFuntion  : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
public:
	UAsyncGlTFRuntimeFuntion(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UPROPERTY(BlueprintAssignable)
	FAsyncGLTFGetComponentDelegate OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FAsyncGLTFGetComponentDelegate OnFail;

public:
	UFUNCTION(BlueprintCallable, Category = "glTF", meta = (BlueprintInternalUseOnly = "true"))
	static UAsyncGlTFRuntimeFuntion* AsyncExecuteGLTFGetComponent(AActor* TargetActor, FString ComponentName);
	
	virtual void Activate() override;

public:
	FString ComponentName;
	AActor* Actor;
};
