template<typename T>
struct TRemoteObject
{
    uintptr_t   remoteBase; // address in the remote process
    T           localCopy;  // your locally read copy of the remote data
};


#define READ_CLASS_FROM_REMOTE(SDKType, remoteAddr, localVar)                   \
    struct __##localVar##_Wrapper {                                             \
        uintptr_t remoteBase;                                                   \
        SDKType   localCopy;                                                    \
    };                                                                           \
    __##localVar##_Wrapper localVar;                                            \
    do {                                                                         \
        localVar.remoteBase = (remoteAddr);                                      \
        localVar.localCopy   = memory.ReadMemory<SDKType>((remoteAddr));         \
    } while(0)

#define READ_CLASS_TRemoteObject(SDKType, remoteAddr, localVar)         \
    TRemoteObject<SDKType> localVar;                                  \
    do {                                                              \
        localVar.remoteBase = (remoteAddr);                           \
        localVar.localCopy   = memory.ReadMemory<SDKType>((remoteAddr)); \
    } while (0)

#define READ_CLASS_FROM_FIELD(parentVar, fieldName, TargetSDKType, outVar) \
    TRemoteObject<TargetSDKType> outVar;                                   \
    do {                                                                   \
        uintptr_t __remotePtr = reinterpret_cast<uintptr_t>(              \
            (parentVar).localCopy.fieldName                               \
        );                                                                 \
        outVar.remoteBase   = __remotePtr;                                 \
        outVar.localCopy    = memory.ReadMemory<TargetSDKType>(__remotePtr); \
    } while(0)


#define GET_PTR_FIELD(localVar, fieldName)                                       \
    memory.ReadMemory<uintptr_t>(                                               \
        localVar.remoteBase + offsetof(decltype(localVar.localCopy), fieldName)  \
    )

#define GET_VAL_FIELD(localVar, fieldName) \
    ( localVar.localCopy.fieldName )
