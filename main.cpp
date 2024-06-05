#include <mod/amlmod.h>
#include <mod/logger.h>
#include <mod/config.h>
#include <string>
#include <chrono>

//#define SCREEN_OPTIMISATION // it eats CPU on weaker phones. No.
#define CALCSCREENCOORS_SPEEDUP

#ifdef AML32
    #include "GTASA_STRUCTS.h"
#else
    #include "GTASA_STRUCTS_210.h"
#endif
#include <searchlights.h>

// Structs
enum BlinkTypes
{
    DEFAULT_BLINK,
    RANDOM_FLASHING,
    T_1S_ON_1S_OFF,
    T_2S_ON_2S_OFF,
    T_3S_ON_3S_OFF,
    T_4S_ON_4S_OFF,
    T_5S_ON_5S_OFF,
    T_6S_ON_4S_OFF
};
class CLODRegisteredCorona
{
public:
    CVector     Coordinates;            // Where is it exactly.
    float       Size;                   // How big is this fellow
    float       Range;                  // How far away is this guy still visible
    uint8_t     Red, Green, Blue;       // Rendering colour.
    uint8_t     Intensity;              // 255 = full
    uintptr_t   Identifier;             // Should be unique for each corona. Address or something (0 = empty)
    
    bool        RegisteredThisFrame;    // Has this guy been registered by game code this frame
    bool        JustCreated;            // If this guy has been created this frame we won't delete it (It hasn't had the time to get its OffScreen cleared) ##SA removed from packed byte ##
    bool        OffScreen;              // Set by the rendering code to be used by the update code

    CLODRegisteredCorona() : Identifier(0) {}
    inline void Update()
    {
        if (!RegisteredThisFrame) Intensity = 0;
        if (!Intensity && !JustCreated) Identifier = 0;
        JustCreated = false;
        RegisteredThisFrame = false;
    }
};
class CLODLightsLinkedListNode
{
private:
    CLODLightsLinkedListNode*           pNext;
    CLODLightsLinkedListNode*           pPrev;
    CLODRegisteredCorona*               pEntry;

private:
    inline void                         Remove()
        { pNext->pPrev = pPrev; pPrev->pNext = pNext; pNext = NULL; }

public:
    inline void                         Init()
        { pNext = pPrev = this; }
    inline void                         Add(CLODLightsLinkedListNode* pHead)
        { if ( pNext ) Remove();
          pNext = pHead->pNext; pPrev = pHead; pHead->pNext->pPrev = this; pHead->pNext = this; }
    inline void                         SetEntry(CLODRegisteredCorona* pEnt)
        { pEntry = pEnt; }
    inline CLODRegisteredCorona*        GetFrom()
        { return pEntry; }
    inline CLODLightsLinkedListNode*    GetNextNode()
        { return pNext; }
    inline CLODLightsLinkedListNode*    GetPrevNode()
        { return pPrev; }

    inline CLODLightsLinkedListNode*    First()
        { return pNext == this ? NULL : pNext; }
};

// Init
MYMOD(net.thirteenag.rusjj.gtasa.2dfx, Project 2DFX, 1.0, ThirteenAG & RusJJ)
NEEDGAME(com.rockstargames.gtasa)
BEGIN_DEPLIST()
    ADD_DEPENDENCY_VER(net.rusjj.aml, 1.2.2)
END_DEPLIST()

Config*             cfg;
uintptr_t           pGTASA;
void*               hGTASA;

// Org Vars
CBaseModelInfo**    ms_modelInfoPtrs;
int*                nActiveInterior;
float               *flWeatherFoggyness, *ms_fTimeStep, *ms_fFarClipZ, *ms_fNearClipZ, *ms_fFOV;
CCamera*            TheCamera;
RwTexture**         gpCoronaTexture;
RwTexture**         gpShadowExplosionTex;
CColourSet*         m_CurrentColours;
uint8_t             *CurrentTimeHours, *CurrentTimeMinutes;
uint32_t            *m_snTimeInMillisecondsPauseMode;
RsGlobalType*       RsGlobal;
float*              UnderWaterness;
float*              game_FPS;
float*              ms_fFarClipPlane;

// Org Fns
char                (*GetIsTimeInRange)(char hourA, char hourB);
float               (*FindGroundZFor3DCoord)(float x, float y, float z, bool* pCollisionResult, CEntity **pGroundObject);
uintptr_t           (*OpenFile)(const char* filepath, const char* mode);
void                (*CloseFile)(uintptr_t fd);
const char*         (*LoadLine)(uintptr_t fd);
CBaseModelInfo*     (*GetModelInfoUInt16)(const char* modelName, uint16_t *index);
int                 (*FindIplSlot)(const char* name);
void                (*RequestIplAndIgnore)(int iplSlot);
void                (*RequestModel)(int modelId, eStreamingFlags flag);
void                (*LoadAllRequestedModels)(bool bOnlyPriorityRequests);
void                (*RwRenderStateSet)(RwRenderState, void*);
void                (*FlushSpriteBuffer)();
bool                (*CalcScreenCoors)(CVector*, CVector*, float*, float*, bool, bool);
void                (*RenderBufferedOneXLUSprite_Rotate_Aspect)(float,float,float,float,float,uint8_t,uint8_t,uint8_t,short,float,float,uint8_t);
void                (*StoreStaticShadow)(uintptr_t ID, UInt8 Type, RwTexture *pTexture, CVector *pCoors, float ForwardX, float ForwardY, float SideX, float SideY, int Brightness, int Red, int Green, int Blue, float ShadowDepth, float TextureScale, float MaxDist, float Margin, bool bFromTree);
CPlayerPed*         (*FindPlayerPed)(int playerId);
void                (*Pre_SearchLightCone)();
void                (*Post_SearchLightCone)();
RwImage*            (*RtPNGImageRead)(const char*);
RwImage*            (*RwImageFindRasterFormat)(RwImage *ipImage, RwInt32 nRasterType, RwInt32 *npWidth, RwInt32 *npHeight, RwInt32 *npDepth, RwInt32 *npFormat);
RwRaster*           (*RwRasterCreate)(RwInt32 width, RwInt32 height, RwInt32 depth, RwInt32 flags);
RwRaster*           (*RwRasterSetFromImage)(RwRaster *raster, RwImage *image);
RwTexture*          (*RwTextureCreate)(RwRaster *raster);
RwBool              (*RwImageDestroy)(RwImage *image);
bool                (*ProcessLineOfSight)(const CVector *vecStart, const CVector *vecEnd, CColPoint *colPoint, CEntity **refEntityPtr, bool bCheckBuildings, bool bCheckVehicles, bool bCheckPeds, bool bCheckObjects, bool bCheckDummies, bool bSeeThroughStuff, bool bIgnoreSomeObjectsForCamera, bool bShootThroughStuff);
void*               (*RwIm3DTransform)(RwIm3DVertex *pVerts, RwUInt32 numVerts, RwMatrix *ltm, RwUInt32 flags);
void                (*RwIm3DRenderIndexedPrimitive)(RwPrimitiveType primType, uint16_t *indices, RwInt32 numIndices);
void                (*RwIm3DEnd)();
CVector             (*MatrixVectorMult)(CMatrix*, CVector*);

void                (*RegisterLODCorona)(uintptr_t nID, unsigned char R, unsigned char G, unsigned char B, unsigned char A, const CVector& Position, float Size, float Range);

// Vars
std::map<uintptr_t, CLODLightsLinkedListNode*> LODLightsUsedMap;
CLODLightsLinkedListNode LODLightsFreeList, LODLightsUsedList;
std::vector<CLODLightsLinkedListNode> LODLightsLinkedList;
std::vector<CLODRegisteredCorona> LODLightsCoronas;
std::map<uintptr_t, CRGBA> LODLightsFestiveLights;
std::vector<CEntity*> lods;
int                 numCoronas;
bool                bCatchLamppostsNow;
RwTexture*          gpCustomCoronaTexture;

// Config Vars
bool                bRenderLodLights;
float               fCoronaRadiusMultiplier;
bool                bSlightlyIncreaseRadiusWithDistance;
float               fCoronaFarClip, fCoronaFarClipSQR;
bool                autoFarClip;
const char*         szCustomCoronaTexturePath;
bool                bRenderStaticShadowsForLODs;
bool                bIncreasePedsCarsShadowsDrawDistance;
float               fStaticShadowsIntensity, fStaticShadowsDrawDistance;
float               fTrafficLightsShadowsIntensity, fTrafficLightsShadowsDrawDistance;
bool                bRenderSearchlightEffects;
bool                bRenderOnlyDuringFoggyWeather;
bool                bRenderHeliSearchlights;
int                 nSmoothEffect;
float               fSearchlightEffectVisibilityFactor;
bool                bEnableDrawDistanceChanger;
float               fMinDrawDistanceOnTheGround, fFactor1, fFactor2, fStaticSunSize;
bool                bAdaptiveDrawDistanceEnabled;
int                 nMinFPSValue, nMaxFPSValue;
float               fNewFarClip = 500.0f, fMaxPossibleDrawDistance;
float               fMaxDrawDistanceForNormalObjects = 300.0f, fTimedObjectsDrawDistance, fNeonsDrawDistance, fLODObjectsDrawDistance;
float               fGenericObjectsDrawDistance, fAllNormalObjectsDrawDistance, fVegetationDrawDistance;
bool                bLoadAllBinaryIPLs, bPreloadLODs;
float               fDrawDistance;
bool                bRandomExplosionEffects, bReplaceSmokeTrailWithBulletTrail, bFestiveLights, bFestiveLightsAlways;

#ifdef AML64
    float           *MaxObjectsDrawDistance;
    float           *MaxStaticShadowsDrawDistance;
#endif

// Funcs
void Init_MiniLA();
RwTexture* CPNGFileReadFromFile(const char* pFileName)
{
    RwTexture* pTexture = nullptr;
    if (RwImage* pImage = RtPNGImageRead(pFileName))
    {
        int dwWidth, dwHeight, dwDepth, dwFlags;
        RwImageFindRasterFormat(pImage, rwRASTERTYPETEXTURE, &dwWidth, &dwHeight, &dwDepth, &dwFlags);
        RwRaster* pRaster;
        if ( (pRaster = RwRasterCreate(dwWidth, dwHeight, dwDepth, dwFlags)) )
        {
            RwRasterSetFromImage(pRaster, pImage);
            pTexture = RwTextureCreate(pRaster);
        }
        RwImageDestroy(pImage);
    }
    return pTexture;
}
static float CalcScreenCoors_Width;
static float CalcScreenCoors_Height;
static float CalcScreenCoors_50FOV;
static float CalcScreenCoors_70FOV;
inline bool CalcScreenCoorsFast(CVector* WorldPos, CVector* ScreenPos, float* ScaleX, float* ScaleY) // +1 FPS. Better than nothing...
{
    CVector screenPos = MatrixVectorMult(&TheCamera->m_viewMatrix, WorldPos); //TheCamera->m_viewMatrix * *WorldPos; // my function is brokey *sad sad* ;-;
    if(screenPos.z <= *ms_fNearClipZ + 1.0f || screenPos.z >= *ms_fFarClipZ) return false;

    float invDepth = 1.0f / screenPos.z;
    float invScreenWidth = invDepth * CalcScreenCoors_Width, invScreenHeight = invDepth * CalcScreenCoors_Height;

    screenPos.x *= invScreenWidth;
    screenPos.y *= invScreenHeight;
    *ScreenPos = screenPos;

    *ScaleX = invScreenWidth * CalcScreenCoors_50FOV;
    *ScaleY = invScreenHeight * CalcScreenCoors_70FOV;

    return true;
}
void RenderBufferedLODLights()
{
  #ifdef SCREEN_OPTIMISATION
    int nWidth = RsGlobal->maximumWidth;
    int nHeight = RsGlobal->maximumHeight;
  #endif

  #ifdef CALCSCREENCOORS_SPEEDUP
    CalcScreenCoors_Width = RsGlobal->maximumWidth;
    CalcScreenCoors_Height = RsGlobal->maximumHeight;
    CalcScreenCoors_50FOV = (50.0f / *ms_fFOV) * (1.4286f * CalcScreenCoors_Height / CalcScreenCoors_Width);
    CalcScreenCoors_70FOV = (70.0f / *ms_fFOV);
  #endif

    RwRaster* pTargetRaster = (gpCustomCoronaTexture != NULL) ? gpCustomCoronaTexture->raster : gpCoronaTexture[1]->raster;
    CVector vecTransformedCoords;
    float fComputedWidth, fComputedHeight;

    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, (void*)false);
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)true);
    RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDONE);
    RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDONE);
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE, (void*)true);
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER, (void*)pTargetRaster);

    float weatherFogScaled = *flWeatherFoggyness * 0.025f;

    size_t size = LODLightsCoronas.size();
    for (size_t i = 0; i < size; ++i)
    {
        CLODRegisteredCorona& corona = LODLightsCoronas[i];
        if (corona.Identifier != 0 && corona.Intensity != 0)
        {
          #ifdef CALCSCREENCOORS_SPEEDUP
            if(CalcScreenCoorsFast(&corona.Coordinates, &vecTransformedCoords, &fComputedWidth, &fComputedHeight) && vecTransformedCoords.z <= corona.Range)
          #else
            if(CalcScreenCoors(&corona.Coordinates, &vecTransformedCoords, &fComputedWidth, &fComputedHeight, true, true) && vecTransformedCoords.z <= corona.Range)
          #endif
            {
              #ifdef SCREEN_OPTIMISATION
                corona.OffScreen = !(vecTransformedCoords.x >= 0.0 && vecTransformedCoords.x <= nWidth && vecTransformedCoords.y >= 0.0 && vecTransformedCoords.y <= nHeight);
                if(corona.OffScreen) continue;
              #endif

                float fInvFarClip = 20.0f / vecTransformedCoords.z;
                float fHalfRange = corona.Range * 0.5f;
                short nFadeIntensity = (short)(corona.Intensity * (vecTransformedCoords.z > fHalfRange ? (1.0f - (vecTransformedCoords.z - fHalfRange) / fHalfRange) : 1.0f));
                float fColourFogMult = fminf(40.0f, vecTransformedCoords.z) * weatherFogScaled + 1.0f;
                float fColourFogMultInv = 1.0f / fColourFogMult;

                RenderBufferedOneXLUSprite_Rotate_Aspect(vecTransformedCoords.x, vecTransformedCoords.y, vecTransformedCoords.z, corona.Size * fComputedWidth, corona.Size * fComputedHeight * fColourFogMult, corona.Red * fColourFogMultInv, corona.Green * fColourFogMultInv, corona.Blue * fColourFogMultInv, nFadeIntensity, fInvFarClip, 0.0f, 255);
            }
            else
            {
                corona.OffScreen = true;
            }
        }
    }
    FlushSpriteBuffer();
}
void LoadDatFile()
{
    uintptr_t fd = OpenFile("data/SALodLights.dat", "r");
    if(fd)
    {
        unsigned short nModel = 0xFFFF, nCurIndexForModel = 0;
        const char* pLine;
        while((pLine = LoadLine(fd)) != NULL)
        {
            if (pLine[0] && pLine[0] != '#')
            {
                if (pLine[0] == '%')
                {
                    nCurIndexForModel = 0;
                    if (strcmp(pLine, "%additional_coronas") != 0)
                    {
                        GetModelInfoUInt16(pLine + 1, &nModel);
                    }
                    else
                    {
                        nModel = 0xFFFE;
                    }
                }
                else
                {
                    float fOffsetX, fOffsetY, fOffsetZ;
                    unsigned int nRed, nGreen, nBlue, nAlpha;
                    float fCustomSize = 1.0f;
                    float fDrawDistance = 0.0f;
                    int nNoDistance = 0;
                    int nDrawSearchlight = 0;
                    int nCoronaShowMode = 0;

                    if (sscanf(pLine, "%3d %3d %3d %3d %f %f %f %f %f %2d %1d %1d", &nRed, &nGreen, &nBlue, &nAlpha, &fOffsetX, &fOffsetY, &fOffsetZ, &fCustomSize, &fDrawDistance, &nCoronaShowMode, &nNoDistance, &nDrawSearchlight) != 12)
                    {
                        sscanf(pLine, "%3d %3d %3d %3d %f %f %f %f %2d %1d %1d", &nRed, &nGreen, &nBlue, &nAlpha, &fOffsetX, &fOffsetY, &fOffsetZ, &fCustomSize, &nCoronaShowMode, &nNoDistance, &nDrawSearchlight);
                    }
                    pFileContent->insert(std::make_pair(PackKey(nModel, nCurIndexForModel++), CLamppostInfo(CVector(fOffsetX, fOffsetY, fOffsetZ), CRGBA((uint8_t)nRed, (uint8_t)nGreen, (uint8_t)nBlue, (uint8_t)nAlpha), fCustomSize, nCoronaShowMode, nNoDistance, nDrawSearchlight, 0.0f, fDrawDistance)));
                }
            }
        }
        bCatchLamppostsNow = true;
        CloseFile(fd);
    }
    else
    {
        bRenderLodLights = false;
        bRenderSearchlightEffects = false;
    }
}
void RegisterCustomCoronas()
{
    unsigned short nModelID = 0xFFFE;

    gpCustomCoronaTexture = CPNGFileReadFromFile(szCustomCoronaTexturePath);
    //if(gpCustomCoronaTexture) gpCoronaTexture[1] = gpCustomCoronaTexture; // We dont need it here. LOD corona is loaded in rendering func

    auto itEnd = pFileContent->upper_bound(PackKey(nModelID, 0xFFFF));
    for (auto it = pFileContent->lower_bound(PackKey(nModelID, 0)); it != itEnd; ++it)
    {
        auto& target = it->second;
        m_pLampposts->push_back(CLamppostInfo(target.vecPos, target.colour, target.fCustomSizeMult, target.nCoronaShowMode, target.nNoDistance, target.nDrawSearchlight, 0.0f));
    }
}
bool IsBlinkingNeeded(int BlinkType)
{
    signed int nOnDuration = 0;
    signed int nOffDuration = 0;

    switch (BlinkType)
    {
    case BlinkTypes::DEFAULT_BLINK:
        return false;
    case BlinkTypes::RANDOM_FLASHING:
        nOnDuration = 500;
        nOffDuration = 500;
        break;
    case BlinkTypes::T_1S_ON_1S_OFF:
        nOnDuration = 1000;
        nOffDuration = 1000;
        break;
    case BlinkTypes::T_2S_ON_2S_OFF:
        nOnDuration = 2000;
        nOffDuration = 2000;
        break;
    case BlinkTypes::T_3S_ON_3S_OFF:
        nOnDuration = 3000;
        nOffDuration = 3000;
        break;
    case BlinkTypes::T_4S_ON_4S_OFF:
        nOnDuration = 4000;
        nOffDuration = 4000;
        break;
    case BlinkTypes::T_5S_ON_5S_OFF:
        nOnDuration = 5000;
        nOffDuration = 5000;
        break;
    case BlinkTypes::T_6S_ON_4S_OFF:
        nOnDuration = 6000;
        nOffDuration = 4000;
        break;
    default:
        return false;
    }
    return *m_snTimeInMillisecondsPauseMode % (nOnDuration + nOffDuration) < nOnDuration;
}
inline void RegisterCoronaMain(uintptr_t nID, unsigned char R, unsigned char G, unsigned char B, unsigned char A, const CVector& Position, float Size, float Range)
{
    CVector vecPosToCheck = Position;
    CVector pCamPos = TheCamera->GetPosition();

    if (Range * Range >= (pCamPos.x - vecPosToCheck.x)*(pCamPos.x - vecPosToCheck.x) + (pCamPos.y - vecPosToCheck.y)*(pCamPos.y - vecPosToCheck.y))
    {
        // Is corona already present?
        CLODRegisteredCorona* pSuitableSlot;
        auto it = LODLightsUsedMap.find(nID);
        if (it != LODLightsUsedMap.end())
        {
            CLODLightsLinkedListNode* node = it->second;
            pSuitableSlot = node->GetFrom();
            if (pSuitableSlot->Intensity == 0 && A == 0)
            {
                // Mark as free
                node->GetFrom()->Identifier = 0;
                node->Add(&LODLightsFreeList);
                LODLightsUsedMap.erase(nID);
                return;
            }
        }
        else
        {
            if (!A) return;

            // Adding a new element
            auto pNewEntry = LODLightsFreeList.First();
            if (!pNewEntry) return;
            pSuitableSlot = pNewEntry->GetFrom();

            // Add to used list and push this index to the map
            pNewEntry->Add(&LODLightsUsedList);
            LODLightsUsedMap[nID] = pNewEntry;
            pSuitableSlot->OffScreen = true;
            pSuitableSlot->JustCreated = true;
            pSuitableSlot->Identifier = nID;
        }

        pSuitableSlot->Red = R;
        pSuitableSlot->Green = G;
        pSuitableSlot->Blue = B;
        pSuitableSlot->Intensity = A;
        pSuitableSlot->Coordinates = Position;
        pSuitableSlot->Size = Size;
        pSuitableSlot->Range = Range;
        pSuitableSlot->RegisteredThisFrame = true;
    }
}
void RegisterNormalCorona(uintptr_t nID, unsigned char R, unsigned char G, unsigned char B, unsigned char A, const CVector& Position, float Size, float Range)
{
    RegisterCoronaMain(nID, R, G, B, A, Position, Size, Range);
}
void RegisterFestiveCorona(uintptr_t nID, unsigned char R, unsigned char G, unsigned char B, unsigned char A, const CVector& Position, float Size, float Range)
{
    auto it = LODLightsFestiveLights.find(nID);
    if (it != LODLightsFestiveLights.end())
    {
        CRGBA color = it->second;
        RegisterCoronaMain(nID, color.r, color.g, color.b, A, Position, Size, Range);
    }
    else
    {
        LODLightsFestiveLights[nID] = CRGBA(rand() % 256, rand() % 256, rand() % 256, 0);
        RegisterCoronaMain(nID, R, G, B, A, Position, Size, Range);
    }
}
void LODLightsUpdate()
{
    auto pNode = LODLightsUsedList.First();
    if (pNode)
    {
        CLODRegisteredCorona* corona;
        while (pNode != &LODLightsUsedList)
        {
            corona = pNode->GetFrom();
            unsigned int nIndex = corona->Identifier;
            auto pNext = pNode->GetNextNode();
            corona->Update();

            // Did it become invalid?
            if (!corona->Identifier)
            {
                // Remove from used list
                pNode->Add(&LODLightsFreeList);
                LODLightsUsedMap.erase(nIndex);
            }
            pNode = pNext;
        }
    }
}
void RegisterLODLights()
{
    if (GetIsTimeInRange(20, 7) && *nActiveInterior == 0)
    {
        unsigned char   bAlpha = 0;
        float           fRadius = 0.0f;
        unsigned int    nTime = *CurrentTimeHours * 60 + *CurrentTimeMinutes;
        unsigned int    curMin = *CurrentTimeMinutes;
        float           fCurCoronaFarClip = autoFarClip ? *ms_fFarClipPlane : fCoronaFarClip;
        float           fCurCoronaFarClipSQR = fCurCoronaFarClip * fCurCoronaFarClip;

        if (nTime >= 20 * 60) bAlpha = (uint8_t)((15.0f / 16.0f) * nTime - 1095.0f);
        else if (nTime < 3 * 60) bAlpha = 255;
        else bAlpha = (uint8_t)((-15.0f / 16.0f) * nTime + 424.0f);

        CVector pCamPos = TheCamera->GetPosition();

        CLamppostInfo* target;
        CVector targetPos;
        auto cend = m_pLampposts->cend();
        for (auto it = m_pLampposts->cbegin(); it != cend; ++it)
        {
            target = (CLamppostInfo*)&*it;
            targetPos = target->vecPos;
            // The line below is moved to 'CSearchLights::RegisterLamppost'
            //if ((targetPos.z >= -15.0f) && (targetPos.z <= 1030.0f))
            {
                float fDist = (pCamPos - targetPos).MagnitudeSqr();
                if (target->nNoDistance || (fDist > 250.0f*250.0f && fDist < fCurCoronaFarClipSQR))
                {
                    fDist = sqrtf(fDist);

                    if (target->nNoDistance) fRadius = 3.5f;
                    else fRadius = (fDist < 300.0f) ? (0.07f) * fDist - 17.5f : 3.5f;

                    if (bSlightlyIncreaseRadiusWithDistance) fRadius *= fminf(0.0025f * fDist + 0.25f, 4.0f);

                    CRGBA Color = target->colour;

                    if (target->fCustomSizeMult != 0.45f)
                    {
                        if (!target->nCoronaShowMode)
                        {
                            RegisterLODCorona((uintptr_t)target, Color.r, Color.g, Color.b, (bAlpha * (Color.a * 0.00390625f)), targetPos, (fRadius * target->fCustomSizeMult * fCoronaRadiusMultiplier), fCurCoronaFarClip);
                            if (bRenderStaticShadowsForLODs)
                            {
                                StoreStaticShadow((uintptr_t)target, 2 /*SHADOW_ADDITIVE*/, *gpShadowExplosionTex, &targetPos, 8.0f, 0.0f, 0.0f, -8.0f, bAlpha, (Color.r / 3), (Color.g / 3), (Color.b / 3), 15.0f, 1.0f, fCoronaFarClip, false, false);
                            }
                        }
                        else
                        {
                            static float blinking;
                            if (IsBlinkingNeeded(target->nCoronaShowMode)) blinking -= *ms_fTimeStep * 0.001f;
                            else blinking += *ms_fTimeStep  * 0.001f;

                            (blinking > 1.0f) ? blinking = 1.0f : (blinking < 0.0f) ? blinking = 0.0f : 0.0f;

                            RegisterLODCorona((uintptr_t)target, Color.r, Color.g, Color.b, blinking * (bAlpha * (Color.a * 0.00390625f)), targetPos, (fRadius * target->fCustomSizeMult * fCoronaRadiusMultiplier), fCurCoronaFarClip);
                        }
                    }
                    else
                    {
                        if ((Color.r >= 250 && Color.g >= 100 && Color.b <= 100) && ((curMin == 9 || curMin == 19 || curMin == 29 || curMin == 39 || curMin == 49 || curMin == 59))) //yellow
                        {
                            RegisterLODCorona((uintptr_t)target, Color.r, Color.g, Color.b, (bAlpha * (Color.a * 0.00390625f)), targetPos, (fRadius * target->fCustomSizeMult * fCoronaRadiusMultiplier), fCurCoronaFarClip);
                        }
                        else
                        {
                            if ((fabsf(target->fHeading) >= (3.1415f / 6.0f) && fabsf(target->fHeading) <= (5.0f * 3.1415f / 6.0f)))
                            {
                                if ((Color.r >= 250 && Color.g < 100 && Color.b == 0) && (((curMin >= 0 && curMin < 9) || (curMin >= 20 && curMin < 29) || (curMin >= 40 && curMin < 49)))) //red
                                {
                                    RegisterLODCorona((uintptr_t)target, Color.r, Color.g, Color.b, (bAlpha * (Color.a * 0.00390625f)), targetPos, (fRadius * target->fCustomSizeMult * fCoronaRadiusMultiplier), fCurCoronaFarClip);
                                }
                                else if ((Color.r == 0 && Color.g >= 250 && Color.b == 0) && (((curMin > 9 && curMin < 19) || (curMin > 29 && curMin < 39) || (curMin > 49 && curMin < 59)))) //green
                                {
                                    RegisterLODCorona((uintptr_t)target, Color.r, Color.g, Color.b, (bAlpha * (Color.a * 0.00390625f)), targetPos, (fRadius * target->fCustomSizeMult * fCoronaRadiusMultiplier), fCurCoronaFarClip);
                                }
                            }
                            else
                            {
                                if ((Color.r == 0 && Color.g >= 250 && Color.b == 0) && (((curMin >= 0 && curMin < 9) || (curMin >= 20 && curMin < 29) || (curMin >= 40 && curMin < 49)))) //red
                                {
                                    RegisterLODCorona((uintptr_t)target, Color.r, Color.g, Color.b, (bAlpha * (Color.a * 0.00390625f)), targetPos, (fRadius * target->fCustomSizeMult * fCoronaRadiusMultiplier), fCurCoronaFarClip);
                                }
                                else if ((Color.r >= 250 && Color.g < 100 && Color.b == 0) && (((curMin > 9 && curMin < 19) || (curMin > 29 && curMin < 39) || (curMin > 49 && curMin < 59)))) //green
                                {
                                    RegisterLODCorona((uintptr_t)target, Color.r, Color.g, Color.b, (bAlpha * (Color.a * 0.00390625f)), targetPos, (fRadius * target->fCustomSizeMult * fCoronaRadiusMultiplier), fCurCoronaFarClip);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    LODLightsUpdate();
}
void DrawDistanceChanger()
{
    fNewFarClip = 500.0f;

    CPlayerPed* pPlayerPed = FindPlayerPed(-1);
    if (pPlayerPed && pPlayerPed->m_nInterior != 8 && *nActiveInterior == 0)
    {
        if (bAdaptiveDrawDistanceEnabled)
        {
            if (*game_FPS < nMinFPSValue)
            {
                fMinDrawDistanceOnTheGround -= 2.0f;
            }
            else if (*game_FPS >= nMaxFPSValue)
            {
                fMinDrawDistanceOnTheGround += 2.0f;
            }

            if (fMinDrawDistanceOnTheGround < 800.0f) fMinDrawDistanceOnTheGround = 800.0f;
            else if (fMinDrawDistanceOnTheGround > fMaxPossibleDrawDistance) fMinDrawDistanceOnTheGround = fMaxPossibleDrawDistance;
        }

        if (*UnderWaterness <= 0.339731634f)
        {
            fNewFarClip = (fFactor1 / fFactor2) * (TheCamera->GetPosition().z) + fMinDrawDistanceOnTheGround;
        }
        else
        {
            fNewFarClip = m_CurrentColours->farclp;
        }
    }

    *ms_fFarClipPlane = fNewFarClip;
}

void SetMaxDrawDistanceForNormalObjects(float v)
{
    if(fMaxDrawDistanceForNormalObjects < v)
    {
        fMaxDrawDistanceForNormalObjects = v;
        
      #ifdef AML32
        aml->WriteFloat(pGTASA + 0x4114E8,  v);
        aml->WriteFloat(pGTASA + 0x4114F4, -v);
        aml->WriteFloat(pGTASA + 0x40F3B4,  v);
        aml->WriteFloat(pGTASA + 0x40F370,  v);
        aml->WriteFloat(pGTASA + 0x40F374,  v);
        aml->WriteFloat(pGTASA + 0x40F378,  v);
        aml->WriteFloat(pGTASA + 0x40F37C,  v);
        aml->WriteFloat(pGTASA + 0x4692F8,  v);
      #else
        *MaxObjectsDrawDistance = v;
      #endif
    }
}

// Hooks
DECL_HOOKv(LoadLevel_LoadingScreen, const char *a1, const char *a2, const char *a3)
{
    LoadLevel_LoadingScreen(a1, a2, a3);
    LoadDatFile();
}
DECL_HOOKv(RenderEffects_MovingThings)
{
    RenderEffects_MovingThings();
    if(bRenderLodLights)
    {
        RenderBufferedLODLights();
    }
    if(bRenderSearchlightEffects)
    {
        // FPS killer feature. We're not ready for this on our smartphones...
        // CSearchLights::RenderSearchLightsSA();
    }
}
DECL_HOOK(CEntity*, LoadObjectInstance, void* a1, char const* a2)
{
    CEntity* ourEntity = LoadObjectInstance(a1, a2);
    if(bCatchLamppostsNow && ourEntity && ourEntity->m_nInterior == 0 && CSearchLights::IsModelALamppost(ourEntity->GetModelIndex()))
    {
        CSearchLights::RegisterLamppost(ourEntity);
    }
    return ourEntity;
}
DECL_HOOKv(RegisterCorona_FarClip, uintptr_t id, CEntity *entity, uint8_t r, uint8_t g, uint8_t b, uint8_t alpha, CVector *pos, float radius, float farClip, void *texture, char flare, char enableReflection, char checkObstacles, int notUsed, float angle, char longDistance, float nearClip, char fadeState, float fadeSpeed, char onlyFromBelow, char reflectionDelay)
{
    //                                                              \/\/\/
    RegisterCorona_FarClip(id, entity, r, g, b, alpha, pos, radius, 500.0f, texture, flare, enableReflection, checkObstacles, notUsed, angle, longDistance, nearClip, fadeState, fadeSpeed, onlyFromBelow, reflectionDelay);
}
DECL_HOOKv(GameInit2_CranesInit)
{
    GameInit2_CranesInit();

    // CLodLights::Init
    if (LODLightsCoronas.size() != numCoronas)
    {
        LODLightsLinkedList.resize(numCoronas);
        LODLightsCoronas.resize(numCoronas);

        // Initialise the lists
        LODLightsFreeList.Init();
        LODLightsUsedList.Init();

        for (size_t i = 0; i < numCoronas; ++i)
        {
            LODLightsLinkedList[i].Add(&LODLightsFreeList);
            LODLightsLinkedList[i].SetEntry(&LODLightsCoronas[i]);
        }
    }
}
DECL_HOOKv(GameInit_LoadingScreen, const char *a1, const char *a2, const char *a3)
{
    GameInit_LoadingScreen(a1, a2, a3);
    RegisterCustomCoronas();
    bCatchLamppostsNow = false;
    m_pLampposts->shrink_to_fit();
    pFileContent->clear();
}
DECL_HOOKv(GameProcess_BridgesUpdate)
{
    GameProcess_BridgesUpdate();
    RegisterLODLights();
}
DECL_HOOKv(Idle_DebugDisplayTextBuffer)
{
    Idle_DebugDisplayTextBuffer();
    DrawDistanceChanger();
}
DECL_HOOKv(DrawDistance_SetCamFarClip, RwCamera* cam, float dist)
{
    DrawDistance_SetCamFarClip(cam, fNewFarClip);
}
DECL_HOOKv(LoadTimeObject_SetTexDictionary, CBaseModelInfo *mi, void *txdStore, const char *modelCdName)
{
    LoadTimeObject_SetTexDictionary(mi, txdStore, modelCdName);
    if(fTimedObjectsDrawDistance || (fNeonsDrawDistance && strstr(mi->m_szModelName, "neon") != NULL))
    {
        if(fTimedObjectsDrawDistance <= 10.0f) mi->m_fDrawDistance *= fTimedObjectsDrawDistance;
        else mi->m_fDrawDistance = fTimedObjectsDrawDistance;
    }
}
CBaseModelInfo* drawdistHackMi = NULL;
DECL_HOOK(CDamageAtomicModelInfo*, LoadObject_AddDamageAtomicModel, int modelId)
{
    CDamageAtomicModelInfo* mi = LoadObject_AddDamageAtomicModel(modelId);
    drawdistHackMi = (CBaseModelInfo*)mi;
    return mi;
}
DECL_HOOK(CAtomicModelInfo*, LoadObject_AddAtomicModel, int modelId)
{
    CAtomicModelInfo* mi = LoadObject_AddAtomicModel(modelId);
    drawdistHackMi = (CBaseModelInfo*)mi;
    return mi;
}
DECL_HOOK(const char*, LoadObject_GetModelCDName, int modelId)
{
    if (fVegetationDrawDistance && (modelId >= 615 && modelId <= 792 && drawdistHackMi->m_fDrawDistance <= 300.0f))
    {
        if (fVegetationDrawDistance <= 10.0f) drawdistHackMi->m_fDrawDistance *= fVegetationDrawDistance;
        else drawdistHackMi->m_fDrawDistance = fVegetationDrawDistance;
        SetMaxDrawDistanceForNormalObjects(drawdistHackMi->m_fDrawDistance);
    }
    else if (drawdistHackMi->m_fDrawDistance > 300.0f)
    {
        if (fLODObjectsDrawDistance)
        {
            if (fLODObjectsDrawDistance <= 10.0f) drawdistHackMi->m_fDrawDistance *= fLODObjectsDrawDistance;
            else drawdistHackMi->m_fDrawDistance = fLODObjectsDrawDistance;
        }
    }
    else
    {
        if (fGenericObjectsDrawDistance)
        {
            if (modelId >= 615 && modelId <= 1572)
            {
                if (fGenericObjectsDrawDistance <= 10.0f) drawdistHackMi->m_fDrawDistance *= fGenericObjectsDrawDistance;
                else drawdistHackMi->m_fDrawDistance = fGenericObjectsDrawDistance;
            }
        }
        else if (fAllNormalObjectsDrawDistance)
        {
            if (fAllNormalObjectsDrawDistance <= 10.0f) drawdistHackMi->m_fDrawDistance *= fAllNormalObjectsDrawDistance;
            else drawdistHackMi->m_fDrawDistance = fAllNormalObjectsDrawDistance;
        }
        SetMaxDrawDistanceForNormalObjects(drawdistHackMi->m_fDrawDistance);
    }
    return LoadObject_GetModelCDName(modelId);
}
DECL_HOOKb(GenericLoad_IplStoreLoad)
{
    GenericLoad_IplStoreLoad();

    // Load Binary IPLs
    static std::vector<std::string> IPLStreamNames = { "LAE_STREAM0", "LAE_STREAM1", "LAE_STREAM2", "LAE_STREAM3", "LAE_STREAM4", "LAE_STREAM5",
                                                       "LAE2_STREAM0", "LAE2_STREAM1", "LAE2_STREAM2", "LAE2_STREAM3", "LAE2_STREAM4", "LAE2_STREAM5", "LAE2_STREAM6", "LAHILLS_STREAM0",
                                                       "LAHILLS_STREAM1", "LAHILLS_STREAM2", "LAHILLS_STREAM3", "LAHILLS_STREAM4", "LAN_STREAM0", "LAN_STREAM1", "LAN_STREAM2", "LAN2_STREAM0",
                                                       "LAN2_STREAM1", "LAN2_STREAM2", "LAN2_STREAM3", "LAS_STREAM0", "LAS_STREAM1", "LAS_STREAM2", "LAS_STREAM3", "LAS_STREAM4", "LAS_STREAM5",
                                                       "LAS2_STREAM0", "LAS2_STREAM1", "LAS2_STREAM2", "LAS2_STREAM3", "LAS2_STREAM4", "LAW_STREAM0", "LAW_STREAM1", "LAW_STREAM2", "LAW_STREAM3", "LAW_STREAM4",

                                                       "LAW_STREAM5", "LAW2_STREAM0", "LAW2_STREAM1", "LAW2_STREAM2", "LAW2_STREAM3", "LAW2_STREAM4", "LAWN_STREAM0", "LAWN_STREAM1", "LAWN_STREAM2", "LAWN_STREAM3",

                                                       "COUNTN2_STREAM0", "COUNTN2_STREAM1", "COUNTN2_STREAM2", "COUNTN2_STREAM3", "COUNTN2_STREAM4", "COUNTN2_STREAM5", "COUNTN2_STREAM6", "COUNTN2_STREAM7", "COUNTN2_STREAM8",

                                                       "COUNTRYE_STREAM0", "COUNTRYE_STREAM1", "COUNTRYE_STREAM2", "COUNTRYE_STREAM3", "COUNTRYE_STREAM4", "COUNTRYE_STREAM5", "COUNTRYE_STREAM6", "COUNTRYE_STREAM7", "COUNTRYE_STREAM8",
                                                       "COUNTRYE_STREAM9", "COUNTRYN_STREAM0", "COUNTRYN_STREAM1", "COUNTRYN_STREAM2", "COUNTRYN_STREAM3", "COUNTRYS_STREAM0", "COUNTRYS_STREAM1", "COUNTRYS_STREAM2", "COUNTRYS_STREAM3", "COUNTRYS_STREAM4",
                                                       "COUNTRYW_STREAM0", "COUNTRYW_STREAM1", "COUNTRYW_STREAM2", "COUNTRYW_STREAM3", "COUNTRYW_STREAM4", "COUNTRYW_STREAM5", "COUNTRYW_STREAM6", "COUNTRYW_STREAM7", "COUNTRYW_STREAM8", "SFE_STREAM0",
                                                       "SFE_STREAM1", "SFE_STREAM2", "SFE_STREAM3", "SFN_STREAM0", "SFN_STREAM1", "SFN_STREAM2", "SFS_STREAM0", "SFS_STREAM1", "SFS_STREAM2", "SFS_STREAM3", "SFS_STREAM4", "SFS_STREAM5", "SFS_STREAM6",
                                                       "SFS_STREAM7", "SFS_STREAM8", "SFSE_STREAM0", "SFSE_STREAM1", "SFSE_STREAM2", "SFSE_STREAM3", "SFSE_STREAM4", "SFSE_STREAM5", "SFSE_STREAM6", "SFW_STREAM0", "SFW_STREAM1", "SFW_STREAM2", "SFW_STREAM3",
                                                       "SFW_STREAM4", "SFW_STREAM5", "VEGASE_STREAM0", "VEGASE_STREAM1", "VEGASE_STREAM2", "VEGASE_STREAM3", "VEGASE_STREAM4", "VEGASE_STREAM5", "VEGASE_STREAM6", "VEGASE_STREAM7", "VEGASE_STREAM8",
                                                       "VEGASN_STREAM0", "VEGASN_STREAM1", "VEGASN_STREAM2", "VEGASN_STREAM3", "VEGASN_STREAM4", "VEGASN_STREAM5", "VEGASN_STREAM6", "VEGASN_STREAM7", "VEGASN_STREAM8", "VEGASS_STREAM0", "VEGASS_STREAM1",
                                                       "VEGASS_STREAM2", "VEGASS_STREAM3", "VEGASS_STREAM4", "VEGASS_STREAM5", "VEGASW_STREAM0", "VEGASW_STREAM1", "VEGASW_STREAM2", "VEGASW_STREAM3", "VEGASW_STREAM4", "VEGASW_STREAM5", "VEGASW_STREAM6",
                                                       "VEGASW_STREAM7", "VEGASW_STREAM8", "VEGASW_STREAM9"
                                                     };

  #ifdef AML32
    aml->Write8(pGTASA + 0x281FE4, 0x00); // CIplStore::RequestIplAndIgnore -> m_bDisableDynamicStreaming = 0;
  #else
    aml->Write32(pGTASA + 0x33D258, 0x2A1F03E8); // CIplStore::RequestIplAndIgnore -> m_bDisableDynamicStreaming = 0;
  #endif
    auto cend = IPLStreamNames.cend();
    for (auto it = IPLStreamNames.cbegin(); it != cend; ++it)
    {
        int iplSlot = FindIplSlot(it->c_str());
        if (iplSlot >= 0) RequestIplAndIgnore(iplSlot);
    }
  #ifdef AML32
    aml->Write8(pGTASA + 0x281FE4, 0x01); // CIplStore::RequestIplAndIgnore -> m_bDisableDynamicStreaming = 1;
  #else
    aml->Write32(pGTASA + 0x33D258, 0x52800028); // CIplStore::RequestIplAndIgnore -> m_bDisableDynamicStreaming = 1;
  #endif

    return true;
}
DECL_HOOKv(GameInit_StartTestScript)
{
    GameInit_StartTestScript();

    std::vector<uint16_t> lods_id(lods.size());
    std::transform(lods.begin(), lods.end(), lods_id.begin(), [](CEntity* entity)
    {
        return entity->m_nModelIndex;
    });
    std::for_each(lods_id.begin(), std::unique(lods_id.begin(), lods_id.end()), [](uint16_t id) { RequestModel(id, eStreamingFlags::STREAMING_GAME_REQUIRED); });
    LoadAllRequestedModels(false);

    // i just did it (created this line, it's never called)
    // Instantiate all lod entities RwObject
    if (false)
    {
        std::for_each(lods.begin(), lods.end(), [](CEntity* entity)
        {
            if (entity->m_pRwObject == NULL) entity->CreateRwObject();
        });
    }
}
DECL_HOOKv(CoronasRegisterFestiveCoronaForEntity, uintptr_t nID, CEntity* entity, unsigned char R, unsigned char G, unsigned char B, unsigned char A, const CVector& Position, float Size, float Range, RwTexture* pTex, char flare, char enableReflection, char checkObstacles, int notUsed, float angle, char longDistance, float nearClip, char fadeState, float fadeSpeed, char onlyFromBelow, char reflectionDelay)
{
    auto it = LODLightsFestiveLights.find(nID);
    if (it != LODLightsFestiveLights.end())
    {
        CRGBA color = it->second;
        CoronasRegisterFestiveCoronaForEntity(nID, entity, color.r, color.g, color.b, A, Position, Size, Range, pTex, flare, enableReflection, checkObstacles, notUsed, angle, longDistance, nearClip, fadeState, fadeSpeed, onlyFromBelow, reflectionDelay);
    }
    else
    {
        LODLightsFestiveLights[nID] = CRGBA(rand() % 256, rand() % 256, rand() % 256, 0);
        CoronasRegisterFestiveCoronaForEntity(nID, entity, R, G, B, A, Position, Size, Range, pTex, flare, enableReflection, checkObstacles, notUsed, angle, longDistance, nearClip, fadeState, fadeSpeed, onlyFromBelow, reflectionDelay);
    }
}

// Patches
uintptr_t LoadScene_BackTo;
extern "C" uintptr_t LoadScene_Patch(CEntity *entity)
{
    if(entity && entity->m_pLod) lods.push_back(entity->m_pLod);
    return LoadScene_BackTo;
}
#ifdef AML32
__attribute__((optnone)) __attribute__((naked)) void LoadScene_Inject(void)
{
    asm volatile(
        "LDR.W R6, [R0, R5, LSL#2]\n" // org
        "PUSH {R0-R3}\n"
        "MOV R0, R6\n"
        "BL LoadScene_Patch\n"
        "MOV R11, R0\n"
        "POP {R0-R3}\n"
        "LDRB.W R0, [R6, #0x38]\n" // org
        "MOV PC, R11");
}

uintptr_t StaticShadowsDrawDistance_BackTo1, StaticShadowsDrawDistance_BackTo2, StaticShadowsDrawDistance_BackTo3, StaticShadowsDrawDistance_BackTo4, StaticShadowsDrawDistance_BackTo5, StaticShadowsDrawDistance_BackTo6;
__attribute__((optnone)) __attribute__((naked)) void StaticShadowsDrawDistance_Inject1(void)
{
    asm volatile(
        "MOV R9, %0\n"
    :: "r" (fStaticShadowsDrawDistance));
    asm volatile(
        "MOV R12, %0\n"
    :: "r" (StaticShadowsDrawDistance_BackTo1));
    asm volatile(
        "LDR R0, [R3, #0x2C]\n" // org
        "MOV PC, R12");
}
__attribute__((optnone)) __attribute__((naked)) void StaticShadowsDrawDistance_Inject2(void)
{
    asm volatile(
        "MOV R12, %0\n"
    :: "r" (StaticShadowsDrawDistance_BackTo2));
    asm volatile(
        "MOV R0, %0\n"
    :: "r" (fStaticShadowsDrawDistance));
    asm volatile(
        "STR R0, [SP, #0x28]\n" // org
        "MOV PC, R12");
}
__attribute__((optnone)) __attribute__((naked)) void StaticShadowsDrawDistance_Inject3(void)
{
    asm volatile(
        "MOV R12, %0\n"
    :: "r" (StaticShadowsDrawDistance_BackTo3));
    asm volatile(
        "MOV R1, %0\n"
    :: "r" (fStaticShadowsDrawDistance));
    asm volatile(
        "LDR R0, [R3, #0xC]\n" // org
        "STR R0, [SP, #0x50]\n" // org
        "VLDR S4, [SP, #0x50]\n" // org
        "MOV PC, R12");
}
__attribute__((optnone)) __attribute__((naked)) void StaticShadowsDrawDistance_Inject4(void)
{
    asm volatile(
        "VCVT.F32.U32 S6, S6\n" // org
        "PUSH {R0}\n"
    );
    asm volatile(
        "MOV R12, %0\n"
    :: "r" (StaticShadowsDrawDistance_BackTo4));
    asm volatile(
        "MOV R1, %0\n"
    :: "r" (fStaticShadowsDrawDistance));
    asm volatile(
        "POP {R0}\n"
        "MOV PC, R12");
}
__attribute__((optnone)) __attribute__((naked)) void StaticShadowsDrawDistance_Inject5(void)
{
    asm volatile(
        "MOV R12, %0\n"
    :: "r" (StaticShadowsDrawDistance_BackTo5));
    asm volatile(
        "MOV R0, %0\n"
    :: "r" (fStaticShadowsDrawDistance));
    asm volatile(
        "STRD R3, R1, [SP, #0x20]\n" // org
        "MOV PC, R12");
}
__attribute__((optnone)) __attribute__((naked)) void StaticShadowsDrawDistance_Inject6(void)
{
    asm volatile(
        "ADD.W R8, SP, #0x48\n" // org
        "PUSH {R0}\n"
    );
    asm volatile(
        "MOV R12, %0\n"
    :: "r" (StaticShadowsDrawDistance_BackTo6));
    asm volatile(
        "MOV R10, %0\n"
    :: "r" (fStaticShadowsDrawDistance));
    asm volatile(
        "POP {R0}\n"
        "MOV PC, R12");
}
#else
__attribute__((optnone)) __attribute__((naked)) void LoadScene_Inject(void)
{
    asm volatile(
        "LDR X8, [X27, X8, LSL#3]\n" // org
        "LDR S0, [X8, #0x38]\n" // org
        "FMUL S0, S0, S1\n" // org
        "FCMP S0, S8\n" // org
        "MOV X0, X23\n"
        "BL LoadScene_Patch\n"
        "BR X0");
}
#endif

/// Main
extern "C" void OnAllModsLoaded()
{
    logger->SetTag("Project2DFX");
    pGTASA = aml->GetLib("libGTASA.so");
    hGTASA = aml->GetLibHandle("libGTASA.so");
    cfg = new Config("SALodLights");

    // Config
    bRenderLodLights = cfg->GetBool("RenderLodLights", true, "LodLights");
    numCoronas = cfg->GetInt("MaxNumberOfLodLights", 18000, "LodLights");
    fCoronaRadiusMultiplier = cfg->GetFloat("CoronaRadiusMultiplier", 1.0f, "LodLights");
    bSlightlyIncreaseRadiusWithDistance = cfg->GetBool("SlightlyIncreaseRadiusWithDistance", 1, "LodLights");
    if(!strcasecmp(cfg->GetString("CoronaFarClip", "auto", "LodLights"), "auto"))
    {
        fCoronaFarClip = cfg->GetFloat("CoronaFarClip", 0.0f, "LodLights");
        fCoronaFarClipSQR = fCoronaFarClip * fCoronaFarClip;
        if(fCoronaFarClip <= 0) autoFarClip = true;
    }
    else autoFarClip = true;
    szCustomCoronaTexturePath = cfg->GetString("CustomCoronaTexturePath", ".\\corona.png", "LodLights");

    bRenderStaticShadowsForLODs = cfg->GetBool("RenderStaticShadowsForLODs", 0, "StaticShadows");
    bIncreasePedsCarsShadowsDrawDistance = cfg->GetBool("IncreaseCarsShadowsDrawDistance", 0, "StaticShadows");
    fStaticShadowsIntensity = cfg->GetFloat("StaticShadowsIntensity", 0.0f, "StaticShadows");
    fStaticShadowsDrawDistance = cfg->GetFloat("StaticShadowsDrawDistance", 0.0f, "StaticShadows");
    fTrafficLightsShadowsIntensity = cfg->GetFloat("TrafficLightsShadowsIntensity", 0.0f, "StaticShadows");
    fTrafficLightsShadowsDrawDistance = cfg->GetFloat("TrafficLightsShadowsDrawDistance", 0.0f, "StaticShadows");

    bRenderSearchlightEffects = cfg->GetBool("RenderSearchlightEffects", 1, "SearchLights");
    bRenderOnlyDuringFoggyWeather = cfg->GetBool("RenderOnlyDuringFoggyWeather", 0, "SearchLights");
    fSearchlightEffectVisibilityFactor = cfg->GetFloat("SearchlightEffectVisibilityFactor", 0.4f, "SearchLights");
    nSmoothEffect = cfg->GetInt("SmoothEffect", 1, "SearchLights");

    bEnableDrawDistanceChanger = cfg->GetBool("Enable", 0, "DrawDistanceChanger");
    fMinDrawDistanceOnTheGround = cfg->GetFloat("MinDrawDistanceOnTheGround", 800.0f, "DrawDistanceChanger");
    fFactor1 = cfg->GetFloat("Factor1", 2.0f, "DrawDistanceChanger");
    fFactor2 = cfg->GetFloat("Factor2", 1.0f, "DrawDistanceChanger");
    fStaticSunSize = cfg->GetFloat("StaticSunSize", 20.0f, "DrawDistanceChanger");

    bAdaptiveDrawDistanceEnabled = cfg->GetBool("Enable", 0, "AdaptiveDrawDistance");
    nMinFPSValue = cfg->GetInt("MinFPSValue", 0, "AdaptiveDrawDistance");
    nMaxFPSValue = cfg->GetInt("MaxFPSValue", 0, "AdaptiveDrawDistance");
    fMaxPossibleDrawDistance = cfg->GetFloat("MaxPossibleDrawDistance", 0.0f, "AdaptiveDrawDistance");

    fMaxDrawDistanceForNormalObjects = cfg->GetFloat("MaxDrawDistanceForNormalObjects", 300.0f, "IDETweaker");
    fTimedObjectsDrawDistance = cfg->GetFloat("TimedObjectsDrawDistance", 0.0f, "IDETweaker");
    fNeonsDrawDistance = cfg->GetFloat("NeonsDrawDistance", 0.0f, "IDETweaker");
    fLODObjectsDrawDistance = cfg->GetFloat("LODObjectsDrawDistance", 0.0f, "IDETweaker");
    fGenericObjectsDrawDistance = cfg->GetFloat("GenericObjectsDrawDistance", 0.0f, "IDETweaker");
    fAllNormalObjectsDrawDistance = cfg->GetFloat("AllNormalObjectsDrawDistance", 0.0f, "IDETweaker");
    fVegetationDrawDistance = cfg->GetFloat("VegetationDrawDistance", 0.0f, "IDETweaker");
    bLoadAllBinaryIPLs = cfg->GetBool("LoadAllBinaryIPLs", 0, "IDETweaker");
    bPreloadLODs = cfg->GetBool("PreloadLODs", 0, "IDETweaker");

    bFestiveLights = cfg->GetBool("FestiveLights", 1, "Misc");
    bFestiveLightsAlways = cfg->GetBool("bFestiveLightsAlways", 0, "Misc");

    // Getters
    SET_TO(ms_modelInfoPtrs,        *(uintptr_t*)(pGTASA + BYBIT(0x6796D4, 0x850DB8)));
    SET_TO(nActiveInterior,         aml->GetSym(hGTASA, "_ZN5CGame8currAreaE"));
    SET_TO(flWeatherFoggyness,      aml->GetSym(hGTASA, "_ZN8CWeather9FoggynessE"));
    SET_TO(ms_fTimeStep,            aml->GetSym(hGTASA, "_ZN6CTimer12ms_fTimeStepE"));
    SET_TO(ms_fFarClipZ,            aml->GetSym(hGTASA, "_ZN5CDraw12ms_fFarClipZE"));
    SET_TO(ms_fNearClipZ,           aml->GetSym(hGTASA, "_ZN5CDraw13ms_fNearClipZE"));
    SET_TO(ms_fFOV,                 aml->GetSym(hGTASA, "_ZN5CDraw7ms_fFOVE"));
    SET_TO(TheCamera,               aml->GetSym(hGTASA, "TheCamera"));
    SET_TO(gpCoronaTexture,         aml->GetSym(hGTASA, "gpCoronaTexture"));
    SET_TO(gpShadowExplosionTex,    aml->GetSym(hGTASA, "gpShadowExplosionTex"));
    SET_TO(RsGlobal,                aml->GetSym(hGTASA, "RsGlobal"));
    SET_TO(m_CurrentColours,        aml->GetSym(hGTASA, "_ZN10CTimeCycle16m_CurrentColoursE"));
    SET_TO(CurrentTimeHours,        aml->GetSym(hGTASA, "_ZN6CClock18ms_nGameClockHoursE"));
    SET_TO(CurrentTimeMinutes,      aml->GetSym(hGTASA, "_ZN6CClock20ms_nGameClockMinutesE"));
    SET_TO(m_snTimeInMillisecondsPauseMode, aml->GetSym(hGTASA, "_ZN6CTimer31m_snTimeInMillisecondsPauseModeE"));
    SET_TO(UnderWaterness,          aml->GetSym(hGTASA, "_ZN8CWeather14UnderWaternessE"));
    SET_TO(game_FPS,                aml->GetSym(hGTASA, "_ZN6CTimer8game_FPSE"));
    SET_TO(ms_fFarClipPlane,        aml->GetSym(hGTASA, "_ZN9CRenderer16ms_fFarClipPlaneE"));

    SET_TO(GetIsTimeInRange,        aml->GetSym(hGTASA, "_ZN6CClock16GetIsTimeInRangeEhh"));
    SET_TO(FindGroundZFor3DCoord,   aml->GetSym(hGTASA, "_ZN6CWorld21FindGroundZFor3DCoordEfffPbPP7CEntity"));
    SET_TO(OpenFile,                aml->GetSym(hGTASA, "_ZN8CFileMgr8OpenFileEPKcS1_"));
    SET_TO(CloseFile,               aml->GetSym(hGTASA, BYBIT("_ZN8CFileMgr9CloseFileEj", "_ZN8CFileMgr9CloseFileEy")));
    SET_TO(LoadLine,                aml->GetSym(hGTASA, BYBIT("_ZN11CFileLoader8LoadLineEj", "_ZN11CFileLoader8LoadLineEy")));
    SET_TO(GetModelInfoUInt16,      aml->GetSym(hGTASA, "_ZN10CModelInfo18GetModelInfoUInt16EPKcPt"));
    SET_TO(FindIplSlot,             aml->GetSym(hGTASA, "_ZN9CIplStore11FindIplSlotEPKc"));
    SET_TO(RequestIplAndIgnore,     aml->GetSym(hGTASA, "_ZN9CIplStore19RequestIplAndIgnoreEi"));
    SET_TO(RequestModel,            aml->GetSym(hGTASA, "_ZN10CStreaming12RequestModelEii"));
    SET_TO(LoadAllRequestedModels,  aml->GetSym(hGTASA, "_ZN10CStreaming22LoadAllRequestedModelsEb"));
    SET_TO(RwRenderStateSet,        aml->GetSym(hGTASA, "_Z16RwRenderStateSet13RwRenderStatePv"));
    SET_TO(FlushSpriteBuffer,       aml->GetSym(hGTASA, "_ZN7CSprite17FlushSpriteBufferEv"));
    SET_TO(CalcScreenCoors,         aml->GetSym(hGTASA, "_ZN7CSprite15CalcScreenCoorsERK5RwV3dPS0_PfS4_bb"));
    SET_TO(RenderBufferedOneXLUSprite_Rotate_Aspect, aml->GetSym(hGTASA, "_ZN7CSprite40RenderBufferedOneXLUSprite_Rotate_AspectEfffffhhhsffh"));
    SET_TO(StoreStaticShadow,       aml->GetSym(hGTASA, BYBIT("_ZN8CShadows17StoreStaticShadowEjhP9RwTextureP7CVectorffffshhhfffbf", "_ZN8CShadows17StoreStaticShadowEyhP9RwTextureP7CVectorffffshhhfffbf")));
    SET_TO(FindPlayerPed,           aml->GetSym(hGTASA, "_Z13FindPlayerPedi"));
    SET_TO(Pre_SearchLightCone,     aml->GetSym(hGTASA, "_ZN5CHeli19Pre_SearchLightConeEv"));
    SET_TO(Post_SearchLightCone,    aml->GetSym(hGTASA, "_ZN5CHeli20Post_SearchLightConeEv"));
    SET_TO(RtPNGImageRead,          aml->GetSym(hGTASA, "_Z14RtPNGImageReadPKc"));
    SET_TO(RwImageFindRasterFormat, aml->GetSym(hGTASA, "_Z23RwImageFindRasterFormatP7RwImageiPiS1_S1_S1_"));
    SET_TO(RwRasterCreate,          aml->GetSym(hGTASA, "_Z14RwRasterCreateiiii"));
    SET_TO(RwRasterSetFromImage,    aml->GetSym(hGTASA, "_Z20RwRasterSetFromImageP8RwRasterP7RwImage"));
    SET_TO(RwTextureCreate,         aml->GetSym(hGTASA, "_Z15RwTextureCreateP8RwRaster"));
    SET_TO(RwImageDestroy,          aml->GetSym(hGTASA, "_Z14RwImageDestroyP7RwImage"));
    SET_TO(ProcessLineOfSight,      aml->GetSym(hGTASA, "_ZN6CWorld18ProcessLineOfSightERK7CVectorS2_R9CColPointRP7CEntitybbbbbbbb"));
    SET_TO(RwIm3DTransform,         aml->GetSym(hGTASA, "_Z15RwIm3DTransformP18RxObjSpace3DVertexjP11RwMatrixTagj"));
    SET_TO(RwIm3DRenderIndexedPrimitive, aml->GetSym(hGTASA, "_Z28RwIm3DRenderIndexedPrimitive15RwPrimitiveTypePti"));
    SET_TO(RwIm3DEnd,               aml->GetSym(hGTASA, "_Z9RwIm3DEndv"));
    SET_TO(MatrixVectorMult,        aml->GetSym(hGTASA, "_ZmlRK7CMatrixRK7CVector"));

    // Hooks
  #ifdef AML32
    HOOKBLX(LoadLevel_LoadingScreen,    pGTASA + 0x466AB2 + 0x1);
    HOOKBLX(RenderEffects_MovingThings, pGTASA + 0x3F6380 + 0x1);
    HOOKBLX(RegisterCorona_FarClip,     pGTASA + 0x5A44E8 + 0x1);
    HOOKBLX(GameInit2_CranesInit,       pGTASA + 0x473022 + 0x1);
  #else
    HOOKBL(LoadLevel_LoadingScreen,     pGTASA + 0x551E20);
    HOOKBL(RenderEffects_MovingThings,  pGTASA + 0x4D88FC);
    HOOKBL(RegisterCorona_FarClip,      pGTASA + 0x6C8438);
    HOOKBL(GameInit2_CranesInit,        pGTASA + 0x55F748);
  #endif

    // Patches
  #ifdef AML32
    aml->Write(pGTASA + 0x362EC8, "\xC4\xF2\x09\x40", 4); // 50.0 -> 548.0 (instead of 550.0 on PC)
    if(*(uint32_t*)(pGTASA + 0x5A3644) == 0x6FB4F5BA) aml->Write(pGTASA + 0x5A3644, "\xBA\xF5\x61\x5F", 4); // Sun reflections
  #else
    aml->Write32(pGTASA + 0x432DF0, 0x90001868); // 50.0 -> 550.0 (1)
    aml->Write32(pGTASA + 0x432DF8, 0xBD4B8508); // 50.0 -> 550.0 (2)
    if(*(uint32_t*)(pGTASA + 0x6C6E80) == 0x7100B75F) aml->Write32(pGTASA + 0x6C6E80, 0x7106575F); // Sun reflections
  #endif

    // P2DFX Init
    RegisterLODCorona = &RegisterNormalCorona;
    if (bRenderLodLights)
    {
        // CLODLights::Inject();
      #ifdef AML32
        HOOKBLX(GameInit_LoadingScreen, pGTASA + 0x3F3784 + 0x1);
        HOOKPLT(LoadObjectInstance, pGTASA + 0x675E6C);
        HOOKBLX(GameProcess_BridgesUpdate, pGTASA + 0x3F4266 + 0x1);
      #else
        HOOKBL(GameInit_LoadingScreen, pGTASA + 0x4D5ABC);
        HOOKPLT(LoadObjectInstance, pGTASA + 0x849D58);
        HOOKBL(LoadObjectInstance, pGTASA + 0x4D20D4);
        HOOKBL(GameProcess_BridgesUpdate, pGTASA + 0x4D6638);
      #endif
    }
    if (bEnableDrawDistanceChanger)
    {
      #ifdef AML32
        HOOKBLX(Idle_DebugDisplayTextBuffer, pGTASA + 0x3F6C7C + 0x1);

        // "ms_fFarClipPlane = x" part
        aml->PlaceNOP(pGTASA + 0x40EEBE + 0x1, 1);
        aml->PlaceNOP(pGTASA + 0x40F8AE + 0x1, 1);
        aml->PlaceNOP(pGTASA + 0x411BA4 + 0x1, 1);
        aml->PlaceNOP(pGTASA + 0x41220C + 0x1, 1);

        // timecycle farclip part
        HOOKBLX(DrawDistance_SetCamFarClip, pGTASA + 0x3F6B32 + 0x1); // Idle
        HOOKBLX(DrawDistance_SetCamFarClip, pGTASA + 0x3F5E12 + 0x1); // NewTileRenderCB
        HOOKBLX(DrawDistance_SetCamFarClip, pGTASA + 0x3F5E36 + 0x1); // NewTileRenderCB
      #else
        HOOKBL(Idle_DebugDisplayTextBuffer, pGTASA + 0x4D9248);

        // "ms_fFarClipPlane = x" part
        aml->PlaceNOP(pGTASA + 0x4F4374, 1);
        aml->PlaceNOP(pGTASA + 0x4F4CA4, 1);
        aml->PlaceNOP(pGTASA + 0x4F70E4, 1);
        aml->PlaceNOP(pGTASA + 0x4F7794, 1);

        // timecycle farclip part
        HOOKBL(DrawDistance_SetCamFarClip, pGTASA + 0x4D9048); // Idle
        HOOKBL(DrawDistance_SetCamFarClip, pGTASA + 0x4D83C0); // NewTileRenderCB
        HOOKBL(DrawDistance_SetCamFarClip, pGTASA + 0x4D83EC); // NewTileRenderCB
      #endif
    }
    if (fStaticSunSize)
    {
      #ifdef AML32
        aml->WriteFloat(pGTASA + 0x5A4010, fStaticSunSize * 2.7335f);
        aml->Write(pGTASA + 0x5A3F00, "\xB0\xEE\x41\x0A", 4);
      #else
        aml->WriteFloat(pGTASA + 0x7634A4, fStaticSunSize * 2.7335f);
        aml->Write32(pGTASA + 0x6C76F8, 0x1E204020);
      #endif
    }
    if (fStaticShadowsDrawDistance)
    {
        // In JPatch! + code below
      #ifdef AML32
        StaticShadowsDrawDistance_BackTo1 = pGTASA + 0x5BD4A8 + 0x1;
        aml->Redirect(pGTASA + 0x5BD49E + 0x1, (uintptr_t)StaticShadowsDrawDistance_Inject1); // CShadows::UpdatePermanentShadows
        
        StaticShadowsDrawDistance_BackTo2 = pGTASA + 0x5BD58C + 0x1;
        aml->Redirect(pGTASA + 0x5BD584 + 0x1, (uintptr_t)StaticShadowsDrawDistance_Inject2); // CShadows::UpdatePermanentShadows
        
        StaticShadowsDrawDistance_BackTo3 = pGTASA + 0x3F19A2 + 0x1;
        aml->Redirect(pGTASA + 0x3F1996 + 0x1, (uintptr_t)StaticShadowsDrawDistance_Inject3); // CFireManager::Update
        
        StaticShadowsDrawDistance_BackTo4 = pGTASA + 0x363124 + 0x1;
        aml->Redirect(pGTASA + 0x36311C + 0x1, (uintptr_t)StaticShadowsDrawDistance_Inject4); // CTrafficLights::DisplayActualLight
        
        StaticShadowsDrawDistance_BackTo5 = pGTASA + 0x3208FA + 0x1;
        aml->Redirect(pGTASA + 0x3208F0 + 0x1, (uintptr_t)StaticShadowsDrawDistance_Inject5); // CPickups::DoMineEffects
        
        StaticShadowsDrawDistance_BackTo6 = pGTASA + 0x320518 + 0x1;
        aml->Redirect(pGTASA + 0x320510 + 0x1, (uintptr_t)StaticShadowsDrawDistance_Inject6); // CPickups::DoCollectableEffects
      #else
        SET_TO(MaxStaticShadowsDrawDistance, pGTASA + 0x70BD9C);
        aml->Unprot((uintptr_t)MaxStaticShadowsDrawDistance);
        *MaxStaticShadowsDrawDistance = fStaticShadowsDrawDistance;

        aml->Write32(pGTASA + 0x6E1D30, 0xD0000148);
        aml->Write32(pGTASA + 0x6E1D3C, 0xBD4D9D08); // CShadows::UpdatePermanentShadows

        aml->Write32(pGTASA + 0x4D38F8, 0x900011C8);
        aml->Write32(pGTASA + 0x4D3904, 0xBD4D9D08); // CFireManager::Update

        aml->Write32(pGTASA + 0x4331A0, 0x900016C8);
        aml->Write32(pGTASA + 0x4331A4, 0xBD4D9D06); // CTrafficLights::DisplayActualLight

        aml->Write32(pGTASA + 0x3E8510, 0xF0001908);
        aml->Write32(pGTASA + 0x3E8518, 0xBD4D9D06); // CPickups::DoMineEffects

        aml->Write32(pGTASA + 0x3E8040, 0xF0001908);
        aml->Write32(pGTASA + 0x3E8048, 0xBD4D9D08); // CPickups::DoCollectableEffects

        // A part for 'CEntity::ProcessLightsForEntity' is in JPatch. This should be done in JPatch itself.
      #endif
    }
    if (fStaticShadowsIntensity)
    {
        fStaticShadowsIntensity *= 0.00390625f;
        // All patches are in CEntity::ProcessLightsForEntity
        // That means JPatch does it`s work and we cant stop it!
    }
    if (fTimedObjectsDrawDistance || fNeonsDrawDistance)
    {
      #ifdef AML32
        HOOKBLX(LoadTimeObject_SetTexDictionary, pGTASA + 0x4696A2 + 0x1);
      #else
        HOOKBL(LoadTimeObject_SetTexDictionary, pGTASA + 0x554B20);
      #endif
    }
    if (fLODObjectsDrawDistance || fGenericObjectsDrawDistance || fAllNormalObjectsDrawDistance || fVegetationDrawDistance)
    {
      #ifdef AML32
        HOOKBLX(LoadObject_AddDamageAtomicModel, pGTASA + 0x4694E4 + 0x1);
        HOOKBLX(LoadObject_AddAtomicModel, pGTASA + 0x4694DE + 0x1);
        HOOKBLX(LoadObject_GetModelCDName, pGTASA + 0x469506 + 0x1);
      #else
        HOOKBL(LoadObject_AddDamageAtomicModel, pGTASA + 0x5548FC);
        HOOKBL(LoadObject_AddAtomicModel, pGTASA + 0x5548F4);
        HOOKBL(LoadObject_GetModelCDName, pGTASA + 0x554928);

        SET_TO(MaxObjectsDrawDistance, pGTASA + 0x70BD98);
        aml->Unprot((uintptr_t)MaxObjectsDrawDistance);
        *MaxObjectsDrawDistance = 300.0f;

        aml->Write32(pGTASA + 0x4F66A8, 0xB00010A8);
        aml->Write32(pGTASA + 0x4F66B4, 0xBD4D9904);
        aml->Write32(pGTASA + 0x4F66CC, 0x1E242000);
        aml->Write32(pGTASA + 0x4F66EC, 0x1E242020);
        aml->Write32(pGTASA + 0x4F6714, 0x1E214082); // CRenderer::SetupEntityVisibility

        aml->Write32(pGTASA + 0x4F44DC, 0xF00010AB);
        aml->Write32(pGTASA + 0x4F44E4, 0xBD4D9961); // CRenderer::ScanWorld

        aml->Write32(pGTASA + 0x554480, 0xF0000DA8);
        aml->Write32(pGTASA + 0x55448C, 0xBD4D9908); // CFileLoader::LoadScene
      #endif
        if (fGenericObjectsDrawDistance || fAllNormalObjectsDrawDistance || fVegetationDrawDistance)
        {
            // usage of fMaxDrawDistanceForNormalObjects,
            //   doing it in funcs manually (SetMaxDrawDistanceForNormalObjects)
        }
    }
    if (bLoadAllBinaryIPLs)
    {
      #ifdef AML32
        //HOOKBLX(GenericLoad_IplStoreLoad, pGTASA + 0x482E18 + 0x1);
      #else
        //HOOKBL(GenericLoad_IplStoreLoad, pGTASA + 0x574CE8);
      #endif
    }
    if (bPreloadLODs)
    {
      #ifdef AML32
        LoadScene_BackTo = pGTASA + 0x4691A8 + 0x1;
        aml->Redirect(pGTASA + 0x4691A0 + 0x1, (uintptr_t)LoadScene_Inject);
        HOOKBLX(GameInit_StartTestScript, pGTASA + 0x3F3788 + 0x1);
      #else
        LoadScene_BackTo = pGTASA + 0x5544EC;
        aml->Redirect(pGTASA + 0x5544DC, (uintptr_t)LoadScene_Inject);
        HOOKBL(GameInit_StartTestScript, pGTASA + 0x4D5AC0);
      #endif
    }
    if (bFestiveLights)
    {
        auto now = std::chrono::system_clock::now();
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        struct tm *date = std::localtime(&now_c);
        if (bFestiveLightsAlways || (date->tm_mon == 0 && date->tm_mday <= 1) || (date->tm_mon == 11 && date->tm_mday >= 31))
        {
            RegisterLODCorona = &RegisterFestiveCorona;

          #ifdef AML32
            HOOKBLX(CoronasRegisterFestiveCoronaForEntity, pGTASA + 0x5A49A4 + 0x1);
            HOOKBLX(CoronasRegisterFestiveCoronaForEntity, pGTASA + 0x5A44E8 + 0x1);
            HOOKBLX(CoronasRegisterFestiveCoronaForEntity, pGTASA + 0x5A4B88 + 0x1);
            HOOKBLX(CoronasRegisterFestiveCoronaForEntity, pGTASA + 0x3EC6C2 + 0x1);
            HOOKBLX(CoronasRegisterFestiveCoronaForEntity, pGTASA + 0x3EC7F0 + 0x1);
          #else
            HOOKBL(CoronasRegisterFestiveCoronaForEntity, pGTASA + 0x6C7ED8);
            HOOKBL(CoronasRegisterFestiveCoronaForEntity, pGTASA + 0x6C80E8);
            HOOKBL(CoronasRegisterFestiveCoronaForEntity, pGTASA + 0x6C8438);
            HOOKBL(CoronasRegisterFestiveCoronaForEntity, pGTASA + 0x4CC490);
          #endif
        }
    }

    // NearClip patch
  #ifdef AML32
    aml->Write(pGTASA + 0x3F60EA, "\xB0\xEE\x49\x0A", 4);
  #else
    aml->Write32(pGTASA + 0x4D8664, 0x1E204120);
  #endif
  
    Init_MiniLA();
}