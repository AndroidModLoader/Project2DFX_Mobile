#include <mod/amlmod.h>

#include <cmath>
#include <stdint.h>

#ifdef AML32
    #include "GTASA_STRUCTS.h"
    #define BYVER(__for32, __for64) (__for32)
#else
    #include "GTASA_STRUCTS_210.h"
    #define BYVER(__for32, __for64) (__for64)
#endif

#include <searchlights.h>

extern float* flWeatherFoggyness;
extern int* nActiveInterior;
extern CCamera* TheCamera;
extern uintptr_t pGTASA;

extern char(*GetIsTimeInRange)(char hourA, char hourB);


std::vector<CLamppostInfo>                   Lampposts;
std::vector<CLamppostInfo>*                  m_pLampposts = &Lampposts;
std::map<unsigned int, CLamppostInfo>        FileContent;
std::map<unsigned int, CLamppostInfo>*       pFileContent = &FileContent;
extern float                                 fSearchlightEffectVisibilityFactor;

void CSearchLights::DrawCustomSpotLightSA(RwV3d StartPoint, RwV3d EndPoint, float TargetRadius, float baseRadius, float slColorFactor1, char slColorFactor2, float slAlpha)
{
    
}

void CSearchLights::RenderSearchLightsSA()
{
    /* It needs a full rework */
    /*if(*flWeatherFoggyness)
    {
        if (GetIsTimeInRange(20, 7) && *nActiveInterior == 0)
        {
            static auto SetRenderStatesForSpotLights = (void(*)()) (pGTASA + 0x194980);
            SetRenderStatesForSpotLights();

            // 0x952126 word_952126
            // 0x95275C word_95275C
            for (auto it = m_pLampposts->cbegin(); it != m_pLampposts->cend(); ++it)
            {
                if (it->nDrawSearchlight)
                {
                    // CCamera* + 2264 = m_pRwCamera
                    CVector*    pCamPos = (&TheCamera + 2264);
                    float        fDistSqr = (pCamPos->x - it->vecPos.x)*(pCamPos->x - it->vecPos.x) + (pCamPos->y - it->vecPos.y)*(pCamPos->y - it->vecPos.y) + (pCamPos->z - it->vecPos.z)*(pCamPos->z - it->vecPos.z);

                    if ((fDistSqr > 50.0f*50.0f) && (fDistSqr < 300.0f*300.0f))
                    {
                        float fVisibility = fSearchlightEffectVisibilityFactor * ((0.0233333f)*sqrt(fDistSqr) - 1.16667f);

                        RwV3d EndPoint = *(RwV3d*)&it->vecPos;
                        EndPoint.z = CLODLightManager::SA::FindGroundZFor3DCoord(it->vecPos.x, it->vecPos.y, it->vecPos.z, 0, 0);

                        if (!(it->colour.r == 255 && it->colour.g == 255 && it->colour.b == 255) && !(it->colour.r == 254 && it->colour.g == 117 && it->colour.b == 134))
                        {
                            //yellow
                            DrawCustomSpotLightSA(*(RwV3d*)&it->vecPos, EndPoint, min((8.0f * (it->vecPos.z - EndPoint.z)), 90.0f), it->fCustomSizeMult / 6.0f, 5.0f, 8, fVisibility);
                        }
                        else if (!(it->colour.r == 254 && it->colour.g == 117 && it->colour.b == 134))
                        {
                            //white
                            DrawCustomSpotLightSA(*(RwV3d*)&it->vecPos, EndPoint, min((8.0f * (it->vecPos.z - EndPoint.z)), 90.0f), it->fCustomSizeMult / 6.0f, 255.0f, 8, fVisibility);
                        }
                        else
                        {
                            //pink
                            DrawCustomSpotLightSA(*(RwV3d*)&it->vecPos, EndPoint, min((8.0f * (it->vecPos.z - EndPoint.z)), 90.0f), it->fCustomSizeMult / 6.0f, 200.0f, 18, fVisibility);
                        }
                    }
                }
            }


            static auto ResetRenderStatesForSpotLights = (void(*)()) (pGTASA + 0x196C1C);
            ResetRenderStatesForSpotLights();
        }
    }*/
}

bool CSearchLights::IsModelALamppost(uint16_t nModelId)
{
    auto   it = pFileContent->lower_bound(PackKey(nModelId, 0));
    return it != pFileContent->end() && it->first >> 16 == nModelId;
}

void CSearchLights::RegisterLamppost(CEntity* pObj)
{
    unsigned short      nModelID = pObj->GetModelIndex();
    CMatrix             dummyMatrix;
    CSimpleTransform&   objTransform = pObj->GetTransform();

    if (objTransform.m_vPosn.x == 0.0f && objTransform.m_vPosn.y == 0.0f)
        return;

    dummyMatrix.SetTranslateOnly(objTransform.m_vPosn);
    dummyMatrix.SetRotateZOnly(objTransform.m_fHeading);

    auto itEnd = pFileContent->upper_bound(PackKey(nModelID, 0xFFFF));
    for (auto it = pFileContent->lower_bound(PackKey(nModelID, 0)); it != itEnd; it++)
    {
        m_pLampposts->push_back(CLamppostInfo(dummyMatrix * it->second.vecPos, it->second.colour, it->second.fCustomSizeMult, it->second.nCoronaShowMode, it->second.nNoDistance, it->second.nDrawSearchlight, pObj->GetTransform().m_fHeading));
    }
}