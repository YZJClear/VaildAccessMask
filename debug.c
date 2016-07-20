#include "ntddk.h"
#define DELAY_ONE_MICROSECOND (-10)
#define DELAY_ONE_MILLISECOND (DELAY_ONE_MICROSECOND*1000)

KTIMER PassObjTimer;
KDPC PassObjDpc;
LARGE_INTEGER PassObjTime;

ULONGLONG Validaccessmask;

typedef struct _SYSTEM_SERVICE_TABLE
{
    PVOID ServiceTableBase;
    PVOID ServiceCounterTableBase;
    ULONGLONG NumberOfServices;
    PVOID ParamTableBase;
}SYSTEM_SERVICE_TABLE,*PSYSTEM_SERVICE_TABLE;

ULONGLONG MyGetKeServiceDescriptorTable64 ()
{
    PUCHAR StartSearchAddress = (PUCHAR)__readmsr(0xC0000082);
    PUCHAR EndSearchAddress = StartSearchAddress+ 500;
    PUCHAR i= NULL; 
    UCHAR b1=0,b2=0,b3=0;
    ULONG templong=0;
    ULONGLONG addr = 0;
    for (i = StartSearchAddress;i<EndSearchAddress;i++)
    {
        if(MmIsAddressValid(i)&& MmIsAddressValid(i+1)&&MmIsAddressValid(i+2))
        {
            b1=*i;
            b2 = *(i+1);
            b3 = *(i+2);
            if(b1 == 0x4c&& b2 == 0x8d && b3 == 0x15)
            {
                memcpy(&templong,i+3,4);
                addr = (ULONGLONG)templong + (ULONGLONG)i+7;
                return addr;
            }
        }
    }
    KdPrint(("==%x",StartSearchAddress));
    return 0;
}


ULONGLONG GetSSDTFunAddress64()
{
    LONG dwtemp = 0;
    ULONGLONG qwtemp=0,stb=0,ret=0;
    PSYSTEM_SERVICE_TABLE ssdt = (PSYSTEM_SERVICE_TABLE)MyGetKeServiceDescriptorTable64();
    stb = (ULONGLONG)(ssdt->ServiceTableBase);
    qwtemp = stb + 4 * 144 ;
    dwtemp = *(PLONG)qwtemp;
    dwtemp = dwtemp >> 4;
    ret = stb +(LONG64)dwtemp;
    return ret;
}

ULONGLONG readpoint()
{
    ULONGLONG readp,result;
    UCHAR savehex[4];
    ULONG debugobjectaddress;
    readp =GetSSDTFunAddress64()+0x7c;
    KdPrint(("==%p",readp));
    savehex[3] = *(UCHAR*)(readp+6);
    savehex[2] = *(UCHAR*)(readp+5);
    savehex[1] = *(UCHAR*)(readp+4);
    savehex[0] = *(UCHAR*)(readp+3);

    debugobjectaddress = *(ULONG*)savehex +(ULONG)readp+7 ;

    KdPrint(("%p",debugobjectaddress));
    (ULONGLONG) result =(ULONGLONG)( readp&0xffffffff00000000) + (ULONGLONG)debugobjectaddress;
    KdPrint(("hex==%x",*(ULONG*)savehex));

    return result;
}


VOID RemoveVaildMaskObj(
        __in struct _KDPC  *Dpc,
        __in_opt PVOID  DeferredContext,
        __in_opt PVOID  SystemArgument1,
        __in_opt PVOID  SystemArgument2
        )
{
    __try
    {

        *((ULONG*)Validaccessmask) = 0x1F000F;
        //*((ULONG*)validaccessmask) = 0;
        KdPrint(("Validaccessmask==%x",*(ULONGLONG*)Validaccessmask ));  

    }
    __except (1)
    {
        KeCancelTimer(&PassObjTimer);
        return;
    }
    KeSetTimer(&PassObjTimer, PassObjTime, &PassObjDpc);
    return;
}

VOID RecoveryValidAccessMask()
{
    ULONGLONG resultaddress = readpoint();

    Validaccessmask = *(ULONGLONG*)resultaddress +0x40 +0x1c ;

    KdPrint(("Validaccessmask==%x",*(ULONGLONG*)Validaccessmask ));  

    PassObjTime.QuadPart = -10000 * 100;
    KeInitializeTimer(&PassObjTimer);
    KeInitializeDpc(&PassObjDpc, &RemoveVaildMaskObj, NULL);
    KeSetTimer(&PassObjTimer, PassObjTime, &PassObjDpc);
}

VOID UnPassObjectMask()
{
    KeCancelTimer(&PassObjTimer);
}

VOID Unload(PDRIVER_OBJECT pDriverObject)
{
    UnPassObjectMask();
}
NTSTATUS DriverEntry(PDRIVER_OBJECT pDriverObject,PUNICODE_STRING Reg_Path)
{
    RecoveryValidAccessMask();
    pDriverObject->DriverUnload = Unload;
    return STATUS_SUCCESS;
}

