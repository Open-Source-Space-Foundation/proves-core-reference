# WSL (Windows 11) Notes
WSL natively does not have access to USB devices. To flash the board properly and allow the GDS to communicate with the deployment board via UART over USB, follow these one-time steps.

1. On Powershell as administrator, install and run **usbipd-win**:
Run Powershell as administrator
```sh
winget install dorssel.usbipd-win
```

2. On Powershell find your USB device:
```sh
usbipd list
```
> [!Note]
> You should see two STM32 devices listed with different BUSID's. One corresponds to the USB PWR and the other to USER USB.

3. On Powershell bind both devices to USBIPD in two separate commands (as administrator):
```sh
usbipd bind --busid <BUSID_USB_PWR> --wsl
```

```sh
usbipd bind --busid <BUSID_USER_USB> --wsl
```

5. On WSL confirm visibility of USB device:
```sh
ls /dev/ttyACM*
```
> [!Note]
> On WSL, the device will usually appear as /dev/ttyACM0. You can check using ls /dev/ttyACM*
