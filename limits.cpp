// We need this for some limits.
// Yes, a built-in limit adjuster.
// It will only adjust limits if they are NOT adjusted by someone.

#include <mod/amlmod.h>
#include <mod/logger.h>
#ifdef AML32
    #include "GTASA_STRUCTS.h"
#else
    #include "GTASA_STRUCTS_210.h"
#endif

extern uintptr_t pGTASA;
extern void*     hGTASA;

void* (*InitMatrixLinkList)(uintptr_t, int);
DECL_HOOKv(InitStaticMatrices)
{
    InitMatrixLinkList(aml->GetSym(hGTASA, "gMatrixList"), 2400);
}

CPool<CBuilding>          **pBuildingPool;
CPool<CDummy>             **pDummyPool;
CPool<CPtrNode>           **pPtrNodeSingleLinkPool;
CPool<CPtrNodeDoubleLink> **pPtrNodeDoubleLinkPool;
DECL_HOOKv(InitPools)
{
    InitPools();

    // Doing a dirty patch. Yeah, im lazy.
    // And we dont need to patch ALL pools.
    (*pBuildingPool)->Flush(); delete *pBuildingPool;
    (*pDummyPool)->Flush(); delete *pDummyPool;
    (*pPtrNodeSingleLinkPool)->Flush(); delete *pPtrNodeSingleLinkPool;
    (*pPtrNodeDoubleLinkPool)->Flush(); delete *pPtrNodeDoubleLinkPool;

    (*pBuildingPool) = new CPool<CBuilding>(32000, "BuildingsAreCool");
    (*pDummyPool) = new CPool<CDummy>(24000, "DummiesAreCool");
    (*pPtrNodeSingleLinkPool) = new CPool<CPtrNode>(240000, "SingleNodeLinksAreCool");
    (*pPtrNodeDoubleLinkPool) = new CPool<CPtrNodeDoubleLink>(24000, "DoubleNodeLinksAreCool");
}

CLinkList<CEntity*> *rwObjectInstances;
DECL_HOOKv(InitStreaming2)
{
    InitStreaming2();

    rwObjectInstances->Shutdown();
    rwObjectInstances->Init(8000);
}

void Init_MiniLA()
{
  #ifdef AML32
    // EntityIPL limit
    if(*(uint32_t*)(pGTASA + 0x28208C) == 0x0045DD2E)
    {
        static void** entityIplPool; // default is 40

        int entitiesIpl = 128;
        entityIplPool = new void*[entitiesIpl] {0};

        // IplEntityIndexArrays / ppEntityIndexArray (v2.10, original name)
        aml->WriteAddr(pGTASA + 0x2805E0, (uintptr_t)entityIplPool - pGTASA - 0x280578);
        aml->WriteAddr(pGTASA + 0x280C78, (uintptr_t)entityIplPool - pGTASA - 0x2809F0);
        aml->WriteAddr(pGTASA + 0x281080, (uintptr_t)entityIplPool - pGTASA - 0x280CEA);
        aml->WriteAddr(pGTASA + 0x2810C8, (uintptr_t)entityIplPool - pGTASA - 0x2810C2);
        aml->WriteAddr(pGTASA + 0x28208C, (uintptr_t)entityIplPool - pGTASA - 0x28207A);
    }

    // EntityPerIPL limit
    if(*(uint32_t*)(pGTASA + 0x4692D0) == 0x005451B0)
    {
        static void** entityPerIplPool; // default is 4096

        int entitiesPerIpl = 16384;
        entityPerIplPool = new void*[entitiesPerIpl] {0};
        
        // gCurrIplInstances / gpLoadedBuildings (v2.10, original name)
        aml->WriteAddr(pGTASA + 0x4692D0, (uintptr_t)entityPerIplPool - pGTASA - 0x4690D0);
        aml->WriteAddr(pGTASA + 0x4692D4, (uintptr_t)entityPerIplPool - pGTASA - 0x4690E2);
        aml->WriteAddr(pGTASA + 0x4692D8, (uintptr_t)entityPerIplPool - pGTASA - 0x4692A8);
        aml->WriteAddr(pGTASA + 0x4692DC, (uintptr_t)entityPerIplPool - pGTASA - 0x4690EE);
        aml->WriteAddr(pGTASA + 0x4692E0, (uintptr_t)entityPerIplPool - pGTASA - 0x469106);
        aml->WriteAddr(pGTASA + 0x4692E8, (uintptr_t)entityPerIplPool - pGTASA - 0x46912A);
        aml->WriteAddr(pGTASA + 0x4692EC, (uintptr_t)entityPerIplPool - pGTASA - 0x46912C);
        aml->WriteAddr(pGTASA + 0x469304, (uintptr_t)entityPerIplPool - pGTASA - 0x469250);
        aml->WriteAddr(pGTASA + 0x46930C, (uintptr_t)entityPerIplPool - pGTASA - 0x469266);
        aml->WriteAddr(pGTASA + 0x46931C, (uintptr_t)entityPerIplPool - pGTASA - 0x468D6A);
        aml->WriteAddr(pGTASA + 0x469330, (uintptr_t)entityPerIplPool - pGTASA - 0x4691A2);
        aml->WriteAddr(pGTASA + 0x469334, (uintptr_t)entityPerIplPool - pGTASA - 0x46919A);
    }

    // Static Matrices
    if(*(uintptr_t*)(pGTASA + 0x675554) == (pGTASA + 0x408AD0 + 0x1))
    {
        SET_TO(InitMatrixLinkList, aml->GetSym(hGTASA, "_ZN15CMatrixLinkList4InitEi"));
        HOOKPLT(InitStaticMatrices, pGTASA + 0x675554);
    }

    // Pools
    if(*(uintptr_t*)(pGTASA + 0x672468) == (pGTASA + 0x40C8B0 + 0x1))
    {
        SET_TO(pBuildingPool, aml->GetSym(hGTASA, "_ZN6CPools16ms_pBuildingPoolE"));
        SET_TO(pDummyPool, aml->GetSym(hGTASA, "_ZN6CPools13ms_pDummyPoolE"));
        SET_TO(pPtrNodeSingleLinkPool, aml->GetSym(hGTASA, "_ZN6CPools25ms_pPtrNodeSingleLinkPoolE"));
        SET_TO(pPtrNodeDoubleLinkPool, aml->GetSym(hGTASA, "_ZN6CPools25ms_pPtrNodeDoubleLinkPoolE"));
        HOOKPLT(InitPools, pGTASA + 0x672468);
    }

    // RwObject instances
    if(*(uint32_t*)(pGTASA + 0x46BE20) == 0x6050F244 && // If FLA didnt touch it
       *(uintptr_t*)(pGTASA + 0x6700D0) == (pGTASA + 0x46BA94 + 0x1)) 
    {
        SET_TO(rwObjectInstances, aml->GetSym(hGTASA, "_ZN10CStreaming20ms_rwObjectInstancesE"));
        HOOKPLT(InitStreaming2, pGTASA + 0x6700D0);
    }
  #else
    // EntityIPL limit
    if(*(uint32_t*)(pGTASA + 0x33B170) == 0xF0002C16)
    {
        static void** entityIplPool; // default is 40

        int entitiesIpl = 128;
        entityIplPool = new void*[entitiesIpl] {0};

        aml->WriteAddr(pGTASA + 0x710B38, (uintptr_t)entityIplPool); // saving a limit pointer!

        // IplEntityIndexArrays / ppEntityIndexArray (v2.10, original name)
        aml->Write32(pGTASA + 0x33B170, 0xB0001EB6);
        aml->Write32(pGTASA + 0x33B178, 0xF9459ED6);
        
        aml->Write32(pGTASA + 0x33B750, 0xB0001EA9);
        aml->Write32(pGTASA + 0x33B754, 0xF9459D29);
        
        aml->Write32(pGTASA + 0x33BBA4, 0xB0001EA9);
        aml->Write32(pGTASA + 0x33BBA8, 0xF9459D29);
        
        aml->Write32(pGTASA + 0x33C024, 0x90001EA8);
        aml->Write32(pGTASA + 0x33C028, 0xF9459D08);
        
        aml->Write32(pGTASA + 0x33D330, 0xF0001E8A);
        aml->Write32(pGTASA + 0x33D334, 0xF9459D4A);
    }

    // EntityPerIPL limit
    if(*(uint32_t*)(pGTASA + 0x553F04) == 0xD000378A)
    {
        static void** entityPerIplPool; // default is 4096

        int entitiesPerIpl = 16384;
        entityPerIplPool = new void*[entitiesPerIpl] {0};

        aml->WriteAddr(pGTASA + 0x712EB8, (uintptr_t)entityPerIplPool); // saving a limit pointer!
        
        // gCurrIplInstances / gpLoadedBuildings (v2.10, original name)
        aml->Write32(pGTASA + 0x553F04, 0xF0000DEA);
        aml->Write32(pGTASA + 0x553F08, 0xF9475D4A);
        
        aml->Write32(pGTASA + 0x5543C0, 0xD0000DEB);
        aml->Write32(pGTASA + 0x5543C8, 0xF9475D6B);
        
        aml->Write32(pGTASA + 0x5543E8, 0xD0000DEB);
        aml->Write32(pGTASA + 0x5543EC, 0xF9475D6B);
        
        aml->Write32(pGTASA + 0x554410, 0xD0000DF5);
        aml->Write32(pGTASA + 0x554414, 0xF9475EB5);
        
        aml->Write32(pGTASA + 0x55459C, 0xD0000DF3);
        aml->Write32(pGTASA + 0x5545A0, 0xF9475E73);
    }

    // Static Matrices
    if(*(uintptr_t*)(pGTASA + 0x848EB0) == (pGTASA + 0x4EC1D4))
    {
        SET_TO(InitMatrixLinkList, aml->GetSym(hGTASA, "_ZN15CMatrixLinkList4InitEi"));
        HOOKPLT(InitStaticMatrices, pGTASA + 0x848EB0);
    }

    // Pools
    if(*(uintptr_t*)(pGTASA + 0x843F18) == (pGTASA + 0x4F1364))
    {
        SET_TO(pBuildingPool, aml->GetSym(hGTASA, "_ZN6CPools16ms_pBuildingPoolE"));
        SET_TO(pDummyPool, aml->GetSym(hGTASA, "_ZN6CPools13ms_pDummyPoolE"));
        SET_TO(pPtrNodeSingleLinkPool, aml->GetSym(hGTASA, "_ZN6CPools25ms_pPtrNodeSingleLinkPoolE"));
        SET_TO(pPtrNodeDoubleLinkPool, aml->GetSym(hGTASA, "_ZN6CPools25ms_pPtrNodeDoubleLinkPoolE"));
        HOOKPLT(InitPools, pGTASA + 0x843F18);
    }

    // RwObject instances
    if(*(uint32_t*)(pGTASA + 0x551E80) == 0x9400151E)
    {
        SET_TO(rwObjectInstances, aml->GetSym(hGTASA, "_ZN10CStreaming20ms_rwObjectInstancesE"));
        HOOKBL(InitStreaming2, pGTASA + 0x551E80);
    }
  #endif
}