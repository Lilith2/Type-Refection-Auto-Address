//#define TESTREAD
// **Now create the MemoryManager**
MemoryManager memory(TARGET_PROCESS_NAME);

struct RenderData
{
    float w2s_coords[2] {0,0};
    int rgba[4]{0,0,0,1};
    bool box = true;
    bool text = true;
    bool ready = false;
    std::string label;
};
 // Used for rendering.
std::vector<RenderData> backBuffer; // Used for gathering.
std::vector<RenderData> frontBuffer;

//following code is psuedo in that I ripped it from bigger funcs.

//assuming you already setup the target process init
#ifndef TESTREAD
    uintptr_t GWorld = 0;
    std::vector<wchar_t> levelName;
    std::vector<TRemoteObject<SDK::APlayerState>> shadowPlayerArray;
    static Camera cam{};
#endif // !TESTREAD

if (inGame)
{
    READ_CLASS_FROM_REMOTE(SDK::UWorld, GWorld, klass); //the real magic, external byte reading with user-defined types that actually captures the correct data as if you're internal for the object
    auto gameState = GET_PTR_FIELD(klass, GameState); 
    auto owningGame = GET_PTR_FIELD(klass, OwningGameInstance); //no more manual address + offset for fields, just type in the EXACT SDK field name for the class you're reading
    if (gameState && owningGame)
    {
        READ_CLASS_FROM_REMOTE(SDK::UGameInstance, owningGame, klass_UGameInstance); //reading the class pointer as it's type, allows us to have our own instance locally
        SDK::TArray<SDK::ULocalPlayer*> lp = GET_VAL_FIELD(klass_UGameInstance, LocalPlayers); //reading non-pointers also works, even TArrays to an extent. Remember, this is reading our cached object
        uintptr_t ptr = *reinterpret_cast<uintptr_t*>(&lp);  //it works because our cache takes the actual raw pointer address from the TArray, not the index 0 unlike most tools. See the player array example bellow.
        if (ptr)
        {
            auto derefPlayer = memory.ReadMemory<uintptr_t>(ptr + 0x0);
            auto PlayerController = memory.ReadMemory<uintptr_t>(derefPlayer + 0x30);
            if (PlayerController)
            {
                auto CameraMangaer = memory.ReadMemory<uintptr_t>(PlayerController + 0x350);
                if (CameraMangaer)
                {
                    READ_CLASS_FROM_REMOTE(SDK::AGameStateBase, gameState, klass_gamestatebase);
                    SDK::TArray<SDK::APlayerState*>  localTArray = GET_VAL_FIELD(klass_gamestatebase, PlayerArray); //full player array read from cached SDK::AGameStateBase pointer
                    uintptr_t remoteDataPtr = *reinterpret_cast<uintptr_t*>(&localTArray);
                    std::vector<RenderData> newData;
                    for (int i = 0; i < localTArray.Num(); i++)
                    {
                        // Read the i-th pointer (APlayerState*) from the remote array:
                        uintptr_t elementPtr = memory.ReadMemory<uintptr_t>(remoteDataPtr + i * sizeof(uintptr_t));

                        // If valid, read that whole APlayerState object and do something with it:
                        if (elementPtr)
                        {
                            READ_CLASS_TRemoteObject(SDK::APlayerState, elementPtr, localPlayer);
                            if (localPlayer.localCopy.PawnPrivate)
                            {
                                READ_CLASS_FROM_FIELD(localPlayer, PawnPrivate, SDK::APawn, privPawn);
                                if (privPawn.remoteBase)
                                {
                                    READ_CLASS_FROM_FIELD(privPawn, RootComponent, SDK::USceneComponent, rootcomp);
                                    if (rootcomp.remoteBase)
                                    {
                                        //0x128 RelativeLocation , we can't use macros. We have to hard code this part.... sad times
                                        Vector3 enemyLocation = memory.ReadMemory<Vector3>(rootcomp.remoteBase + 0x128);
                                        //cam.fov = memory.ReadMemory<float>(CameraMangaer + 0x370);//0x2B0 DesiredFOV , 0x370 FOV inside ViewTarget
                                        cam.location = memory.ReadMemory<Vector3>(CameraMangaer + 0x340);
                                        cam.rotation = memory.ReadMemory<Rotator>(CameraMangaer + 0x358);
                                        // Build the view-projection matrix.
                                        Matrix4 VP = BuildViewProjectionMatrix(cam.location, cam.rotation, 80, (double)3440/1440, 100);
                                        Vector2 screen = {};
                                        ProjectWorldToScreen(screen, VP, enemyLocation, 3440, 1440);
                                        if (screen.x > 0 && screen.y > 0)
                                        {
                                            std::cout << "Projected screen coords: (" << screen.x << ", " << screen.y << ")\n";
                                            RenderData rd = {};
                                            rd.ready = true;
                                            double dist = Distance(cam.location, enemyLocation);
                                            dist /= 100;
                                            rd.label = std::to_string((int)dist);
                                            std::cout << "Meters: " << rd.label << " \n";
                                            rd.w2s_coords[0] = screen.x;
                                            rd.w2s_coords[1] = screen.y;
                                            rd.rgba[0] = 1;//red
                                            rd.rgba[1] = 1;//green
                                            rd.rgba[2] = 0;//blue
                                            rd.rgba[3] = 1;//alpha
                                            newData.push_back(rd);
                                        }
                                    }
                                }
                            }
                        }
                    }
                    {
                        std::lock_guard<std::mutex> lock(mtx);
                        // Instead of clearing and refilling, we swap in the new data.
                        backBuffer.swap(newData);
                    }
                }
                else { std::cout << "Failed to find CameraMangaer\n"; }

            } else{ std::cout << "Failed to find PlayerContoller\n"; }
        }
        else { std::cout << "Failed to find first element in localplayer TArray\n"; } 
    }
}
