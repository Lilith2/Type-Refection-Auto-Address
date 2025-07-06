# Type-Refection-Auto-Address

Read Entire classes from remote process into a local cache backed by real types and definitions.
![devenv_LfRwNlnQLH](https://github.com/user-attachments/assets/69ae1e11-98e7-4d0e-9152-edc94f7301ee)

Read non pointer fields from local cached class objects backed by real types and definitions.
![image](https://github.com/user-attachments/assets/ea3e4e3f-c1fc-40d5-915a-d7cf04c12512)

Read pointer fields from local cached class objects backed by real types and definitions.
![image](https://github.com/user-attachments/assets/fbb73828-5c96-4d75-914a-28dbb89a3f77)

# Limitation of the concept

I don't remember why this was an issue, it was almost a full year ago. 

TRemoteObject custom type is used on remote process reads that are contained with a cached copy.
Seems like struct FVector wasn't cached somehow on the class read.
HOWEVER...
rootcomp.localCopy.RelativeLocation  is perfectly valid syntax when I tested while writing this, I didn't test a live read, just syntax & inheritence.
```
//problem code from what my notes said
READ_CLASS_FROM_FIELD(privPawn, RootComponent, SDK::USceneComponent, rootcomp);
if (rootcomp.remoteBase)
{
    //0x128 RelativeLocation , we can't use macros. We have to hard code this part.... sad times
    Vector3 enemyLocation = memory.ReadMemory<Vector3>(rootcomp.remoteBase + 0x128);
    //check example cpp file for rest of code
}
```

### files included:

"Macros; Remote Process Type Reflection with Auto Address.pdf" White paper of my code.

"TypeReflectionMacro.h" the actual macro code to include in your project

"MemoryManager.h" the memory code I used in my prototypes.

"macro-reflect-example.cpp" several excerpts of my prototype that for legal reasons I will not release in full. Should give you the gist of how it works when combined with the white paper.
