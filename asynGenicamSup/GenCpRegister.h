#ifndef	GEN_CP_REGISTER_H
#define	GEN_CP_REGISTER_H

///
/// Bootstrap Register Map (BRM)
/// These registers are technology agnostic
/// i.e. The same for all GeniCam devices
#define REG_BRM_GENCP_VERSION				0x0000
#define REG_BRM_MANUFACTURER_NAME			0x0004
#define REG_BRM_MODEL_NAME					0x0044
#define REG_BRM_FAMILY_NAME					0x0084
#define REG_BRM_DEVICE_VERSION				0x00c4
#define REG_BRM_MANUFACTURER_INFO			0x0104
#define REG_BRM_SERIAL_NUMBER				0x0144
#define REG_BRM_USER_DEFINED_NAME			0x0184
#define REG_BRM_DEVICE_CAPABILITY			0x01c4
#define REG_BRM_MAX_DEVICE_RESPONSE_TIME	0x01cc
#define REG_BRM_MANIFEST_TABLE_ADDRESS		0x01d0
#define REG_BRM_SBRM_ADDRESS				0x01d8
#define REG_BRM_DEVICE_CONFIGURATION		0x01e0
#define REG_BRM_HEARTBEAT_TIMEOUT			0x01e8
#define REG_BRM_MESSAGE_CHANNEL_ID			0x01ec
#define REG_BRM_TIMESTAMP					0x01f0
#define REG_BRM_TIMESTAMP_LATCH				0x01f8
#define REG_BRM_TIMESTAMP_INCREMENT			0x01fc
#define REG_BRM_ACCESS_PRIVILEGE			0x0204
#define REG_BRM_PROTOCOL_ENDIANESS			0x0208
#define REG_BRM_IMPLEMENTATION_ENDIANESS	0x020c
#define REG_BRM_RESERVED					0x0210

/// Manifest Table organization
/// Offsets relative to base addr from REG_BRM_MANIFEST_TABLE_ADDRESS
/// Offset	Length	Name
///	0		8		Number of entries
/// 8		64		Entry 0
/// 8+1*64	64		Entry 1
/// ...
/// 8+N*64	64		Entry N

/// Manifest Table Entry Macros
#define GENCP_MFT_ENTRY_FILE_MAJOR_VERSION(x)	(((x) >> 24) & 0xFF)
#define GENCP_MFT_ENTRY_FILE_MINOR_VERSION(x)	(((x) >> 16) & 0xFF)
#define GENCP_MFT_ENTRY_FILE_SUB_VERSION(x)		((x) & 0xFFFF)
#define GENCP_MFT_ENTRY_SCHEMA_MAJOR_VERSION(x)	(((x) >> 24) & 0xFF)
#define GENCP_MFT_ENTRY_SCHEMA_MINOR_VERSION(x)	(((x) >> 16) & 0xFF)
#define GENCP_MFT_ENTRY_SCHEMA_TYPE(x)			(((x) >> 10) & 0x3F)
#define GENCP_MFT_ENTRY_SCHEMA_TYPE_UNCMP		0
#define GENCP_MFT_ENTRY_SCHEMA_TYPE_ZIP			1
#define GENCP_MFT_ENTRY_SHA1_SIZE				20
#define GENCP_MFT_ENTRY_RSVD_SIZE				20

/// Manifest Table Entry 
typedef struct GENCP_ATTR
{
	uint32_t		xmlFileVersion;		// GeniCam XML file version
	uint32_t		xmlFileSchema;		// Cmd ID from enum GENCPId
	uint64_t		xmlFileStart;		// Length of SCD section
	uint64_t		xmlFileSize;		// Incrementing request id
	uint8_t			xmlFileSHA1[GENCP_MFT_ENTRY_SHA1_SIZE];
	uint8_t			xmlFileRsvd[GENCP_MFT_ENTRY_RSVD_SIZE];
}	GenCpManifestEntry;

#endif	/* GEN_CP_REGISTER_H */
