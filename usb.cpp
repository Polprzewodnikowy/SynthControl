#include <QPushButton>
#include "usb.h"

BOOL GetDeviceHandle(GUID guidDeviceInterface, PHANDLE hDeviceHandle)
{
    BOOL bResult = TRUE;
    HDEVINFO hDeviceInfo;
    SP_DEVINFO_DATA DeviceInfoData;

    SP_DEVICE_INTERFACE_DATA deviceInterfaceData;
    PSP_DEVICE_INTERFACE_DETAIL_DATA pInterfaceDetailData = NULL;

    ULONG requiredLength = 0;

    LPTSTR lpDevicePath = NULL;

    DWORD index = 0;

    if(hDeviceHandle == INVALID_HANDLE_VALUE)
        return FALSE;

    // Get information about all the installed devices for the specified
    // device interface class.
    hDeviceInfo = SetupDiGetClassDevs(&guidDeviceInterface, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if(hDeviceInfo == INVALID_HANDLE_VALUE)
    {
        // ERROR
        printf("Error SetupDiGetClassDevs: %d.\n", GetLastError());
        LocalFree(lpDevicePath);
        LocalFree(pInterfaceDetailData);
        bResult = SetupDiDestroyDeviceInfoList(hDeviceInfo);
        return bResult;
    }

    //Enumerate all the device interfaces in the device information set.
    DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    for(index = 0; SetupDiEnumDeviceInfo(hDeviceInfo, index, &DeviceInfoData); index++)
    {
        //Reset for this iteration
        if(lpDevicePath)
            LocalFree(lpDevicePath);

        if(pInterfaceDetailData)
            LocalFree(pInterfaceDetailData);

        deviceInterfaceData.cbSize = sizeof(SP_INTERFACE_DEVICE_DATA);

        //Get information about the device interface.
        bResult = SetupDiEnumDeviceInterfaces(hDeviceInfo, &DeviceInfoData, &guidDeviceInterface, 0, &deviceInterfaceData);

        // Check if last item
        if(GetLastError() == ERROR_NO_MORE_ITEMS)
            break;

        //Check for some other error
        if(!bResult)
        {
            printf("Error SetupDiEnumDeviceInterfaces: %d.\n", GetLastError());
            LocalFree(lpDevicePath);
            LocalFree(pInterfaceDetailData);
            bResult = SetupDiDestroyDeviceInfoList(hDeviceInfo);
            return bResult;
        }

        //Interface data is returned in SP_DEVICE_INTERFACE_DETAIL_DATA
        //which we need to allocate, so we have to call this function twice.
        //First to get the size so that we know how much to allocate
        //Second, the actual call with the allocated buffer

        bResult = SetupDiGetDeviceInterfaceDetail(hDeviceInfo, &deviceInterfaceData, NULL, 0, &requiredLength, NULL );

        //Check for some other error
        if(!bResult)
        {
            if((ERROR_INSUFFICIENT_BUFFER == GetLastError()) && (requiredLength > 0))
            {
                //we got the size, allocate buffer
                pInterfaceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)LocalAlloc(LPTR, requiredLength);

                if(!pInterfaceDetailData)
                {
                    //ERROR
                    printf("Error allocating memory for the device detail buffer.\n");
                    LocalFree(lpDevicePath);
                    LocalFree(pInterfaceDetailData);
                    bResult = SetupDiDestroyDeviceInfoList(hDeviceInfo);
                    return bResult;
                }
            }
            else
            {
                printf("Error SetupDiEnumDeviceInterfaces: %d.\n", GetLastError());
                LocalFree(lpDevicePath);
                LocalFree(pInterfaceDetailData);
                bResult = SetupDiDestroyDeviceInfoList(hDeviceInfo);
                return bResult;
            }
        }

        //get the interface detailed data
        pInterfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

        //Now call it with the correct size and allocated buffer
        bResult = SetupDiGetDeviceInterfaceDetail(hDeviceInfo, &deviceInterfaceData, pInterfaceDetailData, requiredLength, NULL, &DeviceInfoData);

        //Check for some other error
        if(!bResult)
        {
            printf("Error SetupDiGetDeviceInterfaceDetail: %d.\n", GetLastError());
            LocalFree(lpDevicePath);
            LocalFree(pInterfaceDetailData);
            bResult = SetupDiDestroyDeviceInfoList(hDeviceInfo);
            return bResult;
        }

        //copy device path
        size_t nLength = wcslen(pInterfaceDetailData->DevicePath) + 1;
        lpDevicePath = (TCHAR *)LocalAlloc(LPTR, nLength * sizeof(TCHAR));
        StringCchCopy(lpDevicePath, nLength, pInterfaceDetailData->DevicePath);
        lpDevicePath[nLength - 1] = 0;
        printf("Device path:  %s\n", lpDevicePath);
    }

    if(!lpDevicePath)
    {
        //Error.
        printf("Error %d.", GetLastError());
        LocalFree(lpDevicePath);
        LocalFree(pInterfaceDetailData);
        bResult = SetupDiDestroyDeviceInfoList(hDeviceInfo);
        return bResult;
    }

    //Open the device
    *hDeviceHandle = CreateFile(lpDevicePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);

    if(*hDeviceHandle == INVALID_HANDLE_VALUE)
    {
        //Error.
        printf("Error %d.", GetLastError());
        LocalFree(lpDevicePath);
        LocalFree(pInterfaceDetailData);
        bResult = SetupDiDestroyDeviceInfoList(hDeviceInfo);
        return bResult;
    }

    LocalFree(lpDevicePath);
    LocalFree(pInterfaceDetailData);
    bResult = SetupDiDestroyDeviceInfoList(hDeviceInfo);
    return bResult;
}

BOOL GetWinUSBHandle(HANDLE hDeviceHandle, PWINUSB_INTERFACE_HANDLE phWinUSBHandle)
{
    if(hDeviceHandle == INVALID_HANDLE_VALUE)
        return FALSE;

    BOOL bResult = WinUsb_Initialize(hDeviceHandle, phWinUSBHandle);
    if(!bResult)
    {
        printf("WinUsb_Initialize Error %d.", GetLastError());
        return FALSE;
    }

    return bResult;
}

BOOL GetUSBDeviceSpeed(WINUSB_INTERFACE_HANDLE hDeviceHandle, UCHAR* pDeviceSpeed)
{
    if(!pDeviceSpeed || hDeviceHandle == INVALID_HANDLE_VALUE)
        return FALSE;

    BOOL bResult = TRUE;

    ULONG length = sizeof(UCHAR);

    bResult = WinUsb_QueryDeviceInformation(hDeviceHandle, DEVICE_SPEED, &length, pDeviceSpeed);
    if(!bResult)
    {
        printf("Error getting device speed: %d.\n", GetLastError());
        return bResult;
    }

    if(*pDeviceSpeed == LowSpeed)
    {
        printf("Device speed: %d (Low speed).\n", *pDeviceSpeed);
        return bResult;
    }
    if(*pDeviceSpeed == FullSpeed)
    {
        printf("Device speed: %d (Full speed).\n", *pDeviceSpeed);
        return bResult;
    }
    if(*pDeviceSpeed == HighSpeed)
    {
        printf("Device speed: %d (High speed).\n", *pDeviceSpeed);
        return bResult;
    }

    return bResult;
}

BOOL QueryDeviceEndpoints(WINUSB_INTERFACE_HANDLE hDeviceHandle, PIPE_ID* pipeid)
{
    if(hDeviceHandle == INVALID_HANDLE_VALUE)
        return FALSE;

    BOOL bResult = TRUE;

    USB_INTERFACE_DESCRIPTOR InterfaceDescriptor;
    ZeroMemory(&InterfaceDescriptor, sizeof(USB_INTERFACE_DESCRIPTOR));

    WINUSB_PIPE_INFORMATION Pipe;
    ZeroMemory(&Pipe, sizeof(WINUSB_PIPE_INFORMATION));

    bResult = WinUsb_QueryInterfaceSettings(hDeviceHandle, 0, &InterfaceDescriptor);

    if(bResult)
    {
        for(int index = 0; index < InterfaceDescriptor.bNumEndpoints; index++)
        {
            bResult = WinUsb_QueryPipe(hDeviceHandle, 0, index, &Pipe);

            if(bResult)
            {
                if(Pipe.PipeType == UsbdPipeTypeControl)
                {
                    printf("Endpoint index: %d Pipe type: Control Pipe ID: %d.\n", index, Pipe.PipeType, Pipe.PipeId);
                }
                if(Pipe.PipeType == UsbdPipeTypeIsochronous)
                {
                    printf("Endpoint index: %d Pipe type: Isochronous Pipe ID: %d.\n", index, Pipe.PipeType, Pipe.PipeId);
                }
                if(Pipe.PipeType == UsbdPipeTypeBulk)
                {
                    if(USB_ENDPOINT_DIRECTION_IN(Pipe.PipeId))
                    {
                        printf("Endpoint index: %d Pipe type: Bulk Pipe ID: %c.\n", index, Pipe.PipeType, Pipe.PipeId);
                        pipeid->PipeInId = Pipe.PipeId;
                    }
                    if(USB_ENDPOINT_DIRECTION_OUT(Pipe.PipeId))
                    {
                        printf("Endpoint index: %d Pipe type: Bulk Pipe ID: %c.\n", index, Pipe.PipeType, Pipe.PipeId);
                        pipeid->PipeOutId = Pipe.PipeId;
                    }

                }
                if(Pipe.PipeType == UsbdPipeTypeInterrupt)
                {
                    printf("Endpoint index: %d Pipe type: Interrupt Pipe ID: %d.\n", index, Pipe.PipeType, Pipe.PipeId);
                }
            }
            else
            {
                continue;
            }
        }
    }

    return bResult;
}

ULONG send_config(WINUSB_INTERFACE_HANDLE h, PIPE_ID p, PUCHAR bf)
{
    ULONG bytes;
    WinUsb_WritePipe(h, p.PipeOutId, bf, 64, &bytes, NULL);
    WinUsb_ReadPipe(h, p.PipeInId, bf, 64, &bytes, NULL);
    if(bf[0] != USBCONTROL_RESPONSE)
    {
        return -1;
//        char chbuf[32];
//        sprintf_s(chbuf, 32, "Error: 0x%02X", bf[0]);
//        QPushButton button(chbuf);
//        button.resize(250, 100);
//        button.show();
//        while(!button.isChecked());
//        exit(-1);
    }
    return bytes;
}
