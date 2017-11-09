//////////////////////////////////////////////////////////////////////////////
// This file is part of 'ADEdtPdv'.
// It is subject to the license terms in the LICENSE.txt file found in the 
// top-level directory of this distribution and at: 
//    https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. 
// No part of 'ADEdtPdv', including this file, 
// may be copied, modified, propagated, or distributed except according to 
// the terms contained in the LICENSE.txt file.
//////////////////////////////////////////////////////////////////////////////
#ifndef	GENCP_PACKET_H
#define	GENCP_PACKET_H
///
/// GeniCam Generic Control Protocol (GenCP) Packet header
/// Defines macros, enums, structures needed for GenCP compliant communications
/// Based on EVMA standard which can be downloaded here:
///		http://www.emva.org/wp-content/uploads/GenCP_1.1.pdf
/// This version created at SLAC for EPICS interfaces to GenCP compliant Camlink cameras
///	Bruce Hill, July 19, 2017
///

#ifndef GC_USER_DEFINED_TYPES
/* The types should be the same as defined in GCTypes.h from GenApi. But in
 * case you do not have this header the necessary types are defined here. */
#  if defined(_WIN32)
#    if defined(_MSC_VER) && _MSC_VER >= 1600 /* VS2010 provides stdint.h */
#      include <stdint.h>
#    elif !defined _STDINT_H && !defined _STDINT
       /* stdint.h is usually not available under Windows */
       typedef unsigned char uint8_t;
       typedef __int32 int32_t;
       typedef unsigned __int32 uint32_t;
       typedef unsigned __int64 uint64_t;
#    endif
#  else
#    include <stdint.h>
#  endif
#  ifdef __cplusplus
     typedef bool bool8_t;
#  else
     typedef uint8_t bool8_t;
#  endif
#endif /* GC_DEFINE_TYPES */

#define	GENCP_ATTR	__attribute__((__packed__))

///
/// GenCP Packet Organization
///	Every GenCP packet has 4 sections
///		Prefix,  technology specific, may be empty
///		Common  Command Data (CCD), technology agnostic, device common
///		Specfic Command Data (SCD), technology agnostic, device specific
///		Postfix, technology specific, may be empty
///

#define	GENCP_SERIAL_PREAMBLE			0x0100	// First two bytes are 1 (SOH) and 0 (NULL)

/// GenCP CCD FLAGS
#define	GENCP_CCD_FLAG_REQACK			0x4000	// Sender requests an acknowldege packet
#define	GENCP_CCD_FLAG_RESEND			0x8000	// This command packet is being sent as a retry

/// GenCP Status Code Bits
#define	GENCP_SC_CODE_MASK				0x0FFF	// Status code is least significant 12 bits
#define	GENCP_SC_NAMESPACE_MASK			0x6000	// Use this mask for the Namespace bits
#define	GENCP_SC_NAMESPACE_GENCP		0x0000	// GenCP Common Status Code
#define	GENCP_SC_NAMESPACE_TECH			0x2000	// Technology specific Status Code
#define	GENCP_SC_NAMESPACE_DEVICE		0x4000	// Device specific Status Code
#define	GENCP_SC_ERROR					0x8000	// 1=Error, 0=Warning/Info

/// GenCP Common Status Codes
#define	GENCP_STATUS_SUCCESS			0x0000	// Success
#define	GENCP_STATUS_NOT_IMPL			0x0001	// Command not implemented
#define	GENCP_STATUS_INVALID_PARAM		0x0002	// Invalid parameter(s) in CCD or SCD
#define	GENCP_STATUS_INVALID_ADDR		0x0003	// Invalid register address
#define	GENCP_STATUS_WRITE_PROTECT		0x0004	// Write attempted to read-only register
#define	GENCP_STATUS_BAD_ALIGNMENT		0x0005	// Bad register address alignment
#define	GENCP_STATUS_ACCESS_DENIED		0x0006	// Access denied, reg not readable or not writable
#define	GENCP_STATUS_BUSY				0x0007	// Command receiver too busy
#define	GENCP_STATUS_MSG_TIMEOUT		0x000B	// Timeout waiting for acknowledge from receiver
#define	GENCP_STATUS_INVALID_HDR		0x000E	// Invalid header in command packet
#define	GENCP_STATUS_WRONG_CONFIG		0x000F	// Current receiver configuration does not allow command
#define	GENCP_STATUS_GENERIC_ERROR		0x0FFF	// Command not implemented

//#define GENCP_READMEM_MAX_BYTES			384		// 100 is recommended max per GenCP standard
#define GENCP_READMEM_MAX_BYTES			64		// 100 is recommended max per GenCP standard

typedef	uint32_t	GENCP_STATUS;

/// GenCP CCD Command Identifiers
typedef enum
{
	GENCP_ID_READMEM_CMD		= 0x0800,
	GENCP_ID_READMEM_ACK		= 0x0801,
	GENCP_ID_WRITEMEM_CMD		= 0x0802,
	GENCP_ID_WRITEMEM_ACK		= 0x0803,
	GENCP_ID_PENDING_ACK		= 0x0805,
	GENCP_ID_EVENT_CMD			= 0x0c00,
	GENCP_ID_EVENT_ACK			= 0x0c01
}	GENCP_ID_COMMAND_TYPE;

/// Technology Specific Prefix: GenCpSerialPrefix
/// Technology Specific Prefix isn't needed for USB or IP transport layers
/// NOTE: All packet fields use big endian byte ordering.
/// Always use inline functions or macros to handle bytes swapping as needed when accessing these fields.
/// For example:
///	#include "asm/byteorder.h"
/// packetPrefix.prefixPreamble = __cpu_to_be16(GENCP_SERIAL_PREAMBLE);
typedef struct GENCP_ATTR
{
	uint16_t		prefixPreamble;		// Always 0x0100 for serial GenCP Packets
	uint16_t		prefixCkSumCCD;		// Cksum over channelId and CCD
	uint16_t		prefixCkSumSCD;		// Cksum over channelId, CCD and SCD
	uint16_t		prefixChannelId;	// 0 for control channel
}	GenCpSerialPrefix;

///
/// Common Command Data Section (CCD)
///

/// Common Command Request Packet Layout (CCD)
typedef struct GENCP_ATTR
{
	uint16_t		ccdFlags;			// See GENCP_CCD_FLAG_*
	uint16_t		ccdCommandId;		// Cmd ID from enum GENCPId
	uint16_t		ccdScdLength;		// Length of SCD section
	uint16_t		ccdRequestId;		// Incrementing request id
}	GenCpCCDRequest;

extern uint16_t	GetRequestId( GenCpCCDRequest * pCCD );

/// Common Acknowledge Packet Layout (CCD)
typedef struct GENCP_ATTR
{
	uint16_t		ccdStatusCode;		// See GENCP_STATUS_*
	uint16_t		ccdCommandId;		// Cmd ID from enum GENCPId
	uint16_t		ccdScdLength;		// Length of SCD section
	uint16_t		ccdRequestId;		// Request id (from matching command packet)
}	GenCpCCDAck;

extern uint16_t	GetRequestId( GenCpCCDAck * pCCD );

///
/// Specific Command Data Section (SCD)
///

/// Specific Command Data ReadMem Request (SCD)
typedef struct GENCP_ATTR
{
	uint64_t		scdRegAddr;		// Register address
	uint16_t		scdReserved;	// Reserved, set to 0
	uint16_t		scdReadSize;	// Number of bytes to read from addr
}	GenCpSCDReadMem;

/// Specific Command Data ReadMem Acknowledge Layout (SCD)
typedef struct GENCP_ATTR
{
	uint8_t			scdReadData[GENCP_READMEM_MAX_BYTES];	// Packet payload
}	GenCpSCDReadAck;


/// Specific Command Data WriteMem Request (SCD)
typedef struct GENCP_ATTR
{
	uint64_t		scdRegAddr;		// Register address
	uint8_t			scdWriteData[GENCP_READMEM_MAX_BYTES];	// Packet payload
	// Remaining data bytes per scdReadSize follow
}	GenCpSCDWriteMem;


/// Specific Command Data WriteMem Acknowledge Layout (SCD)
/// Will be empty if according bit in Device Capability Register not set
typedef struct GENCP_ATTR
{
	uint16_t		scdWriteAckRsvd;	// Reserved
	uint16_t		scdLengthWritten;	// Number of bytes successfully written
}	GenCpSCDWriteAck;


/// Specific Command Data WriteMem Pending Acknowledge Layout (SCD)
typedef struct GENCP_ATTR
{
	uint16_t		scdPendingAckRsvd;	// Reserved
	uint16_t		scdPendingTimeout;	// Additional time needed for this request in ms
}	GenCpSCDPendingAck;

/// Technology Specific Postfix: Not Needed for Serial GenCP Transport Layer

///
/// GenCP Read Memory Packet
///
typedef struct	GENCP_ATTR
{
	GenCpSerialPrefix	serialPrefix;
	GenCpCCDRequest		ccd;
	GenCpSCDReadMem		scd;
}	GenCpReadMemPacket;

typedef struct	GENCP_ATTR
{
	GenCpSerialPrefix	serialPrefix;
	GenCpCCDAck			ccd;
	GenCpSCDReadAck		scd;
}	GenCpReadMemAck;

///
/// GenCP Write Memory Packet
///
typedef struct	GENCP_ATTR
{
	GenCpSerialPrefix	serialPrefix;
	GenCpCCDRequest		ccd;
	GenCpSCDWriteMem	scd;
}	GenCpWriteMemPacket;

typedef struct	GENCP_ATTR
{
	GenCpSerialPrefix	serialPrefix;
	GenCpCCDAck			ccd;
	GenCpSCDWriteAck	scd;
}	GenCpWriteMemAck;

///
/// GenCP Packet function declarations
///

/// Compute 16 bit host checksum for big-endian buffer
uint16_t GenCpChecksum16( uint8_t * pBuffer, uint32_t nNumBytes );

/// GenCpInitReadMemPacket() Initialize a ReadMem packet to read numBytes from regAddr
GENCP_STATUS	GenCpInitReadMemPacket(	GenCpReadMemPacket		*	pPacket,
										uint16_t					requestId,
										uint64_t					regAddr,
										size_t						numBytes );

/// GenCpValidateReadMemAck() Checks for any errors in a ReadMem acknowledge packet
GENCP_STATUS	GenCpValidateReadMemAck( GenCpReadMemAck		*	pPacket, uint32_t expectedRequestId	);

/// GenCpProcessReadMemAck() char buffer
GENCP_STATUS	GenCpProcessReadMemAck(	GenCpReadMemAck			*	pPacket,
										uint32_t					expectedRequestId,
										char					*	pBuffer,
										size_t						sBuffer,
										size_t					*	pnBytesRead );

/// GenCpProcessReadMemAck() 16 bit reg
GENCP_STATUS	GenCpProcessReadMemAck( GenCpReadMemAck			*	pPacket,
										uint32_t					expectedRequestId,
										uint16_t				*	pReg16 );

/// GenCpProcessReadMemAck() 32 bit reg
GENCP_STATUS	GenCpProcessReadMemAck( GenCpReadMemAck			*	pPacket,
										uint32_t					expectedRequestId,
										uint32_t				*	pReg32 );

/// GenCpProcessReadMemAck() 64 bit reg
GENCP_STATUS	GenCpProcessReadMemAck( GenCpReadMemAck			*	pPacket,
										uint32_t					expectedRequestId,
										uint64_t				*	pReg64 );

/// GenCpProcessReadMemAck() 32 bit float reg
GENCP_STATUS	GenCpProcessReadMemAck( GenCpReadMemAck			*	pPacket,
										uint32_t					expectedRequestId,
										float					*	pReg32 );

/// GenCpProcessReadMemAck() 64 bit double reg
GENCP_STATUS	GenCpProcessReadMemAck( GenCpReadMemAck			*	pPacket,
										uint32_t					expectedRequestId,
										double					*	pReg64 );

/// GenCpValidateWriteMemAck() Checks for any errors in a WriteMem acknowledge packet
GENCP_STATUS	GenCpValidateWriteMemAck( GenCpWriteMemAck		*	pPacket, uint32_t expectedRequestId	);

/// GenCpInitWriteMemPacket() Initialize a WriteMem packet to write a string to regAddr
GENCP_STATUS	GenCpInitWriteMemPacket(GenCpWriteMemPacket		*	pPacket,
										uint16_t					requestId,
										uint64_t					regAddr,
										size_t						numBytes,
										const char				*	pString,
										size_t					*	pnBytesSend );

/// GenCpInitWriteMemPacket() Initialize a WriteMem packet to write a uint16 to regAddr
GENCP_STATUS	GenCpInitWriteMemPacket(GenCpWriteMemPacket		*	pPacket,
										uint16_t					requestId,
										uint64_t					regAddr,
										uint16_t					regValue,
										size_t					*	pnBytesSend );

/// GenCpInitWriteMemPacket() Initialize a WriteMem packet to write a uint32 to regAddr
GENCP_STATUS	GenCpInitWriteMemPacket(GenCpWriteMemPacket		*	pPacket,
										uint16_t					requestId,
										uint64_t					regAddr,
										uint32_t					regValue,
										size_t					*	pnBytesSend );

/// GenCpInitWriteMemPacket() Initialize a WriteMem packet to write a uint64 to regAddr
GENCP_STATUS	GenCpInitWriteMemPacket(GenCpWriteMemPacket		*	pPacket,
										uint16_t					requestId,
										uint64_t					regAddr,
										uint64_t					regValue,
										size_t					*	pnBytesSend );

/// GenCpInitWriteMemPacket() Initialize a WriteMem packet to write a float to regAddr
GENCP_STATUS	GenCpInitWriteMemPacket(GenCpWriteMemPacket		*	pPacket,
										uint16_t					requestId,
										uint64_t					regAddr,
										float						regValue,
										size_t					*	pnBytesSend );

/// GenCpInitWriteMemPacket() Initialize a WriteMem packet to write a double to regAddr
GENCP_STATUS	GenCpInitWriteMemPacket(GenCpWriteMemPacket		*	pPacket,
										uint16_t					requestId,
										uint64_t					regAddr,
										double						regValue,
										size_t					*	pnBytesSend );

/// Convenience functions to hide __be32_to_cpu() and other variants
extern uint32_t	GenCpBigEndianToCpu( uint32_t	be32Value );
extern uint64_t	GenCpBigEndianToCpu( uint64_t	be64Value );

#endif	/* GENCP_PACKET_H */
