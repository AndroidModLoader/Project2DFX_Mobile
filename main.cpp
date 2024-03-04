#include <mod/amlmod.h>
#include <mod/logger.h>
#include <mod/config.h>
#include <string>

#ifdef AML32
    #include "GTASA_STRUCTS.h"
    #define BYVER(__for32, __for64) (__for32)
#else
    #include "GTASA_STRUCTS_210.h"
    #define BYVER(__for32, __for64) (__for64)
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
    uint32_t    Identifier;             // Should be unique for each corona. Address or something (0 = empty)
    RwTexture*	pTex;                   // Pointer to the actual texture to be rendered
    float       Size;                   // How big is this fellow
    float       NormalAngle;            // Is corona normal (if relevant) facing the camera?
    float       Range;                  // How far away is this guy still visible
    float       PullTowardsCam;         // How far away is the z value pulled towards camera.
    float       HeightAboveGround;      // Stired so that we don't have to do a ProcessVerticalLine every frame
                                        // The following fields are used for trails behind coronas (glowy lights)
    float       FadeSpeed;              // The speed the corona fades in and out ##SA##
    uint8_t     Red, Green, Blue;       // Rendering colour.
    uint8_t     Intensity;              // 255 = full
    uint8_t     FadedIntensity;         // Intensity that lags behind the given intenisty and fades out if the LOS is blocked
    bool        RegisteredThisFrame;    // Has this guy been registered by game code this frame
    bool        FlareType;              // What type of flare to render
    bool        ReflectionType;         // What type of reflection during wet weather
    bool        LOSCheck : 1;           // Do we check the LOS or do we render at the right Z value
    bool        OffScreen : 1;          // Set by the rendering code to be used by the update code
    bool        JustCreated;            // If this guy has been created this frame we won't delete it (It hasn't had the time to get its OffScreen cleared) ##SA removed from packed byte ##
    bool        NeonFade : 1;           // Does the guy fade out when closer to cam
    bool        OnlyFromBelow : 1;      // This corona is only visible if the camera is below it. ##SA##
    bool        bHasValidHeightAboveGround : 1;
    bool        WhiteCore : 1;          // This corona rendered with a small white core.
    bool        bIsAttachedToEntity : 1;

    CEntity*    pEntityAttachedTo;

    CLODRegisteredCorona()
		: Identifier(0), pEntityAttachedTo(nullptr)
	{}
    void Update()
    {
        if (!RegisteredThisFrame)
        {
            Intensity = 0;
        }
        if (!Intensity && !JustCreated)
        {
            Identifier = 0;
        }
        JustCreated = 0;
        RegisteredThisFrame = 0;
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
        { pNext->pPrev = pPrev; pPrev->pNext = pNext; pNext = nullptr; }

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
        { return pNext == this ? nullptr : pNext; }
};

// Init
MYMOD(net.thirteenag.rusjj.gtasa.2dfx, Project 2DFX, 1.0, ThirteenAG & RusJJ)
//NEEDGAME(com.rockstargames.gtasa)
BEGIN_DEPLIST()
    ADD_DEPENDENCY_VER(net.rusjj.aml, 1.2.1)
END_DEPLIST()

Config*             cfg;
uintptr_t           pGTASA;
void*               hGTASA;

// Org Vars
CBaseModelInfo**    ms_modelInfoPtrs;
int*                nActiveInterior;
float               *flWeatherFoggyness, *ms_fTimeStep, *ms_fFarClipZ;
CCamera*            TheCamera;
RwTexture**         gpCoronaTexture;
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
int                 (*OpenFile)(const char* filepath, const char* mode);
void                (*CloseFile)(int fd);
const char*         (*LoadLine)(int fd);
CBaseModelInfo*     (*GetModelInfoUInt16)(const char* modelName, uint16_t *index);
int                 (*FindIplSlot)(const char* name);
void                (*RequestIplAndIgnore)(int iplSlot);
void                (*RequestModel)(int modelId, eStreamingFlags flag);
void                (*LoadAllRequestedModels)(bool bOnlyPriorityRequests);
void                (*RegisterLODCorona)(unsigned int nID, CEntity *pAttachTo, unsigned char R, unsigned char G, unsigned char B, unsigned char A, const CVector& Position, float Size, float Range, unsigned char coronaType, unsigned char flareType, bool enableReflection, bool checkObstacles, int unused, float normalAngle, bool longDistance, float nearClip, unsigned char bFadeIntensity, float FadeSpeed, bool bOnlyFromBelow, bool reflectionDelay);
void                (*RwRenderStateSet)(RwRenderState, void*);
void                (*FlushSpriteBuffer)();
bool                (*CalcScreenCoors)(CVector*, CVector*, float*, float*, bool, bool);
void                (*RenderBufferedOneXLUSprite_Rotate_Aspect)(float,float,float,float,float,uint8_t,uint8_t,uint8_t,short,float,float,uint8_t);
CPlayerPed*         (*FindPlayerPed)(int playerId);
// Vars
std::map<unsigned int, CLODLightsLinkedListNode*> LODLightsUsedMap;
CLODLightsLinkedListNode LODLightsFreeList, LODLightsUsedList;
std::vector<CLODLightsLinkedListNode> LODLightsLinkedList;
std::vector<CLODRegisteredCorona> LODLightsCoronas;
std::map<unsigned int, CRGBA> LODLightsFestiveLights;
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
float               fNewFarClip, fMaxPossibleDrawDistance;
float               fMaxDrawDistanceForNormalObjects = 300.0f, fTimedObjectsDrawDistance, fNeonsDrawDistance, fLODObjectsDrawDistance;
float               fGenericObjectsDrawDistance, fAllNormalObjectsDrawDistance, fVegetationDrawDistance;
bool                bLoadAllBinaryIPLs, bPreloadLODs;
float               fDrawDistance;
bool                bRandomExplosionEffects, bReplaceSmokeTrailWithBulletTrail, bFestiveLights, bFestiveLightsAlways;

// Funcs
void RenderBufferedLODLights()
{
    int nWidth = RsGlobal->maximumWidth;
    int nHeight = RsGlobal->maximumHeight;

    RwRaster* pLastRaster = NULL;
    bool bLastZWriteRenderState = true;
    CVector* pCamPos = &TheCamera->GetPosition();

    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, (void*)false);
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)true);
    RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDONE);
    RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDONE);
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE, (void*)true);

    int siz = LODLightsCoronas.size();
    for (size_t i = 0; i < siz; i++)
    {
        if (LODLightsCoronas[i].Identifier && LODLightsCoronas[i].Intensity > 0)
        {
            CVector vecCoronaCoords = LODLightsCoronas[i].Coordinates, vecTransformedCoords;
            float fComputedWidth, fComputedHeight;

            if(CalcScreenCoors(&vecCoronaCoords, &vecTransformedCoords, &fComputedWidth, &fComputedHeight, true, true))
            {
                LODLightsCoronas[i].OffScreen = !(vecTransformedCoords.x >= 0.0 && vecTransformedCoords.x <= nWidth &&
						vecTransformedCoords.y >= 0.0 && vecTransformedCoords.y <= nHeight);

                if (LODLightsCoronas[i].Intensity > 0 && vecTransformedCoords.z <= LODLightsCoronas[i].Range)
                {
                    float fInvFarClip = 1.0f / vecTransformedCoords.z;
                    float fHalfRange = LODLightsCoronas[i].Range * 0.5f;
                    short nFadeIntensity = (short)(LODLightsCoronas[i].Intensity * (vecTransformedCoords.z > fHalfRange ? 1.0f - (vecTransformedCoords.z - fHalfRange) / fHalfRange : 1.0f));

                    if (bLastZWriteRenderState != LODLightsCoronas[i].LOSCheck == false)
                    {
                        bLastZWriteRenderState = LODLightsCoronas[i].LOSCheck == false;
                        FlushSpriteBuffer();
                        RwRenderStateSet(rwRENDERSTATEZTESTENABLE, (void*)bLastZWriteRenderState);
                    }

                    if (LODLightsCoronas[i].pTex)
                    {
                        float fColourFogMult = fminf(40.0f, vecTransformedCoords.z) * *flWeatherFoggyness * 0.025f + 1.0f;

                        // Sun core
                        if (LODLightsCoronas[i].Identifier == 1) vecTransformedCoords.z = *ms_fFarClipZ * 0.95f;

                        // This R* tweak broke the sun
                        //RwRenderStateSet(rwRENDERSTATEZTESTENABLE, (void*)true);
                        if (pLastRaster != LODLightsCoronas[i].pTex->raster)
                        {
                            pLastRaster = LODLightsCoronas[i].pTex->raster;
                            FlushSpriteBuffer();
                            RwRenderStateSet(rwRENDERSTATETEXTURERASTER, pLastRaster);
                        }

                        CVector vecCoronaCoordsAfterPull;
                        vecCoronaCoordsAfterPull.x -= ((vecCoronaCoords.x - pCamPos->x) * LODLightsCoronas[i].PullTowardsCam);
                        vecCoronaCoordsAfterPull.y -= ((vecCoronaCoords.y - pCamPos->y) * LODLightsCoronas[i].PullTowardsCam);
                        vecCoronaCoordsAfterPull.z -= ((vecCoronaCoords.z - pCamPos->z) * LODLightsCoronas[i].PullTowardsCam);

                        if (CalcScreenCoors(&vecCoronaCoordsAfterPull, &vecTransformedCoords, &fComputedWidth, &fComputedHeight, true, true))
                        {
                            RenderBufferedOneXLUSprite_Rotate_Aspect(vecTransformedCoords.x, vecTransformedCoords.y, vecTransformedCoords.z, LODLightsCoronas[i].Size * fComputedHeight, LODLightsCoronas[i].Size * fComputedHeight * fColourFogMult, LODLightsCoronas[i].Red / fColourFogMult, LODLightsCoronas[i].Green / fColourFogMult, LODLightsCoronas[i].Blue / fColourFogMult, nFadeIntensity, fInvFarClip * 20.0f, 0.0, 0xFF);
                        }
                    }
                }
            }
            else
            {
                LODLightsCoronas[i].OffScreen = true;
            }
        }
    }
}
void LoadDatFile()
{
    char path[256];
    snprintf(path, sizeof(path), "%s/data/SALodLights.dat", aml->GetAndroidDataPath());

    int fd = OpenFile(path, "r");
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

    gpCustomCoronaTexture = NULL;
    if(gpCustomCoronaTexture) gpCoronaTexture[1] = gpCustomCoronaTexture;

    auto itEnd = pFileContent->upper_bound(PackKey(nModelID, 0xFFFF));
    for (auto it = pFileContent->lower_bound(PackKey(nModelID, 0)); it != itEnd; it++)
    {
        m_pLampposts->push_back(CLamppostInfo(it->second.vecPos, it->second.colour, it->second.fCustomSizeMult, it->second.nCoronaShowMode, it->second.nNoDistance, it->second.nDrawSearchlight, 0.0f));
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
inline void RegisterCoronaMain(unsigned int nID, CEntity* pAttachTo, unsigned char R, unsigned char G, unsigned char B, unsigned char A, const CVector& Position, float Size, float Range, RwTexture* pTex, unsigned char flareType, unsigned char reflectionType, unsigned char LOSCheck, unsigned char unused, float normalAngle, bool bNeonFade, float PullTowardsCam, bool bFadeIntensity, float FadeSpeed, bool bOnlyFromBelow, bool bWhiteCore)
{
    CVector vecPosToCheck = Position;
    CVector* pCamPos = &TheCamera->GetPosition();

    if (Range * Range >= (pCamPos->x - vecPosToCheck.x)*(pCamPos->x - vecPosToCheck.x) + (pCamPos->y - vecPosToCheck.y)*(pCamPos->y - vecPosToCheck.y))
    {
        if (bNeonFade)
        {
            float fDistFromCam = (*pCamPos - vecPosToCheck).Magnitude();
            if (fDistFromCam < 35.0f) return;
            if (fDistFromCam < 50.0f) A *= (uint8_t)((fDistFromCam - 35.0f) * (2.0f / 3.0f));

            // Is corona already present?
            CLODRegisteredCorona* pSuitableSlot;
            auto it = LODLightsUsedMap.find(nID);
            if (it != LODLightsUsedMap.end())
            {
                pSuitableSlot = it->second->GetFrom();
                if (pSuitableSlot->Intensity == 0 && A == 0)
                {
                    // Mark as free
                    it->second->GetFrom()->Identifier = 0;
                    it->second->Add(&LODLightsFreeList);
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
                pSuitableSlot->FadedIntensity = A;
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
            pSuitableSlot->NormalAngle = normalAngle;
            pSuitableSlot->Range = Range;
            pSuitableSlot->pTex = pTex;
            pSuitableSlot->FlareType = flareType;
            pSuitableSlot->ReflectionType = reflectionType;
            pSuitableSlot->LOSCheck = LOSCheck;
            pSuitableSlot->RegisteredThisFrame = true;
            pSuitableSlot->PullTowardsCam = PullTowardsCam;
            pSuitableSlot->FadeSpeed = FadeSpeed;

            pSuitableSlot->NeonFade = bNeonFade;
            pSuitableSlot->OnlyFromBelow = bOnlyFromBelow;
            pSuitableSlot->WhiteCore = bWhiteCore;

            pSuitableSlot->bIsAttachedToEntity = false;
            pSuitableSlot->pEntityAttachedTo = NULL;
        }
    }
}
void RegisterNormalCorona(unsigned int nID, CEntity *pAttachTo, unsigned char R, unsigned char G, unsigned char B, unsigned char A, const CVector& Position, float Size, float Range, unsigned char coronaType, unsigned char flareType, bool enableReflection, bool checkObstacles, int unused, float normalAngle, bool longDistance, float nearClip, unsigned char bFadeIntensity, float FadeSpeed, bool bOnlyFromBelow, bool reflectionDelay)
{
    RegisterCoronaMain(nID, pAttachTo, R, G, B, A, Position, Size, Range, gpCoronaTexture[coronaType], flareType, enableReflection, checkObstacles, unused, normalAngle, longDistance, nearClip, bFadeIntensity, FadeSpeed, bOnlyFromBelow, reflectionDelay);
}
void RegisterFestiveCorona(unsigned int nID, CEntity *pAttachTo, unsigned char R, unsigned char G, unsigned char B, unsigned char A, const CVector& Position, float Size, float Range, unsigned char coronaType, unsigned char flareType, bool enableReflection, bool checkObstacles, int unused, float normalAngle, bool longDistance, float nearClip, unsigned char bFadeIntensity, float FadeSpeed, bool bOnlyFromBelow, bool reflectionDelay)
{
    auto it = LODLightsFestiveLights.find(nID);
    if (it != LODLightsFestiveLights.end())
    {
        RegisterCoronaMain(nID, pAttachTo, it->second.r, it->second.g, it->second.b, A, Position, Size, Range, gpCoronaTexture[coronaType], flareType, enableReflection, checkObstacles, unused, normalAngle, longDistance, nearClip, bFadeIntensity, FadeSpeed, bOnlyFromBelow, reflectionDelay);
    }
    else
    {
        LODLightsFestiveLights[nID] = CRGBA(rand() % 256, rand() % 256, rand() % 256, 0);
        RegisterCoronaMain(nID, pAttachTo, R, G, B, A, Position, Size, Range, gpCoronaTexture[coronaType], flareType, enableReflection, checkObstacles, unused, normalAngle, longDistance, nearClip, bFadeIntensity, FadeSpeed, bOnlyFromBelow, reflectionDelay);
    }
}
void LODLightsUpdate()
{
    auto pNode = LODLightsUsedList.First();
    if (pNode)
    {
        while (pNode != &LODLightsUsedList)
        {
            unsigned int nIndex = pNode->GetFrom()->Identifier;
            auto pNext = pNode->GetNextNode();
            pNode->GetFrom()->Update();

            // Did it become invalid?
            if (!pNode->GetFrom()->Identifier)
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

        if (nTime >= 20 * 60) bAlpha = (uint8_t)((15.0f / 16.0f)*nTime - 1095.0f);
        else if (nTime < 3 * 60) bAlpha = 255;
        else bAlpha = (uint8_t)((-15.0f / 16.0f)*nTime + 424.0f);

        CVector* pCamPos = &TheCamera->GetPosition();
        for (auto it = m_pLampposts->cbegin(); it != m_pLampposts->cend(); it++)
        {
            if ((it->vecPos.z >= -15.0f) && (it->vecPos.z <= 1030.0f))
            {
                float fDistSqr = (*pCamPos - it->vecPos).Magnitude();
                if ((fDistSqr > 250.0f*250.0f && fDistSqr < fCoronaFarClipSQR) || it->nNoDistance)
                {
                    if (it->nNoDistance) fRadius = 3.5f;
                    else fRadius = (fDistSqr < 300.0f*300.0f) ? (0.07f) * sqrtf(fDistSqr) - 17.5f : 3.5f;

                    if (bSlightlyIncreaseRadiusWithDistance) fRadius *= fminf((0.0025f)*sqrtf(fDistSqr) + 0.25f, 4.0f);

                    if (it->fCustomSizeMult != 0.45f)
                    {
                        if (!it->nCoronaShowMode)
                        {
                            RegisterLODCorona(reinterpret_cast<unsigned int>(&*it), nullptr, it->colour.r, it->colour.g, it->colour.b, (bAlpha * (it->colour.a / 255.0f)), it->vecPos, (fRadius * it->fCustomSizeMult * fCoronaRadiusMultiplier), fCoronaFarClip, 1, 0, false, false, 0, 0.0f, false, 0.0f, 0, 255.0f, false, false);
                            if (bRenderStaticShadowsForLODs)
                            {
                                // StoreStaticShadow
                            }
                        }
                        else
                        {
                            static float blinking;
                            if (IsBlinkingNeeded(it->nCoronaShowMode)) blinking -= *ms_fTimeStep / 1000.0f;
                            else blinking += *ms_fTimeStep / 1000.0f;

                            (blinking > 1.0f) ? blinking = 1.0f : (blinking < 0.0f) ? blinking = 0.0f : 0.0f;

                            RegisterLODCorona(reinterpret_cast<unsigned int>(&*it), nullptr, it->colour.r, it->colour.g, it->colour.b, blinking * (bAlpha * (it->colour.a / 255.0f)), it->vecPos, (fRadius * it->fCustomSizeMult * fCoronaRadiusMultiplier), fCoronaFarClip, 1, 0, false, false, 0, 0.0f, false, 0.0f, 0, 255.0f, false, false);
                        }
                    }
                    else
                    {
                        if ((it->colour.r >= 250 && it->colour.g >= 100 && it->colour.b <= 100) && ((curMin == 9 || curMin == 19 || curMin == 29 || curMin == 39 || curMin == 49 || curMin == 59))) //yellow
                        {
                            RegisterLODCorona(reinterpret_cast<unsigned int>(&*it), nullptr, it->colour.r, it->colour.g, it->colour.b, (bAlpha * (it->colour.a / 255.0f)), it->vecPos, (fRadius * it->fCustomSizeMult * fCoronaRadiusMultiplier), fCoronaFarClip, 1, 0, false, false, 0, 0.0f, false, 0.0f, 0, 255.0f, false, false);
                        }
                        else
                        {
                            if ((fabsf(it->fHeading) >= (3.1415f / 6.0f) && fabsf(it->fHeading) <= (5.0f * 3.1415f / 6.0f)))
                            {
                                if ((it->colour.r >= 250 && it->colour.g < 100 && it->colour.b == 0) && (((curMin >= 0 && curMin < 9) || (curMin >= 20 && curMin < 29) || (curMin >= 40 && curMin < 49)))) //red
                                {
                                    RegisterLODCorona(reinterpret_cast<unsigned int>(&*it), nullptr, it->colour.r, it->colour.g, it->colour.b, (bAlpha * (it->colour.a / 255.0f)), it->vecPos, (fRadius * it->fCustomSizeMult * fCoronaRadiusMultiplier), fCoronaFarClip, 1, 0, false, false, 0, 0.0f, false, 0.0f, 0, 255.0f, false, false);
                                }
                                else if ((it->colour.r == 0 && it->colour.g >= 250 && it->colour.b == 0) && (((curMin > 9 && curMin < 19) || (curMin > 29 && curMin < 39) || (curMin > 49 && curMin < 59)))) //green
                                {
                                    RegisterLODCorona(reinterpret_cast<unsigned int>(&*it), nullptr, it->colour.r, it->colour.g, it->colour.b, (bAlpha * (it->colour.a / 255.0f)), it->vecPos, (fRadius * it->fCustomSizeMult * fCoronaRadiusMultiplier), fCoronaFarClip, 1, 0, false, false, 0, 0.0f, false, 0.0f, 0, 255.0f, false, false);
                                }
                            }
                            else
                            {
                                if ((it->colour.r == 0 && it->colour.g >= 250 && it->colour.b == 0) && (((curMin >= 0 && curMin < 9) || (curMin >= 20 && curMin < 29) || (curMin >= 40 && curMin < 49)))) //red
                                {
                                    RegisterLODCorona(reinterpret_cast<unsigned int>(&*it), nullptr, it->colour.r, it->colour.g, it->colour.b, (bAlpha * (it->colour.a / 255.0f)), it->vecPos, (fRadius * it->fCustomSizeMult * fCoronaRadiusMultiplier), fCoronaFarClip, 1, 0, false, false, 0, 0.0f, false, 0.0f, 0, 255.0f, false, false);
                                }
                                else if ((it->colour.r >= 250 && it->colour.g < 100 && it->colour.b == 0) && (((curMin > 9 && curMin < 19) || (curMin > 29 && curMin < 39) || (curMin > 49 && curMin < 59)))) //green
                                {
                                    RegisterLODCorona(reinterpret_cast<unsigned int>(&*it), nullptr, it->colour.r, it->colour.g, it->colour.b, (bAlpha * (it->colour.a / 255.0f)), it->vecPos, (fRadius * it->fCustomSizeMult * fCoronaRadiusMultiplier), fCoronaFarClip, 1, 0, false, false, 0, 0.0f, false, 0.0f, 0, 255.0f, false, false);
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

        aml->WriteFloat(pGTASA + 0x4114E8,  v);
        aml->WriteFloat(pGTASA + 0x40F3B4,  v);
        aml->WriteFloat(pGTASA + 0x40F370,  v);
        aml->WriteFloat(pGTASA + 0x40F374,  v);
        aml->WriteFloat(pGTASA + 0x40F378,  v);
        aml->WriteFloat(pGTASA + 0x40F37C,  v);
        aml->WriteFloat(pGTASA + 0x4692F8,  v);
        aml->WriteFloat(pGTASA + 0x4114F4, -v);
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
        
    }
}
DECL_HOOK(CEntity*, LoadObjectInstance, void* a1, char const* a2)
{
    CEntity* ourEntity = LoadObjectInstance(a1, a2);
    if(ourEntity && bCatchLamppostsNow && CSearchLights::IsModelALamppost(ourEntity->GetModelIndex())) CSearchLights::RegisterLamppost(ourEntity);
    return ourEntity;
}
DECL_HOOKv(RegisterCorona_FarClip, int id, CEntity *entity, uint8_t r, uint8_t g, uint8_t b, uint8_t alpha, CVector *pos, float radius, float farClip, void *texture, char flare, char enableReflection, char checkObstacles, int notUsed, float angle, char longDistance, float nearClip, char fadeState, float fadeSpeed, char onlyFromBelow, char reflectionDelay)
{
    //                                                              \/\/\/\/
    RegisterCorona_FarClip(id, entity, r, g, b, alpha, pos, radius, 3000.0f, texture, flare, enableReflection, checkObstacles, notUsed, angle, longDistance, nearClip, fadeState, fadeSpeed, onlyFromBelow, reflectionDelay);
}
DECL_HOOKv(GameInit2_CranesInit)
{

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
    drawdistHackMi = mi;
    return mi;
}
DECL_HOOK(CAtomicModelInfo*, LoadObject_AddAtomicModel, int modelId)
{
    CAtomicModelInfo* mi = LoadObject_AddAtomicModel(modelId);
    drawdistHackMi = mi;
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
    aml->Write8(pGTASA + 0x281FE4, 0x00); // CIplStore::RequestIplAndIgnore -> m_bDisableDynamicStreaming = 0;
    for (auto it = IPLStreamNames.cbegin(); it != IPLStreamNames.cend(); it++)
    {
        RequestIplAndIgnore(FindIplSlot(it->c_str()));
    }
    aml->Write8(pGTASA + 0x281FE4, 0x01); // CIplStore::RequestIplAndIgnore -> m_bDisableDynamicStreaming = 1;

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

    // i just did it.
    // Instantiate all lod entities RwObject
    if (false)
    {
        std::for_each(lods.begin(), lods.end(), [](CEntity* entity)
        {
            if (entity->m_pRwObject == NULL) entity->CreateRwObject();
        });
    }
}
DECL_HOOKv(CoronasRegisterFestiveCoronaForEntity, unsigned int nID, CEntity* entity, unsigned char R, unsigned char G, unsigned char B, unsigned char A, const CVector& Position, float Size, float Range, RwTexture* pTex, char flare, char enableReflection, char checkObstacles, int notUsed, float angle, char longDistance, float nearClip, char fadeState, float fadeSpeed, char onlyFromBelow, char reflectionDelay)
{
    auto it = LODLightsFestiveLights.find(nID);
    if (it != LODLightsFestiveLights.end())
    {
        CoronasRegisterFestiveCoronaForEntity(nID, entity, it->second.r, it->second.g, it->second.b, A, Position, Size, Range, pTex, flare, enableReflection, checkObstacles, notUsed, angle, longDistance, nearClip, fadeState, fadeSpeed, onlyFromBelow, reflectionDelay);
    }
    else
    {
        LODLightsFestiveLights[nID] = CRGBA(rand() % 256, rand() % 256, rand() % 256, 0);
        CoronasRegisterFestiveCoronaForEntity(nID, entity, R, G, B, A, Position, Size, Range, pTex, flare, enableReflection, checkObstacles, notUsed, angle, longDistance, nearClip, fadeState, fadeSpeed, onlyFromBelow, reflectionDelay);
    }
}

// Patches
uintptr_t CoronaFarClip_BackTo, LoadScene_BackTo;
extern "C" uintptr_t CoronaFarClip_Patch(C2dEffect *effect)
{
    effect->light.m_fCoronaFarClip = 3000.0f;
    return CoronaFarClip_BackTo;
}
extern "C" uintptr_t LoadScene_Patch(CEntity *entity)
{
    if(entity && entity->m_pLod) lods.push_back(entity->m_pLod);
    return LoadScene_BackTo;
}
__attribute__((optnone)) __attribute__((naked)) void CoronaFarClip_Inject(void)
{
    asm volatile(
        "VMOV S1, R1\n" // org
        "VMOV S10, R3\n" // org
        "PUSH {R0-R2}\n"
        "MOV R0, R8\n"
        "BL CoronaFarClip_Patch\n"
        "MOV R3, R0\n"
        "POP {R0-R2}\n"
        "VLDR S4, [R8, #0x14]\n" // org
        "MOV PC, R3");
}
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

/// Main
extern "C" void OnModLoad()
{
    logger->SetTag("Project2DFX");
    pGTASA = aml->GetLib("libGTASA.so");
    hGTASA = aml->GetLibHandle("libGTASA.so");
    cfg = new Config("SALodLights");

    // Config
    bRenderLodLights = cfg->GetBool("RenderLodLights", true, "LodLights");
    numCoronas = cfg->GetInt("MaxNumberOfLodLights", 25000, "LodLights");
    fCoronaRadiusMultiplier = cfg->GetFloat("CoronaRadiusMultiplier", 1.0f, "LodLights");
    bSlightlyIncreaseRadiusWithDistance = cfg->GetBool("SlightlyIncreaseRadiusWithDistance", 1, "LodLights");
    if(!strcasecmp(cfg->GetString("CoronaFarClip", "auto", "LodLights"), "auto"))
    {
        fCoronaFarClip = cfg->GetFloat("CoronaFarClip", 0.0f, "LodLights");
        fCoronaFarClipSQR = fCoronaFarClip * fCoronaFarClip;
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
    SET_TO(ms_modelInfoPtrs, *(uintptr_t*)(pGTASA + 0x6796D4));
    SET_TO(nActiveInterior, aml->GetSym(hGTASA, "_ZN5CGame8currAreaE"));
    SET_TO(flWeatherFoggyness, aml->GetSym(hGTASA, "_ZN8CWeather9FoggynessE"));
    SET_TO(ms_fTimeStep, aml->GetSym(hGTASA, "_ZN6CTimer12ms_fTimeStepE"));
    SET_TO(ms_fFarClipZ, aml->GetSym(hGTASA, "_ZN5CDraw12ms_fFarClipZE"));
    SET_TO(TheCamera, aml->GetSym(hGTASA, "TheCamera"));
    SET_TO(gpCoronaTexture, aml->GetSym(hGTASA, "gpCoronaTexture"));
    SET_TO(RsGlobal, aml->GetSym(hGTASA, "RsGlobal"));
    SET_TO(m_CurrentColours, aml->GetSym(hGTASA, "_ZN10CTimeCycle16m_CurrentColoursE"));
    SET_TO(CurrentTimeHours, aml->GetSym(hGTASA, "_ZN6CClock18ms_nGameClockHoursE"));
    SET_TO(CurrentTimeMinutes, aml->GetSym(hGTASA, "_ZN6CClock20ms_nGameClockMinutesE"));
    SET_TO(m_snTimeInMillisecondsPauseMode, aml->GetSym(hGTASA, "_ZN6CTimer31m_snTimeInMillisecondsPauseModeE"));
    SET_TO(UnderWaterness, aml->GetSym(hGTASA, "_ZN8CWeather14UnderWaternessE"));
    SET_TO(game_FPS, aml->GetSym(hGTASA, "_ZN6CTimer8game_FPSE"));
    SET_TO(ms_fFarClipPlane, aml->GetSym(hGTASA, "_ZN9CRenderer16ms_fFarClipPlaneE"));

    SET_TO(GetIsTimeInRange, aml->GetSym(hGTASA, "_ZN6CClock16GetIsTimeInRangeEhh"));
    SET_TO(FindGroundZFor3DCoord, aml->GetSym(hGTASA, "_ZN6CWorld21FindGroundZFor3DCoordEfffPbPP7CEntity"));
    SET_TO(OpenFile, aml->GetSym(hGTASA, "_ZN8CFileMgr8OpenFileEPKcS1_"));
    SET_TO(CloseFile, aml->GetSym(hGTASA, "_ZN8CFileMgr9CloseFileEj"));
    SET_TO(LoadLine, aml->GetSym(hGTASA, "_ZN11CFileLoader8LoadLineEj"));
    SET_TO(GetModelInfoUInt16, aml->GetSym(hGTASA, "_ZN10CModelInfo18GetModelInfoUInt16EPKcPt"));
    SET_TO(FindIplSlot, aml->GetSym(hGTASA, "_ZN9CIplStore11FindIplSlotEPKc"));
    SET_TO(RequestIplAndIgnore, aml->GetSym(hGTASA, "_ZN9CIplStore19RequestIplAndIgnoreEi"));
    SET_TO(RequestModel, aml->GetSym(hGTASA, "_ZN10CStreaming12RequestModelEii"));
    SET_TO(LoadAllRequestedModels, aml->GetSym(hGTASA, "_ZN10CStreaming22LoadAllRequestedModelsEb"));
    SET_TO(RwRenderStateSet, aml->GetSym(hGTASA, "_Z16RwRenderStateSet13RwRenderStatePv"));
    SET_TO(FlushSpriteBuffer, aml->GetSym(hGTASA, "_ZN7CSprite17FlushSpriteBufferEv"));
    SET_TO(CalcScreenCoors, aml->GetSym(hGTASA, "_ZN7CSprite15CalcScreenCoorsERK5RwV3dPS0_PfS4_bb"));
    SET_TO(RenderBufferedOneXLUSprite_Rotate_Aspect, aml->GetSym(hGTASA, "_ZN7CSprite40RenderBufferedOneXLUSprite_Rotate_AspectEfffffhhhsffh"));
    SET_TO(FindPlayerPed, aml->GetSym(hGTASA, "_Z13FindPlayerPedi"));

    // Hooks
    HOOKBLX(LoadLevel_LoadingScreen, pGTASA + 0x466AB2 + 0x1);
    HOOKBLX(RenderEffects_MovingThings, pGTASA + 0x3F6380 + 0x1);
    HOOKBLX(RegisterCorona_FarClip, pGTASA + 0x5A44E8 + 0x1); // CoronaFarClip_Inject replacement
    HOOKBLX(GameInit2_CranesInit, pGTASA + 0x473022 + 0x1);

    // Patches
    aml->Write(pGTASA + 0x362EC8, "\xC4\xF2\x41\x40", 4); // 50.0 -> 576.0 (instead of 550.0 on PC)
    aml->Write(pGTASA + 0x5A3644, "\xBA\xF5\x61\x5F", 4); // Sun reflections
    //CoronaFarClip_BackTo = pGTASA + 0x5A443C + 0x1;
    //aml->Redirect(pGTASA + 0x5A4430 + 0x1, (uintptr_t)CoronaFarClip_Inject); // brokey

    // P2DFX Init
    RegisterLODCorona = &RegisterNormalCorona;
    if (bRenderLodLights)
    {
        // CLODLights::Inject();
        HOOKBLX(GameInit_LoadingScreen, pGTASA + 0x3F3784 + 0x1);
        HOOKPLT(LoadObjectInstance, pGTASA + 0x675E6C);
        HOOKBLX(GameProcess_BridgesUpdate, pGTASA + 0x3F4266 + 0x1);
    }
    if (bEnableDrawDistanceChanger) // incomplete
    {
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
    }
    if (fStaticSunSize)
    {
        
    }
    if (fStaticShadowsDrawDistance)
    {
        // In JPatch! and also...
        // 5BD49E ?
    }
    if (fStaticShadowsIntensity)
    {
        fStaticShadowsIntensity *= 0.00390625f;
        // In JPatch!
    }
    if (fTimedObjectsDrawDistance || fNeonsDrawDistance)
    {
        HOOKBLX(LoadTimeObject_SetTexDictionary, pGTASA + 0x4696A2 + 0x1);
    }
    if (fLODObjectsDrawDistance || fGenericObjectsDrawDistance || fAllNormalObjectsDrawDistance || fVegetationDrawDistance)
    {
        HOOKBLX(LoadObject_AddDamageAtomicModel, pGTASA + 0x4694E4 + 0x1);
        HOOKBLX(LoadObject_AddAtomicModel, pGTASA + 0x4694DE + 0x1);
        HOOKBLX(LoadObject_GetModelCDName, pGTASA + 0x469506 + 0x1);
        if (fGenericObjectsDrawDistance || fAllNormalObjectsDrawDistance || fVegetationDrawDistance)
        {
            // usage of fMaxDrawDistanceForNormalObjects, doing it in funcs manually (SetMaxDrawDistanceForNormalObjects)
        }
    }
    if (bLoadAllBinaryIPLs)
    {
        HOOKBLX(GenericLoad_IplStoreLoad, pGTASA + 0x482E18 + 0x1);
    }
    if (bPreloadLODs)
    {
        LoadScene_BackTo = pGTASA + 0x4691A8 + 0x1;
        aml->Redirect(pGTASA + 0x4691A0 + 0x1, (uintptr_t)LoadScene_Inject);
        HOOKBLX(GameInit_StartTestScript, pGTASA + 0x3F3788 + 0x1);
    }
    if (bFestiveLights)
    {
        auto now = std::chrono::system_clock::now();
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        struct tm *date = std::localtime(&now_c);
        if (bFestiveLightsAlways || (date->tm_mon == 0 && date->tm_mday <= 1) || (date->tm_mon == 11 && date->tm_mday >= 31))
        {
            RegisterLODCorona = &RegisterFestiveCorona;

            HOOKBLX(CoronasRegisterFestiveCoronaForEntity, pGTASA + 0x5A49A4 + 0x1);
            HOOKBLX(CoronasRegisterFestiveCoronaForEntity, pGTASA + 0x5A44E8 + 0x1);
            HOOKBLX(CoronasRegisterFestiveCoronaForEntity, pGTASA + 0x5A4B88 + 0x1);
            HOOKBLX(CoronasRegisterFestiveCoronaForEntity, pGTASA + 0x3EC6C2 + 0x1);
            HOOKBLX(CoronasRegisterFestiveCoronaForEntity, pGTASA + 0x3EC7F0 + 0x1);
        }
    }
}