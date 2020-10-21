#ifndef SBG_INTERFACE_PX4_SERIAL_H
#define SBG_INTERFACE_PX4_SERIAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "sbgInterfaceSerial.h"
#include <poll.h>

typedef SbgErrorCode (*SbgInterfaceCallbackReadFunc) (void*, size_t*, void*);
typedef SbgErrorCode (*SbgInterfaceCallbackWriteFunc) (void*, size_t*, void*);

typedef struct _SbgInterfacePx4Serial
{
	void								*userArg;
	SbgInterfaceCallbackReadFunc		 readCallback;
	SbgInterfaceCallbackWriteFunc		 writeCallback;
	int									 serialHandle;
} SbgInterfacePx4Serial; 

SbgErrorCode sbgInterfacePx4SerialCreate(SbgInterface *pSbgInterface, int serialHandle, uint32_t baudRate);

SbgErrorCode sbgInterfacePx4SerialFlush(SbgInterface *pHandle);

SbgErrorCode sbgInterfacePx4SerialRead(SbgInterface *pSbgInterface, void *pBuffer, size_t *pBytesRead, size_t bytesToRead);

SbgErrorCode sbgInterfacePx4SerialWrite(SbgInterface *pHandle, const void *pBuffer, size_t bytesToWrite);

void sbgInterfacePx4SerialSetReadCallback(SbgInterface *pHandle, SbgInterfaceCallbackReadFunc callback, void *user_arg);
#ifdef __cplusplus
}
#endif

#endif /* SBG_INTERFACE_PX4_SERIAL */
