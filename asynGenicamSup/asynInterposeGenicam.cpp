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

	asynStatus	GenicamToAscii( asynUser			*	pasynUser,
								char				*	pBuffer,
								size_t					nBytesReadMax,
								size_t				*	pnRead,
								int					*	eomReason );

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
	GenCpReadMemAck		m_genCpReadMemAck;
	GenCpWriteMemAck	m_genCpWriteMemAck;
	char				m_GenCpResponsePending[GENCP_RESPONSE_MAX];
};


/* asynOctet methods */
static asynStatus writeOctet(
	void			*	ppvt,
	asynUser		*	pasynUser,
    const char		*	data,
	size_t				maxChars,
	size_t			*	pnWritten );

static asynStatus readOctet(
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
    writeOctet, readOctet, flushIt,
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
{
	asynInterposeGenicam *	pInterposeGenicam	= reinterpret_cast<asynInterposeGenicam *>( ppvt );
	asynStatus				status			= asynSuccess;
    static const char	*	functionName	= "asynInterposeGenicam  writeOctet";

	asynPrint(	pasynUser, ASYN_TRACE_FLOW,
				"%s: %s maxChars %zu: %s\n", functionName, pInterposeGenicam->m_portName, maxChars, data );

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

static asynStatus readOctet(
	void			*	ppvt,
	asynUser		*	pasynUser,
    char			*	data,
	size_t				nBytesReadMax,
	size_t			*	pnRead,
	int				*	eomReason )
{
	asynInterposeGenicam *	pInterposeGenicam	= reinterpret_cast<asynInterposeGenicam *>( ppvt );
	asynStatus				status			= asynSuccess;
    static const char	*	functionName	= "asynInterposeGenicam  readOctet";

	asynPrint(	pasynUser, ASYN_TRACE_FLOW,
				"%s: %s nBytesReadMax %zu\n", functionName, pInterposeGenicam->m_portName, nBytesReadMax );
	if ( DEBUG_GENICAM >= 3 ) printf(
				"%s: %s nBytesReadMax %zu\n", functionName, pInterposeGenicam->m_portName, nBytesReadMax );

	if ( pnRead )
		*pnRead = 0;
	if ( eomReason )
		*eomReason = ASYN_EOM_EOS;

	if ( nBytesReadMax == 0 )
		return asynSuccess;

	status	= pInterposeGenicam->GenicamToAscii( pasynUser, data, nBytesReadMax, pnRead, eomReason );

	if ( pnRead && *pnRead > 0 )
	{
		if ( DEBUG_GENICAM >= 3 )
			printf( "%s: %s Read %zu: %s\n", functionName, pInterposeGenicam->m_portName, *pnRead, data );
		asynPrintIO(	pasynUser, ASYN_TRACEIO_DRIVER, data, *pnRead,
						"%s: %s read %zu\n",
						functionName, pInterposeGenicam->m_portName, *pnRead );
		asynPrint(		pasynUser, ASYN_TRACE_FLOW,
						"%s: %s read %zu, status %d, Buffer: %s\n",
						functionName, pInterposeGenicam->m_portName, *pnRead, status, data	);
	}

    return status;
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
	long long int			intValue		= 0LL;
	unsigned long long		regAddr			= 0LL;
	int						scanCount		= -1;
	const char			*	pEqualSign		= strchr( data, '=' );

	if ( ppSendBufferRet == NULL || psSendBufferRet == NULL )
		return asynError;

	m_GenCpResponsePending[0] = '\0';

	// Parse the simple streamdevice ascii protocol and replace it w/ a GenCpReadMemPacket.
	switch ( *data )
	{
	case 'C':
		char	stringValue[128];
		scanCount = sscanf( data, "C%u %Li %c%127s", &cmdCount, &regAddr, &cGetSet, &stringValue[0] );
		asynPrint(	pasynUser, ASYN_TRACE_FLOW,
					"%s %s: scanCount=%d, cmdCount=%u, regAddr=0x%llX, cGetSet=%c, intValue=%lld, command: %s\n",
					functionName, m_portName, scanCount, cmdCount, regAddr, cGetSet, intValue, data );
		if ( scanCount == 4 && cGetSet == '=' && cmdCount > 0 )
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
		{
			epicsSnprintf(	pasynUser->errorMessage, pasynUser->errorMessageSize,
							"%s: %s scan error: scanCount=%d, cGetSet=%c, cmdCount=%u: %s\n",
							functionName, m_portName, scanCount, cGetSet, cmdCount, data	);
			scanCount = -1;
		}
		break;

	case 'U':
		scanCount = sscanf( data, "U%u %Li %c%Li", &cmdCount, &regAddr, &cGetSet, &intValue );
		asynPrint(	pasynUser, ASYN_TRACE_FLOW,
					"%s %s: scanCount=%d, cmdCount=%u, regAddr=0x%llX, cGetSet=%c, intValue=%lld, command: %s\n",
					functionName, m_portName, scanCount, cmdCount, regAddr, cGetSet, intValue, data );
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
			printf( "REQUESTID %-5hu: Sending  %zu bytes, responseSize=%u\n",
					requestId, *psSendBufferRet, m_GenCpResponseSize );
	}

	return asynSuccess;
}

asynStatus	asynInterposeGenicam::GenicamToAscii(
	asynUser			*	pasynUser,
	char				*	pBuffer,
	size_t					nBytesReadMax,
	size_t				*	pnRead,
	int					*	eomReason	)
{
	asynStatus				status			= asynSuccess;
    static const char	*	functionName	= "asynInterposeGenicam::GenicamToAscii";
	char					genCpResponseBuffer[GENCP_RESPONSE_MAX];
    
	if ( pnRead )
		*pnRead = 0;
	if ( eomReason )
		*eomReason = ASYN_EOM_EOS;

	if ( nBytesReadMax == 0 )
	{
		epicsSnprintf(	pasynUser->errorMessage, pasynUser->errorMessageSize,
						"%s: %s nBytesReadMax is 0! Why?\n", functionName, m_portName );
		return asynError;
	}


	char		*	pReadBuffer	= NULL;
	size_t			sReadBuffer	= 0;
	size_t			nBytesPending = strlen( m_GenCpResponsePending );
	if ( nBytesPending > 0 )
	{
		if ( DEBUG_GENICAM >= 3 ) printf(
				"%s Entry: %s nBytesPending %zu: %s\n", functionName, m_portName, nBytesPending, m_GenCpResponsePending );
		// Unable to return entire response on last call
		// Typically because streamdevice sets nBytesReadMax to 1 on first call
		if ( pnRead )
			*pnRead = nBytesPending;
		strncpy( pBuffer, m_GenCpResponsePending, nBytesReadMax );
		m_GenCpResponsePending[0] = '\0';
		if ( eomReason )
			*eomReason = ASYN_EOM_END;

		if ( DEBUG_GENICAM >= 3 )
			printf( "%s: %s Read pending %zu: %s\n", functionName, m_portName, nBytesPending, pBuffer );
		asynPrintIO(	pasynUser, ASYN_TRACEIO_DRIVER, pBuffer, nBytesPending,
						"%s: %s read %zu of %zu\n",
						functionName, m_portName, nBytesPending, nBytesPending );
		asynPrint(	pasynUser, ASYN_TRACE_FLOW,
					"%s: %s read pending %zu, status %d, Buffer: %s\n",
					functionName, m_portName, nBytesPending, status, pBuffer	);

		return asynSuccess;
	}

	switch ( m_GenCpResponseType )
	{
	case GENCP_TY_RESP_ACK:
		pReadBuffer	= reinterpret_cast<char *>( &m_genCpWriteMemAck );
		sReadBuffer	= m_GenCpResponseSize;
		break;
	case GENCP_TY_RESP_STRING:
	case GENCP_TY_RESP_UINT:
	case GENCP_TY_RESP_INT:
	case GENCP_TY_RESP_FLOAT:
	case GENCP_TY_RESP_DOUBLE:
		pReadBuffer	= reinterpret_cast<char *>( &m_genCpReadMemAck );
		sReadBuffer	= m_GenCpResponseSize;
		break;
	default:
		epicsSnprintf(	pasynUser->errorMessage, pasynUser->errorMessageSize,
						"%s: %s Invalid Response Type: %u\n",
						functionName, m_portName, m_GenCpResponseType );
		break;
	}

	size_t		nRead	= 0;
	for (;;)
	{
		if ( DEBUG_GENICAM >= 4 )
			printf( "%s: %s nBytesReadMax %zu, sReadBuffer %zu, timeout %e ...\n",
					functionName, m_portName, nBytesReadMax, sReadBuffer, pasynUser->timeout );

		if ( pReadBuffer != NULL && sReadBuffer > 0 )
    	status = m_pasynOctetDrv->read(	m_drvPvt, pasynUser, pReadBuffer, sReadBuffer, &nRead, eomReason );
		if( nRead > 0 )
		{
			if ( DEBUG_GENICAM >= 3 )
				printf( "%s: %s Read %zu bytes ...\n", functionName, m_portName, nRead );
		}
		else
		{
			if ( pnRead )
				*pnRead = 0;
            if ( DEBUG_GENICAM >= 4 )
                printf( "%s: %s nRead=0 ...\n", functionName, m_portName );

            return asynSuccess;
        }

		// If we read something
		if( nRead > 0 )
			break;			/* If we have something, we're done. */

		// Handle errors
		if (	(nRead < 0)
			&&	(errno != EWOULDBLOCK)
			&&	(errno != EINTR)
			&&	(errno != EAGAIN) )
		{
			epicsSnprintf(	pasynUser->errorMessage, pasynUser->errorMessageSize,
							"%s: %s read error: %s\n",
							functionName, m_portName, strerror(errno)	);
			status = asynError;
			m_fInputFlushNeeded = true;
			break;		/* If we have an error, we're done. */
		}
		if ( pasynUser->timeout > 0 )
			break;			/* If we aren't waiting forever, we're done. */
	}	// end forever loop

	if ( nRead == 0 && (pasynUser->timeout > 0) && (status == asynSuccess))	/* If we don't have anything, not even an error	*/
	{
		status = asynTimeout;					/* then we must have timed out.					*/
		if ( eomReason )
			*eomReason = ASYN_EOM_EOS;
	}

	{
	size_t					nBytesRead;
	GENCP_STATUS			genStatus;
	GenCpReadMemAck		*	pReadAck	= reinterpret_cast<GenCpReadMemAck *>(	pReadBuffer );
	GenCpWriteMemAck	*	pWriteAck	= reinterpret_cast<GenCpWriteMemAck *>(	pReadBuffer );
	assert( GetRequestId(&pReadAck->ccd) == GetRequestId(&pWriteAck->ccd) );
	if ( DEBUG_GENICAM >= 3 )
		printf( "REQUESTID %-5hu: Received %zu bytes\n", GetRequestId(&pReadAck->ccd), nRead );

	switch ( m_GenCpResponseType )
	{
	case GENCP_TY_RESP_ACK:
		genStatus = GenCpValidateWriteMemAck( pWriteAck, m_GenCpRequestId-1 );
		if ( genStatus != GENCP_STATUS_SUCCESS )
		{
			// TODO: Add status code to error msg translation here
			snprintf( genCpResponseBuffer, GENCP_RESPONSE_MAX, "ERR %d (0x%X)\n", genStatus, genStatus );
			fprintf( stderr, "%s: ReadMemString Validate Error: %d (0x%X)\n", functionName, genStatus, genStatus );
			m_fInputFlushNeeded = true;
			return asynError;
		}
		strncpy( genCpResponseBuffer, "OK\n", GENCP_RESPONSE_MAX );
		break;
	case GENCP_TY_RESP_STRING:
		snprintf( genCpResponseBuffer, GENCP_RESPONSE_MAX, "R0x%LX=", m_GenCpRegAddr );
		genStatus = GenCpProcessReadMemAck( pReadAck, m_GenCpRequestId-1, genCpResponseBuffer + strlen(genCpResponseBuffer), (size_t)(GENCP_RESPONSE_MAX - strlen(genCpResponseBuffer)), &nBytesRead );
		if ( genStatus != GENCP_STATUS_SUCCESS )
		{
			// TODO: Add status code to error msg translation here
			snprintf( genCpResponseBuffer, GENCP_RESPONSE_MAX, "ERR %d (0x%X)\n", genStatus, genStatus );
			fprintf( stderr, "%s: ReadMemString Validate Error: %d (0x%X)\n", functionName, genStatus, genStatus );
			m_fInputFlushNeeded = true;
			return asynError;
		}
		strcat( genCpResponseBuffer, "\n" );
		break;
	case GENCP_TY_RESP_UINT:
		switch ( m_GenCpResponseCount )
		{
		case 16:
			uint16_t	valueUint16;
			genStatus = GenCpProcessReadMemAck( pReadAck, m_GenCpRequestId-1, &valueUint16 );
			snprintf( genCpResponseBuffer, GENCP_RESPONSE_MAX, "R0x%llX=%hu (0x%02hX)\n", m_GenCpRegAddr, valueUint16, valueUint16 );
			break;
		case 32:
			uint32_t	valueUint32;
			genStatus = GenCpProcessReadMemAck( pReadAck, m_GenCpRequestId-1, &valueUint32 );
			snprintf( genCpResponseBuffer, GENCP_RESPONSE_MAX, "R0x%llX=%u (0x%04X)\n", m_GenCpRegAddr, valueUint32, valueUint32 );
			break;
		case 64:
			uint64_t	valueUint64;
			genStatus = GenCpProcessReadMemAck( pReadAck, m_GenCpRequestId-1, &valueUint64 );
			snprintf( genCpResponseBuffer, GENCP_RESPONSE_MAX, "R0x%llX=%llu (0x%08llX)\n", m_GenCpRegAddr,
					(long long unsigned int) valueUint64, (long long unsigned int) valueUint64 );
			break;
		default:
			genStatus	= GENCP_STATUS_INVALID_PARAM;
			break;
		}
		if ( genStatus != GENCP_STATUS_SUCCESS )
		{
			// TODO: Add status code to error msg translation here
			snprintf( genCpResponseBuffer, GENCP_RESPONSE_MAX, "ERR %d (0x%X)\n", genStatus, genStatus );
			fprintf( stderr, "%s: Uint ProcessReadMem Error: %d (0x%X)\n", functionName, genStatus, genStatus );
			m_fInputFlushNeeded = true;
			return asynError;
		}
		break;
	case GENCP_TY_RESP_FLOAT:
		switch ( m_GenCpResponseCount )
		{
		case 32:
			float		floatValue;
			genStatus = GenCpProcessReadMemAck( pReadAck, m_GenCpRequestId-1, &floatValue );
			snprintf( genCpResponseBuffer, GENCP_RESPONSE_MAX, "R0x%llX=%f\n", m_GenCpRegAddr, floatValue );
			break;
		case 64:
			double		doubleValue;
			genStatus = GenCpProcessReadMemAck( pReadAck, m_GenCpRequestId-1, &doubleValue );
			snprintf( genCpResponseBuffer, GENCP_RESPONSE_MAX, "R0x%llX=%lf\n", m_GenCpRegAddr, doubleValue );
			break;
		default:
			genStatus	= GENCP_STATUS_INVALID_PARAM;
			break;
		}
		if ( genStatus != GENCP_STATUS_SUCCESS )
		{
			// TODO: Add status code to error msg translation here
			snprintf( genCpResponseBuffer, GENCP_RESPONSE_MAX, "ERR %d (0x%X)\n", genStatus, genStatus );
			fprintf( stderr, "%s: Uint ProcessReadMem Error: %d (0x%X)\n", functionName, genStatus, genStatus );
			m_fInputFlushNeeded = true;
			return asynError;
		}
		break;
	case GENCP_TY_RESP_INT:
	default:
		fprintf( stderr, "%s: Unsupported response type: %d\n", functionName, m_GenCpResponseType );
		return asynError;
		break;
	}
	}

	{
	size_t		nBytesResponse = strlen( genCpResponseBuffer );
	m_GenCpResponsePending[0] = '\0';
	if ( nBytesResponse > nBytesReadMax )
	{
		if ( DEBUG_GENICAM >= 3 ) printf(
				"%s Exit: %s Rtn %zu, pend %zu: %s\n", functionName, m_portName,
				nBytesReadMax, nBytesResponse - nBytesReadMax, m_GenCpResponsePending );
		// Copy requested number of characters to return buffer
		strncpy( pBuffer, genCpResponseBuffer, nBytesReadMax );
		if ( pnRead )
			*pnRead = nBytesReadMax;
		if ( eomReason )
			*eomReason = ASYN_EOM_CNT;
		// Save remaining response characters for next call to readOctet
		strncpy( m_GenCpResponsePending, genCpResponseBuffer + nBytesReadMax, GENCP_RESPONSE_MAX - nBytesReadMax );
	}
	else
	{
		if ( DEBUG_GENICAM >= 3 ) printf(
				"%s Exit: %s Rtn %zu, endOfmsg: %s\n", functionName, m_portName, nBytesResponse, m_GenCpResponsePending );
		// Copy response to return buffer
		strncpy( pBuffer, genCpResponseBuffer, nBytesReadMax );
		if ( pnRead )
			*pnRead = nBytesResponse;
		if ( eomReason )
			*eomReason = ASYN_EOM_EOS | ASYN_EOM_END;
	}
	}

    return status;
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
epicsExportAddress( int, DEBUG_GENICAM );
