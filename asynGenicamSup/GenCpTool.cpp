#include <assert.h>
#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
// #include <zlib.h>
#include "edtinc.h"
#include "pciload.h"
#include "GenCpPacket.h"
#include "GenCpRegister.h"


// GenCp Request ID, start at 0, increment each request
static uint16_t		localGenCpRequestId	= 0;

void usage( const char * msg )
{
    printf( "%s", msg );
    printf( "GenCpTool Usage: \n" );
    printf(
       "    -h              - Help message\n"
       "    --help          - Help message\n"
       "    -c N            - channel #\n"
       "    --channel N     - channel #\n"
       "    -u N            - Unit number (default 0)\n"
       "    --unit N        - Unit number (default 0)\n"
       "    --readXml fname - Read XML GeniCam file and write to fname\n"
       "    --U16 Addr      - Read 16 bit unsigned value from address\n"
       "    --U32 Addr      - Read 32 bit unsigned value from address\n"
       "    --U64 Addr      - Read 64 bit unsigned value from address\n"
       "    --C20 Addr      - Read 20 character string from address\n"
       "    --C82 Addr      - Read 82 character string from address, etc for other counts\n"
       "    -v              - Verbose\n"
    );
}

GENCP_STATUS PdvGenCpReadUint(
    EdtDev			*	pPdv,
	uint64_t			regAddr,
	size_t				numBytes,
	uint64_t		*	pnResult	)
{
	const char		*	functionName = "PdvGenCpReadUint";
	GENCP_STATUS		status;
	GenCpReadMemPacket	readMemPacket;
	GenCpReadMemAck		ackPacket;
	uint64_t			result = static_cast<unsigned int>( -1 );

	if ( pnResult != NULL )
		*pnResult = result;

	status = GenCpInitReadMemPacket( &readMemPacket, localGenCpRequestId++, regAddr, numBytes );
	if ( status != GENCP_STATUS_SUCCESS )
	{
		fprintf( stderr, "%s: GenCP Error: %0x04X\n", functionName, status );
		return status;
	}

	status = pdv_serial_write( pPdv, reinterpret_cast<char *>( &readMemPacket ), sizeof(readMemPacket) );
	int		nMsTimeout		= 500;
	size_t	nBytesReadMax	= sizeof(ackPacket);
	int		nAvailToRead	= pdv_serial_wait( pPdv, nMsTimeout, nBytesReadMax );

	int     nRead = 0;
	if ( nAvailToRead > 0 )
	{
		int     nToRead = nAvailToRead;
		if( nToRead > static_cast<int>( nBytesReadMax ) )
		{
			printf( "%s: Clipping nAvailToRead %d to nBytesReadMax %zu\n",
					functionName, nAvailToRead, nBytesReadMax );
			nToRead = static_cast<int>(nBytesReadMax);
		}
		nRead = pdv_serial_read( pPdv, reinterpret_cast<char *>(&ackPacket), nToRead );
	}

	if ( nRead <= 0 )
	{
		fprintf( stderr, "%s Error: Timeout with no reply!\n", functionName );
		return GENCP_STATUS_MSG_TIMEOUT | GENCP_SC_ERROR;
	}

	if ( numBytes == 2 )
	{
		uint16_t	result16 = 0xFF;

		status = GenCpProcessReadMemAck( &ackPacket, localGenCpRequestId-1, &result16 );
		if ( status != GENCP_STATUS_SUCCESS )
		{
			fprintf( stderr, "GenCP ReadMem16 Validate Error: %d (0x%X)\n", status, status );
			return status;
		}
		result = static_cast<uint64_t>( result16 );
	}
	else if ( numBytes == 4 )
	{
		uint32_t	result32 = static_cast<unsigned int>( -1 );

		status = GenCpProcessReadMemAck( &ackPacket, localGenCpRequestId-1, &result32 );
		if ( status != GENCP_STATUS_SUCCESS )
		{
			fprintf( stderr, "GenCP ReadMem32 Validate Error: %d (0x%X)\n", status, status );
			return status;
		}
		result = static_cast<uint64_t>( result32 );
	}
	else if ( numBytes == 8 )
	{
		uint64_t	result64 = static_cast<unsigned int>( -1 );

		status = GenCpProcessReadMemAck( &ackPacket, localGenCpRequestId-1, &result64 );
		if ( status != GENCP_STATUS_SUCCESS )
		{
			fprintf( stderr, "GenCP ReadMem64 Validate Error: %d (0x%X)\n", status, status );
			return status;
		}
		result = static_cast<uint64_t>( result64 );
	}

	if ( pnResult != NULL )
		*pnResult = result;
	return GENCP_STATUS_SUCCESS;
}

GENCP_STATUS PdvGenCpReadString(
    EdtDev			*	pPdv,
	uint64_t			regAddr,
	size_t				numBytes,
	char			*	pBuffer,
	size_t				sBuffer )
{
	const char		*	functionName = "PdvGenCpReadString";
	GENCP_STATUS		status;
	GenCpReadMemPacket	readMemPacket;
	GenCpReadMemAck		ackPacket;

	if ( pBuffer != NULL )
		*pBuffer = 0;

	status = GenCpInitReadMemPacket( &readMemPacket, localGenCpRequestId++, regAddr, numBytes );
	if ( status != GENCP_STATUS_SUCCESS )
	{
		fprintf( stderr, "GenCP Error: %d\n", status );
		return status;
	}

	status = pdv_serial_write( pPdv, reinterpret_cast<char *>( &readMemPacket ), sizeof(readMemPacket) );
	int		nMsTimeout		= 500;
	size_t	nBytesReadMax	= sizeof(ackPacket);
	int		nAvailToRead	= pdv_serial_wait( pPdv, nMsTimeout, nBytesReadMax );

	int     nRead = 0;
	if ( nAvailToRead > 0 )
	{
		int     nToRead = nAvailToRead;
		if( nToRead > static_cast<int>( nBytesReadMax ) )
		{
			printf( "%s: Clipping nAvailToRead %d to nBytesReadMax %zu\n",
					functionName, nAvailToRead, nBytesReadMax );
			nToRead = static_cast<int>(nBytesReadMax);
		}
		nRead = pdv_serial_read( pPdv, reinterpret_cast<char *>(&ackPacket), nToRead );
	}

	if ( nRead <= 0 )
	{
		fprintf( stderr, "%s Error: Timeout with no reply!\n", functionName );
		return GENCP_STATUS_MSG_TIMEOUT | GENCP_SC_ERROR;
	}

	size_t	nBytesRead;
	status = GenCpProcessReadMemAck( &ackPacket, localGenCpRequestId-1, pBuffer, numBytes, &nBytesRead );
	if ( status != GENCP_STATUS_SUCCESS )
	{
		fprintf( stderr, "GenCP ReadMemString Validate Error: %d (0x%X)\n", status, status );
		return status;
	}

	return GENCP_STATUS_SUCCESS;
}

GENCP_STATUS PdvGenCpReadXmlFile(
    EdtDev			*	pPdv,
	unsigned int		iFileEntry,
	unsigned char	*	pBuffer,
	size_t				sBuffer,
	const char		*	pFileName )
{
	const char		*	functionName = "PdvGenCpReadXmlFile";
	GENCP_STATUS		status;

	if ( pBuffer != NULL )
		*pBuffer = 0;

	uint64_t			addrManifestTable;
	status = PdvGenCpReadUint( pPdv, REG_BRM_MANIFEST_TABLE_ADDRESS, 8, &addrManifestTable );
	if ( status != GENCP_STATUS_SUCCESS )
	{
		fprintf( stderr, "%s: GenCP Error reading manifest table address: %d\n", functionName, status );
		return status;
	}

	GenCpManifestEntry	xmlFileEntry;
	uint64_t addrFileEntry = addrManifestTable + sizeof(uint64_t) + iFileEntry * sizeof(GenCpManifestEntry);
	status = PdvGenCpReadString( pPdv,					addrFileEntry,		sizeof(GenCpManifestEntry),
								reinterpret_cast<char *>(&xmlFileEntry),	sizeof(GenCpManifestEntry) );
	if ( status != GENCP_STATUS_SUCCESS )
	{
		fprintf( stderr, "%s: GenCP Error reading manifest table entry: %0x04X\n", functionName, status );
		return status;
	}
	uint32_t		xmlFileVersion	= GenCpBigEndianToCpu( xmlFileEntry.xmlFileVersion );
	uint32_t		xmlFileSchema	= GenCpBigEndianToCpu( xmlFileEntry.xmlFileSchema );
	uint64_t		xmlFileStart	= GenCpBigEndianToCpu( xmlFileEntry.xmlFileStart );
	uint64_t		xmlFileSize		= GenCpBigEndianToCpu( xmlFileEntry.xmlFileSize );

	unsigned char	*	pReadBuffer	= pBuffer;
	size_t				sReadBuffer	= sBuffer;

	if ( xmlFileSize > sReadBuffer )
	{
		fprintf( stderr, "%s GenCP Error: XML File size %zu > buffer size %zu.\n", functionName, xmlFileSize, sReadBuffer );
		return status;
	}

	// Read the file
	size_t		nBytesRead	= 0;
	while ( nBytesRead < xmlFileSize )
	{
		size_t		nBytesReq	= xmlFileSize - nBytesRead;
		char	*	nextAddr	= reinterpret_cast<char *>( pReadBuffer + nBytesRead );
		if( nBytesReq > GENCP_READMEM_MAX_BYTES )
			nBytesReq = GENCP_READMEM_MAX_BYTES;
		status = PdvGenCpReadString( pPdv, xmlFileStart + nBytesRead, nBytesReq,
									nextAddr, sReadBuffer - nBytesRead );
		if ( status != GENCP_STATUS_SUCCESS )
		{
			fprintf( stderr, "%s: GenCP Error while reading XML file version 0x%08X: %0x04X\n", functionName, xmlFileVersion, status );
			return status;
		}
		nBytesRead	+= nBytesReq;
	}

	char		tempFileName[100];
	strncpy( tempFileName, pFileName, 100 );
	if ( GENCP_MFT_ENTRY_SCHEMA_TYPE(xmlFileSchema) == GENCP_MFT_ENTRY_SCHEMA_TYPE_ZIP )
	{
		strncat( tempFileName + strlen(tempFileName), ".zip", 100 - 1 - strlen(tempFileName) );
	}

	FILE	*	outFile	= fopen( tempFileName, "wb" );
	if ( outFile == NULL )
	{
		fprintf( stderr, "%s: GenCP unable to create temp file: %s\n", functionName, tempFileName );
		return status;
	}
	(void) fwrite( pReadBuffer, sizeof(char), nBytesRead, outFile );
	(void) fclose( outFile );
	printf( "Genicam file written to %s\n", tempFileName );

	//	Check the SHA1 hash
	//	TODO: Should this be before or after unzip?
	uint8_t		xmlFileSHA1[GENCP_MFT_ENTRY_SHA1_SIZE];
	SHA1( pReadBuffer, xmlFileSize, xmlFileSHA1 );

	if ( memcmp( xmlFileSHA1, xmlFileEntry.xmlFileSHA1, GENCP_MFT_ENTRY_SHA1_SIZE ) == 0 )
	{
		printf( "Genicam file matches SHA1: " );
		for ( size_t i = 0; i < GENCP_MFT_ENTRY_SHA1_SIZE; i++ )
			printf( "%02x", xmlFileSHA1[i] );
		putchar( '\n' );
	}
	else
	{
		printf( "Genicam file written to %s\n", tempFileName );
		fprintf( stderr, "%s GenCP Error: SHA1 hash does not match!\n", functionName );
		// return status;
	}

	return GENCP_STATUS_SUCCESS;
}


GENCP_STATUS EdtGenCpReadUint(
	unsigned int		iUnit,
	unsigned int		iChannel,
	uint64_t			regAddr,
	size_t				numBytes,
	uint64_t		*	pnResult	)
{
	GENCP_STATUS		status;
    EdtDev			*	pPdv;
	uint64_t			result = static_cast<unsigned int>( -1 );

	if ( pnResult != NULL )
		*pnResult = result;

    /* open a handle to the device     */
    pPdv = pdv_open_channel(EDT_INTERFACE, iUnit, iChannel);
    if ( pPdv == NULL )
    {
        pdv_perror( (char *) EDT_INTERFACE );
        return GENCP_STATUS_INVALID_PARAM | GENCP_SC_ERROR;
    }
	pdv_serial_read_enable( pPdv );
    // Flush the read buffer
	char		flushBuf[1000];
	(void) pdv_serial_read( pPdv, flushBuf, 1000 );


	status = PdvGenCpReadUint( pPdv, regAddr, numBytes, &result );

	pdv_close( pPdv );

	if ( status != GENCP_STATUS_SUCCESS )
	{
		fprintf( stderr, "Error reading %zu bytes from regAddr 0x%08lX\n", numBytes, regAddr );
	}
	else
	{
		printf( "regAddr 0x%08lX = %lu = 0x%lx\n", regAddr, result, result );
	}

	if ( pnResult != NULL )
		*pnResult = result;
	return GENCP_STATUS_SUCCESS;
}


GENCP_STATUS EdtGenCpReadString(
	unsigned int		iUnit,
	unsigned int		iChannel,
	uint64_t			regAddr,
	size_t				numBytes,
	char			*	pBuffer,
	size_t				sBuffer )
{
	GENCP_STATUS		status;
    EdtDev			*	pPdv;

	if ( pBuffer != NULL )
		*pBuffer = 0;

    /* open a handle to the device     */
    pPdv = pdv_open_channel(EDT_INTERFACE, iUnit, iChannel);
    if ( pPdv == NULL )
    {
        pdv_perror( (char *) EDT_INTERFACE );
        return GENCP_STATUS_INVALID_PARAM | GENCP_SC_ERROR;
    }
	pdv_serial_read_enable( pPdv );
    // Flush the read buffer
	char		flushBuf[1000];
	(void) pdv_serial_read( pPdv, flushBuf, 1000 );

	status = PdvGenCpReadString( pPdv, regAddr, numBytes, pBuffer, sBuffer );

	pdv_close( pPdv );

	if ( status != GENCP_STATUS_SUCCESS )
	{
		fprintf( stderr, "Error reading %zu characters from regAddr 0x%08lX\n", numBytes, regAddr );
	}
	else
	{
		printf( "regAddr 0x%08lX = %zu char string: %-.64s\n", regAddr, strlen(pBuffer), pBuffer );
	}

	return GENCP_STATUS_SUCCESS;
}


GENCP_STATUS EdtGenCpReadXmlFile(
	unsigned int		iUnit,
	unsigned int		iChannel,
	unsigned int		iFileEntry,
	unsigned char	*	pBuffer,
	size_t				sBuffer,
	const char		*	pFileName )
{
	const char		*	functionName = "EdtGenCpReadXmlFile";
	GENCP_STATUS		status;
    EdtDev			*	pPdv;

	if ( pBuffer != NULL )
		*pBuffer = 0;

    /* open a handle to the device     */
    pPdv = pdv_open_channel((char *) EDT_INTERFACE, iUnit, iChannel);
    if ( pPdv == NULL )
    {
        pdv_perror( (char *) EDT_INTERFACE );
        return GENCP_STATUS_INVALID_PARAM | GENCP_SC_ERROR;
    }
	pdv_serial_read_enable( pPdv );
    // Flush the read buffer
	char		flushBuf[1000];
	(void) pdv_serial_read( pPdv, flushBuf, 1000 );

	status = PdvGenCpReadXmlFile( pPdv, iFileEntry, pBuffer, sBuffer, pFileName );

	pdv_close( pPdv );

	if ( status != GENCP_STATUS_SUCCESS )
	{
		fprintf( stderr, "%s: GenCP error reading XML file!: 0x%04X\n", functionName, status );
	}

	return GENCP_STATUS_SUCCESS;
}


int main( int argc, char **argv )
{
	int				status;
    unsigned int	channel = 0;
    unsigned int	unit 	= 0;
	unsigned int	iFile	= 0;
    bool	     	verbose = FALSE;

    for ( int iArg = 1; iArg < argc; iArg++ )
    {
		if	(	strcmp( argv[iArg], "-c" ) == 0
			||	strcmp( argv[iArg], "--channel" ) == 0 )
		{
			if ( ++iArg >= argc )
			{
				usage( "Error: Missing channel number.\n" );
				exit( -1 );
			}
			channel = atoi( argv[iArg] );
		}
		else if (	strcmp( argv[iArg], "-u" ) == 0
				||	strcmp( argv[iArg], "--unit" ) == 0 )
        {
			if ( ++iArg >= argc )
			{
				usage( "Error: Missing unit number.\n" );
				exit( -1 );
			}
			unit = atoi( argv[iArg] );
		}
		else if (	strncmp( argv[iArg], "--C", 3 ) == 0 )
		{
			char			buffer[1001];
			unsigned int	numBytes	= atoi( &argv[iArg][3] );
			if ( numBytes == 0 || numBytes > 1000 )
			{
				fprintf( stderr, "Invalid number of bytes for --C option: %s\n", argv[iArg] );
				exit( 1 );
			}
			iArg++;
			if ( iArg >= argc )
			{
				usage( "Error: Missing address.\n" );
				exit( -1 );
			}

			uint64_t		regAddr		= strtoull( argv[iArg], NULL, 0 );
			status = EdtGenCpReadString( unit, channel, regAddr, numBytes, buffer, 1000 );
		}
		else if (	strncmp( argv[iArg], "--U", 3 ) == 0 )
		{
       		//   --U32 Addr  Read 32 bit unsigned value from address\n"
			uint64_t		result64;
			unsigned int	numBits		= atoi( &argv[iArg][3] );
			unsigned int	numBytes	= numBits / 8;
			if ( numBytes == 0 || numBytes > 8 )
			{
				fprintf( stderr, "Invalid number of bits for --U option: %s\n", argv[iArg] );
				exit( 1 );
			}
			iArg++;
			if ( iArg >= argc )
			{
				usage( "Error: Missing address.\n" );
				exit( -1 );
			}

			uint64_t		regAddr		= strtoull( argv[iArg], NULL, 0 );
			//if ( sscanf( argv[iArg], "%LX", &regAddr ) != 1 )
			//{
			//	fprintf( stderr, "Invalid reg addr for --U option: %s\n", argv[iArg] );
			//	exit( 1 );
			//}
			status = EdtGenCpReadUint( unit, channel, regAddr, numBytes, &result64 );
		}
		else if ( strcmp( argv[iArg], "--readXml" ) == 0 )
        {
			iArg++;
			if ( iArg >= argc )
			{
				usage( "Error: Missing fileName.\n" );
				exit( -1 );
			}

			unsigned char	xmlFileBuffer[100000];
			status = EdtGenCpReadXmlFile( unit, channel, iFile, xmlFileBuffer, 100000, argv[iArg] );
		}
		else if (	strcmp( argv[iArg], "-v" ) == 0
				||	strcmp( argv[iArg], "--verbose" ) == 0 )
        {
			verbose = 1;
		}
		else if (	strcmp( argv[iArg], "-h" ) == 0
				||	strcmp( argv[iArg], "--help" ) == 0 )
		{
			usage( "" );
			exit( 0 );
		}
		else
		{
			fprintf( stderr, "unknown option: %s\n", argv[iArg] );
			usage( "" );
			exit( 1 );
		}
    }

    return (0);
}
