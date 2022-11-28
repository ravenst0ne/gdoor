# Welcome to GDOOR
GDOOR is a project to collect and document findings about the Gira Door System.
Gira is a german manufacturer of house appliances, including door systems.
These systems are connected via a proprietary two wire bus system.

Contributions are welcome!

# Protocol
## Hardware Layer
The bus is formed by two wires and is single ended, meaning one of the wire carries
power and data and the other wire is ground.

To power the bus devices, a central device called controller powers the bus with ~26 Vdc.
Bus commands and analog data (voice and video!) are modulated onto this 26V.
The exact electrical scheme for the modulations is not known, but capacitive coupling the signals
on the 26 V seems to work.

Digital bus commands indicate the start and end of analog audio transmission,
each participating bus device decides on its own to:
- send audio on the bus
- receive audio from the bus

The outdoor station can be commanded to send audio to the bus, the indoor stations
will do this only if a user pressed the "call accept" button.

## Digital Signaling
![Example of Bus voltage](https://raw.githubusercontent.com/gdoor-org/gdoor/main/doc/busvoltage.png)

The digital bus signal is based on a non standardized modulation scheme, similar to OOK, but not quite OOK.
A ~ +-2V, 60 kHz sine carrier is turned on and off, where the duration of the carrier indicates a one or zero bit value. These pulses (aka bits) are separated with a fixed length pause (with no carrier signal).
So it is similar to OOK, but the bit period depends on the bit value.

All data transmission starts with a start bit which contains ~60 sine periods. A one is signaled by ~12 sine periods and a zero is signaled by ~32 sine periods.

## Bits and Bytes
![Bit stream order](https://raw.githubusercontent.com/gdoor-org/gdoor/main/doc/wavedrom-bitstream.png)

Data transmission is LSB first and each Byte has an additional 9th odd parity bit.
The last byte in the transmission is kind of a checksum, literally the sum of all previous bytes.

## Bus protocol / Frame
![Byte Frame](https://raw.githubusercontent.com/gdoor-org/gdoor/main/doc/wavedrom-byteframe.png)
The exact meaning of the bytes is unknown, but certain clues can be made by observing the bus and sending
to the bus and observing device behavior:

| Byte          | Description   |
| ------------- | ------------- |
| ?Length?      | Maybe frame length. Content is 0x01 if no destination fields in frame, otherwise 0x02  |
| ?Status?      | Unknown, fixed values for different commands  |
| Action        | The real command, like "open door", "accept call" etc.  |
| Source        | 3 Byte value with device bus address, unique per device!  |
| Parameter     | 2 Byte value which e.g. specifies pressed button  |
| Device Type   | Fixed value for each hardware device type  |
| Destination   | 3 Byte value with device bus address, unique per device!  |

### ?Length?
| Byte-Value    | Description   |
| ------------- | ------------- |
| 0x01          | Frame with 9 Bytes, no destination bytes |
| 0x02          | Frame with 12 Bytes, incl. destination bytes |

### ?Status?
| Byte-Value    | Description   |
| ------------- | ------------- |
| 0x00          | Often in combination with ?Length? = 0x02 |
| 0x10          | Often in combination with ?Length? = 0x01 |

### Action
| Byte-Value    | Description   |
| ------------- | ------------- |
| 0x00          | Programming mode - Stop|
| 0x01          | Programming mode - Start|
| 0x02          | Door opener programming - Stop|
| 0x03          | Door opener programming - Start|
| 0x04          | Learn doorbell button|
| 0x05          | Confirm learned doorbell button|
| 0x08          | Reset device configuration (announcement)|
| 0x0F          | Confirm learned door opener|
| 0x11          | Door bell button pressed - which button is send in ?Param?|
| 0x13          | Floor bell button pressed|
| 0x20          | Close call|
| 0x21          | Open call|
| 0x31          | Open door|
| 0x41          | Generic button pressed|
| 0x42          | Unlearned button pressed|


