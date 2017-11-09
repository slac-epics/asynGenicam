#include "GenTL.h"
#include "GenCpPacket.h"
#include "GenCpRegister.h"
#include <asm/byteorder.h>	// For __cpu_to_be64() and variants
#include <assert.h>
#include <stdio.h>
#include <string.h>

int		DEBUG_GENCP	= 0;

/// GenCpCheckSum16() from Allied Vision Goldeye G/CL Features Reference V1.2.0
/// Assumes packet data exists in memory in big endian format and the host is little endian.
/// Return value is host format
uint16_t GenCpChecksum16( uint8_t* pBuffer, uint32_t nNumBytes )
{
	uint32_t nChecksum = 0;
	uint32_t nByteCounter;
	uint32_t nNumBytesEven = nNumBytes & ~(sizeof(uint16_t) - 1);

	// for reasons of performance, this function is limited to 64Kb length.
	// Since the GenCP standard recommends to have packets <= 1Kb, this should not be a problem.
	assert(nNumBytes < 65535);
	for (nByteCounter = 0; nByteCounter < nNumBytesEven; nByteCounter += sizeof(uint16_t))
	{
		// Two bytes from pBuffer are interpreted as a big endian 16 bit value
		// Couldn't we just do this?
		uint16_t	nCurVal2=	__be16_to_cpup( reinterpret_cast<__be16*>( pBuffer + nByteCounter ) );
		uint16_t	nCurVal	=	(	(( (uint16_t) pBuffer[nByteCounter    ] ) << 8)
								|	 ( (uint16_t) pBuffer[nByteCounter + 1] ) );
		if ( nCurVal2 != nCurVal )
			printf( "Error: nCurVal=0x%04X, nCurVal2=0x%04X\n", nCurVal, nCurVal2 );
		nChecksum += (uint32_t) nCurVal;
	}
	if ((nNumBytes & (sizeof(uint16_t) - 1)) != 0)
	{
		// special case: buffer length is odd number
		nChecksum += (((uint32_t) pBuffer[nNumBytesEven]) << 8);
	}
	while ((nChecksum & 0xFFFF0000) != 0)
	{
		nChecksum = (nChecksum & 0xFFFF) + (nChecksum >> 16);
	}
	return(~((uint16_t) nChecksum));
}

uint16_t	GetRequestId( GenCpCCDRequest * pCCD )
{
	return( __cpu_to_be16( pCCD->ccdRequestId ) );
}

uint16_t	GetRequestId( GenCpCCDAck * pCCD )
{
	return( __cpu_to_be16( pCCD->ccdRequestId ) );
}

/// GenCpInitReadMemPacket()
GENCP_STATUS	GenCpInitReadMemPacket(
	GenCpReadMemPacket		*	pPacket,
	uint16_t					requestId,
	uint64_t					regAddr,
	size_t						numBytes )
{
	const char	*	funcName = "GenCpInitReadMemPacket";
	if ( pPacket == NULL )
		return GENCP_STATUS_GENERIC_ERROR | GENCP_SC_ERROR;

	pPacket->serialPrefix.prefixPreamble	= __cpu_to_be16( GENCP_SERIAL_PREAMBLE );
	pPacket->serialPrefix.prefixChannelId	= 0;
	pPacket->ccd.ccdFlags					= __cpu_to_be16( GENCP_CCD_FLAG_REQACK );
	pPacket->ccd.ccdCommandId				= __cpu_to_be16( GENCP_ID_READMEM_CMD );
	pPacket->ccd.ccdScdLength				= __cpu_to_be16( sizeof( GenCpSCDReadMem ) );
	pPacket->ccd.ccdRequestId				= __cpu_to_be16( requestId );
	pPacket->scd.scdRegAddr					= __cpu_to_be64( regAddr );
	pPacket->scd.scdReserved				= 0;
	pPacket->scd.scdReadSize				= __cpu_to_be16( numBytes );

	// Compute CCD and SCD Checksums
	uint32_t	ckSumCCD	= GenCpChecksum16(	reinterpret_cast<uint8_t *>( &pPacket->serialPrefix.prefixChannelId ),
												sizeof(uint16_t) + sizeof(GenCpCCDRequest) );
	uint32_t	ckSumSCD	= GenCpChecksum16(	reinterpret_cast<uint8_t *>( &pPacket->serialPrefix.prefixChannelId ),
												sizeof(uint16_t) + sizeof(GenCpCCDRequest) + sizeof(GenCpSCDReadMem) );
	pPacket->serialPrefix.prefixCkSumCCD	= __cpu_to_be16( ckSumCCD );
	pPacket->serialPrefix.prefixCkSumSCD	= __cpu_to_be16( ckSumSCD );

	if ( DEBUG_GENCP >= 2 )
		printf( "%s: Read %zu bytes from reg 0x%llX\n", funcName, numBytes, (long long unsigned int) regAddr );
	return GENCP_STATUS_SUCCESS;
}

/// GenCpValidateReadMemAck()
GENCP_STATUS	GenCpValidateReadMemAck(
	GenCpReadMemAck			*	pPacket,
	uint32_t					expectedRequestId )
{
	const	char 			*	funcName = "GenCpValidateReadMemAck";
	if ( pPacket == NULL )
		return GENCP_STATUS_GENERIC_ERROR | GENCP_SC_ERROR;

	uint16_t	prefixPreamble	= __be16_to_cpu( pPacket->serialPrefix.prefixPreamble );
	uint16_t	ccdRequestId	= __be16_to_cpu( pPacket->ccd.ccdRequestId );
	if ( prefixPreamble	!= GENCP_SERIAL_PREAMBLE )
	{
		fprintf( stderr, "%s Error: Req %u, Invalid preamble, 0x%02X\n", funcName, ccdRequestId, prefixPreamble );
		return GENCP_STATUS_INVALID_PARAM | GENCP_SC_ERROR;
	}
	if ( expectedRequestId != ccdRequestId )
	{
		fprintf( stderr, "%s Error: Req %u, expected req %u\n", funcName, ccdRequestId, expectedRequestId );
		return GENCP_STATUS_INVALID_PARAM | GENCP_SC_ERROR;
	}

	// Validate CCD Checksum
	uint32_t	ckSumCCD	= GenCpChecksum16(	reinterpret_cast<uint8_t *>( &pPacket->serialPrefix.prefixChannelId ),
												sizeof(uint16_t) + sizeof(GenCpCCDAck) );
	if ( ckSumCCD != __be16_to_cpu( pPacket->serialPrefix.prefixCkSumCCD ) )
	{
		fprintf( stderr, "%s Error: Req %u, Packet CCD cksum, 0x%04X, computed 0x%04X\n", funcName,
				ccdRequestId, ckSumCCD, __be16_to_cpu( pPacket->serialPrefix.prefixCkSumCCD ) );
		return GENCP_STATUS_INVALID_PARAM | GENCP_SC_ERROR;
	}

	// Validate SCD Checksum
	uint16_t	ckSumSCD	= GenCpChecksum16(	reinterpret_cast<uint8_t *>( &pPacket->serialPrefix.prefixChannelId ),
												sizeof(uint16_t) + sizeof(GenCpCCDAck) + __be16_to_cpu( pPacket->ccd.ccdScdLength ) );
	if ( ckSumSCD != __be16_to_cpu( pPacket->serialPrefix.prefixCkSumSCD ) )
	{
		fprintf( stderr, "%s Error: Req %u, Packet SCD cksum, 0x%04X, computed 0x%04X, length %d\n", funcName,
				ccdRequestId, __be16_to_cpu( pPacket->serialPrefix.prefixCkSumSCD ), ckSumSCD, __be16_to_cpu( pPacket->ccd.ccdScdLength ) );
		return GENCP_STATUS_INVALID_PARAM | GENCP_SC_ERROR;
	}

	uint16_t	ccdCommandId	= __be16_to_cpu( pPacket->ccd.ccdCommandId );
	if ( ccdCommandId	!= GENCP_ID_READMEM_ACK )
	{
		fprintf( stderr, "%s Error: Req %u, Invalid commandId, 0x%02X\n", funcName, ccdRequestId, ccdCommandId );
		return GENCP_STATUS_INVALID_PARAM | GENCP_SC_ERROR;
	}

	uint16_t	ccdScdLength	= __be16_to_cpu( pPacket->ccd.ccdScdLength );
	if ( ccdScdLength > GENCP_READMEM_MAX_BYTES )
	{
		fprintf( stderr, "%s Error: Req %u, SCD Length %u greater than max %u\n", funcName,
				ccdRequestId, ccdScdLength, GENCP_READMEM_MAX_BYTES );
		return GENCP_STATUS_INVALID_PARAM | GENCP_SC_ERROR;
	}

	uint16_t	ccdStatus		= __be16_to_cpu( pPacket->ccd.ccdStatusCode );
	uint16_t	ccdStatusCode	= ccdStatus & GENCP_SC_CODE_MASK;
	if ( ccdStatus & GENCP_SC_ERROR )
	{
		// uint16_t	ccdStatusNS		= ccdStatusCode & GENCP_SC_NAMESPACE_MASK;
		// if ( ccdStatusNS == GENCP_SC_NAMESPACE_GENCP )
		//		fprintf( stderr, "%s Error: Req %u, StatusCode Error %u: %s\n", funcName, ccdRequestId, ccdStatusCode,
		//				GenCpStatusCodeToString(ccdStatusCode) );
		fprintf( stderr, "%s Error: Req %u, StatusCode Error %u\n", funcName, ccdRequestId, ccdStatusCode );
		return ccdStatusCode;
	}
	if ( DEBUG_GENCP >= 2 )
		printf( "%s: statusCode=%u, commandId=0x%X, scdLength=%u, reqId=%u\n",
				funcName, ccdStatusCode, ccdCommandId, ccdScdLength, ccdRequestId );

	return GENCP_STATUS_SUCCESS;
}

/// GenCpProcessReadMemAck() char buffer
GENCP_STATUS	GenCpProcessReadMemAck(
	GenCpReadMemAck			*	pPacket,
	uint32_t					expectedRequestId,
	char					*	pBuffer,
	size_t						numBytes,
	size_t					*	pnBytesRead )
{
	const	char 			*	funcName = "GenCpProcessReadMemAck(char*)";
	if ( pPacket == NULL )
		return GENCP_STATUS_GENERIC_ERROR | GENCP_SC_ERROR;
	if ( pBuffer == NULL )
		return GENCP_STATUS_GENERIC_ERROR | GENCP_SC_ERROR;
	if ( numBytes == 0 )
		return GENCP_STATUS_GENERIC_ERROR | GENCP_SC_ERROR;
	if ( pnBytesRead != NULL )
		*pnBytesRead = 0;

	GENCP_STATUS	statusCode	= GenCpValidateReadMemAck( pPacket, expectedRequestId );
	if ( statusCode	!= GENCP_STATUS_SUCCESS )
	{
		fprintf( stderr, "%s Error: %u\n", funcName, statusCode );
		return statusCode;
	}

	uint16_t	ccdCommandId	= __be16_to_cpu( pPacket->ccd.ccdCommandId	);
	uint16_t	ccdScdLength	= __be16_to_cpu( pPacket->ccd.ccdScdLength	);
	uint16_t	ccdRequestId	= __be16_to_cpu( pPacket->ccd.ccdRequestId	);
	if ( ccdScdLength > numBytes )
	{
		fprintf( stderr, "%s Error: Req %u, SCD Length %d > numBytes %zu\n", funcName,
				ccdRequestId, ccdScdLength, numBytes );
		return GENCP_STATUS_INVALID_PARAM | GENCP_SC_ERROR;
	}

	// Copy the character array to the buffer
	memcpy( reinterpret_cast<void *>(pBuffer),
			reinterpret_cast<void *>(&pPacket->scd.scdReadData[0]), ccdScdLength );

#if 0	// Shouldn't need to terminate strings according to std
// If we terminate here, we need a different routine to read a raw array of bytes
	// Make sure it's NULL terminated
	if ( ccdScdLength < numBytes )
		*(pBuffer + ccdScdLength) = 0;
	else
		*(pBuffer + numBytes - 1) = 0;
#endif

	if ( pnBytesRead != NULL )
		*pnBytesRead = ccdScdLength;

	if ( DEBUG_GENCP >= 2 )
		printf( "%s: commandId=0x%X, scdLength=%u, reqId=%u, data: %02X %02X %02X %02X\n",
				funcName, ccdCommandId, ccdScdLength, ccdRequestId,
				pPacket->scd.scdReadData[0], pPacket->scd.scdReadData[1],
				pPacket->scd.scdReadData[2], pPacket->scd.scdReadData[3] );

	return GENCP_STATUS_SUCCESS;
}

/// GenCpProcessReadMemAck() 16 bit reg
GENCP_STATUS	GenCpProcessReadMemAck(
	GenCpReadMemAck			*	pPacket,
	uint32_t					expectedRequestId,
	uint16_t				*	pReg16 )
{
	const	char 			*	funcName = "GenCpProcessReadMemAck(uint16_t)";
	if ( pPacket == NULL )
		return GENCP_STATUS_GENERIC_ERROR | GENCP_SC_ERROR;
	if ( pReg16 == NULL )
		return GENCP_STATUS_GENERIC_ERROR | GENCP_SC_ERROR;

	GENCP_STATUS	statusCode	= GenCpValidateReadMemAck( pPacket, expectedRequestId );
	if ( statusCode	!= GENCP_STATUS_SUCCESS )
	{
		fprintf( stderr, "%s Error: %u\n", funcName, statusCode );
		return statusCode;
	}

	if ( pReg16 != NULL )
	{
		__be16	*	pBigEndianReg16	= reinterpret_cast<__be16 *>( &pPacket->scd.scdReadData[0] );
		*pReg16 = __be16_to_cpu( *pBigEndianReg16 );
	}
	return GENCP_STATUS_SUCCESS;
}

/// GenCpProcessReadMemAck() 32 bit reg
GENCP_STATUS	GenCpProcessReadMemAck(
	GenCpReadMemAck			*	pPacket,
	uint32_t					expectedRequestId,
	uint32_t				*	pReg32 )
{
	const	char 			*	funcName = "GenCpProcessReadMemAck(uint32_t)";
	if ( pPacket == NULL )
		return GENCP_STATUS_GENERIC_ERROR | GENCP_SC_ERROR;
	if ( pReg32 == NULL )
		return GENCP_STATUS_GENERIC_ERROR | GENCP_SC_ERROR;

	GENCP_STATUS	statusCode	= GenCpValidateReadMemAck( pPacket, expectedRequestId );
	if ( statusCode	!= GENCP_STATUS_SUCCESS )
	{
		fprintf( stderr, "%s Error: %u\n", funcName, statusCode );
		return statusCode;
	}

	uint16_t	ccdCommandId	= __be16_to_cpu( pPacket->ccd.ccdCommandId	);
	uint16_t	ccdScdLength	= __be16_to_cpu( pPacket->ccd.ccdScdLength	);
	uint16_t	ccdRequestId	= __be16_to_cpu( pPacket->ccd.ccdRequestId	);

	if ( pReg32 != NULL )
	{
		__be32	*	pBigEndianReg32	= reinterpret_cast<__be32 *>( &pPacket->scd.scdReadData[0] );
		*pReg32 = __be32_to_cpu( *pBigEndianReg32 );
	}

	if ( DEBUG_GENCP >= 2 )
		printf( "%s: commandId=0x%X, scdLength=%u, reqId=%u, data: %02X %02X %02X %02X\n",
				funcName, ccdCommandId, ccdScdLength, ccdRequestId,
				pPacket->scd.scdReadData[0], pPacket->scd.scdReadData[1],
				pPacket->scd.scdReadData[2], pPacket->scd.scdReadData[3] );
	return GENCP_STATUS_SUCCESS;
}

/// GenCpProcessReadMemAck() 64 bit reg
GENCP_STATUS	GenCpProcessReadMemAck(
	GenCpReadMemAck			*	pPacket,
	uint32_t					expectedRequestId,
	uint64_t				*	pReg64 )
{
	const	char 			*	funcName = "GenCpProcessReadMemAck(uint64_t)";
	if ( pPacket == NULL )
		return GENCP_STATUS_GENERIC_ERROR | GENCP_SC_ERROR;
	if ( pReg64 == NULL )
		return GENCP_STATUS_GENERIC_ERROR | GENCP_SC_ERROR;

	GENCP_STATUS	statusCode	= GenCpValidateReadMemAck( pPacket, expectedRequestId );
	if ( statusCode	!= GENCP_STATUS_SUCCESS )
	{
		fprintf( stderr, "%s Error: %u\n", funcName, statusCode );
		return statusCode;
	}

	if ( pReg64 != NULL )
	{
		__be64	*	pBigEndianReg64	= reinterpret_cast<__be64 *>( &pPacket->scd.scdReadData[0] );
		*pReg64 = __be64_to_cpu( *pBigEndianReg64 );
	}
	return GENCP_STATUS_SUCCESS;
}

/// GenCpProcessReadMemAck() 32 bit float reg
GENCP_STATUS	GenCpProcessReadMemAck(
	GenCpReadMemAck			*	pPacket,
	uint32_t					expectedRequestId,
	float					*	pFloatRet )
{
	const	char 			*	funcName = "GenCpProcessReadMemAck(float)";
	if ( pPacket == NULL )
		return GENCP_STATUS_GENERIC_ERROR | GENCP_SC_ERROR;
	if ( pFloatRet == NULL )
		return GENCP_STATUS_GENERIC_ERROR | GENCP_SC_ERROR;

	GENCP_STATUS	statusCode	= GenCpValidateReadMemAck( pPacket, expectedRequestId );
	if ( statusCode	!= GENCP_STATUS_SUCCESS )
	{
		fprintf( stderr, "%s Error: %u\n", funcName, statusCode );
		return statusCode;
	}

	uint16_t	ccdCommandId	= __be16_to_cpu( pPacket->ccd.ccdCommandId	);
	uint16_t	ccdScdLength	= __be16_to_cpu( pPacket->ccd.ccdScdLength	);
	uint16_t	ccdRequestId	= __be16_to_cpu( pPacket->ccd.ccdRequestId	);

	if ( DEBUG_GENCP >= 2 )
		printf( "%s: commandId=0x%X, scdLength=%u, reqId=%u, data: %02X %02X %02X %02X\n",
				funcName, ccdCommandId, ccdScdLength, ccdRequestId,
				pPacket->scd.scdReadData[0], pPacket->scd.scdReadData[1],
				pPacket->scd.scdReadData[2], pPacket->scd.scdReadData[3] );

	assert( sizeof(__be32) == sizeof(float) );
	union
	{
		__le32		leValue;
		float		floatValue;
	}	leAccess;
	if ( pFloatRet != NULL )
	{
		__be32	*	pBigEndianReg32	= reinterpret_cast<__be32 *>( &pPacket->scd.scdReadData[0] );
		leAccess.leValue = __be32_to_cpu( *pBigEndianReg32 );
		*pFloatRet = leAccess.floatValue;
	}
	return GENCP_STATUS_SUCCESS;
}

/// GenCpProcessReadMemAck() 64 bit double reg
GENCP_STATUS	GenCpProcessReadMemAck(
	GenCpReadMemAck			*	pPacket,
	uint32_t					expectedRequestId,
	double					*	pDoubleRet )
{
	const	char 			*	funcName = "GenCpProcessReadMemAck(double)";
	if ( pPacket == NULL )
		return GENCP_STATUS_GENERIC_ERROR | GENCP_SC_ERROR;
	if ( pDoubleRet == NULL )
		return GENCP_STATUS_GENERIC_ERROR | GENCP_SC_ERROR;

	GENCP_STATUS	statusCode	= GenCpValidateReadMemAck( pPacket, expectedRequestId );
	if ( statusCode	!= GENCP_STATUS_SUCCESS )
	{
		fprintf( stderr, "%s Error: %u\n", funcName, statusCode );
		return statusCode;
	}

	uint16_t	ccdCommandId	= __be16_to_cpu( pPacket->ccd.ccdCommandId	);
	uint16_t	ccdScdLength	= __be16_to_cpu( pPacket->ccd.ccdScdLength	);
	uint16_t	ccdRequestId	= __be16_to_cpu( pPacket->ccd.ccdRequestId	);

	union
	{
		__be64		beValue;
		double		doubleValue;
	}	beAccess;
	__be64	*	pBigEndianReg64	= reinterpret_cast<__be64 *>( &pPacket->scd.scdReadData[0] );
	beAccess.beValue = __be64_to_cpu( *pBigEndianReg64 );
	*pDoubleRet = beAccess.doubleValue;

	if ( DEBUG_GENCP >= 2 )
		printf( "%s: commandId=0x%X, scdLength=%u, reqId=%u, data: %02X %02X %02X %02X %02X %02X %02X %02X\n",
				funcName, ccdCommandId, ccdScdLength, ccdRequestId,
				pPacket->scd.scdReadData[0], pPacket->scd.scdReadData[1],
				pPacket->scd.scdReadData[2], pPacket->scd.scdReadData[3],
				pPacket->scd.scdReadData[4], pPacket->scd.scdReadData[5],
				pPacket->scd.scdReadData[6], pPacket->scd.scdReadData[7] );
	return GENCP_STATUS_SUCCESS;
}

/// GenCpValidateWriteMemAck() Checks for any errors in a WriteMem acknowledge packet
GENCP_STATUS	GenCpValidateWriteMemAck(
	GenCpWriteMemAck		*	pPacket,
	uint32_t					expectedRequestId )
{
	const	char 			*	funcName = "GenCpValidateWriteMemAck";
	if ( pPacket == NULL )
		return GENCP_STATUS_GENERIC_ERROR | GENCP_SC_ERROR;

	uint16_t	prefixPreamble	= __be16_to_cpu( pPacket->serialPrefix.prefixPreamble );
	uint16_t	ccdRequestId	= __be16_to_cpu( pPacket->ccd.ccdRequestId );
	if ( prefixPreamble	!= GENCP_SERIAL_PREAMBLE )
	{
		fprintf( stderr, "%s Error: Req %u, Invalid preamble, 0x%02X\n", funcName, ccdRequestId, prefixPreamble );
		return GENCP_STATUS_INVALID_PARAM | GENCP_SC_ERROR;
	}
	if ( expectedRequestId != ccdRequestId )
	{
		fprintf( stderr, "%s Error: Req %u, expected req %u\n", funcName, ccdRequestId, expectedRequestId );
		// return GENCP_STATUS_INVALID_PARAM | GENCP_SC_ERROR;
	}

	// Validate CCD Checksum
	uint32_t	ckSumCCD	= GenCpChecksum16(	reinterpret_cast<uint8_t *>( &pPacket->serialPrefix.prefixChannelId ),
												sizeof(uint16_t) + sizeof(GenCpCCDAck) );
	if ( ckSumCCD != __be16_to_cpu( pPacket->serialPrefix.prefixCkSumCCD ) )
	{
		fprintf( stderr, "%s Error: Req %u, Packet CCD cksum, 0x%04X, computed 0x%04X\n", funcName,
				ccdRequestId, ckSumCCD, __be16_to_cpu( pPacket->serialPrefix.prefixCkSumCCD ) );
		return GENCP_STATUS_INVALID_PARAM | GENCP_SC_ERROR;
	}

	// Validate SCD Checksum
	uint16_t	ckSumSCD	= GenCpChecksum16(	reinterpret_cast<uint8_t *>( &pPacket->serialPrefix.prefixChannelId ),
												sizeof(uint16_t) + sizeof(GenCpCCDAck) + __be16_to_cpu( pPacket->ccd.ccdScdLength ) );
	if ( ckSumSCD != __be16_to_cpu( pPacket->serialPrefix.prefixCkSumSCD ) )
	{
		fprintf( stderr, "%s Error: Req %u, Packet SCD cksum, 0x%04X, computed 0x%04X, length %d\n", funcName,
				ccdRequestId, __be16_to_cpu( pPacket->serialPrefix.prefixCkSumSCD ), ckSumSCD, __be16_to_cpu( pPacket->ccd.ccdScdLength ) );
		return GENCP_STATUS_INVALID_PARAM | GENCP_SC_ERROR;
	}

	uint16_t	ccdCommandId	= __be16_to_cpu( pPacket->ccd.ccdCommandId );
	if ( ccdCommandId	!= GENCP_ID_WRITEMEM_ACK )
	{
		fprintf( stderr, "%s Error: Req %u, Invalid commandId, 0x%02X\n", funcName, ccdRequestId, ccdCommandId );
		return GENCP_STATUS_INVALID_PARAM | GENCP_SC_ERROR;
	}

	uint16_t	ccdScdLength	= __be16_to_cpu( pPacket->ccd.ccdScdLength );
	if ( ccdScdLength > GENCP_READMEM_MAX_BYTES )
	{
		fprintf( stderr, "%s Error: Req %u, SCD Length %u greater than max %u\n", funcName,
				ccdRequestId, ccdScdLength, GENCP_READMEM_MAX_BYTES );
		return GENCP_STATUS_INVALID_PARAM | GENCP_SC_ERROR;
	}

	uint16_t	ccdStatus		= __be16_to_cpu( pPacket->ccd.ccdStatusCode );
	uint16_t	ccdStatusCode	= ccdStatus & GENCP_SC_CODE_MASK;
	if ( ccdStatus & GENCP_SC_ERROR )
	{
		// uint16_t	ccdStatusNS		= ccdStatusCode & GENCP_SC_NAMESPACE_MASK;
		// if ( ccdStatusNS == GENCP_SC_NAMESPACE_GENCP )
		//		fprintf( stderr, "%s Error: Req %u, StatusCode Error %u: %s\n", funcName, ccdRequestId, ccdStatusCode,
		//				GenCpStatusCodeToString(ccdStatusCode) );
		fprintf( stderr, "%s Error: Req %u, StatusCode Error %u\n", funcName, ccdRequestId, ccdStatusCode );
		return ccdStatusCode;
	}

	uint16_t	scdLengthWritten	= __be16_to_cpu( pPacket->scd.scdLengthWritten );

	if ( DEBUG_GENCP >= 2 )
		printf( "%s: statusCode=%u, commandId=0x%X scdLength=%u, reqId=%u, scdLengthWritten=%u\n",
				funcName, ccdStatusCode, ccdCommandId, ccdScdLength, ccdRequestId, scdLengthWritten );
	return GENCP_STATUS_SUCCESS;
}

/// GenCpInitWriteMemPacket() Initialize a WriteMem packet to write a string to regAddr
GENCP_STATUS	GenCpInitWriteMemPacket(
	GenCpWriteMemPacket		*	pPacket,
	uint16_t					requestId,
	uint64_t					regAddr,
	size_t						numBytes,
	const char				*	pString,
	size_t					*	pnBytesSend )
{
	const	char 			*	funcName = "GenCpInitWriteMemPacket";
	if ( pnBytesSend )
		*pnBytesSend = 0;
	if ( pPacket == NULL )
		return GENCP_STATUS_GENERIC_ERROR | GENCP_SC_ERROR;

	uint16_t	ccdScdLength = sizeof( uint64_t ) + numBytes;
	pPacket->serialPrefix.prefixPreamble	= __cpu_to_be16( GENCP_SERIAL_PREAMBLE );
	pPacket->serialPrefix.prefixChannelId	= 0;
	pPacket->ccd.ccdFlags					= __cpu_to_be16( GENCP_CCD_FLAG_REQACK );
	pPacket->ccd.ccdCommandId				= __cpu_to_be16( GENCP_ID_WRITEMEM_CMD );
	pPacket->ccd.ccdScdLength				= __cpu_to_be16( ccdScdLength );
	pPacket->ccd.ccdRequestId				= __cpu_to_be16( requestId );
	pPacket->scd.scdRegAddr					= __cpu_to_be64( regAddr );

	assert( numBytes < GENCP_READMEM_MAX_BYTES );
	memcpy( (char *) &pPacket->scd.scdWriteData[0], pString, numBytes );

	// Compute CCD and SCD Checksums
	uint32_t	ckSumCCD	= GenCpChecksum16(	reinterpret_cast<uint8_t *>( &pPacket->serialPrefix.prefixChannelId ),
												sizeof(uint16_t) + sizeof(GenCpCCDRequest) );
	uint32_t	ckSumSCD	= GenCpChecksum16(	reinterpret_cast<uint8_t *>( &pPacket->serialPrefix.prefixChannelId ),
												sizeof(uint16_t) + sizeof(GenCpCCDRequest) + ccdScdLength );
	pPacket->serialPrefix.prefixCkSumCCD	= __cpu_to_be16( ckSumCCD );
	pPacket->serialPrefix.prefixCkSumSCD	= __cpu_to_be16( ckSumSCD );

	if ( pnBytesSend )
		*pnBytesSend = sizeof(GenCpSerialPrefix) + sizeof(GenCpCCDRequest) + ccdScdLength;

	if ( DEBUG_GENCP >= 2 )
		printf( "%s: commandId=0x%X, regAddr=0x%llX, scdLength=%u, reqId=%u\n",
				funcName, GENCP_ID_WRITEMEM_CMD, (long long unsigned int) regAddr, ccdScdLength, requestId );
	return GENCP_STATUS_SUCCESS;
}

/// GenCpInitWriteMemPacket() Initialize a WriteMem packet to write a uint16 to regAddr
GENCP_STATUS	GenCpInitWriteMemPacket(
	GenCpWriteMemPacket		*	pPacket,
	uint16_t					requestId,
	uint64_t					regAddr,
	uint16_t					regValue,
	size_t					*	pnBytesSend )
{
	const	char 			*	funcName = "GenCpInitWriteMemPacket";
	if ( pnBytesSend )
		*pnBytesSend = 0;
	if ( pPacket == NULL )
		return GENCP_STATUS_GENERIC_ERROR | GENCP_SC_ERROR;

	uint16_t	ccdScdLength = sizeof( uint64_t ) + sizeof(uint16_t);
	pPacket->serialPrefix.prefixPreamble	= __cpu_to_be16( GENCP_SERIAL_PREAMBLE );
	pPacket->serialPrefix.prefixChannelId	= 0;
	pPacket->ccd.ccdFlags					= __cpu_to_be16( GENCP_CCD_FLAG_REQACK );
	pPacket->ccd.ccdCommandId				= __cpu_to_be16( GENCP_ID_WRITEMEM_CMD );
	pPacket->ccd.ccdScdLength				= __cpu_to_be16( ccdScdLength );
	pPacket->ccd.ccdRequestId				= __cpu_to_be16( requestId );
	pPacket->scd.scdRegAddr					= __cpu_to_be64( regAddr );

	union
	{
		__be16		beValue;
		char		beBytes[4];
	}	beAccess;
	beAccess.beValue	= __cpu_to_be16( regValue );
	memcpy( (char *) &pPacket->scd.scdWriteData[0], beAccess.beBytes, sizeof(uint16_t) );

	// Compute CCD and SCD Checksums
	uint32_t	ckSumCCD	= GenCpChecksum16(	reinterpret_cast<uint8_t *>( &pPacket->serialPrefix.prefixChannelId ),
												sizeof(uint16_t) + sizeof(GenCpCCDRequest) );
	uint32_t	ckSumSCD	= GenCpChecksum16(	reinterpret_cast<uint8_t *>( &pPacket->serialPrefix.prefixChannelId ),
												sizeof(uint16_t) + sizeof(GenCpCCDRequest) + ccdScdLength );
	pPacket->serialPrefix.prefixCkSumCCD	= __cpu_to_be16( ckSumCCD );
	pPacket->serialPrefix.prefixCkSumSCD	= __cpu_to_be16( ckSumSCD );

	if ( pnBytesSend )
		*pnBytesSend = sizeof(GenCpSerialPrefix) + sizeof(GenCpCCDRequest) + ccdScdLength;

	if ( DEBUG_GENCP >= 2 )
		printf( "%s: commandId=0x%X, regAddr=0x%llX, scdLength=%u, reqId=%u\n",
				funcName, GENCP_ID_WRITEMEM_CMD, (long long unsigned int) regAddr, ccdScdLength, requestId );
	return GENCP_STATUS_SUCCESS;
}

/// GenCpInitWriteMemPacket() Initialize a WriteMem packet to write a uint32 to regAddr
GENCP_STATUS	GenCpInitWriteMemPacket(
	GenCpWriteMemPacket		*	pPacket,
	uint16_t					requestId,
	uint64_t					regAddr,
	uint32_t					regValue,
	size_t					*	pnBytesSend )
{
	const	char 			*	funcName = "GenCpInitWriteMemPacket";
	if ( pnBytesSend )
		*pnBytesSend = 0;
	if ( pPacket == NULL )
		return GENCP_STATUS_GENERIC_ERROR | GENCP_SC_ERROR;

	uint16_t	ccdScdLength = sizeof( uint64_t ) + sizeof(uint32_t);
	pPacket->serialPrefix.prefixPreamble	= __cpu_to_be16( GENCP_SERIAL_PREAMBLE );
	pPacket->serialPrefix.prefixChannelId	= 0;
	pPacket->ccd.ccdFlags					= __cpu_to_be16( GENCP_CCD_FLAG_REQACK );
	pPacket->ccd.ccdCommandId				= __cpu_to_be16( GENCP_ID_WRITEMEM_CMD );
	pPacket->ccd.ccdScdLength				= __cpu_to_be16( ccdScdLength );
	pPacket->ccd.ccdRequestId				= __cpu_to_be16( requestId );
	pPacket->scd.scdRegAddr					= __cpu_to_be64( regAddr );

	union
	{
		__be32		beValue;
		char		beBytes[4];
	}	beAccess;
	beAccess.beValue	= __cpu_to_be32( regValue );
	memcpy( (char *) &pPacket->scd.scdWriteData[0], beAccess.beBytes, sizeof(uint32_t) );

	// Compute CCD and SCD Checksums
	uint32_t	ckSumCCD	= GenCpChecksum16(	reinterpret_cast<uint8_t *>( &pPacket->serialPrefix.prefixChannelId ),
												sizeof(uint16_t) + sizeof(GenCpCCDRequest) );
	uint32_t	ckSumSCD	= GenCpChecksum16(	reinterpret_cast<uint8_t *>( &pPacket->serialPrefix.prefixChannelId ),
												sizeof(uint16_t) + sizeof(GenCpCCDRequest) + ccdScdLength );
	pPacket->serialPrefix.prefixCkSumCCD	= __cpu_to_be16( ckSumCCD );
	pPacket->serialPrefix.prefixCkSumSCD	= __cpu_to_be16( ckSumSCD );

	if ( pnBytesSend )
		*pnBytesSend = sizeof(GenCpSerialPrefix) + sizeof(GenCpCCDRequest) + ccdScdLength;

	if ( DEBUG_GENCP >= 2 )
		printf( "%s: commandId=0x%X, regAddr=0x%llX, scdLength=%u, reqId=%u, data: %02X %02X %02X %02X\n",
				funcName, GENCP_ID_WRITEMEM_CMD, (long long unsigned int) regAddr, ccdScdLength, requestId,
				pPacket->scd.scdWriteData[0], pPacket->scd.scdWriteData[1],
				pPacket->scd.scdWriteData[2], pPacket->scd.scdWriteData[3] );
	return GENCP_STATUS_SUCCESS;
}

/// GenCpInitWriteMemPacket() Initialize a WriteMem packet to write a uint64 to regAddr
GENCP_STATUS	GenCpInitWriteMemPacket(
	GenCpWriteMemPacket		*	pPacket,
	uint16_t					requestId,
	uint64_t					regAddr,
	uint64_t					regValue,
	size_t					*	pnBytesSend )
{
	const	char 			*	funcName = "GenCpInitWriteMemPacket";
	if ( pnBytesSend )
		*pnBytesSend = 0;
	if ( pPacket == NULL )
		return GENCP_STATUS_GENERIC_ERROR | GENCP_SC_ERROR;

	uint16_t	ccdScdLength = sizeof( uint64_t ) + sizeof(uint64_t);
	pPacket->serialPrefix.prefixPreamble	= __cpu_to_be16( GENCP_SERIAL_PREAMBLE );
	pPacket->serialPrefix.prefixChannelId	= 0;
	pPacket->ccd.ccdFlags					= __cpu_to_be16( GENCP_CCD_FLAG_REQACK );
	pPacket->ccd.ccdCommandId				= __cpu_to_be16( GENCP_ID_WRITEMEM_CMD );
	pPacket->ccd.ccdScdLength				= __cpu_to_be16( ccdScdLength );
	pPacket->ccd.ccdRequestId				= __cpu_to_be16( requestId );
	pPacket->scd.scdRegAddr					= __cpu_to_be64( regAddr );

	union
	{
		__be64		beValue;
		char		beBytes[4];
	}	beAccess;
	beAccess.beValue	= __cpu_to_be64( regValue );
	memcpy( (char *) &pPacket->scd.scdWriteData[0], beAccess.beBytes, sizeof(uint64_t) );

	// Compute CCD and SCD Checksums
	uint32_t	ckSumCCD	= GenCpChecksum16(	reinterpret_cast<uint8_t *>( &pPacket->serialPrefix.prefixChannelId ),
												sizeof(uint16_t) + sizeof(GenCpCCDRequest) );
	uint32_t	ckSumSCD	= GenCpChecksum16(	reinterpret_cast<uint8_t *>( &pPacket->serialPrefix.prefixChannelId ),
												sizeof(uint16_t) + sizeof(GenCpCCDRequest) + ccdScdLength );
	pPacket->serialPrefix.prefixCkSumCCD	= __cpu_to_be16( ckSumCCD );
	pPacket->serialPrefix.prefixCkSumSCD	= __cpu_to_be16( ckSumSCD );

	if ( pnBytesSend )
		*pnBytesSend = sizeof(GenCpSerialPrefix) + sizeof(GenCpCCDRequest) + ccdScdLength;

	if ( DEBUG_GENCP >= 2 )
		printf( "%s: commandId=0x%X, regAddr=0x%llX, scdLength=%u, reqId=%u\n",
				funcName, GENCP_ID_WRITEMEM_CMD, (long long unsigned int) regAddr, ccdScdLength, requestId );
	return GENCP_STATUS_SUCCESS;
}

/// GenCpInitWriteMemPacket() Initialize a WriteMem packet to write a float to regAddr
GENCP_STATUS	GenCpInitWriteMemPacket(
	GenCpWriteMemPacket		*	pPacket,
	uint16_t					requestId,
	uint64_t					regAddr,
	float						regValue,
	size_t					*	pnBytesSend )
{
	const	char 			*	funcName = "GenCpInitWriteMemPacket";
	if ( pnBytesSend )
		*pnBytesSend = 0;
	if ( pPacket == NULL )
		return GENCP_STATUS_GENERIC_ERROR | GENCP_SC_ERROR;

	uint16_t	ccdScdLength = sizeof( uint64_t ) + sizeof(uint32_t);
	pPacket->serialPrefix.prefixPreamble	= __cpu_to_be16( GENCP_SERIAL_PREAMBLE );
	pPacket->serialPrefix.prefixChannelId	= 0;
	pPacket->ccd.ccdFlags					= __cpu_to_be16( GENCP_CCD_FLAG_REQACK );
	pPacket->ccd.ccdCommandId				= __cpu_to_be16( GENCP_ID_WRITEMEM_CMD );
	pPacket->ccd.ccdScdLength				= __cpu_to_be16( ccdScdLength );
	pPacket->ccd.ccdRequestId				= __cpu_to_be16( requestId );
	pPacket->scd.scdRegAddr					= __cpu_to_be64( regAddr );

	union
	{
		__be32		beValue;
		char		beBytes[4];
	}	beAccess;
	beAccess.beValue	= __cpu_to_be32( static_cast<uint32_t>(regValue) );
	memcpy( (char *) &pPacket->scd.scdWriteData[0], beAccess.beBytes, sizeof(uint32_t) );

	// Compute CCD and SCD Checksums
	uint32_t	ckSumCCD	= GenCpChecksum16(	reinterpret_cast<uint8_t *>( &pPacket->serialPrefix.prefixChannelId ),
												sizeof(uint16_t) + sizeof(GenCpCCDRequest) );
	uint32_t	ckSumSCD	= GenCpChecksum16(	reinterpret_cast<uint8_t *>( &pPacket->serialPrefix.prefixChannelId ),
												sizeof(uint16_t) + sizeof(GenCpCCDRequest) + ccdScdLength );
	pPacket->serialPrefix.prefixCkSumCCD	= __cpu_to_be16( ckSumCCD );
	pPacket->serialPrefix.prefixCkSumSCD	= __cpu_to_be16( ckSumSCD );

	if ( pnBytesSend )
		*pnBytesSend = sizeof(GenCpSerialPrefix) + sizeof(GenCpCCDRequest) + ccdScdLength;

	if ( DEBUG_GENCP >= 2 )
		printf( "%s: commandId=0x%X, regAddr=0x%llX, scdLength=%u, reqId=%u, data: %02X %02X %02X %02X\n",
				funcName, GENCP_ID_WRITEMEM_CMD, (long long unsigned int) regAddr, ccdScdLength, requestId,
				pPacket->scd.scdWriteData[0], pPacket->scd.scdWriteData[1],
				pPacket->scd.scdWriteData[2], pPacket->scd.scdWriteData[3] );
	return GENCP_STATUS_SUCCESS;
}

/// GenCpInitWriteMemPacket() Initialize a WriteMem packet to write a double to regAddr
GENCP_STATUS	GenCpInitWriteMemPacket(
	GenCpWriteMemPacket		*	pPacket,
	uint16_t					requestId,
	uint64_t					regAddr,
	double						regValue,
	size_t					*	pnBytesSend )
{
	const	char 			*	funcName = "GenCpInitWriteMemPacket";
	if ( pnBytesSend )
		*pnBytesSend = 0;
	if ( pPacket == NULL )
		return GENCP_STATUS_GENERIC_ERROR | GENCP_SC_ERROR;

	uint16_t	ccdScdLength = sizeof( uint64_t ) + sizeof(uint32_t);
	pPacket->serialPrefix.prefixPreamble	= __cpu_to_be16( GENCP_SERIAL_PREAMBLE );
	pPacket->serialPrefix.prefixChannelId	= 0;
	pPacket->ccd.ccdFlags					= __cpu_to_be16( GENCP_CCD_FLAG_REQACK );
	pPacket->ccd.ccdCommandId				= __cpu_to_be16( GENCP_ID_WRITEMEM_CMD );
	pPacket->ccd.ccdScdLength				= __cpu_to_be16( ccdScdLength );
	pPacket->ccd.ccdRequestId				= __cpu_to_be16( requestId );
	pPacket->scd.scdRegAddr					= __cpu_to_be64( regAddr );

	union
	{
		__be64		beValue;
		char		beBytes[8];
	}	beAccess;
	beAccess.beValue	= __cpu_to_be64( static_cast<uint64_t>(regValue) );
	memcpy( (char *) &pPacket->scd.scdWriteData[0], beAccess.beBytes, sizeof(uint64_t) );

	// Compute CCD and SCD Checksums
	uint32_t	ckSumCCD	= GenCpChecksum16(	reinterpret_cast<uint8_t *>( &pPacket->serialPrefix.prefixChannelId ),
												sizeof(uint16_t) + sizeof(GenCpCCDRequest) );
	uint32_t	ckSumSCD	= GenCpChecksum16(	reinterpret_cast<uint8_t *>( &pPacket->serialPrefix.prefixChannelId ),
												sizeof(uint16_t) + sizeof(GenCpCCDRequest) + ccdScdLength );
	pPacket->serialPrefix.prefixCkSumCCD	= __cpu_to_be16( ckSumCCD );
	pPacket->serialPrefix.prefixCkSumSCD	= __cpu_to_be16( ckSumSCD );

	if ( pnBytesSend )
		*pnBytesSend = sizeof(GenCpSerialPrefix) + sizeof(GenCpCCDRequest) + ccdScdLength;

	if ( DEBUG_GENCP >= 2 )
		printf( "%s: commandId=0x%X, regAddr=0x%llX, scdLength=%u, reqId=%u, data: %02X %02X %02X %02X %02X %02X %02X %02X\n",
				funcName, GENCP_ID_WRITEMEM_CMD, (long long unsigned int) regAddr, ccdScdLength, requestId,
				pPacket->scd.scdWriteData[0], pPacket->scd.scdWriteData[1],
				pPacket->scd.scdWriteData[2], pPacket->scd.scdWriteData[3],
				pPacket->scd.scdWriteData[4], pPacket->scd.scdWriteData[5],
				pPacket->scd.scdWriteData[6], pPacket->scd.scdWriteData[7] );
	return GENCP_STATUS_SUCCESS;
}

uint32_t	GenCpBigEndianToCpu( uint32_t	be32Value )
{
	return __be32_to_cpu( static_cast<__be32>(be32Value) );
}

uint64_t	GenCpBigEndianToCpu( uint64_t	be64Value )
{
	return __be64_to_cpu( static_cast<__be64>(be64Value) );
}
