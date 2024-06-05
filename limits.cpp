// We need this for some limits.
// Yes, a built-in limit adjuster.
// It will only adjust limits if they are NOT adjusted by someone.

#include <mod/amlmod.h>
#include <mod/logger.h>
#ifdef AML32
    #include <AArchASMHelper/Thumbv7_ASMHelper.h>
    #include "GTASA_STRUCTS.h"
#else
    #include <AArchASMHelper/ARMv8_ASMHelper.h>
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
    //if(*(uint32_t*)(pGTASA + 0x0) == 0x0)
    {
        (*pBuildingPool)->Flush(); delete *pBuildingPool;
        (*pBuildingPool) = new CPool<CBuilding>(64000, "BuildingsAreCool");
    }

    //if(*(uint32_t*)(pGTASA + 0x0) == 0x0)
    {
        (*pDummyPool)->Flush(); delete *pDummyPool;
        (*pDummyPool) = new CPool<CDummy>(48000, "DummiesAreCool");
    }

    if(*(uint32_t*)(pGTASA + 0x40C8C0) == 0x70C0F242)
    {
        (*pPtrNodeSingleLinkPool)->Flush(); delete *pPtrNodeSingleLinkPool;
        (*pPtrNodeSingleLinkPool) = new CPool<CPtrNode>(320000, "SingleNodeLinksAreCool");
    }

    if(*(uint16_t*)(pGTASA + 0x40C92E) == 0xF641)
    {
        (*pPtrNodeDoubleLinkPool)->Flush(); delete *pPtrNodeDoubleLinkPool;
        (*pPtrNodeDoubleLinkPool) = new CPool<CPtrNodeDoubleLink>(32000, "DoubleNodeLinksAreCool");
    }
}

CLinkList<CEntity*> *rwObjectInstances;
DECL_HOOKv(InitStreaming2)
{
    InitStreaming2();

    rwObjectInstances->Shutdown();
    rwObjectInstances->Init(12000);
}

void Init_MiniLA()
{
  #ifdef AML32
    // EntityIPL limit
    if(*(uint32_t*)(pGTASA + 0x28208C) == 0x0045DD2E)
    {
        static void** entityIplPool; // default is 40

        int entitiesIpl = 1024;
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

        int entitiesPerIpl = 32768;
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
    if(*(uintptr_t*)(pGTASA + 0x675554) == (pGTASA + 0x408AD0 + 0x1) &&
       *(uint32_t*)(pGTASA + 0x471D1A) == 0xEED0F52D)
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

    // Coronas Limit
    if(*(uintptr_t*)(pGTASA + 0x676C44) == (pGTASA + 0xA25B44))
    {
        static void* coronasPool; // default is 64

        int coronas = 192; // max value __with these patches__ is 255
                           // anyways it should be 192... Because of "Write" patches.
        coronasPool = new char[coronas * sizeof(CRegisteredCorona)] {0};

        aml->WriteAddr(pGTASA + 0x676C44, (uintptr_t)coronasPool);

        aml->Write(pGTASA + 0x5A2314, "\xB4\xF5\x34\x5F", 4);
        aml->Write(pGTASA + 0x5A2B06, "\xBA\xF1\xC0\x0F", 4);
        aml->Write(pGTASA + 0x5A2F76, "\xB0\xF5\x34\x5F", 4);
        aml->Write(pGTASA + 0x5A2F24, "\xB8\xF5\x34\x5F", 4);
        aml->Write(pGTASA + 0x5A382A, "\xB2\xF5\x34\x5F", 4);
        aml->Write8(pGTASA + 0x5A3B5A, (uint8_t)coronas);
        aml->Write8(pGTASA + 0x5A3B86, (uint8_t)coronas);
        aml->Write(pGTASA + 0x5A3D94, "\xBC\xF1\xC0\x0F", 4);
        aml->Write8(pGTASA + 0x5A4C9C, (uint8_t)coronas);
        aml->Write8(pGTASA + 0x5A4CA2, (uint8_t)coronas);
    }
  #else
    // EntityIPL limit
    if(*(uint32_t*)(pGTASA + 0x33B170) == 0xF0002C16)
    {
        static void** entityIplPool; // default is 40

        int entitiesIpl = 1024;
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

        int entitiesPerIpl = 32768;
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

    // Visible entity ptrs
    /*if(*(uintptr_t*)(pGTASA + 0x84D210) == (pGTASA + 0xBCF900))
    {
        static void** visiblesPool; // default is 1024

        int visibles = 12000;
        visiblesPool = new void*[visibles] {0};

        aml->WriteAddr(pGTASA + 0x84D210, (uintptr_t)visiblesPool);


    }*/

    // Coronas Limit
    if(*(uintptr_t*)(pGTASA + 0x84B8D0) == (pGTASA + 0xCC6A80))
    {
        static void* coronasPool; // default is 64

        int coronas = 256; // max value __with these patches__ is 819 (because of MOVN)
        coronasPool = new char[coronas * sizeof(CRegisteredCorona)] {0};

        aml->WriteAddr(pGTASA + 0x84B8D0, (uintptr_t)coronasPool);

        aml->Write32(pGTASA + 0x6C5CA4, ARMv8::MOVBits::Create (coronas, 20, false));
        aml->Write32(pGTASA + 0x6C6440, ARMv8::CMPBits::Create (coronas, 23, true ));
        aml->Write32(pGTASA + 0x6C67DC, ARMv8::CMPBits::Create (coronas, 22, true ));
        aml->Write32(pGTASA + 0x6C6834, ARMv8::MOVBits::Create (coronas,  8, false));
        aml->Write32(pGTASA + 0x6C7084, ARMv8::MOVNBits::Create(coronas * sizeof(CRegisteredCorona), 8, true ));
        aml->Write32(pGTASA + 0x6C7380, ARMv8::CMPBits::Create (coronas, 14, false));
        aml->Write32(pGTASA + 0x6C73B4, ARMv8::CMPBits::Create (coronas, 14, false));
        aml->Write32(pGTASA + 0x6C75B4, ARMv8::CMPBits::Create (coronas, 12, false));
        aml->Write32(pGTASA + 0x6C822C, ARMv8::CMPBits::Create (coronas,  8, true ));
        aml->Write32(pGTASA + 0x6C823C, ARMv8::CMPBits::Create (coronas,  9, false));
    }
  #endif
}