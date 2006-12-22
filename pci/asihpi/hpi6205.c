/******************************************************************************

    AudioScience HPI driver
    Copyright (C) 1997-2003  AudioScience Inc. <support@audioscience.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of version 2 of the GNU General Public License as
    published by the Free Software Foundation;

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA


 Hardware Programming Interface (HPI) for AudioScience ASI5000
 series adapters.
 These PCI bus adapters are based on a TI C6205 PCI bus mastering DSP

 Exported functions:
 void HPI_6205( HPI_MESSAGE *phm, HPI_RESPONSE *phr )

 #defines
 HPI6205_EE_PCI_CONFIG_FAILED
	 NO EEPROM BOOT MODE - development only
	 SUBSYS ID = 0x00000000
	 HSR bit C6205_HSR_EEREAD ste to 0
 TEST_8700_ON_5000
	 Load 87xx code instead of 5000 code onto ASI5000 card.

(C) Copyright AudioScience Inc. 1998-2003
*******************************************************************************/
#include "hpi.h"
#include "hpidebug.h"
#include "hpi6205.h"
#include "hpidspcd.h"
#include "hpicmn.h"

////////////////////////////////////////////////////////////////////////////
// local defines
#define MAX_ISTREAMS HPI_MAX_STREAMS
#define MAX_OSTREAMS HPI_MAX_STREAMS

// for C6205 PCI i/f
// Host Status Register (HSR) bitfields
#define C6205_HSR_INTSRC 	0x01
#define C6205_HSR_INTAVAL 	0x02
#define C6205_HSR_INTAM 	0x04
#define C6205_HSR_CFGERR 	0x08
#define C6205_HSR_EEREAD 	0x10
// Host-to-DSP Control Register (HDCR) bitfields
#define C6205_HDCR_WARMRESET 	0x01
#define C6205_HDCR_DSPINT	 	0x02
#define C6205_HDCR_PCIBOOT	 	0x04
// DSP Page Register (DSPP) bitfields (defines 4 Mbyte page that BAR0 points to).
#define C6205_DSPP_MAP1 	0x400

// BAR0 maps to prefetchable 4 Mbyte memory block set by DSPP.
// BAR1 maps to non-prefetchable 8 Mbyte memory block of DSP memory mapped registers (starting at 0x01800000).
// 0x01800000 is hardcoded in the PCI i/f, so that only the offset from this needs to be added to the BAR1
// base address set in the PCI config reg
#define C6205_BAR1_PCI_IO_OFFSET (0x027FFF0L)
#define C6205_BAR1_HSR (C6205_BAR1_PCI_IO_OFFSET)
#define C6205_BAR1_HDCR (C6205_BAR1_PCI_IO_OFFSET+4)
#define C6205_BAR1_DSPP (C6205_BAR1_PCI_IO_OFFSET+8)

#define C6205_BAR0_TIMER1_CTL (0x01980000L)	// used to control LED (revA) and reset C6713 (revB)

// For first 6713 in CE1 space, using DA17,16,2
#define HPICL_ADDR		0x01400000L
#define HPICH_ADDR     	0x01400004L
#define HPIAL_ADDR     	0x01410000L
#define HPIAH_ADDR     	0x01410004L
#define HPIDIL_ADDR    	0x01420000L
#define HPIDIH_ADDR    	0x01420004L
#define HPIDL_ADDR     	0x01430000L
#define HPIDH_ADDR     	0x01430004L

#define C6713_EMIF_GCTL   		0x01800000
#define C6713_EMIF_CE1			0x01800004
#define C6713_EMIF_CE0        	0x01800008
#define C6713_EMIF_CE2        	0x01800010
#define C6713_EMIF_CE3        	0x01800014
#define C6713_EMIF_SDRAMCTL   	0x01800018
#define C6713_EMIF_SDRAMTIMING  0x0180001C
#define C6713_EMIF_SDRAMEXT   	0x01800020

typedef struct
{

	 // PCI registers
	 __iomem HW32 *	prHSR;
	 __iomem HW32 *	prHDCR;
	 __iomem HW32 *	prDSPP;

#ifdef HPI_OS_WIN16
	HW16 awBar0Selectors[0x400000L/0x10000L];	// 4 Mbytes of 64k selectors
	HW16 awBar1Selectors[0x800000L/0x10000L];	// 8 Mbytes of 64k selectors
#endif
	HW32 dwDspPage;

	HpiOs_LockedMem_Handle hLockedMem;
	tBusMasteringInterfaceBuffer *pInterfaceBuffer;

	HW16 flagOStreamJustReset[MAX_OSTREAMS];
	HpiOs_LockedMem_Handle InStreamHostBuffers[MAX_ISTREAMS];
	HpiOs_LockedMem_Handle OutStreamHostBuffers[MAX_OSTREAMS];
	HW32 InStreamHostBufferSize[MAX_ISTREAMS];
	HW32 OutStreamHostBufferSize[MAX_OSTREAMS];

	HpiOs_LockedMem_Handle hControlCache;
	HpiOs_LockedMem_Handle hAsyncEventBuffer;
	tHPIControlCacheSingle *pControlCache;
	HPI_ASYNC_EVENT *pAsyncEventBuffer;
}
HPI_HW_OBJ;

////////////////////////////////////////////////////////////////////////////
// local prototypes

#ifdef HPI_OS_WIN16
HW32 * Hpi6205_Abstract_ComputeAddress(HPI_ADAPTER_OBJ *pao, HW32 dwAddress);
#endif

static short Hpi6205_AdapterBootLoadDsp( HPI_ADAPTER_OBJ *pao, HW32 *pdwOsErrorCode );
static short Hpi6205_MessageResponseSequence(HPI_ADAPTER_OBJ *pao,HPI_MESSAGE *phm,HPI_RESPONSE *phr);
static void  HW_Message( HPI_ADAPTER_OBJ *pao, HPI_MESSAGE *phm, HPI_RESPONSE *phr);
static HW16  Hpi6205_Error( int nDspIndex, int nError );

#define TIMEOUT 1000000

static void SubSysCreateAdapter(HPI_ADAPTERS_LIST *adaptersList, HPI_MESSAGE *phm, HPI_RESPONSE *phr);
static void SubSysDeleteAdapter(HPI_ADAPTERS_LIST *adaptersList, HPI_MESSAGE *phm, HPI_RESPONSE *phr);

static void AdapterGetAsserts(HPI_ADAPTER_OBJ *pao,HPI_MESSAGE *phm, HPI_RESPONSE *phr);

static short CreateAdapterObj( HPI_ADAPTER_OBJ *pao, HW32 *pdwOsErrorCode );
static void DeleteAdapterObj(HPI_ADAPTER_OBJ *pao);

static void OutStreamHostBufferAllocate(HPI_ADAPTER_OBJ *pao,HPI_MESSAGE *phm, HPI_RESPONSE *phr);
static void OutStreamHostBufferFree(HPI_ADAPTER_OBJ *pao,HPI_MESSAGE *phm, HPI_RESPONSE *phr);
static void OutStreamWrite(HPI_ADAPTER_OBJ *pao,HPI_MESSAGE *phm, HPI_RESPONSE *phr);
static void OutStreamGetInfo(HPI_ADAPTER_OBJ *pao,HPI_MESSAGE *phm, HPI_RESPONSE *phr);
static void OutStreamStart(HPI_ADAPTER_OBJ *pao,HPI_MESSAGE *phm, HPI_RESPONSE *phr);	// for debug
static void OutStreamOpen(HPI_ADAPTER_OBJ *pao,HPI_MESSAGE *phm, HPI_RESPONSE *phr);
static void OutStreamReset(HPI_ADAPTER_OBJ *pao,HPI_MESSAGE *phm, HPI_RESPONSE *phr);

static void InStreamHostBufferAllocate(HPI_ADAPTER_OBJ *pao,HPI_MESSAGE *phm, HPI_RESPONSE *phr);
static void InStreamHostBufferFree(HPI_ADAPTER_OBJ *pao,HPI_MESSAGE *phm, HPI_RESPONSE *phr);
static void InStreamRead(HPI_ADAPTER_OBJ *pao,HPI_MESSAGE *phm, HPI_RESPONSE *phr);
static void InStreamGetInfo(HPI_ADAPTER_OBJ *pao,HPI_MESSAGE *phm, HPI_RESPONSE *phr);
static void InStreamStart(HPI_ADAPTER_OBJ *pao,HPI_MESSAGE *phm, HPI_RESPONSE *phr);	// for debug

static HW32 BootLoader_ReadMem32(HPI_ADAPTER_OBJ *pao, int nDSPIndex, HW32 dwAddress);
static HW16 BootLoader_WriteMem32(HPI_ADAPTER_OBJ *pao, int nDSPIndex, HW32 dwAddress, HW32 dwData);
static HW16 BootLoader_ConfigEMIF(HPI_ADAPTER_OBJ *pao, int nDSPIndex);
static HW16 BootLoader_TestMemory(HPI_ADAPTER_OBJ *pao, int nDSPIndex, HW32 dwAddress, HW32 dwLength);
static HW16 BootLoader_TestInternalMemory(HPI_ADAPTER_OBJ *pao, int nDSPIndex);
static HW16 BootLoader_TestExternalMemory(HPI_ADAPTER_OBJ *pao, int nDSPIndex);
//static HW16 BootLoader_BlockWrite32( HPI_ADAPTER_OBJ *pao, HW16 wDspIndex, HW32 dwDspDestinationAddress, HW32 dwSourceAddress, HW32 dwCount);
static HW16 BootLoader_TestPld(HPI_ADAPTER_OBJ *pao, int nDSPIndex);


////////////////////////////////////////////////////////////////////////////
// local globals
static HW32 gadwHpiSpecificError[4];
static HPI_ADAPTERS_LIST adapters;

static void SubSysMessage( HPI_ADAPTERS_LIST *adaptersList, HPI_MESSAGE *phm, HPI_RESPONSE *phr ) {

	switch(phm->wFunction) {
		case HPI_SUBSYS_OPEN:
		case HPI_SUBSYS_CLOSE:
		case HPI_SUBSYS_DRIVER_UNLOAD:
			phr->wError=0;
			break;
		case HPI_SUBSYS_DRIVER_LOAD:
			WipeAdapterList( adaptersList );
			phr->wError=0;
			break;
		case HPI_SUBSYS_GET_INFO:
			SubSysGetAdapters( adaptersList, phr );
			break;
		case HPI_SUBSYS_FIND_ADAPTERS:
			phr->wError = HPI_ERROR_INVALID_FUNC;
			break;
		case HPI_SUBSYS_CREATE_ADAPTER:
			SubSysCreateAdapter( adaptersList, phm, phr );
			break;
		case HPI_SUBSYS_DELETE_ADAPTER:
			SubSysDeleteAdapter( adaptersList, phm, phr );
			break;
		default:
			phr->wError = HPI_ERROR_INVALID_FUNC;
			break;
	}
}

static void ControlMessage( HPI_ADAPTER_OBJ *pao, HPI_MESSAGE *phm, HPI_RESPONSE *phr ) {

	HPI_HW_OBJ *pHw6205 = pao->priv;

	switch(phm->wFunction) {
		case HPI_CONTROL_GET_STATE:
			if (pao->wHasControlCache)
				if (CheckControlCache(
					&pHw6205->pControlCache[phm->u.c.wControlIndex],
					phm, phr))
					break;
			HW_Message( pao, phm, phr );
			break;
		case HPI_CONTROL_GET_INFO:
			HW_Message( pao, phm, phr);
			break;
		case HPI_CONTROL_SET_STATE:
			HW_Message( pao, phm, phr);
			if (pao->wHasControlCache)
				SyncControlCache(&pHw6205->pControlCache[phm->u.c.wControlIndex], phm, phr);
			break;
		default:
			phr->wError = HPI_ERROR_INVALID_FUNC;
			break;
	}
}

static void AdapterMessage( HPI_ADAPTER_OBJ *pao, HPI_MESSAGE *phm, HPI_RESPONSE *phr ) {

	switch(phm->wFunction) {
		case HPI_ADAPTER_GET_INFO:
			HW_Message( pao, phm, phr);
			break;
		case HPI_ADAPTER_GET_ASSERT:
			AdapterGetAsserts( pao, phm, phr );
			break;
		case HPI_ADAPTER_OPEN:
		case HPI_ADAPTER_CLOSE:
		case HPI_ADAPTER_TEST_ASSERT:
		case HPI_ADAPTER_SELFTEST:
		case HPI_ADAPTER_GET_MODE:
		case HPI_ADAPTER_SET_MODE:
		case HPI_ADAPTER_FIND_OBJECT:
		case HPI_ADAPTER_GET_PROPERTY:
			HW_Message( pao, phm, phr);
			break;
		default:
			phr->wError = HPI_ERROR_INVALID_FUNC;
			break;
	}
}

static void OStreamMessage( HPI_ADAPTER_OBJ *pao, HPI_MESSAGE *phm, HPI_RESPONSE *phr ) {

	if ( phm->u.d.wStreamIndex >= MAX_OSTREAMS ) {
		phr->wError = HPI_ERROR_INVALID_STREAM;
		HPI_DEBUG_LOG2(WARNING,"Message referencing invalid stream %d on adapter index %d\n",
				phm->u.d.wStreamIndex, phm->wAdapterIndex);
		return;
	}

	switch(phm->wFunction) {
		case HPI_OSTREAM_WRITE:
			OutStreamWrite( pao, phm, phr );
			break;
		case HPI_OSTREAM_GET_INFO:
			OutStreamGetInfo( pao, phm, phr );
			break;
		case HPI_OSTREAM_HOSTBUFFER_ALLOC:
			OutStreamHostBufferAllocate( pao, phm, phr );
			break;
		case HPI_OSTREAM_HOSTBUFFER_FREE:
			OutStreamHostBufferFree( pao, phm, phr );
			break;
		case HPI_OSTREAM_START:
			OutStreamStart( pao, phm, phr );
			break;
		case HPI_OSTREAM_OPEN:
			OutStreamOpen( pao, phm, phr );
			break;
		case HPI_OSTREAM_RESET:
			OutStreamReset( pao, phm, phr );
			break;
		default:
			HW_Message( pao, phm, phr );
			break;
	}
}

static void IStreamMessage( HPI_ADAPTER_OBJ *pao, HPI_MESSAGE *phm, HPI_RESPONSE *phr ) {

	if ( phm->u.d.wStreamIndex >= MAX_ISTREAMS ) {
		phr->wError = HPI_ERROR_INVALID_STREAM;
		HPI_DEBUG_LOG2(WARNING,"Message referencing invalid stream %d on adapter index %d\n",
				phm->u.d.wStreamIndex, phm->wAdapterIndex);
		return;
	}

	switch(phm->wFunction) {
		case HPI_ISTREAM_READ:
			InStreamRead( pao, phm, phr );
			break;
		case HPI_ISTREAM_GET_INFO:
			InStreamGetInfo( pao, phm, phr );
			break;
		case HPI_ISTREAM_HOSTBUFFER_ALLOC:
			InStreamHostBufferAllocate( pao, phm, phr );
			break;
		case HPI_ISTREAM_HOSTBUFFER_FREE:
			InStreamHostBufferFree( pao, phm, phr );
			break;
		case HPI_ISTREAM_START:
			InStreamStart( pao, phm, phr );
			break;
		default:
			HW_Message( pao, phm, phr);
			break;
	}
}

////////////////////////////////////////////////////////////////////////////
// HPI_6205()
// Entry point from HPIMAN
// All calls to the HPI start here

void HPI_6205( HPI_MESSAGE *phm, HPI_RESPONSE *phr )
{
	HPI_ADAPTER_OBJ *pao = NULL;

	// subsytem messages get executed by every HPI.
	// All other messages are ignored unless the adapter index matches
	// an adapter in the HPI
	HPI_DEBUG_LOG2(DEBUG,"HPI Obj=%d, Func=%d\n", phm->wObject, phm->wFunction);

	//phr->wError=0;      // SGT JUN-15-01 - so that modules can't overwrite errors from FindAdapter

	// if Dsp has crashed then do not try and communicated with it any more
	if( phm->wObject != HPI_OBJ_SUBSYSTEM ) {
		pao = FindAdapter( &adapters, phm->wAdapterIndex );
	        if ( !pao ) {
			HPI_DEBUG_LOG2(DEBUG," %d,%d refused, for another HPI?\n", phm->wObject, phm->wFunction);
			return; // message probably meant for another HPI module
		}

		if( pao->wDspCrashed ) {
			HPI_InitResponse(phr, phm->wObject, phm->wFunction, HPI_ERROR_DSP_HARDWARE );
			HPI_DEBUG_LOG2(WARNING," %d,%d dsp crashed.\n", phm->wObject, phm->wFunction);
			return;
        	}
	}

	//Init default response ( sets the size field in the response structure )
	if( phm->wFunction != HPI_SUBSYS_CREATE_ADAPTER )
		HPI_InitResponse(phr, phm->wObject, phm->wFunction, HPI_ERROR_PROCESSING_MESSAGE );

	HPI_DEBUG_LOG0(VERBOSE,"start of switch\n");
	switch(phm->wType) {
		case HPI_TYPE_MESSAGE:
		switch(phm->wObject) {
			case HPI_OBJ_SUBSYSTEM:
				SubSysMessage( &adapters, phm, phr );
				break;

			case HPI_OBJ_ADAPTER:
				phr->wSize=HPI_RESPONSE_FIXED_SIZE + sizeof(HPI_ADAPTER_RES);
				AdapterMessage( pao, phm, phr );
				break;

			case HPI_OBJ_CONTROL:
				ControlMessage( pao, phm, phr );
				break;

			case HPI_OBJ_OSTREAM:
				OStreamMessage( pao, phm, phr );
				break;

			case HPI_OBJ_ISTREAM:
				IStreamMessage( pao, phm, phr );
				break;

			default:
				HW_Message( pao, phm, phr );
				break;
		}
		break;

		default:
			phr->wError = HPI_ERROR_INVALID_TYPE;
			break;
	}
}

////////////////////////////////////////////////////////////////////////////
// SUBSYSTEM

// Create an adapter object and initialise it based on resource information
// passed in in the message
// **** NOTE - you cannot use this function AND the FindAdapters function at the
// same time, the application must use only one of them to get the adapters ******

static void SubSysCreateAdapter(HPI_ADAPTERS_LIST *adaptersList, HPI_MESSAGE *phm, HPI_RESPONSE *phr)
{
	HPI_ADAPTER_OBJ ao;	// create temp adapter obj, because we don't know what index yet
	HW32 dwOsErrorCode;
	short nError=0;

	HPI_DEBUG_LOG0(DEBUG, " SubSysCreateAdapter\n" );

	memset( &ao, 0, sizeof(HPI_ADAPTER_OBJ) );

	// this HPI only creates adapters for TI/PCI devices
	if (phm->u.s.Resource.wBusType != HPI_BUS_PCI)
		return;
	if ( phm->u.s.Resource.r.Pci->wVendorId != HPI_PCI_VENDOR_ID_TI )
		return;
	if ( phm->u.s.Resource.r.Pci->wDeviceId != HPI_ADAPTER_DSP6205 )
		return;

	ao.priv = HpiOs_MemAlloc( sizeof(HPI_HW_OBJ) );
	memset( ao.priv, 0, sizeof(HPI_HW_OBJ) );
	// create the adapter object based on the resource information
	//?memcpy(&ao.Pci,phm->u.s.Resource.r.Pci,sizeof(ao.Pci));
	ao.Pci=*phm->u.s.Resource.r.Pci;

	nError = CreateAdapterObj( &ao, &dwOsErrorCode );
	if(nError)
	{
		phr->u.s.dwData = dwOsErrorCode;	// OS error, if any, is in dwOsErrorCode
		DeleteAdapterObj(&ao);
		phr->wError = nError;
		return;
	}

	// add to adapter list - but don't allow two adapters of same number!
	if( phr->u.s.awAdapterList[ ao.wIndex ] != 0)
	{
		DeleteAdapterObj(&ao);
		phr->wError = HPI_DUPLICATE_ADAPTER_NUMBER;
		return;
	}
	//memcpy( &adaptersList->adapter[ ao.wIndex ], &ao, sizeof(HPI_ADAPTER_OBJ));
	adaptersList->adapter[ ao.wIndex ] = ao;

	HpiOs_Dsplock_Init( &adaptersList->adapter[ ao.wIndex ]);

	adaptersList->gwNumAdapters++;         // inc the number of adapters known by this HPI
	phr->u.s.awAdapterList[ ao.wIndex ] = ao.wAdapterType;
	phr->u.s.wAdapterIndex = ao.wIndex;
	phr->u.s.wNumAdapters++;    // add the number of adapters recognised by this HPI to the system total
	phr->wError = 0;            // the function completed OK;
}

// delete an adapter - required by WDM driver
static void SubSysDeleteAdapter(HPI_ADAPTERS_LIST *adaptersList, HPI_MESSAGE *phm, HPI_RESPONSE *phr)
{
	HPI_ADAPTER_OBJ *pao = NULL;

	pao = FindAdapter( adaptersList, phm->wAdapterIndex );
	if(!pao) return; // message probably meant for another HPI module

	DeleteAdapterObj(pao);
	adaptersList->gwNumAdapters--;
	phr->wError = 0;
}


// this routine is called from SubSysFindAdapter and SubSysCreateAdapter
static short CreateAdapterObj( HPI_ADAPTER_OBJ *pao, HW32 *pdwOsErrorCode )
{
	HPI_HW_OBJ *pHw6205 = pao->priv;
	tBusMasteringInterfaceBuffer *interface;
	HW32 dwPhysAddr;
	HW32 dwTimeOut = TIMEOUT;
	volatile HW32 dwTemp1;
	int i;
	short nBootError=0;
	HW16 wError;

	// init error reporting
	pao->wDspCrashed = 0;

#ifdef HPI_OS_DOS
	// This HPI does not currently support DOS.
	// can't work in DOS mode because
	// BAR0 is 4Mbytes
	// BAR1 is 8Mbytes
	// BAR2 is 4 16bit I/O registers
	return HPI_ERROR_BAD_ADAPTER;
#endif

	for(i=0;i<MAX_OSTREAMS;i++)
		pHw6205->flagOStreamJustReset[i]=1;

#ifdef HPI_OS_WIN16
	HpiPci_TranslateAddressRange(&(pao->Pci),0,&(pHw6205->awBar0Selectors[0]), 0x400000L/0x10000L);
	HpiPci_TranslateAddressRange(&(pao->Pci),1,&(pHw6205->awBar1Selectors[0]), 0x800000L/0x10000L);
	pao->Pci.apMemBase[0] = 0;				// under Win16 use artifical memory addresses as they are only used to
	pao->Pci.apMemBase[1] = (HW32 *)0x4000000;		// lookup up selector indices.
	pHw6205->prHSR = Hpi6205_Abstract_ComputeAddress(pao,(0x4000000 + C6205_BAR1_HSR));
	pHw6205->prHDCR = Hpi6205_Abstract_ComputeAddress(pao,(0x4000000 + C6205_BAR1_HDCR));
	pHw6205->prDSPP = Hpi6205_Abstract_ComputeAddress(pao,(0x4000000 + C6205_BAR1_DSPP));
#else
	// The C6205 has the following address map
	// BAR0 - 4Mbyte window into DSP memory
	// BAR1 - 8Mbyte window into DSP registers
	pHw6205->prHSR = pao->Pci.apMemBase[1] + C6205_BAR1_HSR/sizeof(*pao->Pci.apMemBase[1]);
	pHw6205->prHDCR = pao->Pci.apMemBase[1] + C6205_BAR1_HDCR/sizeof(*pao->Pci.apMemBase[1]);
	pHw6205->prDSPP = pao->Pci.apMemBase[1] + C6205_BAR1_DSPP/sizeof(*pao->Pci.apMemBase[1]);
#endif
	pao->wHasControlCache = 0;

	if( HpiOs_LockedMem_Alloc(	&pHw6205->hLockedMem,
								sizeof(tBusMasteringInterfaceBuffer),
								(void *)pao->Pci.pOsData
								) )
		pHw6205->pInterfaceBuffer = NULL;
	else
		if(HpiOs_LockedMem_GetVirtAddr( pHw6205->hLockedMem, (void *)&pHw6205->pInterfaceBuffer ))
			pHw6205->pInterfaceBuffer = NULL;

	HPI_DEBUG_LOG1(DEBUG,"Interface buffer address %p\n",pHw6205->pInterfaceBuffer);
	if(pHw6205->pInterfaceBuffer) {
		memset((void *)pHw6205->pInterfaceBuffer,0,sizeof(tBusMasteringInterfaceBuffer));
		pHw6205->pInterfaceBuffer->dwDspAck = -1;
	}

	if (0 != (nBootError = Hpi6205_AdapterBootLoadDsp(pao,pdwOsErrorCode)))
	{
		return(nBootError); //error - no need to clean up as SubSysCreateAdapter calls DeleteAdapter on error.
	}
	HPI_DEBUG_LOG0(INFO, "Load DSP code OK\n" );

	// allow boot load even if mem alloc wont work
	if (!pHw6205->pInterfaceBuffer)
		return( Hpi6205_Error(0, HPI6205_ERROR_MEM_ALLOC));

	interface=pHw6205->pInterfaceBuffer;

	// wait for first interrupt indicating the DSP init is done
	dwTimeOut = TIMEOUT*10;
	dwTemp1=0;
	while(((dwTemp1 & C6205_HSR_INTSRC) == 0) && --dwTimeOut)
	{
		dwTemp1 = HPIOS_MEMREAD32(pHw6205->prHSR);
	}
	if(dwTemp1 & C6205_HSR_INTSRC)
	{
		HPI_DEBUG_LOG0(INFO,"Interrupt confirming DSP code running OK\n");
	}
	else
	{
		HPI_DEBUG_LOG0(ERROR,"Timed out waiting for interrupt confirming DSP code running\n");
		return(Hpi6205_Error( 0, HPI6205_ERROR_6205_NO_IRQ));
	}

	// reset the interrupt
	HPIOS_MEMWRITE32(pHw6205->prHSR,C6205_HSR_INTSRC);

	// make sure the DSP has started ok
	dwTimeOut = 100;
	while( (interface->dwDspAck != H620_HIF_RESET) && dwTimeOut )
	{
		dwTimeOut--;
		HpiOs_DelayMicroSeconds(10000);
	}

	if(dwTimeOut==0) {
		HPI_DEBUG_LOG0(ERROR,"Timed out waiting ack \n");
	    return(Hpi6205_Error( 0, HPI6205_ERROR_6205_INIT_FAILED));
	}
    // Note that *pao, *pHw6205 are zeroed after allocation, so pointers and flags are NULL by default.
	// allocate bus mastering control cache buffer and tell the DSP about it
	if(interface->aControlCache.dwNumberOfControls) {
		wError = HpiOs_LockedMem_Alloc(
								&pHw6205->hControlCache,
								interface->aControlCache.dwNumberOfControls*sizeof(tHPIControlCacheSingle),
								(void *)pao->Pci.pOsData
								);
		if(!wError)
			wError = HpiOs_LockedMem_GetVirtAddr(
									pHw6205->hControlCache,
									(void *)&pHw6205->pControlCache
									);
		if(!wError)
			memset((void *)pHw6205->pControlCache,0,
					interface->aControlCache.dwNumberOfControls*sizeof(tHPIControlCacheSingle));
		if(!wError) {
			wError = HpiOs_LockedMem_GetPhysAddr(pHw6205->hControlCache,&dwPhysAddr);
			interface->aControlCache.dwPhysicalPCI32address=dwPhysAddr;
		}

		if(!wError)
			pao->wHasControlCache=1;
		else
		{
			if(pHw6205->hControlCache)
			{
				HpiOs_LockedMem_Free(pHw6205->hControlCache);
				pHw6205->pControlCache = NULL;
			}
			pao->wHasControlCache=0;
		}
	}

	// allocate bus mastering async buffer and tell the DSP about it
	if(interface->aAsyncBuffer.b.dwSize) {
		wError = HpiOs_LockedMem_Alloc(
								&pHw6205->hAsyncEventBuffer,
								interface->aAsyncBuffer.b.dwSize*sizeof(HPI_ASYNC_EVENT),
								(void *)pao->Pci.pOsData
								);
		if(!wError)
			wError = HpiOs_LockedMem_GetVirtAddr(
									pHw6205->hAsyncEventBuffer,
									(void *)&pHw6205->pAsyncEventBuffer
									);
		if(!wError)
			memset((void *)pHw6205->pAsyncEventBuffer,0,
					interface->aAsyncBuffer.b.dwSize*sizeof(HPI_ASYNC_EVENT));
		if(!wError) {
			wError = HpiOs_LockedMem_GetPhysAddr(pHw6205->hAsyncEventBuffer,&dwPhysAddr);
			interface->aAsyncBuffer.dwPhysicalPCI32address=dwPhysAddr;
		}
		if(wError) {
			if(pHw6205->hAsyncEventBuffer) {
				HpiOs_LockedMem_Free( pHw6205->hAsyncEventBuffer );
				pHw6205->pAsyncEventBuffer = NULL;
			}
		}
	}

	// set interface to idle
	interface->dwHostCmd = H620_HIF_IDLE;
	// interrupt the DSP again
	dwTemp1 = HPIOS_MEMREAD32(pHw6205->prHDCR );  //read the control register
	dwTemp1 |= (HW32)C6205_HDCR_DSPINT;
	HPIOS_MEMWRITE32(pHw6205->prHDCR,dwTemp1);
	dwTemp1 &= ~(HW32)C6205_HDCR_DSPINT;
	HPIOS_MEMWRITE32(pHw6205->prHDCR,dwTemp1);

	// get info about the adapter by asking the adapter
	// send a HPI_ADAPTER_GET_INFO message
	{
		HPI_MESSAGE     hM;
		HPI_RESPONSE    hR;
		//HW16 wError=0;

		//HpiOs_Printf("GetInfo-"); //*************** debug
		HPI_DEBUG_LOG0(VERBOSE, "HPI6205.C - send ADAPTER_GET_INFO\n" );
		memset(&hM, 0, sizeof(HPI_MESSAGE));
		hM.wType = HPI_TYPE_MESSAGE;
		hM.wSize = sizeof(HPI_MESSAGE);
		hM.wObject = HPI_OBJ_ADAPTER;
		hM.wFunction =  HPI_ADAPTER_GET_INFO;
		hM.wAdapterIndex = 0;
		memset(&hR, 0, sizeof(HPI_RESPONSE));
		hR.wSize = sizeof(HPI_RESPONSE);

		wError = Hpi6205_MessageResponseSequence( pao,&hM,&hR );
		if(wError)
		{
		    HPI_DEBUG_LOG1(ERROR, "message transport error %d\n",wError );
			return(wError); //error
		}
		if(hR.wError)
		{
			return(hR.wError); //error
		}
		pao->wAdapterType = hR.u.a.wAdapterType;
		pao->wIndex = hR.u.a.wAdapterIndex;
	}

	HPI_DEBUG_LOG0(VERBOSE, "Get adapter info OK\n" );
	pao->wOpen=0;	// upon creation the adapter is closed

	HPI_DEBUG_LOG0(INFO, "Bootload DSP OK\n" );
	return(0);  //sucess!
}

/** Free memory areas allocated by adapter
 this routine is called from SubSysDeleteAdapter, and SubSysCreateAdapter if duplicate index
*/
static void DeleteAdapterObj(HPI_ADAPTER_OBJ *pao)
{
	HPI_HW_OBJ *pHw6205;
	int i;

	pHw6205 = pao->priv;

	if(pHw6205->hAsyncEventBuffer) {
		HpiOs_LockedMem_Free( pHw6205->hAsyncEventBuffer );
		pHw6205->pAsyncEventBuffer = NULL;
	}

	if(pHw6205->hControlCache) {
		HpiOs_LockedMem_Free( pHw6205->hControlCache );
		pHw6205->pControlCache = NULL;
	}

	if(pHw6205->hLockedMem) {
		HpiOs_LockedMem_Free( pHw6205->hLockedMem );
		pHw6205->pInterfaceBuffer = NULL;
	}

	for(i=0;i<MAX_ISTREAMS;i++)
		if( pHw6205->InStreamHostBuffers[i] ) {
			HpiOs_LockedMem_Free( pHw6205->InStreamHostBuffers[i] );
			pHw6205->InStreamHostBuffers[i] = NULL;
			pHw6205->InStreamHostBufferSize[i] = 0;
        }

	for(i=0;i<MAX_OSTREAMS;i++)
		if( pHw6205->OutStreamHostBuffers[i] ) {
			HpiOs_LockedMem_Free( pHw6205->OutStreamHostBuffers[i] );
			pHw6205->OutStreamHostBuffers[i] = NULL;
			pHw6205->OutStreamHostBufferSize[i] = 0;
        }

#ifdef HPI_OS_WIN16
	HpiPci_FreeSelectors(&(pHw6205->awBar0Selectors[0]), 0x400000L/0x10000L);
	HpiPci_FreeSelectors(&(pHw6205->awBar1Selectors[0]), 0x800000L/0x10000L);
#endif

	HpiOs_MemFree( pHw6205 );
	memset( pao, 0, sizeof(HPI_ADAPTER_OBJ) );
}

////////////////////////////////////////////////////////////////////////////
// ADAPTER

static void AdapterGetAsserts(HPI_ADAPTER_OBJ *pao,HPI_MESSAGE *phm, HPI_RESPONSE *phr)
{
	HW_Message( pao, phm,phr);    //get DSP asserts
	return;
}

//////////////////////////////////////////////////////////////////////
//						OutStream Host buffer functions
//////////////////////////////////////////////////////////////////////

static void OutStreamHostBufferAllocate(HPI_ADAPTER_OBJ *pao,HPI_MESSAGE *phm, HPI_RESPONSE *phr)
{
	HW16 wError;
	HW32 dwSizeToAllocate;
	HW32 dwCommand = phm->u.d.u.Buffer.dwCommand;
	HPI_HW_OBJ *pHw6205 = pao->priv;
	tBusMasteringInterfaceBuffer *interface=pHw6205->pInterfaceBuffer;

	HPI_InitResponse(phr, phm->wObject, phm->wFunction, 0 );
	dwSizeToAllocate = phm->u.d.u.Data.dwDataSize;
	phr->u.d.u.stream_info.dwDataAvailable = pHw6205->OutStreamHostBufferSize[phm->u.d.wStreamIndex];
	phr->u.d.u.stream_info.dwBufferSize = dwSizeToAllocate;

	if( dwCommand == HPI_BUFFER_CMD_EXTERNAL ||
		dwCommand == HPI_BUFFER_CMD_INTERNAL_ALLOC )
	{
		if(pHw6205->OutStreamHostBufferSize[phm->u.d.wStreamIndex]==dwSizeToAllocate)
		{
			// Same size, no action required, use dwError to skip Hpi6205_Message() section
			// below but return success
			wError = 1;
		}
		else
		{
			if(pHw6205->OutStreamHostBuffers[phm->u.d.wStreamIndex])
				HpiOs_LockedMem_Free( pHw6205->OutStreamHostBuffers[phm->u.d.wStreamIndex] );

			wError = HpiOs_LockedMem_Alloc(
							&pHw6205->OutStreamHostBuffers[phm->u.d.wStreamIndex],
							phm->u.d.u.Data.dwDataSize,
							(void *)pao->Pci.pOsData
							);

			if(wError)
			{
				phr->wSize = HPI_RESPONSE_FIXED_SIZE + sizeof(HPI_STREAM_RES);
				phr->wError = HPI_ERROR_INVALID_DATASIZE;
				pHw6205->OutStreamHostBufferSize[phm->u.d.wStreamIndex] = 0;
			}
			else
			{
				pHw6205->OutStreamHostBufferSize[phm->u.d.wStreamIndex] = dwSizeToAllocate;
			}
		}
	} else {
		wError = 0; /*hush the compiler's warning about dwError being used uninitialized */
	}

	if( ( dwCommand == HPI_BUFFER_CMD_EXTERNAL && wError == 0 ) ||
		dwCommand == HPI_BUFFER_CMD_INTERNAL_GRANTADAPTER )
	{
		H620_HOSTBUFFER_STATUS *status;
		HW32 dwPhysicalPCIaddress;

		status = &interface->aOutStreamHostBufferStatus[phm->u.d.wStreamIndex];
		status->dwSamplesProcessed = 0;
		status->dwStreamState = HPI_STATE_STOPPED;
		status->dwDSPIndex = 0;
		status->dwHostIndex = 0;
		status->dwSizeInBytes = phm->u.d.u.Data.dwDataSize;
		wError = HpiOs_LockedMem_GetPhysAddr(pHw6205->OutStreamHostBuffers[phm->u.d.wStreamIndex], &dwPhysicalPCIaddress );
		if(wError)
		{
			phr->wError = HPI_ERROR_DOS_MEMORY_ALLOC;
		}
		else
		{
			phm->u.d.u.Buffer.dwPciAddress = dwPhysicalPCIaddress;
			HW_Message( pao, phm,phr);
			if(phr->wError && dwCommand == HPI_BUFFER_CMD_EXTERNAL )
			{
				// free the buffer
				HpiOs_LockedMem_Free( pHw6205->OutStreamHostBuffers[phm->u.d.wStreamIndex] );
				pHw6205->OutStreamHostBufferSize[phm->u.d.wStreamIndex] = 0;
				pHw6205->OutStreamHostBuffers[phm->u.d.wStreamIndex] = NULL;
			}
		}
	}
}
static void OutStreamHostBufferFree(HPI_ADAPTER_OBJ *pao,HPI_MESSAGE *phm, HPI_RESPONSE *phr)
{
	HPI_HW_OBJ *pHw6205 = pao->priv;
	HW32 dwCommand = phm->u.d.u.Buffer.dwCommand;

	if( pHw6205->OutStreamHostBuffers[phm->u.d.wStreamIndex] ) {
		if( dwCommand == HPI_BUFFER_CMD_EXTERNAL ||
			dwCommand == HPI_BUFFER_CMD_INTERNAL_REVOKEADAPTER )
		{
			HW_Message( pao, phm,phr);		// Tell adapter to stop using the host buffer.
		}
		if( dwCommand == HPI_BUFFER_CMD_EXTERNAL ||
			dwCommand == HPI_BUFFER_CMD_INTERNAL_FREE )
		{
			HpiOs_LockedMem_Free( pHw6205->OutStreamHostBuffers[phm->u.d.wStreamIndex] );
			pHw6205->OutStreamHostBufferSize[phm->u.d.wStreamIndex] = 0;
			pHw6205->OutStreamHostBuffers[phm->u.d.wStreamIndex] = NULL;
		}
	}
/* Should an error be returned if no host buffer is allocated?
	else {
		HPI_InitResponse( phr, HPI_OBJ_OSTREAM, HPI_OSTREAM_HOSTBUFFER_FREE,  HPI_ERROR_INVALID_OPERATION );

	}
*/
}

#if 1
static long OutStreamGetSpaceAvailable(H620_HOSTBUFFER_STATUS *status)
{
	long nDiff;

	// When dwDSPindex==dwHostIndex the buffer is empty
	// Need to add code to the HOST to make sure that the buffer is never filled
	// to the point that dwDSPindex==dwHostIndex.
	nDiff =  (long)(status->dwDSPIndex) - (long)(status->dwHostIndex) - 4;	// - 4 bytes at end so we don't overfill
	if(nDiff<0)
		nDiff += status->dwSizeInBytes;
	return nDiff;
}
#else
//EWB this version I can understand:
static long OutStreamGetSpaceAvailable(H620_HOSTBUFFER_STATUS *status)
{
	long nUsed,nSpace;

	// When dwDSPindex==dwHostIndex the buffer is empty
	// Need to add code to the HOST to make sure that the buffer is never filled
	// to the point that dwDSPindex==dwHostIndex.
	nUsed =  (long)(status->dwHostIndex)- (long)(status->dwDSPIndex) + 4;	// + 4 bytes at end so we don't overfill
	if(nUsed >= 0 )
		nSpace = status->dwSizeInBytes - nUsed;
	else
		nSpace = -nUsed; // status->dwSizeInBytes - (nUsed + status->dwSizeInBytes)

	return nSpace;
}
#endif

static void OutStreamWrite(HPI_ADAPTER_OBJ *pao,HPI_MESSAGE *phm, HPI_RESPONSE *phr)
{
	HPI_HW_OBJ *pHw6205 = pao->priv;
	tBusMasteringInterfaceBuffer *interface=pHw6205->pInterfaceBuffer;
	H620_HOSTBUFFER_STATUS *status;
	long dwSpaceAvailable;
	HW8 HUGE * pBBMData;
	long lFirstWrite;
	long lSecondWrite;
	HW8 HUGE *pAppData = (HW8 HUGE *)phm->u.d.u.Data.pbData;
	if( !pHw6205->OutStreamHostBuffers[phm->u.d.wStreamIndex])
	{
		HW_Message( pao, phm, phr);
		return;
	}
	HPI_InitResponse(phr, phm->wObject, phm->wFunction, 0 );

	if(HpiOs_LockedMem_GetVirtAddr( pHw6205->OutStreamHostBuffers[phm->u.d.wStreamIndex], (void *)&pBBMData ))
	{
		phr->wError = HPI_ERROR_INVALID_OPERATION;
		return;
	}

	// check whether we need to send the format to the DSP
	if(pHw6205->flagOStreamJustReset[phm->u.d.wStreamIndex])
	{
		pHw6205->flagOStreamJustReset[phm->u.d.wStreamIndex]=0;
		phm->wFunction = HPI_OSTREAM_SET_FORMAT;
		HW_Message( pao, phm, phr);		// send the format to the DSP
		if(phr->wError)
			return;
	}

	status = &interface->aOutStreamHostBufferStatus[phm->u.d.wStreamIndex];
	dwSpaceAvailable = OutStreamGetSpaceAvailable(status);
	if(dwSpaceAvailable < (long)phm->u.d.u.Data.dwDataSize)
	{
		phr->wError = HPI_ERROR_INVALID_DATASIZE;
		return;
	}

	lFirstWrite = status->dwSizeInBytes - status->dwHostIndex;
	if(lFirstWrite > (long)phm->u.d.u.Data.dwDataSize)
		lFirstWrite = (long)phm->u.d.u.Data.dwDataSize;
	lSecondWrite = (long)phm->u.d.u.Data.dwDataSize - lFirstWrite;

	{
		HW32 dwHostIndex = status->dwHostIndex;

		memcpy(&pBBMData[dwHostIndex], &pAppData[0], lFirstWrite);
		dwHostIndex += (HW32)lFirstWrite;
		if(dwHostIndex >= status->dwSizeInBytes)
			dwHostIndex -= status->dwSizeInBytes;
		if(lSecondWrite)
		{
			memcpy(&pBBMData[dwHostIndex], &pAppData[lFirstWrite], lSecondWrite);
			dwHostIndex += (HW32)lSecondWrite;
			if(dwHostIndex >= status->dwSizeInBytes)
				dwHostIndex -= status->dwSizeInBytes;
		}
		status->dwHostIndex = dwHostIndex;
	}
}
static void OutStreamGetInfo(HPI_ADAPTER_OBJ *pao,HPI_MESSAGE *phm, HPI_RESPONSE *phr)
{
	HPI_HW_OBJ *pHw6205 = pao->priv;
	tBusMasteringInterfaceBuffer *interface=pHw6205->pInterfaceBuffer;
	H620_HOSTBUFFER_STATUS *status;

	if( !pHw6205->OutStreamHostBuffers[phm->u.d.wStreamIndex])
	{
		HW_Message( pao, phm,phr);
		return;
	}

	HPI_InitResponse(phr, phm->wObject, phm->wFunction, 0 );

	status = &interface->aOutStreamHostBufferStatus[phm->u.d.wStreamIndex];

	phr->u.d.u.stream_info.wState = (HW16)status->dwStreamState;
	phr->u.d.u.stream_info.dwSamplesTransferred = status->dwSamplesProcessed;
	phr->u.d.u.stream_info.dwBufferSize = status->dwSizeInBytes;
	phr->u.d.u.stream_info.dwDataAvailable = status->dwSizeInBytes - OutStreamGetSpaceAvailable(status);
	phr->u.d.u.stream_info.dwAuxiliaryDataAvailable = status->dwAuxiliaryDataAvailable;
}
static void OutStreamStart(HPI_ADAPTER_OBJ *pao,HPI_MESSAGE *phm, HPI_RESPONSE *phr)
{
	HW_Message( pao, phm,phr);
}
static void OutStreamReset(HPI_ADAPTER_OBJ *pao,HPI_MESSAGE *phm, HPI_RESPONSE *phr)
{
	HPI_HW_OBJ *pHw6205 = pao->priv;
	pHw6205->flagOStreamJustReset[phm->u.d.wStreamIndex] = 1;
	HW_Message( pao, phm,phr);
}
static void OutStreamOpen(HPI_ADAPTER_OBJ *pao,HPI_MESSAGE *phm, HPI_RESPONSE *phr)
{
	OutStreamReset(pao,phm,phr);
}


//////////////////////////////////////////////////////////////////////
//						InStream Host buffer functions
//////////////////////////////////////////////////////////////////////

static void InStreamHostBufferAllocate(HPI_ADAPTER_OBJ *pao,HPI_MESSAGE *phm, HPI_RESPONSE *phr)
{
	HW16 wError;
	HW32 dwSizeToAllocate;
	HW32 dwCommand = phm->u.d.u.Buffer.dwCommand;
	HPI_HW_OBJ *pHw6205 = pao->priv;
	tBusMasteringInterfaceBuffer *interface=pHw6205->pInterfaceBuffer;

	HPI_InitResponse(phr, phm->wObject, phm->wFunction, 0 );
	dwSizeToAllocate = phm->u.d.u.Data.dwDataSize;
	phr->u.d.u.stream_info.dwDataAvailable = pHw6205->InStreamHostBufferSize[phm->u.d.wStreamIndex];
	phr->u.d.u.stream_info.dwBufferSize = dwSizeToAllocate;

	if( dwCommand == HPI_BUFFER_CMD_EXTERNAL ||
		dwCommand == HPI_BUFFER_CMD_INTERNAL_ALLOC )
	{
		if(pHw6205->InStreamHostBufferSize[phm->u.d.wStreamIndex]==dwSizeToAllocate)
		{
			// Same size, no action required, use dwError to skip Hpi6205_Message() section
			// below but return success
			wError = 1;
		}
		else
		{
			if(pHw6205->InStreamHostBuffers[phm->u.d.wStreamIndex])
				HpiOs_LockedMem_Free( pHw6205->InStreamHostBuffers[phm->u.d.wStreamIndex] );

			wError = HpiOs_LockedMem_Alloc(
							&pHw6205->InStreamHostBuffers[phm->u.d.wStreamIndex],
							phm->u.d.u.Data.dwDataSize,
							(void *)pao->Pci.pOsData
							);

			if(wError)
			{
				phr->wError = HPI_ERROR_INVALID_DATASIZE;
				pHw6205->InStreamHostBufferSize[phm->u.d.wStreamIndex] = 0;
			}
			else
			{
				pHw6205->InStreamHostBufferSize[phm->u.d.wStreamIndex] = dwSizeToAllocate;
			}
		}
	} else {
		wError = 0; /*hush the compiler's warning about dwError being used uninitialized */
	}

	if( ( dwCommand == HPI_BUFFER_CMD_EXTERNAL && wError == 0 ) ||
		dwCommand == HPI_BUFFER_CMD_INTERNAL_GRANTADAPTER )
	{
		H620_HOSTBUFFER_STATUS *status;
		HW32 dwPhysicalPCIaddress;

		// Why doesn't this work ?? - causes strange behaviour under Win16
		//memset(buffer,0,sizeof(H620_HOSTBUFFER_STATUS) + phm->u.d.u.Data.dwDataSize);
		status = &interface->aInStreamHostBufferStatus[phm->u.d.wStreamIndex];
		status->dwSamplesProcessed = 0;
		status->dwStreamState = HPI_STATE_STOPPED;
		status->dwDSPIndex = 0;
		status->dwHostIndex = 0;
		status->dwSizeInBytes = phm->u.d.u.Data.dwDataSize;
		wError = HpiOs_LockedMem_GetPhysAddr(pHw6205->InStreamHostBuffers[phm->u.d.wStreamIndex], &dwPhysicalPCIaddress );
		if(wError)
		{
			phr->wError = HPI_ERROR_DOS_MEMORY_ALLOC;
		}
		else
		{
			phm->u.d.u.Buffer.dwPciAddress = dwPhysicalPCIaddress;
			HW_Message( pao, phm,phr);
			if(phr->wError && dwCommand == HPI_BUFFER_CMD_EXTERNAL )
			{
				// free the buffer
				HpiOs_LockedMem_Free( pHw6205->InStreamHostBuffers[phm->u.d.wStreamIndex] );
				pHw6205->InStreamHostBufferSize[phm->u.d.wStreamIndex] = 0;
				pHw6205->InStreamHostBuffers[phm->u.d.wStreamIndex] = NULL;
			}
		}
	}
}

static void InStreamHostBufferFree(HPI_ADAPTER_OBJ *pao,HPI_MESSAGE *phm, HPI_RESPONSE *phr)
{
	HPI_HW_OBJ *pHw6205 = pao->priv;
	HW32 dwCommand = phm->u.d.u.Buffer.dwCommand;

	if( pHw6205->InStreamHostBuffers[phm->u.d.wStreamIndex] )
	{
		if( dwCommand == HPI_BUFFER_CMD_EXTERNAL ||
			dwCommand == HPI_BUFFER_CMD_INTERNAL_REVOKEADAPTER )
		{
			HW_Message( pao, phm,phr);
		}

		if( dwCommand == HPI_BUFFER_CMD_EXTERNAL ||
			dwCommand == HPI_BUFFER_CMD_INTERNAL_FREE )
		{
			HpiOs_LockedMem_Free( pHw6205->InStreamHostBuffers[phm->u.d.wStreamIndex] );
			pHw6205->InStreamHostBufferSize[phm->u.d.wStreamIndex] = 0;
			pHw6205->InStreamHostBuffers[phm->u.d.wStreamIndex] = NULL;
		}
	}
}
//short nValue=0;
static void InStreamStart(HPI_ADAPTER_OBJ *pao,HPI_MESSAGE *phm, HPI_RESPONSE *phr)
{
/*
	if( pHw6205->InStreamHostBuffers[phm->u.d.wStreamIndex] )	// preset the buffer values
	{
		int i;
		short HUGE *pData;
		H620_HOSTBUFFER_STATUS *buffer;

		if(HpiOs_LockedMem_GetVirtAddr( pHw6205->InStreamHostBuffers[phm->u.d.wStreamIndex], (void *)&buffer ))
		{
			phr->wError = HPI_ERROR_INVALID_OPERATION;
			return;
		}
		pData = ((short HUGE *)buffer) + sizeof(H620_HOSTBUFFER_STATUS)/sizeof(short);
		for(i=0;i<buffer->dwSizeInBytes/2/sizeof(short);i++)
		{
			*pData++ = nValue;
			*pData++ = nValue;
			nValue = (nValue+1) & 0x7fff;
		}
	}
*/
	HW_Message( pao, phm,phr);
}

static long InStreamGetBytesAvailable(	H620_HOSTBUFFER_STATUS *status)
{
	long nDiff;

	// When dwDSPindex==dwHostIndex the buffer is empty
	// Need to add code to the DSP to make sure that the buffer is never fulled
	// to the point that dwDSPindex==dwHostIndex.
	nDiff =  (long)(status->dwDSPIndex) - (long)(status->dwHostIndex);
	if(nDiff<0)
		nDiff += status->dwSizeInBytes;
	return nDiff;
}

static void InStreamRead(HPI_ADAPTER_OBJ *pao,HPI_MESSAGE *phm, HPI_RESPONSE *phr)
{
	HPI_HW_OBJ *pHw6205 = pao->priv;
	tBusMasteringInterfaceBuffer *interface=pHw6205->pInterfaceBuffer;
	H620_HOSTBUFFER_STATUS *status;
	long dwDataAvailable;
	HW8 HUGE * pBBMData;
	long lFirstRead;
	long lSecondRead;
	HW8 HUGE *pAppData = (HW8 HUGE *)phm->u.d.u.Data.pbData;
	/* DEBUG
	int i;
	long *pTest;
	*/
	if( !pHw6205->InStreamHostBuffers[phm->u.d.wStreamIndex])
	{
		HW_Message( pao, phm,phr);
		return;
	}
	HPI_InitResponse(phr, phm->wObject, phm->wFunction, 0 );

	if(HpiOs_LockedMem_GetVirtAddr( pHw6205->InStreamHostBuffers[phm->u.d.wStreamIndex], (void *)&pBBMData ))
	{
		phr->wError = HPI_ERROR_INVALID_OPERATION;
		return;
	}

	status = &interface->aInStreamHostBufferStatus[phm->u.d.wStreamIndex];
	dwDataAvailable= InStreamGetBytesAvailable(status);
	if(dwDataAvailable<(long)phm->u.d.u.Data.dwDataSize)
	{
		phr->wError = HPI_ERROR_INVALID_DATASIZE;
		return;
	}

	lFirstRead = status->dwSizeInBytes - status->dwHostIndex;
	if(lFirstRead > (long)phm->u.d.u.Data.dwDataSize)
		lFirstRead = (long)phm->u.d.u.Data.dwDataSize;
	lSecondRead = (long)phm->u.d.u.Data.dwDataSize - lFirstRead;

	{  // avoid having status->dwHostIndex invalid, even momentarily
		HW32 dwHostIndex =  status->dwHostIndex;

		memcpy(&pAppData[0], &pBBMData[dwHostIndex],lFirstRead);
		dwHostIndex += (HW32)lFirstRead;
		if(dwHostIndex >= status->dwSizeInBytes)
			dwHostIndex -= status->dwSizeInBytes;
		if(lSecondRead)
		{
			memcpy(&pAppData[lFirstRead], &pBBMData[dwHostIndex],lSecondRead);
			dwHostIndex += (HW32)lSecondRead;
			if(dwHostIndex >= status->dwSizeInBytes)
				dwHostIndex -= status->dwSizeInBytes;
		}
		status->dwHostIndex = dwHostIndex;
	}

	/* DBEUG */
	/*
	pTest = (long *)phm->u.d.u.Data.dwpbData;
	for(i=0;i<phm->u.d.u.Data.dwDataSize/sizeof(long);i++)
	{
		if(pTest[i])
			pTest[i]--;
	}
	pTest = (long *)((char *)buffer) + sizeof(H620_HOSTBUFFER_STATUS);
	for(i=0;i<buffer->dwSizeInBytes/sizeof(long);i++)
	{
		if(pTest[i])
			pTest[i]--;
	}
	*/
}

static void InStreamGetInfo(HPI_ADAPTER_OBJ *pao,HPI_MESSAGE *phm, HPI_RESPONSE *phr)
{
	HPI_HW_OBJ *pHw6205 = pao->priv;
	tBusMasteringInterfaceBuffer *interface=pHw6205->pInterfaceBuffer;
	H620_HOSTBUFFER_STATUS *status;
	if( !pHw6205->InStreamHostBuffers[phm->u.d.wStreamIndex])
	{
		HW_Message( pao, phm,phr);
		return;
	}

	status = &interface->aInStreamHostBufferStatus[phm->u.d.wStreamIndex];

	HPI_InitResponse(phr, phm->wObject, phm->wFunction, 0 );

	phr->u.d.u.stream_info.wState = (HW16)status->dwStreamState;
	phr->u.d.u.stream_info.dwSamplesTransferred = status->dwSamplesProcessed;
	phr->u.d.u.stream_info.dwBufferSize = status->dwSizeInBytes;
	phr->u.d.u.stream_info.dwDataAvailable = InStreamGetBytesAvailable(status);
	phr->u.d.u.stream_info.dwAuxiliaryDataAvailable = status->dwAuxiliaryDataAvailable;
}

////////////////////////////////////////////////////////////////////////////
// LOW-LEVEL
///////////////////////////////////////////////////////////////////////////

#if 0
static short Hpi6205_AdapterCheckPresent( HPI_ADAPTER_OBJ *pao )
{
	 return 0;
}
#endif

#define MAX_FILES_TO_LOAD 3

static short Hpi6205_AdapterBootLoadDsp( HPI_ADAPTER_OBJ *pao, HW32 *pdwOsErrorCode)
{
	HPI_HW_OBJ *pHw6205 = pao->priv;
	DSP_CODE DspCode;
	HW16 anBootLoadFamily[MAX_FILES_TO_LOAD];
	volatile HW32 dwTemp;
	int nDsp=0, i=0;
	HW16 wError=0;

	// by default there is no DSP code to load
	for(i=0;i<MAX_FILES_TO_LOAD;i++)
		anBootLoadFamily[i]=0;

	switch(pao->Pci.wSubSysDeviceId)
	{
		case HPI_ADAPTER_FAMILY_ASI5000:
			anBootLoadFamily[0] = Load5000;	// base 6205 code
			break;
		case HPI_ADAPTER_FAMILY_ASI6400:
			anBootLoadFamily[0] = Load6205;	// base 6205 code
			anBootLoadFamily[1] = Load6413;	// 6713 code
			break;
		case HPI_ADAPTER_FAMILY_ASI6500:
		case HPI_ADAPTER_FAMILY_ASI6600:
			anBootLoadFamily[0] = Load6205;	// base 6205 code
			anBootLoadFamily[1] = Load6600;	// 6713 code
			break;
		case HPI_ADAPTER_FAMILY_ASI8700:
			anBootLoadFamily[0] = Load6205;	// base 6205 code
			anBootLoadFamily[1] = Load8713;	// 6713 code
			break;
		default:
			return(Hpi6205_Error(0,HPI6205_ERROR_UNKNOWN_PCI_DEVICE));
	}

	//////////////////////////////////////////////////
	// check we can read the 6205 registers ok

	// reset DSP by writing a 1 to the WARMRESET bit
	dwTemp = C6205_HDCR_WARMRESET;
	HPIOS_MEMWRITE32(pHw6205->prHDCR, dwTemp );
	HpiOs_DelayMicroSeconds(1000);
//	for(i=0;i<1000; i++) dwTemp = HPIOS_MEMREAD32(pao,pHw6205->prHDCR );  //delay

	// check that PCI i/f was configured by EEPROM
	dwTemp = HPIOS_MEMREAD32(pHw6205->prHSR );  //read the HSR register.
	if((dwTemp & (C6205_HSR_CFGERR|C6205_HSR_EEREAD)) != C6205_HSR_EEREAD)
		return Hpi6205_Error(0, HPI6205_ERROR_6205_EEPROM);
	dwTemp |= 0x04;
	HPIOS_MEMWRITE32(pHw6205->prHSR, dwTemp );	// disable PINTA interrupt


	// check control register reports PCI boot mode
	dwTemp = HPIOS_MEMREAD32(pHw6205->prHDCR );  //read the control register.
	if( !(dwTemp&C6205_HDCR_PCIBOOT) )
		return Hpi6205_Error(0, HPI6205_ERROR_6205_REG);

	// try writing a couple of numbers to the DSP page register and reading them back.
	dwTemp = 1 ;
	HPIOS_MEMWRITE32(pHw6205->prDSPP, dwTemp );
	if( (dwTemp | C6205_DSPP_MAP1)  != HPIOS_MEMREAD32(pHw6205->prDSPP ) )
		return Hpi6205_Error(0, HPI6205_ERROR_6205_DSPPAGE);
	dwTemp = 2;
	HPIOS_MEMWRITE32(pHw6205->prDSPP, dwTemp );
	if((dwTemp | C6205_DSPP_MAP1) != HPIOS_MEMREAD32(pHw6205->prDSPP ) )
		return Hpi6205_Error(0, HPI6205_ERROR_6205_DSPPAGE);
	dwTemp = 3;
	HPIOS_MEMWRITE32(pHw6205->prDSPP, dwTemp );
	if((dwTemp | C6205_DSPP_MAP1) != HPIOS_MEMREAD32(pHw6205->prDSPP) )
		return Hpi6205_Error(0, HPI6205_ERROR_6205_DSPPAGE);
	// reset DSP page to the correct number
	dwTemp = 0;
	HPIOS_MEMWRITE32(pHw6205->prDSPP, dwTemp );
	if( (dwTemp | C6205_DSPP_MAP1) != HPIOS_MEMREAD32(pHw6205->prDSPP) )
		return Hpi6205_Error(0, HPI6205_ERROR_6205_DSPPAGE);
	pHw6205->dwDspPage = 0;

	/* release 6713 from reset before 6205 is bootloaded. This ensures that the EMIF
		is inactive, and the 6713 HPI gets the correct bootmode etc
	*/
	if(anBootLoadFamily[1]!=0) {
		// DSP 1 is a C6713
		BootLoader_WriteMem32( pao, 0, (0x018C0024L),0x00002202);	// CLKX0 <- '1' release the C6205 bootmode pulldowns
		HpiOs_DelayMicroSeconds(100);
		BootLoader_WriteMem32( pao, 0, C6205_BAR0_TIMER1_CTL,0);	// Reset the 6713 #1 - revB

		// dummy read every 4 words for 6205 advisory 1.4.4
		BootLoader_ReadMem32( pao, 0, 0);

		HpiOs_DelayMicroSeconds(100);
		BootLoader_WriteMem32( pao, 0, C6205_BAR0_TIMER1_CTL,4);	// Release C6713 from reset - revB
		HpiOs_DelayMicroSeconds(100);
	}


	for(nDsp=0;nDsp<MAX_FILES_TO_LOAD;nDsp++)
	{

		if(anBootLoadFamily[nDsp]==0)	// is there a DSP to load
			continue;

		// for each DSP

		// configure EMIF
		wError=BootLoader_ConfigEMIF(pao, nDsp);
		if (wError)
			return(wError);

		// check internal memory
		wError=BootLoader_TestInternalMemory(pao, nDsp);
		if (wError)
			return(wError);

		// test SDRAM
		wError=BootLoader_TestExternalMemory(pao, nDsp);
		if (wError)
			return(wError);

		// test for PLD located on DSPs EMIF bus
		wError=BootLoader_TestPld(pao, nDsp);
		if (wError)
			return(wError);

		///////////////////////////////////////////////////////////
		// write the DSP code down into the DSPs memory
#if defined DSPCODE_FIRMWARE
	    DspCode.psDev = pao->Pci.pOsData;
#endif
		if ((wError=HpiDspCode_Open(anBootLoadFamily[nDsp],&DspCode,pdwOsErrorCode))!= 0)
			return( wError );

		while (1)
		{
			HW32 dwLength;
			HW32 dwAddress;
			HW32 dwType;
			HW32 *pdwCode;

			if ((wError=HpiDspCode_ReadWord(&DspCode,&dwLength))!= 0)
				break;
			if (dwLength == 0xFFFFFFFF)
				break; // end of code

#ifdef DSPCODE_ARRAY
			// check for end of array with continuation to another one
			if (dwLength == 0xFFFFFFFEL )
			{
				DspCode.nArrayNum++;
				DspCode.dwOffset = 0;
				if ((wError = HpiDspCode_ReadWord(&DspCode,&dwLength))!= 0)
					break;
			}
#endif
			if ((wError=HpiDspCode_ReadWord(&DspCode,&dwAddress))!= 0)
				break;
			if ((wError=HpiDspCode_ReadWord(&DspCode,&dwType))!= 0)
				break;
			if ((wError=HpiDspCode_ReadBlock(dwLength,&DspCode,&pdwCode))!= 0)
				break;
			//if ((wError=BootLoader_BlockWrite32( pao, nDsp, dwAddress, (HW32)pdwCode, dwLength))
			//			!= 0)
			//	break;
			for(i=0; i<(int)dwLength; i++)
			{
				wError = BootLoader_WriteMem32( pao, nDsp, dwAddress, *pdwCode);
				if(wError)
						break;
				// dummy read every 4 words for 6205 advisory 1.4.4
				if(i%4==0)
						BootLoader_ReadMem32( pao, nDsp, dwAddress);
				pdwCode++;
				dwAddress += 4;
			}

		}
		if (wError) {
		    HpiDspCode_Close(&DspCode);
		    return(wError);
		}
		/////////////////////////////
		// verify code
		HpiDspCode_Rewind(&DspCode);
		while (1)
		{
			HW32 dwLength=0;
			HW32 dwAddress=0;
			HW32 dwType=0;
			HW32 *pdwCode=NULL;
			HW32 dwData=0;

			HpiDspCode_ReadWord(&DspCode,&dwLength);
			if (dwLength == 0xFFFFFFFF)
					break; // end of code

#ifdef DSPCODE_ARRAY
			// check for end of array with continuation to another one
			if (dwLength == 0xFFFFFFFEL )
			{
                    DspCode.nArrayNum++;
						  DspCode.dwOffset = 0;
						  HpiDspCode_ReadWord(&DspCode,&dwLength);
			}
#endif

			HpiDspCode_ReadWord(&DspCode,&dwAddress);
			HpiDspCode_ReadWord(&DspCode,&dwType);
			HpiDspCode_ReadBlock(dwLength,&DspCode,&pdwCode);

			for(i=0; i<(int)dwLength; i++)
			{
				dwData = BootLoader_ReadMem32( pao, nDsp, dwAddress);      //read the data back
				if(dwData != *pdwCode)
				{
						wError = 0;
						break;
				}
				pdwCode++;
				dwAddress += 4;
			}
			if (wError)
				break;
		}
		HpiDspCode_Close(&DspCode);
		if (wError)
			return (wError);
	}

	// After bootloading all DSPs, start DSP0 running
	// The DSP0 code will handle starting and synchronizing with its slaves
	if (pHw6205->pInterfaceBuffer)
	{
		// we need to tell the card the physical PCI address
		HW32 dwPhysicalPCIaddress;
		tBusMasteringInterfaceBuffer *interface=pHw6205->pInterfaceBuffer;
		HW32 dwHostMailboxAddressOnDsp;
		HW32 dwPhysicalPCIaddressVerify=0;
		int nTimeOut=10;
		// set ack so we know when DSP is ready to go (dwDspAck will be changed to HIF_RESET)
		interface->dwDspAck = H620_HIF_UNKNOWN;

		wError = HpiOs_LockedMem_GetPhysAddr( pHw6205->hLockedMem, &dwPhysicalPCIaddress );

		// locate the host mailbox on the DSP.
		dwHostMailboxAddressOnDsp = 0x80000000;
		while( (dwPhysicalPCIaddress != dwPhysicalPCIaddressVerify) && nTimeOut-- )
		{
			wError = BootLoader_WriteMem32( pao, 0, dwHostMailboxAddressOnDsp, dwPhysicalPCIaddress);
			dwPhysicalPCIaddressVerify = BootLoader_ReadMem32( pao, 0, dwHostMailboxAddressOnDsp);
		}
	}
	HPI_DEBUG_LOG0(DEBUG,"Starting DSPs running\n");
	// enable interrupts
	dwTemp = HPIOS_MEMREAD32(pHw6205->prHSR );  //read the control register
	dwTemp &= ~(HW32)C6205_HSR_INTAM;
	HPIOS_MEMWRITE32(pHw6205->prHSR,dwTemp);

	// start code running...
	// need to remove from here because actual implementation will depend on DSP index.
	dwTemp = HPIOS_MEMREAD32(pHw6205->prHDCR );  //read the control register
	dwTemp |= (HW32)C6205_HDCR_DSPINT;
	HPIOS_MEMWRITE32(pHw6205->prHDCR,dwTemp);

	// give the DSP 10ms to start up
	HpiOs_DelayMicroSeconds(10000);
	return wError;

}

//////////////////////////////////////////////////////////////////////
//						Bootloader utility functions
//////////////////////////////////////////////////////////////////////


static HW32 BootLoader_ReadMem32(HPI_ADAPTER_OBJ *pao, int nDSPIndex, HW32 dwAddress)
{
	HPI_HW_OBJ *pHw6205 = pao->priv;
	HW32 dwData=0;
	__iomem HW32 * pData;

	if(nDSPIndex==0)
	{
		// DSP 0 is always C6205
		// DSP 0 is always C6205
		if((dwAddress >= 0x01800000) & (dwAddress < 0x02000000 ))
		{
			// BAR1 register access
#ifdef HPI_OS_WIN16
			pData = Hpi6205_Abstract_ComputeAddress(pao, (HW32)pao->Pci.apMemBase[1] + (dwAddress&0x007fffff));
			if (!pData) return 0;
#else
			pData = pao->Pci.apMemBase[1] + (dwAddress&0x007fffff)/sizeof(*pao->Pci.apMemBase[1]);
#endif
		}
		else
		{
			HW32 dw4MPage = dwAddress >> 22L;
			if(dw4MPage != pHw6205->dwDspPage)
			{
				pHw6205->dwDspPage = dw4MPage;
				HPIOS_MEMWRITE32(pHw6205->prDSPP, pHw6205->dwDspPage );
			}
			dwAddress &= 0x3fffff;	// address within 4M page
			// BAR0 memory access
#ifdef HPI_OS_WIN16
			pData = Hpi6205_Abstract_ComputeAddress(pao, (HW32)pao->Pci.apMemBase[0] + dwAddress);
			if (!pData) return 0;
#else
			pData = pao->Pci.apMemBase[0] + dwAddress/sizeof(HW32);
#endif
		}
		dwData = HPIOS_MEMREAD32(pData);
	}
	else if (nDSPIndex==1)
	{	// DSP 1 is a C6713
		HW32 dwLsb;
		BootLoader_WriteMem32(pao, 0, HPIAL_ADDR, dwAddress);
		BootLoader_WriteMem32(pao, 0, HPIAH_ADDR, dwAddress>>16);
		dwLsb=BootLoader_ReadMem32(pao, 0, HPIDL_ADDR);
		dwData=BootLoader_ReadMem32(pao, 0, HPIDH_ADDR);
		dwData = (dwData<<16) | (dwLsb & 0xFFFF);
	}
	return dwData;
}
static HW16 BootLoader_WriteMem32(HPI_ADAPTER_OBJ *pao, int nDSPIndex, HW32 dwAddress, HW32 dwData)
{
	HPI_HW_OBJ *pHw6205 = pao->priv;
	HW16 nError=0;
	__iomem HW32 * pData;
	//	HW32 dwVerifyData=0;

	if(nDSPIndex==0)
	{
		// DSP 0 is always C6205
		if((dwAddress >= 0x01800000) & (dwAddress < 0x02000000 ))
		{
			// BAR1 - DSP  register access using Non-prefetchable PCI access
#ifdef HPI_OS_WIN16
			pData = Hpi6205_Abstract_ComputeAddress(pao, (HW32)pao->Pci.apMemBase[1] + (dwAddress&0x007fffff));
			if (!pData) return 0;
#else
			pData = pao->Pci.apMemBase[1] + (dwAddress&0x007fffff)/sizeof(*pao->Pci.apMemBase[1]);
#endif
		}
		else // BAR0 access - all of DSP memory using pre-fetchable PCI access
		{
			HW32 dw4MPage = dwAddress >> 22L;
			if(dw4MPage != pHw6205->dwDspPage)
			{
				pHw6205->dwDspPage = dw4MPage;
				HPIOS_MEMWRITE32(pHw6205->prDSPP, pHw6205->dwDspPage  );
			}
			dwAddress &= 0x3fffff;	// address within 4M page
#ifdef HPI_OS_WIN16
			pData = Hpi6205_Abstract_ComputeAddress(pao, (HW32)pao->Pci.apMemBase[0] + dwAddress);
			if (!pData) return 0;
#else
			pData = pao->Pci.apMemBase[0] + dwAddress/sizeof(HW32);
#endif
			/*  !EWB verifying writes to 6713 HPID causes problems!!!
			{
				//			if ((dwAddress !=HPIDL_ADDR) && (dwAddress !=HPIDH_ADDR )) {
				//!EWB verifying writes to 6713 HPID causes problems!!! temporarily disable
				// create a new function that doesn't do it specially?
				HW32 dwVerifyData = Hpi6205_Abstract_MEMREAD32(pao, pao->Pci.dwMemBase[0] + dwAddress);
				if(dwVerifyData != dwData)
				{
					nError = HPI_ERROR_DSP_HARDWARE;
					return nError;
				}
			} */
		}
		HPIOS_MEMWRITE32(pData,dwData);
	}
	else if (nDSPIndex==1)
	{	// DSP 1 is a C6713
		BootLoader_WriteMem32(pao, 0, HPIAL_ADDR, dwAddress);
		BootLoader_WriteMem32(pao, 0, HPIAH_ADDR, dwAddress>>16);

		// dummy read every 4 words for 6205 advisory 1.4.4
		BootLoader_ReadMem32( pao, 0, 0);

		BootLoader_WriteMem32(pao, 0, HPIDL_ADDR, dwData);
		BootLoader_WriteMem32(pao, 0, HPIDH_ADDR, dwData>>16);

		// dummy read every 4 words for 6205 advisory 1.4.4
		BootLoader_ReadMem32( pao, 0, 0);
	}
	else
		nError = Hpi6205_Error(nDSPIndex, HPI6205_ERROR_BAD_DSPINDEX);
	return nError;
}
/*
HW16 BootLoader_BlockWrite32( HPI_ADAPTER_OBJ *pao, HW16 wDspIndex, HW32 dwDspDestinationAddress, HW32 dwSourceAddress, HW32 dwCount)
{
	HW32 dwIndex;
	HW16 wError;
	HW32 *pdwData=(HW32 *)dwSourceAddress;

	for(dwIndex=0; dwIndex<dwCount; dwIndex++)
	{
		wError = BootLoader_WriteMem32(pao, wDspIndex, dwDspDestinationAddress+dwIndex*4, pdwData[dwIndex]);
		if(wError)
			break;
	}
	return wError;
}
*/
static HW16 BootLoader_ConfigEMIF(HPI_ADAPTER_OBJ *pao, int nDSPIndex)
{
	HW16 nError=0;

	if(nDSPIndex==0)
	{
		HW32 dwSetting;

		// DSP 0 is always C6205

		// Set the EMIF
		// memory map of C6205
		// 00000000-0000FFFF	16Kx32 internal program
		// 00400000-00BFFFFF	CE0	2Mx32 SDRAM running @ 100MHz

		// EMIF config
		//------------
		// Global EMIF control
		BootLoader_WriteMem32( pao, nDSPIndex, 0x01800000, 0x3779 );
#define WS_OFS 28
#define WST_OFS 22
#define WH_OFS 20
#define RS_OFS 16
#define RST_OFS 8
#define MTYPE_OFS 4
#define RH_OFS 0

		// EMIF CE0 setup - 2Mx32 Sync DRAM on ASI5000 cards only
		dwSetting = 0x00000030;
		BootLoader_WriteMem32( pao, nDSPIndex, 0x01800008, dwSetting);  // CE0
		if(dwSetting!=BootLoader_ReadMem32(pao, nDSPIndex, 0x01800008) )
			return Hpi6205_Error( nDSPIndex , HPI6205_ERROR_DSP_EMIF);

		// EMIF CE1 setup - 32 bit async. This is 6713 #1 HPI, which occupies D15..0. 6713 starts at 27MHz, so need
		// plenty of wait states. See dsn8701.rtf, and 6713 errata.
		dwSetting =
			(1L<<WS_OFS)  |
			(63L<<WST_OFS) |
			 (1L<<WH_OFS)  |
			 (1L<<RS_OFS)  |
			(63L<<RST_OFS) |  // should be 71, but 63 is max possible
			 (1L<<RH_OFS)  |
			 (2L<<MTYPE_OFS);
		BootLoader_WriteMem32( pao, nDSPIndex, 0x01800004, dwSetting); // CE1
		if(dwSetting!=BootLoader_ReadMem32(pao, nDSPIndex, 0x01800004) )
			return Hpi6205_Error( nDSPIndex , HPI6205_ERROR_DSP_EMIF);

		// EMIF CE2 setup - 32 bit async. This is 6713 #2 HPI, which occupies D15..0. 6713 starts at 27MHz, so need
		// plenty of wait states
		dwSetting =
			 (1L<<WS_OFS)  |
			(28L<<WST_OFS) |
			 (1L<<WH_OFS)  |
			 (1L<<RS_OFS)  |
			(63L<<RST_OFS) |
			 (1L<<RH_OFS)  |
			 (2L<<MTYPE_OFS);
		BootLoader_WriteMem32( pao, nDSPIndex, 0x01800010, dwSetting);  // CE2
		if(dwSetting!=BootLoader_ReadMem32(pao, nDSPIndex, 0x01800010) )
			return Hpi6205_Error( nDSPIndex , HPI6205_ERROR_DSP_EMIF);

		// EMIF CE3 setup - 32 bit async. This is the PLD on the ASI5000 cards only
		dwSetting =
			 (1L<<WS_OFS)  |
			(10L<<WST_OFS) |
			 (1L<<WH_OFS)  |
			 (1L<<RS_OFS)  |
			(10L<<RST_OFS) |
			 (1L<<RH_OFS)  |
			 (2L<<MTYPE_OFS);
		BootLoader_WriteMem32( pao, nDSPIndex, 0x01800014, dwSetting); // CE3
		if(dwSetting!=BootLoader_ReadMem32(pao, nDSPIndex, 0x01800014) )
			return Hpi6205_Error( nDSPIndex , HPI6205_ERROR_DSP_EMIF);

		// EMIF SDRAM control - set up for a 2Mx32 SDRAM (512x32x4 bank)
		BootLoader_WriteMem32( pao, nDSPIndex, 0x01800018, 0x07117000);	//  need to use this else DSP code crashes?

		// EMIF SDRAM Refresh Timing
		BootLoader_WriteMem32( pao, nDSPIndex, 0x0180001C,0x00000410);	// EMIF SDRAM timing  (orig = 0x410, emulator = 0x61a)

	}
	else if (nDSPIndex==1)
	{
		// test access to the C6713s HPI registers
		HW32 dwWriteData=0, dwReadData=0, i=0;

		// HPIC - Set up HPIC for little endian, by setiing HPIC:HWOB=1
		dwWriteData = 1;
		BootLoader_WriteMem32( pao, 0, HPICL_ADDR, dwWriteData);
		BootLoader_WriteMem32( pao, 0, HPICH_ADDR, dwWriteData);
		dwReadData= 0xFFF7& BootLoader_ReadMem32( pao, 0, HPICL_ADDR);	// C67 HPI is on lower 16bits of 32bit EMIF
		if(dwWriteData != dwReadData)
		{
			nError = Hpi6205_Error(nDSPIndex, HPI6205_ERROR_C6713_HPIC);
				gadwHpiSpecificError[0] = HPICL_ADDR;
			gadwHpiSpecificError[1] = dwWriteData;
            gadwHpiSpecificError[2] = dwReadData;
			return nError;
		}

		// HPIA - walking ones test
		dwWriteData = 1;
		for(i=0; i<32; i++)
		{
			BootLoader_WriteMem32( pao, 0, HPIAL_ADDR, dwWriteData);
			BootLoader_WriteMem32( pao, 0, HPIAH_ADDR, (dwWriteData >> 16));
			dwReadData= 0xFFFF & BootLoader_ReadMem32( pao, 0, HPIAL_ADDR);
			dwReadData= dwReadData | ((0xFFFF & BootLoader_ReadMem32( pao, 0, HPIAH_ADDR))<<16);
			if(dwReadData != dwWriteData)
			{
				nError = Hpi6205_Error(nDSPIndex, HPI6205_ERROR_C6713_HPIA);
            	gadwHpiSpecificError[0] = HPIAH_ADDR;
				gadwHpiSpecificError[1] = dwWriteData;
            	gadwHpiSpecificError[2] = dwReadData;
				return nError;
			}
			dwWriteData = dwWriteData << 1;
		}

		// setup C67x PLL
		// ** C6713 datasheet says we cannot program PLL from HPI, and indeed if we try to set the
		// PLL multiply from the HPI, the PLL does not seem to lock, so we enable the PLL and use the default
		// multiply of x 7, which for a 27MHz clock gives a DSP speed of 189MHz
		BootLoader_WriteMem32( pao,nDSPIndex, 0x01B7C100, 0x0000 );  // bypass PLL
		HpiOs_DelayMicroSeconds(1000);
		BootLoader_WriteMem32( pao,nDSPIndex, 0x01B7C120, 0x8002 );  // EMIF = 189/3=63MHz
		BootLoader_WriteMem32( pao,nDSPIndex, 0x01B7C11C, 0x8001 );  // peri = 189/2
		BootLoader_WriteMem32( pao,nDSPIndex, 0x01B7C118, 0x8000 );  // cpu  = 189/1
		HpiOs_DelayMicroSeconds(1000);
		// ** SGT test to take GPO3 high when we start the PLL and low when the delay is completed
		BootLoader_WriteMem32( pao, 0, (0x018C0024L),0x00002A0A);	// FSX0 <- '1' (GPO3)
		BootLoader_WriteMem32( pao,nDSPIndex, 0x01B7C100, 0x0001 );  // PLL not bypassed
		HpiOs_DelayMicroSeconds(1000);
		BootLoader_WriteMem32( pao, 0, (0x018C0024L),0x00002A02);	// FSX0 <- '0' (GPO3)

		// 6205 EMIF CE1 resetup - 32 bit async. Now 6713 #1 is running at 189MHz can reduce waitstates
		BootLoader_WriteMem32( pao, 0, 0x01800004,  // CE1
			 (1L<<WS_OFS)  |
			 (8L<<WST_OFS) |
			 (1L<<WH_OFS)  |
			 (1L<<RS_OFS)  |
			(12L<<RST_OFS) |
			 (1L<<RH_OFS)  |
			 (2L<<MTYPE_OFS)
			 );

		HpiOs_DelayMicroSeconds(1000);

		// check that we can read one of the PLL registers
		if( (BootLoader_ReadMem32( pao,nDSPIndex,0x01B7C100 ) & 0xF) != 0x0001 )	// PLL should not be bypassed!
		{
			nError = Hpi6205_Error( nDSPIndex , HPI6205_ERROR_C6713_PLL);
			return nError;
		}

		// setup C67x EMIF
		BootLoader_WriteMem32( pao,nDSPIndex, C6713_EMIF_GCTL,       0x000034A8 );  // global control (orig=C6711 only = 0x3488)
		BootLoader_WriteMem32( pao,nDSPIndex, C6713_EMIF_CE0,        0x00000030);  // CE0
		BootLoader_WriteMem32( pao,nDSPIndex, C6713_EMIF_SDRAMEXT,   0x001BDF29 );   // need to use this else DSP code crashes?
		BootLoader_WriteMem32( pao,nDSPIndex, C6713_EMIF_SDRAMCTL,   0x47117000);	//  need to use this else DSP code crashes?
		BootLoader_WriteMem32( pao,nDSPIndex, C6713_EMIF_SDRAMTIMING,0x00000410);	// EMIF SDRAM timing  (orig = 0x410, emulator = 0x61a)

		HpiOs_DelayMicroSeconds(1000);
	}
	else if (nDSPIndex==2)
	{
		// DSP 2 is a C6713

	}
	else
		nError = Hpi6205_Error( nDSPIndex , HPI6205_ERROR_BAD_DSPINDEX);
	return nError;
}

static HW16 BootLoader_TestMemory(HPI_ADAPTER_OBJ *pao, int nDSPIndex, HW32 dwStartAddress, HW32 dwLength)
{
	HW32 i=0, j=0;
	HW32 dwTestAddr=0;
	HW32 dwTestData=0, dwData=0;

	dwLength = 1000; // **** SGT only test 1000 locations for now

	// for 1st word, test each bit in the 32bit word, dwLength specifies number of 32bit words to test
	//for(i=0; i<dwLength; i++)
	i=0;
	{
		dwTestAddr=dwStartAddress+(HW32)i*4;
		dwTestData=0x00000001;
		for(j=0; j<32; j++)
		{
			BootLoader_WriteMem32( pao, nDSPIndex, dwTestAddr, dwTestData);   //write the data to internal DSP mem
			dwData = BootLoader_ReadMem32( pao, nDSPIndex, dwTestAddr);      //read the data back
			if(dwData != dwTestData)
			{
				gadwHpiSpecificError[0] = dwTestAddr;
				gadwHpiSpecificError[1] = dwTestData;
				gadwHpiSpecificError[2] = dwData;
				gadwHpiSpecificError[3] = nDSPIndex;
                HPI_DEBUG_LOG4(VERBOSE,"Memtest error details  %08x %08x %08x %i\n", dwTestAddr,dwTestData,dwData,nDSPIndex);
				return(1);	// error
			}
			dwTestData = dwTestData << 1;
		} // for(j)
	} // for(i)

	// for the next 100 locations test each location, leaving it as zero
	// write a zero to the next word in memory before we read the previous write to make sure every memory
	// location is unique
	for(i=0; i<100; i++)
	{
		dwTestAddr=dwStartAddress+(HW32)i*4;
		dwTestData=0xA5A55A5A;
		BootLoader_WriteMem32( pao, nDSPIndex, dwTestAddr, dwTestData);   //write the data to mem
		BootLoader_WriteMem32( pao, nDSPIndex, dwTestAddr+4, 0);   		//write 0 to mem+1
		dwData = BootLoader_ReadMem32( pao, nDSPIndex, dwTestAddr);      //read the data back
		if(dwData != dwTestData)
		{
			gadwHpiSpecificError[0] = dwTestAddr;
			gadwHpiSpecificError[1] = dwTestData;
			gadwHpiSpecificError[2] = dwData;
			gadwHpiSpecificError[3] = nDSPIndex;
            HPI_DEBUG_LOG4(VERBOSE,"Memtest error details  %08x %08x %08x %i\n", dwTestAddr,dwTestData,dwData,nDSPIndex);
			return(1);	// error
		}
		BootLoader_WriteMem32( pao, nDSPIndex, dwTestAddr, 0x0);   // leave location as zero
	}

	// zero out entire memory block
	for(i=0; i<dwLength; i++)
	{
		dwTestAddr=dwStartAddress+(HW32)i*4;
		BootLoader_WriteMem32( pao, nDSPIndex, dwTestAddr, 0x0);   // leave location as zero
	}
	return(0);	//success!
}

static HW16 BootLoader_TestInternalMemory(HPI_ADAPTER_OBJ *pao, int nDSPIndex)
{
	int nError=0;
	if(nDSPIndex==0)
	{
		// DSP 0 is a C6205
		nError = BootLoader_TestMemory( pao, nDSPIndex, 0x00000000, 0x10000);    // 64K prog mem
		if(!nError)
			nError = BootLoader_TestMemory( pao, nDSPIndex, 0x80000000, 0x10000);    // 64K data mem
	}
	else if ((nDSPIndex==1) || (nDSPIndex==2))
	{
		// DSP 1&2 are a C6713
		nError = BootLoader_TestMemory( pao, nDSPIndex, 0x00000000, 0x30000);    // 192K internal mem
		if(!nError)
			nError = BootLoader_TestMemory( pao, nDSPIndex, 0x00030000, 0x10000);    // 64K internal mem / L2 cache
	}
	else
		return  Hpi6205_Error(nDSPIndex, HPI6205_ERROR_BAD_DSPINDEX);

	if(nError)
		return( Hpi6205_Error( nDSPIndex, HPI6205_ERROR_DSP_INTMEM));
	else
		return 0;
}
static HW16 BootLoader_TestExternalMemory(HPI_ADAPTER_OBJ *pao, int nDSPIndex)
{
	HW32 dwDRAMStartAddress=0;
	HW32 dwDRAMSize=0;

	if(nDSPIndex==0)
	{
		// only test for SDRAM if an ASI5000 card
		if(pao->Pci.wSubSysDeviceId == 0x5000)
		{
			// DSP 0 is always C6205
			dwDRAMStartAddress=0x00400000;
			dwDRAMSize=0x200000;
			//dwDRAMinc=1024;
		}
		else
			return(0);
	}
	else if ((nDSPIndex==1) || (nDSPIndex==2))
	{
		// DSP 1 is a C6713
		dwDRAMStartAddress=0x80000000;
		dwDRAMSize=0x200000;
		//dwDRAMinc=1024;
	}
	else
		return  Hpi6205_Error(nDSPIndex, HPI6205_ERROR_BAD_DSPINDEX);

   if(BootLoader_TestMemory( pao, nDSPIndex, dwDRAMStartAddress, dwDRAMSize))
	   return( Hpi6205_Error(nDSPIndex, HPI6205_ERROR_DSP_EXTMEM));
   return 0;
}


static HW16 BootLoader_TestPld(HPI_ADAPTER_OBJ *pao, int nDSPIndex)
{
	HW32 dwData=0;
	// only test for PLD on ASI5000 card
	if(nDSPIndex==0)
	{
		if(pao->Pci.wSubSysDeviceId == 0x5000)
		{
			// PLD is located at CE3=0x03000000
			dwData = BootLoader_ReadMem32( pao, nDSPIndex, 0x03000008);
			if((dwData & 0xF) != 0x5)
				return( Hpi6205_Error(nDSPIndex, HPI6205_ERROR_DSP_PLD) );
			dwData = BootLoader_ReadMem32( pao, nDSPIndex, 0x0300000C);
			if((dwData & 0xF) != 0xA)
				return( Hpi6205_Error(nDSPIndex, HPI6205_ERROR_DSP_PLD) );
			// 5000 - just for fun, turn off the LED attached to the PLD
			//BootLoader_WriteMem32(pao,0,pao->Pci.dwMemBase[0]+C6205_BAR0_TIMER1_CTL,4);	// SYSRES- = 1
			//BootLoader_WriteMem32( pao, nDSPIndex, 0x03000014, 0x02);                       // LED off
			//BootLoader_WriteMem32(pao,0,pao->Pci.dwMemBase[0]+C6205_BAR0_TIMER1_CTL,0);	// SYSRES- = 0 , LED back on
		}
		//BootLoader_WriteMem32( pao, nDSPIndex, 0x018C0024, 0x00003501);                       // 8705 - LED on
	}
	else if (nDSPIndex==1)
	{
		// DSP 1 is a C6713
		if(pao->Pci.wSubSysDeviceId == 0x8700)
		{
			// PLD is located at CE1=0x90000000
			dwData = BootLoader_ReadMem32( pao, nDSPIndex, 0x90000010);
			if((dwData & 0xFF) != 0xAA)
				return( Hpi6205_Error(nDSPIndex, HPI6205_ERROR_DSP_PLD) );
			BootLoader_WriteMem32( pao, nDSPIndex, 0x90000000, 0x02);                       // 8713 - LED on
		}
	}
	return(0);
}


static short Hpi6205_TransferData( HPI_ADAPTER_OBJ *pao,
				   HW8 HUGE *pData,
				   HW32 dwDataSize,
				   int nOperation)		// H620_H620_HIF_SEND_DATA or H620_HIF_GET_DATA
{
	HPI_HW_OBJ *pHw6205 = pao->priv;
	HW32 dwDataTransferred=0;
	//HW8 HUGE *pData =(HW8 HUGE *)phm->u.d.u.Data.dwpbData;
	//HW16 wTimeOut=8;
	HW16 wError=0;
	HW32 dwTimeOut,dwTemp1,dwTemp2;
	tBusMasteringInterfaceBuffer *interface=pHw6205->pInterfaceBuffer;

	dwDataSize &= ~3L;  // round dwDataSize down to nearest 4 bytes

	// make sure state is IDLE
	dwTimeOut = TIMEOUT;
	dwTemp2=0;
	while( (interface->dwDspAck != H620_HIF_IDLE) && dwTimeOut--	)
	{
		HpiOs_DelayMicroSeconds(1);
	}


	if (interface->dwDspAck != H620_HIF_IDLE)
		return HPI_ERROR_DSP_HARDWARE;

	interface->dwHostCmd = nOperation;

    while(dwDataTransferred < dwDataSize)
    {
		HW32 nThisCopy = dwDataSize - dwDataTransferred;

		if(nThisCopy>HPI6205_SIZEOF_DATA)
			nThisCopy = HPI6205_SIZEOF_DATA;

		if(nOperation==H620_HIF_SEND_DATA)
			memcpy((void *)&interface->u.bData[0],&pData[dwDataTransferred],nThisCopy);

		interface->dwTransferSizeInBytes = nThisCopy;

		// interrupt the DSP
		dwTemp1 = HPIOS_MEMREAD32(pHw6205->prHDCR );  //read the control register
		dwTemp1 |= (HW32)C6205_HDCR_DSPINT;
		HPIOS_MEMWRITE32(pHw6205->prHDCR,dwTemp1);
		dwTemp1 &= ~(HW32)C6205_HDCR_DSPINT;
		HPIOS_MEMWRITE32(pHw6205->prHDCR,dwTemp1);


		// spin waiting on the result
		dwTimeOut = TIMEOUT;
		dwTemp2=0;
		while( (dwTemp2 == 0) && dwTimeOut--)
		{
		    // give 16k bus mastering transfer time to happen
		    //(16k / 132Mbytes/s = 122usec)
		    HpiOs_DelayMicroSeconds(20);
		    dwTemp2 = 	HPIOS_MEMREAD32(pHw6205->prHSR);
		    dwTemp2 &=  C6205_HSR_INTSRC;
		}
		HPI_DEBUG_LOG2(DEBUG,"Spun %d times for data xfer of %d\n", TIMEOUT-dwTimeOut, nThisCopy);
		if(dwTemp2 ==  C6205_HSR_INTSRC)
		{
			HPI_DEBUG_LOG0(VERBOSE,"HPI6205.C - Interrupt from HIF <data> module OK\n");
			/*
			if(interface->dwDspAck != nOperation) {
			    HPI_DEBUG_LOG0(DEBUG("interface->dwDspAck=%d, expected %d \n",
					    interface->dwDspAck,nOperation);
			}
			*/
		}
// need to handle this differently...
		else {

#ifdef  HPI_OS_WDM
			DbgPrint("HPI6205.C - Interrupt from HIF <data> module BAD\n");
#endif
			wError=HPI_ERROR_DSP_HARDWARE;
		}

		// reset the interrupt from the DSP
		HPIOS_MEMWRITE32(pHw6205->prHSR,C6205_HSR_INTSRC);

		if(nOperation==H620_HIF_GET_DATA)
			memcpy(&pData[dwDataTransferred],(void *)&interface->u.bData[0],nThisCopy);

		  dwDataTransferred += nThisCopy;
    }
    if(interface->dwDspAck != nOperation /*HIF_DATA_DONE */) {
		HPI_DEBUG_LOG2(DEBUG,"interface->dwDspAck=%d, expected %d\n",
						interface->dwDspAck,nOperation);
		//			wError=HPI_ERROR_DSP_HARDWARE;
    }

	// set interface back to idle
	interface->dwHostCmd = H620_HIF_IDLE;
	// interrupt the DSP again
	dwTemp1 = HPIOS_MEMREAD32(pHw6205->prHDCR );  //read the control register
	dwTemp1 |= (HW32)C6205_HDCR_DSPINT;
	HPIOS_MEMWRITE32(pHw6205->prHDCR,dwTemp1);
	dwTemp1 &= ~(HW32)C6205_HDCR_DSPINT;
	HPIOS_MEMWRITE32(pHw6205->prHDCR,dwTemp1);

	return wError;
}

static unsigned int messageCount=0;
static short Hpi6205_MessageResponseSequence(
	HPI_ADAPTER_OBJ *pao,
    HPI_MESSAGE *phm,
    HPI_RESPONSE *phr)
{
	HW32 dwTemp1,dwTemp2,dwTimeOut,dwTimeOut2;
	HPI_HW_OBJ *pHw6205 = pao->priv;
	tBusMasteringInterfaceBuffer *interface=pHw6205->pInterfaceBuffer;
	HW16 wError=0;

	messageCount++;
	/* Assume buffer of type tBusMasteringInterfaceBuffer is allocated "noncacheable" */

	// make sure state is IDLE
	dwTimeOut = TIMEOUT;
	dwTemp2=0;
	while( (interface->dwDspAck != H620_HIF_IDLE) && --dwTimeOut	)
	{
		HpiOs_DelayMicroSeconds(1);
	}
	if(dwTimeOut==0) {
	    HPI_DEBUG_LOG0(DEBUG,"Timeout waiting for idle\n");
		return( Hpi6205_Error(0, HPI6205_ERROR_MSG_RESP_IDLE_TIMEOUT) );
	}

	// copy the message in to place
	//? memcpy((void *)&interface->u.MessageBuffer,phm,sizeof(HPI_MESSAGE));
	interface->u.MessageBuffer = *phm;
	// signal we want a response
	interface->dwHostCmd = H620_HIF_GET_RESP;

	// interrupt the DSP
	dwTemp1 = HPIOS_MEMREAD32(pHw6205->prHDCR );  //read the control register
	dwTemp1 |= (HW32)C6205_HDCR_DSPINT;
	HpiOs_DelayMicroSeconds(1);
	HPIOS_MEMWRITE32(pHw6205->prHDCR,dwTemp1);
	dwTemp1 &= ~(HW32)C6205_HDCR_DSPINT;
	HPIOS_MEMWRITE32(pHw6205->prHDCR,dwTemp1);

	// spin waiting on state change (start of msg process)
	dwTimeOut2 = TIMEOUT;
	dwTemp2=0;
	while((interface->dwDspAck != H620_HIF_GET_RESP) && --dwTimeOut2)
	{
		dwTemp2 = HPIOS_MEMREAD32(pHw6205->prHSR);
		dwTemp2 &=  C6205_HSR_INTSRC;
	}
	if(dwTimeOut2 ==0) {
	    HPI_DEBUG_LOG2(DEBUG,"(%u)Timed out waiting for GET_RESP state [%x]\n",messageCount,interface->dwDspAck);
	} else {
	    HPI_DEBUG_LOG2(VERBOSE,"(%u)Transition to GET_RESP after %u\n",messageCount,TIMEOUT-dwTimeOut2);
	}


	// spin waiting on HIF interrupt flag (end of msg process)
	dwTimeOut = TIMEOUT;
	dwTemp2=0;
	while( (dwTemp2 == 0) && --dwTimeOut)
	{
		dwTemp2 = 	HPIOS_MEMREAD32(pHw6205->prHSR);
		dwTemp2 &=  C6205_HSR_INTSRC;
		// HpiOs_DelayMicroSeconds(5);
	}
	if(dwTemp2 ==  C6205_HSR_INTSRC)
	{
	    if ((interface->dwDspAck != H620_HIF_GET_RESP)) {
		HPI_DEBUG_LOG3(DEBUG,"(%u)interface->dwDspAck(0x%x) != H620_HIF_GET_RESP, t=%u\n",messageCount,interface->dwDspAck,TIMEOUT-dwTimeOut);
	    } else {
		HPI_DEBUG_LOG2(VERBOSE,"(%u)Int with GET_RESP after %u\n",messageCount,TIMEOUT-dwTimeOut);
	    }


	}
// need to handle this differently...
#ifdef  HPI_OS_WDM
	else
		DbgPrint("HPI6205.C - Interrupt from HIF module BAD (wFunction %x)\n",phm->wFunction);
#endif


	// reset the interrupt from the DSP
	HPIOS_MEMWRITE32(pHw6205->prHSR,C6205_HSR_INTSRC);

	// read the result
	if (dwTimeOut != 0)
	//?	memcpy(phr,(void *)&interface->u.ResponseBuffer,sizeof(HPI_RESPONSE));
	*phr=interface->u.ResponseBuffer;

	// set interface back to idle
	interface->dwHostCmd = H620_HIF_IDLE;
	// interrupt the DSP again
	dwTemp1 = HPIOS_MEMREAD32(pHw6205->prHDCR );  //read the control register
	dwTemp1 |= (HW32)C6205_HDCR_DSPINT;
	HPIOS_MEMWRITE32(pHw6205->prHDCR,dwTemp1);
	dwTemp1 &= ~(HW32)C6205_HDCR_DSPINT;
	HPIOS_MEMWRITE32(pHw6205->prHDCR,dwTemp1);

// EWB move timeoutcheck to after IDLE command, maybe recover?
	if ((dwTimeOut==0) || (dwTimeOut2 ==0)) {
	    HPI_DEBUG_LOG0(DEBUG,"Something timed out!\n");
		return Hpi6205_Error( 0, HPI6205_ERROR_MSG_RESP_TIMEOUT );
	}

	// special case for adapter close - wait for the DSP to indicate it is idle
	if(phm->wFunction==HPI_ADAPTER_CLOSE)
	{
		// make sure state is IDLE
		dwTimeOut = TIMEOUT;
		dwTemp2=0;
		while( (interface->dwDspAck != H620_HIF_IDLE) && --dwTimeOut	)
		{
			HpiOs_DelayMicroSeconds(1);
		}
		if(dwTimeOut==0) {
			HPI_DEBUG_LOG0(DEBUG,"Timeout waiting for idle (on AdapterClose)\n");
			return( Hpi6205_Error(0, HPI6205_ERROR_MSG_RESP_IDLE_TIMEOUT) );
		}
	}
	wError = HpiValidateResponse( phm, phr); // SGT - validate the response message
	return wError;
}

static void  HW_Message( HPI_ADAPTER_OBJ *pao, HPI_MESSAGE *phm, HPI_RESPONSE *phr)
{

	HW16 nError = 0;
	HPIOS_LOCK_FLAGS(flags);

	HpiOs_Dsplock_Lock( pao, &flags );

	nError = Hpi6205_MessageResponseSequence( pao, phm, phr );

	 // maybe an error response
	 if( nError )
	 {
		  phr->wError = nError;		// something failed in the HPI/DSP interface
		  phr->wSize = HPI_RESPONSE_FIXED_SIZE;  // just the header of the response is valid
		  goto err;
	 }
	 if  (phr->wError != 0)          // something failed in the DSP
		  goto err;

	 switch (phm->wFunction) {
	 case HPI_OSTREAM_WRITE:
	 case HPI_ISTREAM_ANC_WRITE:
	   // nError = Hpi6205_TransferData( pao, phm, phr,H620_H620_HIF_SEND_DATA);
		nError = Hpi6205_TransferData( pao,
					phm->u.d.u.Data.pbData ,phm->u.d.u.Data.dwDataSize,
					H620_HIF_SEND_DATA);
		break;

	case HPI_ISTREAM_READ:
	case HPI_OSTREAM_ANC_READ:
		nError = Hpi6205_TransferData( pao,
					phm->u.d.u.Data.pbData ,phm->u.d.u.Data.dwDataSize,
					H620_HIF_GET_DATA);
		break;

	case HPI_CONTROL_SET_STATE:
		if (phm->wObject==HPI_OBJ_CONTROLEX && phm->u.cx.wAttribute==HPI_COBRANET_SET_DATA)
			nError = Hpi6205_TransferData( pao,
				phm->u.cx.u.cobranet_bigdata.pbData ,
				phm->u.cx.u.cobranet_bigdata.dwByteCount,
				H620_HIF_SEND_DATA);
		break;

	 case HPI_CONTROL_GET_STATE:
		if (phm->wObject==HPI_OBJ_CONTROLEX && phm->u.cx.wAttribute==HPI_COBRANET_GET_DATA)
			nError = Hpi6205_TransferData( pao,
					phm->u.cx.u.cobranet_bigdata.pbData,
					phr->u.cx.u.cobranet_data.dwByteCount,
					H620_HIF_GET_DATA);
		break;
	}
	phr->wError = nError;

err:
	 HpiOs_Dsplock_UnLock( pao, &flags );
	 return;
}

/*
	Win16 code to map to various 64k memory spaces.
*/

#ifdef HPI_OS_WIN16
HW32 dwBarSize[2]={0x400000,0x800000};
//#error Hpi6205_Abstract_ComputeAddress needs fixup now membase is a pointer
HW32 * Hpi6205_Abstract_ComputeAddress(HPI_ADAPTER_OBJ *pao, HW32 dwAddress)
{
	HW16 *BarSelectors[2];
	HPI_HW_OBJ *pHw6205 = pao->priv;
	int i;
	HW32 dwVirtualAddr=0;

	BarSelectors[0] = &(pHw6205->awBar0Selectors[0]);
	BarSelectors[1] = &(pHw6205->awBar1Selectors[0]);
	for(i=0;i<2;i++)
	{
		if( dwAddress >= (HW32)pao->Pci.apMemBase[i] )
		if( dwAddress < (HW32)pao->Pci.apMemBase[i] + dwBarSize[i] )
		{
			HW32 dwOffset = (dwAddress - (HW32)pao->Pci.apMemBase[i]);
			HW16 wSelectorIndex = (HW16)(dwOffset/0x10000L);
			dwVirtualAddr = MAKELONG(0,BarSelectors[i][wSelectorIndex]) ;
			dwVirtualAddr += (dwOffset & 0xFFFFL);
			return((HW32 *)dwVirtualAddr);
		}
	}
	return 0;
}
#endif

static HW16 Hpi6205_Error( int nDspIndex, int nError )
{
	return( (HW16)(HPI6205_ERROR_BASE + nDspIndex*100 + nError) );
}


///////////////////////////////////////////////////////////////////////////
