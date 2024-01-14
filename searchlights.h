#ifndef _SEARCHLIGHTS
#define _SEARCHLIGHTS

#include <vector>
#include <map>

class CEntity;
class CLamppostInfo;

extern std::vector<CLamppostInfo>* m_pLampposts;
extern std::map<unsigned int, CLamppostInfo>* pFileContent;

inline uint32_t PackKey(unsigned short nModel, unsigned short nIndex)
{
    return nModel << 16 | nIndex;
}

class CLamppostInfo
{
public:
    CVector         vecPos;
    CRGBA           colour;
    float           fCustomSizeMult;
    int             nNoDistance;
    int             nDrawSearchlight;
    float           fHeading;
    int             nCoronaShowMode;
    float           fObjectDrawDistance;

    CLamppostInfo(const CVector& pos, const CRGBA& col, float fCustomMult, int CoronaShowMode, int nNoDistance, int nDrawSearchlight, float heading, float ObjectDrawDistance = 0.0f)
        : vecPos(pos), colour(col), fCustomSizeMult(fCustomMult), nCoronaShowMode(CoronaShowMode), nNoDistance(nNoDistance), nDrawSearchlight(nDrawSearchlight), fHeading(heading), fObjectDrawDistance(ObjectDrawDistance)
    {}
};

class CSearchLights
{
public:
    static void DrawCustomSpotLightSA(RwV3d StartPoint, RwV3d EndPoint, float TargetRadius, float baseRadius, float slColorFactor1, char slColorFactor2, float slAlpha = 1.0f);
    static void RenderSearchLightsSA();
    static bool IsModelALamppost(uint16_t nModelId);
    static void RegisterLamppost(CEntity* pObj);
};

#endif // _SEARCHLIGHTS