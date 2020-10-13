#include "sbgInterfacePx4Serial.h"
#include <stdio.h>
#include <termios.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>

#include <time.h>

#ifdef __PX4_NUTTX
#include <nuttx/clock.h>
#include <nuttx/arch.h>
#endif

static uint32 sbgInterfacePx4SerialGetBaudRateConst(uint32 baudRate)
{
	uint32								 baudRateConst;

	switch (baudRate)
	{
		case 9600:
			baudRateConst = B9600;
			break;
		case 19200:
			baudRateConst = B19200;
			break;
#ifdef B38400
		case 38400:
			baudRateConst = B38400;
			break;
#endif
#ifdef B57600
		case 57600:
			baudRateConst = B57600;
			break;
#endif
#ifdef B115200
		case 115200:
			baudRateConst = B115200;
			break;
#endif
#ifdef B230400
		case 230400:
			baudRateConst = B230400;
			break;
#endif
#ifdef B460800
		case 460800:
			baudRateConst = B460800;
			break;
#endif
#ifdef B921600
		case 921600:
			baudRateConst = B921600;
			break;
#endif // B921600
		default:
			baudRateConst = baudRate;
	}

	return baudRateConst;
}
SbgErrorCode sbgInterfacePx4SerialCreate(SbgInterface *pSbgInterface, int serialHandle, uint32_t baudRate)
{
	SbgErrorCode						 errorCode;
	SbgInterfacePx4Serial				*pPx4Interface;
	struct termios						 config;
	uint32								 baudRateConst;

   	if (pSbgInterface)
	{
		if (serialHandle >= 0)
		{
			baudRateConst = sbgInterfacePx4SerialGetBaudRateConst(baudRate);

			pPx4Interface = (SbgInterfacePx4Serial*) malloc(sizeof(SbgInterfacePx4Serial));

			if (pPx4Interface)
			{
				pPx4Interface->readCallback = NULL;

				pPx4Interface->serialHandle = serialHandle;

				if (tcgetattr((pPx4Interface->serialHandle), &config) != -1)
				{

					// Control Modes 
					config.c_cflag &= ~PARENB;	// Disable parity
					config.c_cflag &= ~CRTSCTS;	// Disable RTS/CTS hardware flow control
					config.c_cflag &= ~CSTOPB;	// Only one stop bit 
					config.c_cflag |= CS8;		// 8 bits per byte
					config.c_cflag |= CLOCAL;	// Ignore control lines
					config.c_cflag |= CREAD;	// Turn on READ
					
					// Local Modes
					config.c_lflag &= ~ICANON;	// Disable canonical mode
					config.c_lflag &= ~ECHO;	// Disable echo
					config.c_lflag &= ~ECHOE;	// Disable erasure
					config.c_lflag &= ~ECHONL;	// Disable new-line echo
					config.c_lflag &= ~ISIG;	// Disable interpretation of INTR, QUIT and SUSP

					// Input Modes
					config.c_iflag &= ~(IXON | IXOFF | IXANY); // Disable software flow control
					config.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); // Disable any special handling of received bytes

					//Output Modes
					config.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes
					config.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed

					config.c_cc[VMIN]  = 0; // No blocking
					config.c_cc[VTIME] = 0;	// Return immediately with what is available

					if ((cfsetispeed(&config, baudRateConst) >= 0) && (cfsetospeed(&config, baudRateConst) >= 0))
					{
						if (tcsetattr((pPx4Interface->serialHandle), TCSANOW, &config) == 0)
						{
							pSbgInterface->handle		= (void*)pPx4Interface;
							pSbgInterface->type			= SBG_IF_TYPE_SERIAL;
							pSbgInterface->pReadFunc	= sbgInterfacePx4SerialRead;
							pSbgInterface->pWriteFunc	= sbgInterfacePx4SerialWrite;

							errorCode = sbgInterfacePx4SerialFlush(pSbgInterface);
						}
						else
						{
							errorCode = SBG_ERROR;
							SBG_LOG_ERROR(errorCode, "tcsetattr fails");
						}
					}
					else
					{
						errorCode = SBG_ERROR;
						SBG_LOG_ERROR(errorCode, "unable to set speed");
					}
				}
				else
				{
					errorCode = SBG_ERROR;
					SBG_LOG_ERROR(errorCode, "tcgetattr fails");
				}
			}
			else
			{
				errorCode = SBG_ERROR;
				SBG_LOG_ERROR(errorCode, "malloc fails");

			}
		}
		else
		{
			errorCode = SBG_INVALID_PARAMETER;
		}
	}
	else
	{
		errorCode = SBG_NULL_POINTER;
		SBG_LOG_ERROR(errorCode, "invalid parameter");
	}

	return errorCode;
}

SbgErrorCode sbgInterfacePx4SerialRead(SbgInterface *pSbgInterface, void *pBuffer, size_t *pBytesRead, size_t bytesToRead)
{
	SbgErrorCode						 errorCode;
	int									 bytesRead;
	SbgInterfacePx4Serial				*pPx4Interface;

	assert(pSbgInterface);
	assert(pBuffer);
	assert(pBytesRead);

	pPx4Interface = (SbgInterfacePx4Serial*)pSbgInterface->handle;

	bytesRead = read(pPx4Interface->serialHandle, pBuffer, bytesToRead);

	if (bytesRead >= 0)
	{
		*pBytesRead = (size_t)bytesRead;
		errorCode = SBG_NO_ERROR;
	}
	else
	{
		*pBytesRead = 0;
		errorCode = SBG_READ_ERROR;
		SBG_LOG_ERROR(errorCode, "unable to read data");
	}

	if (errorCode == SBG_NO_ERROR && pPx4Interface->readCallback)
	{
	   pPx4Interface->readCallback(pBuffer, pBytesRead, pPx4Interface->userArg);
	}

	return errorCode;
}

SbgErrorCode sbgInterfacePx4SerialWrite(SbgInterface *pSbgInterface, const void *pBuffer, size_t bytesToWrite)
{
	SbgErrorCode						 errorCode;
	size_t								 bytesLeftToWrite;
	const char							*pCurrentBuffer;
	ssize_t								 bytesWritten;
	SbgInterfacePx4Serial				*pPx4Interface;

	assert(pSbgInterface);
	assert(pBuffer);

	pPx4Interface = pSbgInterface->handle;

	assert(pPx4Interface);

	errorCode = SBG_NO_ERROR;

	bytesLeftToWrite = bytesToWrite;

	pCurrentBuffer = pBuffer;

	while (bytesLeftToWrite > 0)
	{
		bytesWritten = write(pPx4Interface->serialHandle, pCurrentBuffer, bytesLeftToWrite);

		if (bytesWritten >= 0)
		{
			bytesLeftToWrite -= bytesWritten;
			pCurrentBuffer += (size_t)bytesWritten;
		}
		else
		{
			errorCode = SBG_WRITE_ERROR;
			SBG_LOG_ERROR(errorCode, "unable to write data");
			break;
		}
	}

	return errorCode;
}

SbgErrorCode sbgInterfacePx4SerialFlush(SbgInterface *pSbgInterface)
{
	SbgErrorCode						 errorCode;
	SbgInterfacePx4Serial				*pPx4Interface;

	assert(pSbgInterface);

	pPx4Interface = pSbgInterface->handle;

	assert(pPx4Interface);

	if (tcflush(pPx4Interface->serialHandle, TCIOFLUSH) == 0)
	{
	  errorCode = SBG_NO_ERROR;
	}
	else
	{
		errorCode = SBG_ERROR;
		SBG_LOG_ERROR(errorCode, "unable to flush");
	}

	return errorCode;
}

void sbgInterfacePx4SerialSetReadCallback(SbgInterface *pSbgInterface, SbgInterfaceCallbackReadFunc callback, void* user_arg)
{
	SbgInterfacePx4Serial				*pPx4Interface;

	assert(pSbgInterface);
	assert(callback);
	assert(user_arg);

	pPx4Interface = pSbgInterface->handle;

	assert(pPx4Interface);

	pPx4Interface->readCallback = callback;
	pPx4Interface->userArg = user_arg;
}

void sbgInterfacePx4SerialSetWriteCallback(SbgInterface *pSbgInterface, SbgInterfaceCallbackWriteFunc callback, void* user_arg)
{
	SbgInterfacePx4Serial				*pPx4Interface;

	assert(pSbgInterface);
	assert(callback);
	assert(user_arg);

	pPx4Interface = pSbgInterface->handle;

	assert(pPx4Interface);

	pPx4Interface->writeCallback = callback;
	pPx4Interface->userArg = user_arg;
}
