// asynInterposeGenicam.cpp

//
// Author: Bruce Hill
// Translates a simple ascii register read/write protocol
// compatible w/ streamdevice into Genicam packets
// and translates the response into streamdevice parsible
// ascii strings.
//

#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "cantProceed.h"
#include "epicsStdio.h"
#include "epicsString.h"
#include "epicsThread.h"
#include "epicsExport.h"
#include "iocsh.h"

#include "asynDriver.h"
#include "asynOctet.h"
#include "asynShellCommands.h"
#include "asynInterposeGenicam.h"
#include "GenTL.h"
#include "GenCpPacket.h"

#ifndef FALSE
#define	FALSE 0
#endif
#ifndef TRUE
#define	TRUE 1
#endif

int		DEBUG_GENICAM	= 0;

class asynInterposeGenicam
{
//	Public member functions
public:
	asynInterposeGenicam( const char *	portName, int addr );
	~asynInterposeGenicam( );

	asynStatus	AsciiToGenicam(	asynUser			*	pasynUser,
								const char			*	data,
								size_t					maxChars,
								const char			**	ppSendBufferRet,
								size_t				*	psSendBufferRet	);

//	Public member data
public:
    asynInterface		m_octet;
    asynOctet     	*	m_pasynOctetDrv;
    void        	*	m_drvPvt;

	// TODO: std::string m_portName
    char          	*	m_portName;
    int           		m_addr;
	bool				m_fInputFlushNeeded;
//	Private member data
private:
	unsigned long long	m_GenCpRegAddr;
	epicsUInt16			m_GenCpRequestId;
	unsigned int		m_GenCpResponseType;
	unsigned int		m_GenCpResponseCount;
	unsigned int		m_GenCpResponseSize;
	GenCpReadMemPacket	m_genCpReadMemPacket;
	GenCpWriteMemPacket	m_genCpWriteMemPacket;
	char				m_GenCpResponsePending[GENCP_RESPONSE_MAX];
};


/* asynOctet methods */
static asynStatus writeOctet(
	void			*	ppvt,
	asynUser		*	pasynUser,
    const char		*	data,
	size_t				maxChars,
	size_t			*	pnWritten );

static asynStatus readIt(
	void			*	ppvt,
	asynUser		*	pasynUser,
    char			*	data,
	size_t				maxchars,
	size_t			*	nbytesTransfered,
	int				*	eomReason );

static asynStatus flushIt(
	void			*	ppvt,
	asynUser		*	pasynUser );

static asynStatus registerInterruptUser(
	void			*	drvPvt,
	asynUser		*	pasynUser,
    interruptCallbackOctet callback,
	void			*	userPvt,
	void			**	registrarPvt );

static asynStatus cancelInterruptUser(
	void			*	drvPvt,
	asynUser		*	pasynUser,
    void			*	registrarPvt );

static asynStatus setInputEos(
	void			*	ppvt,
	asynUser		*	pasynUser,
    const char		*	eos,
	int					eoslen );

static asynStatus getInputEos(
	void			*	ppvt,
	asynUser		*	pasynUser,
    char			*	eos,
	int					eossize,
	int				*	eoslen );

static asynStatus setOutputEos(
	void			*	ppvt,
	asynUser		*	pasynUser,
    const char		*	eos,
	int					eoslen );

static asynStatus getOutputEos(
	void			*	ppvt,
	asynUser		*	pasynUser,
    char			*	eos,
	int					eossize,
	int				*	eoslen );

static asynOctet genicamOctetInterface =
{
    writeOctet, readIt, flushIt,
    registerInterruptUser, cancelInterruptUser,
    setInputEos, getInputEos, setOutputEos, getOutputEos
};
 
extern "C" epicsShareFunc int
asynInterposeGenicamConfig( const char *	portName, int addr )
{
    asynStatus			status;
    asynInterface	*	pasynOctet;

	asynInterposeGenicam	*	pInterposeGenicam	= new asynInterposeGenicam( portName, addr );
    status = pasynManager->interposeInterface(portName, addr, &pInterposeGenicam->m_octet, &pasynOctet );
    if ( status != asynSuccess || pasynOctet == NULL )
	{
        printf( "%s asynInterposeGenicamConfig failed.\n", portName );
        delete pInterposeGenicam;
        return -1;
    }
    pInterposeGenicam->m_pasynOctetDrv	= (asynOctet *)pasynOctet->pinterface;
    pInterposeGenicam->m_drvPvt			= pasynOctet->drvPvt;
    return 0;
}
 
/* asynOctet methods */
static asynStatus writeOctet(
	void				*	ppvt,
	asynUser 			*	pasynUser,
    const char			*	data,
	size_t					maxChars,
	size_t				*	pnWritten )
#if 1
{
	asynInterposeGenicam *	pInterposeGenicam	= reinterpret_cast<asynInterposeGenicam *>( ppvt );
	asynStatus				status			= asynSuccess;
    static const char	*	functionName	= "asynEdtPdvSerial::writeOctet";

	asynPrint(	pasynUser, ASYN_TRACE_FLOW,
				"%s: %s maxChars %zu\n", functionName, pInterposeGenicam->m_portName, maxChars );

	if ( pnWritten )
		*pnWritten = 0;

	if ( maxChars == 0 )
		return asynSuccess;
	const char		*	pSendBuffer	= NULL;
	size_t				sSendBuffer	= 0;
	status	= pInterposeGenicam->AsciiToGenicam( pasynUser, data, maxChars,
											&pSendBuffer, &sSendBuffer );

	size_t		nSent	= 0;
	if ( pSendBuffer && sSendBuffer )
	{
		status = pInterposeGenicam->m_pasynOctetDrv->write(
					pInterposeGenicam->m_drvPvt,
					pasynUser, pSendBuffer, sSendBuffer, &nSent );

		if ( status == 0 )
		{
			*pnWritten = strlen( data );

			asynPrint(	pasynUser,	ASYN_TRACE_FLOW,
						"%s: sent %zu pkt to %s for: %s\n",
						functionName, sSendBuffer, pInterposeGenicam->m_portName, data	);
			asynPrintIO(	pasynUser, ASYN_TRACEIO_DRIVER, data, *pnWritten,
							"%s: %s wrote %zu\n", functionName, pInterposeGenicam->m_portName, sSendBuffer );
		}
		else
		{
			epicsSnprintf(	pasynUser->errorMessage, pasynUser->errorMessageSize,
							"%s: %s write error: %s\n",
							functionName, pInterposeGenicam->m_portName, strerror(errno)	);
			status = asynError;
			pInterposeGenicam->m_fInputFlushNeeded = TRUE;
		}
	}

    return status;
}
#else
{
    asynInterposeGenicam	*	pInterposeGenicam = (asynInterposeGenicam *)ppvt;
    int i;
    size_t n;
    asynStatus status;

    if (pnWritten)
        *pnWritten = 0;
    for (i = 0 ; i < maxChars ; i++)
	{
        status = pInterposeGenicam->m_pasynOctetDrv->write(
					pInterposeGenicam->m_drvPvt,
            		pasynUser, data, 1, &n );
        data += n;
        if (pnWritten)
            *pnWritten += n;
        if (status != asynSuccess)
            return status;
    }
    return asynSuccess;
}
#endif

static asynStatus readIt(
	void			*	ppvt,
	asynUser		*	pasynUser,
    char			*	data,
	size_t				maxchars,
	size_t			*	nbytesTransfered,
	int				*	eomReason )
{
    asynInterposeGenicam	*	pInterposeGenicam = (asynInterposeGenicam *)ppvt;

    return pInterposeGenicam->m_pasynOctetDrv->read( pInterposeGenicam->m_drvPvt,
        pasynUser, data, maxchars, nbytesTransfered, eomReason );
}

static asynStatus flushIt(
	void			*	ppvt,
	asynUser		*	pasynUser )
{
    asynInterposeGenicam *pInterposeGenicam = (asynInterposeGenicam *)ppvt;
    
    return pInterposeGenicam->m_pasynOctetDrv->flush(
        pInterposeGenicam->m_drvPvt, pasynUser );
}

static asynStatus registerInterruptUser(
	void *ppvt,
	asynUser *pasynUser,
    interruptCallbackOctet callback,
	void *userPvt,
	void **registrarPvt )
{
    asynInterposeGenicam *pInterposeGenicam = (asynInterposeGenicam *)ppvt;

    return pInterposeGenicam->m_pasynOctetDrv->registerInterruptUser(
        pInterposeGenicam->m_drvPvt, pasynUser, callback, userPvt, registrarPvt );
}

static asynStatus cancelInterruptUser(
	void *drvPvt,
	asynUser *pasynUser,
    void *registrarPvt )
{
    asynInterposeGenicam *pInterposeGenicam = (asynInterposeGenicam *)drvPvt;

    return pInterposeGenicam->m_pasynOctetDrv->cancelInterruptUser(
        pInterposeGenicam->m_drvPvt, pasynUser, registrarPvt );
}

static asynStatus setInputEos(
	void *ppvt,
	asynUser *pasynUser,
    const char *eos,
	int eoslen )
{
    asynInterposeGenicam *pInterposeGenicam = (asynInterposeGenicam *)ppvt;

    return pInterposeGenicam->m_pasynOctetDrv->setInputEos(
        pInterposeGenicam->m_drvPvt, pasynUser, eos, eoslen );
}

static asynStatus getInputEos(
	void *ppvt,
	asynUser *pasynUser,
    char *eos,
	int eossize,
	int *eoslen )
{
    asynInterposeGenicam *pInterposeGenicam = (asynInterposeGenicam *)ppvt;

    return pInterposeGenicam->m_pasynOctetDrv->getInputEos(
        pInterposeGenicam->m_drvPvt, pasynUser, eos, eossize, eoslen );
}

static asynStatus setOutputEos(
	void *ppvt,
	asynUser *pasynUser,
    const char *eos,
	int eoslen )
{
    asynInterposeGenicam *pInterposeGenicam = (asynInterposeGenicam *)ppvt;

    return pInterposeGenicam->m_pasynOctetDrv->setOutputEos(
        pInterposeGenicam->m_drvPvt, pasynUser, eos, eoslen );
}

static asynStatus getOutputEos(
	void *ppvt,
	asynUser *pasynUser,
    char *eos,
	int eossize,
	int *eoslen )
{
    asynInterposeGenicam *pInterposeGenicam = (asynInterposeGenicam *)ppvt;

    return pInterposeGenicam->m_pasynOctetDrv->getOutputEos(
        pInterposeGenicam->m_drvPvt, pasynUser, eos, eossize, eoslen );
}


//
// asynInterposeGenicam class member functions
//

/// asynInterposeGenicam constructor
asynInterposeGenicam::asynInterposeGenicam( const char *	portName, int addr )
    :	m_octet(							),
    	m_pasynOctetDrv(			NULL	),
    	m_drvPvt(					NULL	),
    	m_portName(					NULL	),
    	m_addr(						addr	),
		m_fInputFlushNeeded(		FALSE	),			
		m_GenCpRegAddr(				0LL		),			
		m_GenCpRequestId(			0		),
		m_GenCpResponseType(		0		),
		m_GenCpResponseCount(		0		),
		m_GenCpResponseSize(		0		),
		m_genCpReadMemPacket(				),
		m_genCpWriteMemPacket(				),
		m_GenCpResponsePending(				)
{
	m_portName = epicsStrDup( portName );
    m_octet.interfaceType = asynOctetType;
    m_octet.pinterface = &genicamOctetInterface;
    m_octet.drvPvt = this;
}

asynInterposeGenicam::~asynInterposeGenicam()
{
	free( (void *)m_portName );
	m_portName = NULL;
}

asynStatus	asynInterposeGenicam::AsciiToGenicam(
	asynUser			*	pasynUser,
    const char			*	data,
	size_t					maxChars,
	const char			**	ppSendBufferRet,
	size_t				*	psSendBufferRet	)
{
    static const char	*	functionName	= "asynInterposeGenicam::AsciiToGenicam";
	uint16_t				requestId		= 0xFFFF;
	
	GENCP_STATUS			genStatus;
	char					cGetSet;	// '?' is a Get, '=' is a Set
	unsigned int			cmdCount;
	const char			*	pString;
	double					doubleValue		= 0.0;
	int64_t					intValue		= 0LL;
	uint64_t				regAddr			= 0LL;
	int						scanCount		= -1;
	const char			*	pEqualSign		= strchr( data, '=' );

	if ( ppSendBufferRet == NULL || psSendBufferRet == NULL )
		return asynError;

	m_GenCpResponsePending[0] = '\0';

	// Parse the simple streamdevice ascii protocol and replace it w/ a GenCpReadMemPacket.
	switch ( *data )
	{
	case 'C':
		scanCount = sscanf( data, "C%u %Li %c", &cmdCount, (long long int *) &regAddr, &cGetSet );
		if ( scanCount == 3 && cGetSet == '=' && cmdCount > 0 )
		{
			assert( pEqualSign != NULL );
			pString		= pEqualSign + 1;
			requestId	= m_GenCpRequestId;
			genStatus	= GenCpInitWriteMemPacket(	&m_genCpWriteMemPacket, m_GenCpRequestId++, regAddr,
													cmdCount, pString, psSendBufferRet );
			*ppSendBufferRet		= reinterpret_cast<char *>( &m_genCpWriteMemPacket );
			m_GenCpResponseCount	= cmdCount;
			m_GenCpResponseType		= GENCP_TY_RESP_ACK;
			m_GenCpResponseSize		= sizeof(GenCpWriteMemAck);
		}
		else if ( scanCount == 3 && cGetSet == '?' && cmdCount > 0 )
		{
			requestId	= m_GenCpRequestId;
			genStatus	= GenCpInitReadMemPacket( &m_genCpReadMemPacket, m_GenCpRequestId++, regAddr, cmdCount );
			*ppSendBufferRet		= reinterpret_cast<char *>( &m_genCpReadMemPacket );
			*psSendBufferRet		= sizeof(m_genCpReadMemPacket);
			m_GenCpResponseCount	= cmdCount;
			m_GenCpResponseType		= GENCP_TY_RESP_STRING;
			m_GenCpResponseSize		= sizeof(GenCpSerialPrefix) + sizeof(GenCpCCDAck) + cmdCount;
		}
		else
			scanCount = -1;
		break;

	case 'U':
		scanCount = sscanf( data, "U%u %Li %c%Li", &cmdCount, (long long int *) &regAddr, &cGetSet,
							(long long int *) &intValue );
		asynPrint(	pasynUser, ASYN_TRACE_FLOW,
					"%s %s: scanCount=%d, cmdCount=%u, regAddr=0x%llX, cGetSet=%c, intValue=%lld, command: %s\n",
					functionName, m_portName, scanCount, cmdCount, (long long unsigned int) regAddr,
					cGetSet, (long long int) intValue, data );
		if ( scanCount == 4 && cGetSet == '=' && cmdCount > 0 )
		{
			uint16_t	value16	= static_cast<uint16_t>( intValue );
			uint32_t	value32	= static_cast<uint32_t>( intValue );
			uint64_t	value64	= static_cast<uint64_t>( intValue );
			assert( pEqualSign != NULL );
			switch ( cmdCount )
			{
			case 16:
				requestId	= m_GenCpRequestId;
				genStatus	= GenCpInitWriteMemPacket(	&m_genCpWriteMemPacket, m_GenCpRequestId++, regAddr,
														value16, psSendBufferRet );
				break;
			case 32:	
				requestId	= m_GenCpRequestId;
				genStatus	= GenCpInitWriteMemPacket(	&m_genCpWriteMemPacket, m_GenCpRequestId++, regAddr,
														value32, psSendBufferRet );
				break;
			case 64:	
				requestId	= m_GenCpRequestId;
				genStatus	= GenCpInitWriteMemPacket(	&m_genCpWriteMemPacket, m_GenCpRequestId++, regAddr,
														value64, psSendBufferRet );
				break;
			}
			*ppSendBufferRet		= reinterpret_cast<char *>( &m_genCpWriteMemPacket );
			m_GenCpResponseCount	= cmdCount;
			m_GenCpResponseType		= GENCP_TY_RESP_ACK;
			m_GenCpResponseSize		= sizeof(GenCpWriteMemAck);
		}
		else if ( scanCount == 3 && cGetSet == '?' && cmdCount > 0 )
		{
			requestId	= m_GenCpRequestId;
			genStatus	= GenCpInitReadMemPacket( &m_genCpReadMemPacket, m_GenCpRequestId++, regAddr, cmdCount / 8 );
			*ppSendBufferRet		= reinterpret_cast<char *>( &m_genCpReadMemPacket );
			*psSendBufferRet		= sizeof(m_genCpReadMemPacket);
			m_GenCpResponseCount	= cmdCount;
			m_GenCpResponseType		= GENCP_TY_RESP_UINT;
			m_GenCpResponseSize		= sizeof(GenCpSerialPrefix) + sizeof(GenCpCCDAck) + cmdCount / 8;
		}
		else
			scanCount = -1;
		break;

	case 'F':
		scanCount = sscanf( data, "F%u %Li %c%lf", &cmdCount, (long long int *) &regAddr, &cGetSet, &doubleValue );
		asynPrint(	pasynUser, ASYN_TRACE_FLOW,
					"%s %s: scanCount=%d, cmdCount=%u, regAddr=0x%llX, cGetSet=%c, doubleValue=%lf, command: %s\n",
					functionName, m_portName, scanCount, cmdCount, (long long unsigned int) regAddr,
					cGetSet, doubleValue, data );
		if ( scanCount == 4 && cGetSet == '=' && cmdCount > 0 )
		{
			float	floatValue	= static_cast<float>( doubleValue );
			assert( pEqualSign != NULL );
			switch ( cmdCount )
			{
			case 32:	
				requestId	= m_GenCpRequestId;
				genStatus	= GenCpInitWriteMemPacket(	&m_genCpWriteMemPacket, m_GenCpRequestId++, regAddr,
														floatValue, psSendBufferRet );
				break;
			case 64:	
				requestId	= m_GenCpRequestId;
				genStatus	= GenCpInitWriteMemPacket(	&m_genCpWriteMemPacket, m_GenCpRequestId++, regAddr,
														doubleValue, psSendBufferRet );
				break;
			}
			*ppSendBufferRet		= reinterpret_cast<char *>( &m_genCpWriteMemPacket );
			m_GenCpResponseCount	= cmdCount;
			m_GenCpResponseType		= GENCP_TY_RESP_ACK;
			m_GenCpResponseSize		= sizeof(GenCpWriteMemAck);
		}
		else if ( scanCount == 3 && cGetSet == '?' && cmdCount > 0 )
		{
			requestId	= m_GenCpRequestId;
			genStatus	= GenCpInitReadMemPacket( &m_genCpReadMemPacket, m_GenCpRequestId++, regAddr, cmdCount / 8 );
			*ppSendBufferRet		= reinterpret_cast<char *>( &m_genCpReadMemPacket );
			*psSendBufferRet		= sizeof(m_genCpReadMemPacket);
			m_GenCpResponseCount	= cmdCount;
			if ( cmdCount == 32 )
				m_GenCpResponseType		= GENCP_TY_RESP_FLOAT;
			else
				m_GenCpResponseType		= GENCP_TY_RESP_DOUBLE;
			m_GenCpResponseSize		= sizeof(GenCpSerialPrefix) + sizeof(GenCpCCDAck) + cmdCount / 8;
		}
		else
			scanCount = -1;
		break;

	case 'I':
	default:
		break;
	}
	m_GenCpRegAddr = regAddr;

	if ( scanCount == -1 )
	{
		epicsSnprintf(	pasynUser->errorMessage, pasynUser->errorMessageSize,
						"%s: %s Invalid GenCP command: %s\n",
						functionName, m_portName, data	);
		m_fInputFlushNeeded = TRUE;
		return asynError;
	}

	asynPrint(	pasynUser, ASYN_TRACE_FLOW,
				"%s %s: responseType=%u, responseCount=%u, responseSize=%u\n",
				functionName, m_portName, m_GenCpResponseType, m_GenCpResponseCount, m_GenCpResponseSize );

	if ( requestId != 0xFFFF )
	{
		if ( DEBUG_GENICAM >= 3 )
			printf( "REQUESTID %-5hu: Sending  %zu bytes\n", requestId, *psSendBufferRet );
	}

	return asynSuccess;
}


/* register asynInterposeGenicamConfig*/
static const iocshArg asynInterposeGenicamConfigArg0 =
    { "portName", iocshArgString };
static const iocshArg asynInterposeGenicamConfigArg1 =
    { "addr", iocshArgInt };
static const iocshArg *asynInterposeGenicamConfigArgs[] =
{
    &asynInterposeGenicamConfigArg0,
    &asynInterposeGenicamConfigArg1,
};
static const iocshFuncDef asynInterposeGenicamConfigFuncDef =
{	"asynInterposeGenicamConfig",
	2,
	asynInterposeGenicamConfigArgs
};
static void asynInterposeGenicamConfigCallFunc( const iocshArgBuf *args)
{
    asynInterposeGenicamConfig( args[0].sval, args[1].ival );
}

static void asynInterposeGenicamRegister(void)
{
    static int firstTime = 1;
    if ( firstTime )
	{
        firstTime = 0;
        iocshRegister( &asynInterposeGenicamConfigFuncDef,
            			asynInterposeGenicamConfigCallFunc );
    }
}

epicsExportRegistrar( asynInterposeGenicamRegister );
