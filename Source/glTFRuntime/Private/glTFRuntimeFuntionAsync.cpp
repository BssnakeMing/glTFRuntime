#include "glTFRuntimeFuntionAsync.h"

UAsyncGlTFRuntimeFuntion::UAsyncGlTFRuntimeFuntion(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
}

UAsyncGlTFRuntimeFuntion* UAsyncGlTFRuntimeFuntion::AsyncExecuteGLTFGetComponent(AActor* TargetActor, FString ComponentName)
{
	UAsyncGlTFRuntimeFuntion* instance = NewObject<UAsyncGlTFRuntimeFuntion>();
	instance->ComponentName = ComponentName;
	instance->Actor = TargetActor;
	return instance;
}

void UAsyncGlTFRuntimeFuntion::Activate()
{
	TArray<USceneComponent*> Childrens;
	Actor->GetRootComponent()->GetChildrenComponents(true,Childrens);
	for (auto& childrenComponent : Childrens)
	{
		auto objName = childrenComponent->GetName();
		objName = objName.Replace(TEXT(" "),TEXT("_"));
		objName = objName.Replace(TEXT(":"),TEXT(""));
		objName = objName.Replace(TEXT("["),TEXT(""));
		objName = objName.Replace(TEXT("]"),TEXT(""));
		objName = objName.Replace(TEXT("."),TEXT(""));
		objName = objName.Replace(TEXT("/"),TEXT(""));

		if ( objName == ComponentName)
		{
			OnSuccess.Broadcast(ComponentName,childrenComponent);
			return;
		}
	}
	OnFail.Broadcast(FString(),nullptr);
}
