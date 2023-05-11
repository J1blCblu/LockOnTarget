// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#include "LockOnTargetTypes.h"
#include "TargetComponent.h"
#include "GameFramework/Actor.h"

/********************************************************************
 * FTargetInfo
 ********************************************************************/

const FTargetInfo FTargetInfo::NULL_TARGET = { nullptr, NAME_None };

bool FTargetInfo::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	//[ IsTargetValid? ]~[ NetGUID/Name ][ IsDefaultSocket? ]~[ Socket Idx ]

	bool bTargetMapped = true;
	bOutSuccess = true;

	uint8 bIsValid = IsValid(TargetComponent);
	Ar.SerializeBits(&bIsValid, 1);

	if(bIsValid)
	{
		//As of 5.1, the mapping of components is broken, they're mapped only once, so their owner is serialized instead.
		UObject* TargetOwner = nullptr;

		if(Ar.IsSaving())
		{
			TargetOwner = TargetComponent->GetOwner();
		}
		
		bTargetMapped = Map->SerializeObject(Ar, AActor::StaticClass(), TargetOwner);

		if(Ar.IsLoading())
		{
			if(bTargetMapped)
			{
				TargetComponent = static_cast<AActor*>(TargetOwner)->FindComponentByClass<UTargetComponent>();
				checkf(TargetComponent, TEXT("Serialized Target %s doesn't have UTargetComponent."), *GetFullNameSafe(TargetOwner));
			}
			else
			{
				//Target isn't mapped, so we need to clear the old one if it exists.
				TargetComponent = nullptr;
			}
		}

		//Serialize the Socket index instead of the full string.
		uint32 SocketIdx = 0;

		//Default Socket is 0 index.
		uint8 bDefaultSocket = 1;

		if(Ar.IsSaving())
		{
			SocketIdx = GetSocketIndex();
			bDefaultSocket = SocketIdx == 0;
		}

		Ar.SerializeBits(&bDefaultSocket, 1);
		
		if(!bDefaultSocket)
		{
			//We could only serialize significant bits with FMath::CeilLogTwo(MaxIndex), 
			//but if the Target isn't mapped, we can't safely deserialize it.
			Ar.SerializeIntPacked(SocketIdx);
		}

		if (Ar.IsLoading())
		{
			//If the Target is mapped and TargetComponent is found, we can find the actual Socket.
			if (IsValid(TargetComponent))
			{
				const TArray<FName>& Sockets = TargetComponent->GetSockets();

				if (ensureMsgf(Sockets.IsValidIndex(SocketIdx), TEXT("An invalid Socket index was received from NetSerialize.")))
				{
					Socket = Sockets[SocketIdx];
				}
				else
				{
					bOutSuccess = false;
					Socket = NAME_None;
				}
			}
		}
	}
	else
	{
		TargetComponent = nullptr;
		Socket = NAME_None;
	}

	return bTargetMapped;
}

uint32 FTargetInfo::GetSocketIndex() const
{
	int32 Index = 0;

	if (IsValid(TargetComponent))
	{
		if(int32 FoundIdx = TargetComponent->GetSockets().IndexOfByKey(Socket); FoundIdx != INDEX_NONE)
		{
			Index = FoundIdx;
		}
	}

	return Index;
}

#if 0

/**
 * As components remapping is broken, I've been trying to use DeltaSerialization, but it doesn't work either.
 * Don't want to delete this code because it might be useful.
 * 
 * Refs: FastArraySerializer.h, GameplayDebuggerCategoryReplicator.cpp.
 */

#include "LockOnTargetComponent.h"

class FTargetNetDeltaState : public INetDeltaBaseState
{
public:

	FTargetNetDeltaState(TWeakObjectPtr<UTargetComponent> Target)
		: Target(Target)
	{}

	virtual bool IsStateEqual(INetDeltaBaseState* OtherState) override
	{
		return Target == static_cast<FTargetNetDeltaState*>(OtherState)->Target;
	}

	TWeakObjectPtr<UTargetComponent> Target;
};

bool FTargetInfo::NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
{
	if (DeltaParms.GatherGuidReferences)
	{
		DeltaParms.GatherGuidReferences->Add(NetGUID);
		return true;
	}

	if (DeltaParms.MoveGuidToUnmapped)
	{
		bool bMoved = false;

		if(*DeltaParms.MoveGuidToUnmapped == NetGUID)
		{
			const FTargetInfo OldTarget = *this;
			
			//Probably not needed, cause Target notifies the destruction.
			TargetComponent = nullptr;
			static_cast<ULockOnTargetComponent*>(DeltaParms.Object)->OnTargetInfoUpdated(OldTarget);

			bMoved = true;
		}

		return bMoved;
	}

	if (DeltaParms.bUpdateUnmappedObjects)
	{
		if (!DeltaParms.Map->IsGUIDBroken(NetGUID, false))
		{
			if (UObject* MappedObject = DeltaParms.Map->GetObjectFromNetGUID(NetGUID, false))
			{
				DeltaParms.bOutSomeObjectsWereMapped = true;

				const FTargetInfo OldTarget = *this;
				TargetComponent = static_cast<UTargetComponent*>(MappedObject);
				static_cast<ULockOnTargetComponent*>(DeltaParms.Object)->OnTargetInfoUpdated(OldTarget);
			}
		}

		return true;
	}

	if (DeltaParms.Writer)
	{
		FBitWriter& Writer = *DeltaParms.Writer;

		if (FTargetNetDeltaState* OldState = static_cast<FTargetNetDeltaState*>(DeltaParms.OldState))
		{
			if (OldState->Target == MakeWeakObjectPtr(TargetComponent))
			{
				*DeltaParms.NewState = DeltaParms.OldState->AsShared();
				return false;
			}
		}

		*DeltaParms.NewState = MakeShared<FTargetNetDeltaState>(MakeWeakObjectPtr(TargetComponent));

		uint8 bIsValid = IsValid(TargetComponent);
		Writer.SerializeBits(&bIsValid, 1);

		if (bIsValid)
		{
			Writer << TargetComponent;
		}
	}
	else if (DeltaParms.Reader)
	{
		FBitReader& Reader = *DeltaParms.Reader;

		const FTargetInfo OldTarget = *this;

		uint8 TargetFlag = 0;
		Reader.SerializeBits(&TargetFlag, 1);

		if (TargetFlag)
		{
			DeltaParms.bGuidListsChanged = true;

			UObject* Target = TargetComponent;
			DeltaParms.bOutHasMoreUnmapped = !DeltaParms.Map->SerializeObject(Reader, UTargetComponent::StaticClass(), Target, &NetGUID);
			TargetComponent = static_cast<UTargetComponent*>(Target);
		}
		else
		{
			//DeltaParms.bOutHasMoreUnmapped = false;
			*this = NULL_TARGET;
		}

		static_cast<ULockOnTargetComponent*>(DeltaParms.Object)->OnTargetInfoUpdated(OldTarget);
	}

	return true;
}

#endif
