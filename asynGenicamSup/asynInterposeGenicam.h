/*asynInterposeGenicam.h*/
/***********************************************************************
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory, and the Regents of the University of
* California, as Operator of Los Alamos National Laboratory, and
* Berliner Elektronenspeicherring-Gesellschaft m.b.H. (BESSY).
* asynDriver is distributed subject to a Software License Agreement
* found in file LICENSE that is included with this distribution.
***********************************************************************/

#ifndef asynInterposeGenicam_H
#define asynInterposeGenicam_H

#include <shareLib.h>

#define	GENCP_TY_RESP_ACK		0
#define	GENCP_TY_RESP_STRING	1
#define	GENCP_TY_RESP_UINT		2
#define	GENCP_TY_RESP_INT		3
#define	GENCP_TY_RESP_FLOAT		4
#define	GENCP_TY_RESP_DOUBLE	5

#define	GENCP_RESPONSE_MAX		128

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

epicsShareFunc int asynInterposeGenicamConfig( const char *	portName, int addr );

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif /* asynInterposeGenicam_H */
