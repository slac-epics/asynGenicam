/***********************************************************************
 * Copyright (c) 2002 - 2011 UChicago Argonne LLC, as Operator of
 *    Argonne National Laboratory
 * asynInterposeGenicam is distributed subject to a Software License
 * Agreement found in the file LICENSE that is included with this
 * distribution.
 ***********************************************************************/

/* asynInterposeGenicam.c */

/* Genicam octets to a port */
/* 
 * Author: Eric Norum
 * A quick hack based on the interposeEos code
 * Genicams characters out  s  l  o  w  l  y
 */

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <cantProceed.h>
#include <epicsStdio.h>
#include <epicsString.h>
#include <epicsThread.h>
#include <iocsh.h>

#include <epicsExport.h>
#include "asynDriver.h"
#include "asynOctet.h"
#include "asynShellCommands.h"
#include "asynInterposeGenicam.h"
#include <epicsExport.h>

#define DEFAULT_INTERVAL     100
#define MS_PER_SECOND       1000.

typedef struct interposePvt {
    char          *portName;
    int           addr;
    asynInterface octet;
    asynOctet     *pasynOctetDrv;
    void          *drvPvt;
    double        interval;
} interposePvt;

/* asynOctet methods */
static asynStatus writeIt(void *ppvt, asynUser *pasynUser,
    const char *data, size_t numchars, size_t *nbytesTransfered);
static asynStatus readIt(void *ppvt, asynUser *pasynUser,
    char *data, size_t maxchars, size_t *nbytesTransfered, int *eomReason);
static asynStatus flushIt(void *ppvt, asynUser *pasynUser);
static asynStatus registerInterruptUser(void *drvPvt, asynUser *pasynUser,
    interruptCallbackOctet callback, void *userPvt, void **registrarPvt);
static asynStatus cancelInterruptUser(void *drvPvt, asynUser *pasynUser,
    void *registrarPvt);
static asynStatus setInputEos(void *ppvt, asynUser *pasynUser,
    const char *eos, int eoslen);
static asynStatus getInputEos(void *ppvt, asynUser *pasynUser,
    char *eos, int eossize, int *eoslen);
static asynStatus setOutputEos(void *ppvt, asynUser *pasynUser,
    const char *eos, int eoslen);
static asynStatus getOutputEos(void *ppvt, asynUser *pasynUser,
    char *eos, int eossize, int *eoslen);
static asynOctet octet = {
    writeIt, readIt, flushIt,
    registerInterruptUser, cancelInterruptUser,
    setInputEos, getInputEos, setOutputEos, getOutputEos
};

epicsShareFunc int
asynInterposeGenicamConfig(const char *portName, int addr, int interval)
{
    interposePvt *pinterposePvt;
    asynStatus status;
    asynInterface *poctetasynInterface;

    pinterposePvt = callocMustSucceed(1, sizeof(interposePvt),
        "interposeInterfaceInit");
    pinterposePvt->portName = epicsStrDup(portName);
    pinterposePvt->addr = addr;
    pinterposePvt->octet.interfaceType = asynOctetType;
    pinterposePvt->octet.pinterface = &octet;
    pinterposePvt->octet.drvPvt = pinterposePvt;
    if (interval <= 0)
        interval = DEFAULT_INTERVAL;
    pinterposePvt->interval = (double) interval / MS_PER_SECOND;
    status = pasynManager->interposeInterface(portName, addr,
        &pinterposePvt->octet, &poctetasynInterface);
    if ((status != asynSuccess) || !poctetasynInterface) {
        printf("%s interposeInterface failed.\n", portName);
        free((void *)pinterposePvt->portName);
        free(pinterposePvt);
        return -1;
    }
    pinterposePvt->pasynOctetDrv = (asynOctet *)poctetasynInterface->pinterface;
    pinterposePvt->drvPvt = poctetasynInterface->drvPvt;
    return 0;
}

/* asynOctet methods */
static asynStatus writeIt(void *ppvt, asynUser *pasynUser,
    const char *data, size_t numchars, size_t *nbytesTransfered)
{
    interposePvt *pinterposePvt = (interposePvt *)ppvt;
    int i;
    size_t n;
    asynStatus status;

    if (nbytesTransfered)
        *nbytesTransfered = 0;
    for (i = 0 ; i < numchars ; i++) {
        status = pinterposePvt->pasynOctetDrv->write(pinterposePvt->drvPvt,
            pasynUser, data, 1, &n);
        data += n;
        if (nbytesTransfered)
            *nbytesTransfered += n;
        if (status != asynSuccess)
            return status;
        epicsThreadSleep(pinterposePvt->interval);
    }
    return asynSuccess;
}

static asynStatus readIt(void *ppvt, asynUser *pasynUser,
    char *data, size_t maxchars, size_t *nbytesTransfered, int *eomReason)
{
    interposePvt *pinterposePvt = (interposePvt *)ppvt;

    return pinterposePvt->pasynOctetDrv->read(pinterposePvt->drvPvt,
        pasynUser, data, maxchars, nbytesTransfered, eomReason);
}

static asynStatus flushIt(void *ppvt, asynUser *pasynUser)
{
    interposePvt *pinterposePvt = (interposePvt *)ppvt;
    
    return pinterposePvt->pasynOctetDrv->flush(
        pinterposePvt->drvPvt, pasynUser);
}

static asynStatus registerInterruptUser(void *ppvt, asynUser *pasynUser,
    interruptCallbackOctet callback, void *userPvt, void **registrarPvt)
{
    interposePvt *pinterposePvt = (interposePvt *)ppvt;

    return pinterposePvt->pasynOctetDrv->registerInterruptUser(
        pinterposePvt->drvPvt, pasynUser, callback, userPvt, registrarPvt);
}

static asynStatus cancelInterruptUser(void *drvPvt, asynUser *pasynUser,
    void *registrarPvt)
{
    interposePvt *pinterposePvt = (interposePvt *)drvPvt;

    return pinterposePvt->pasynOctetDrv->cancelInterruptUser(
        pinterposePvt->drvPvt, pasynUser, registrarPvt);
}

static asynStatus setInputEos(void *ppvt, asynUser *pasynUser,
    const char *eos, int eoslen)
{
    interposePvt *pinterposePvt = (interposePvt *)ppvt;

    return pinterposePvt->pasynOctetDrv->setInputEos(
        pinterposePvt->drvPvt, pasynUser, eos, eoslen);
}

static asynStatus getInputEos(void *ppvt, asynUser *pasynUser,
    char *eos, int eossize, int *eoslen)
{
    interposePvt *pinterposePvt = (interposePvt *)ppvt;

    return pinterposePvt->pasynOctetDrv->getInputEos(
        pinterposePvt->drvPvt, pasynUser, eos, eossize, eoslen);
}

static asynStatus setOutputEos(void *ppvt, asynUser *pasynUser,
    const char *eos, int eoslen)
{
    interposePvt *pinterposePvt = (interposePvt *)ppvt;

    return pinterposePvt->pasynOctetDrv->setOutputEos(
        pinterposePvt->drvPvt, pasynUser, eos, eoslen);
}

static asynStatus getOutputEos(void *ppvt, asynUser *pasynUser,
    char *eos, int eossize, int *eoslen)
{
    interposePvt *pinterposePvt = (interposePvt *)ppvt;

    return pinterposePvt->pasynOctetDrv->getOutputEos(
        pinterposePvt->drvPvt, pasynUser, eos, eossize, eoslen);
}

/* register asynInterposeGenicamConfig*/
static const iocshArg asynInterposeGenicamConfigArg0 =
    { "portName", iocshArgString };
static const iocshArg asynInterposeGenicamConfigArg1 =
    { "addr", iocshArgInt };
static const iocshArg asynInterposeGenicamConfigArg2 =
    { "[interval]", iocshArgDouble };
static const iocshArg *asynInterposeGenicamConfigArgs[] = {
    &asynInterposeGenicamConfigArg0,
    &asynInterposeGenicamConfigArg1,
    &asynInterposeGenicamConfigArg2
};
static const iocshFuncDef asynInterposeGenicamConfigFuncDef =
    {"asynInterposeGenicamConfig", 3, asynInterposeGenicamConfigArgs};
static void asynInterposeGenicamConfigCallFunc(const iocshArgBuf *args)
{
    asynInterposeGenicamConfig(args[0].sval, args[1].ival, args[2].dval);
}

static void asynInterposeGenicamRegister(void)
{
    static int firstTime = 1;
    if (firstTime) {
        firstTime = 0;
        iocshRegister(&asynInterposeGenicamConfigFuncDef,
            asynInterposeGenicamConfigCallFunc);
    }
}
epicsExportRegistrar(asynInterposeGenicamRegister);
