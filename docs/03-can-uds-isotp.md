# 03 — Controller Area Network (CAN) / Controller Area Network Flexible Data-Rate (CAN FD) / ISO 15765-2 Transport Protocol (ISO-TP) / Unified Diagnostic Services (UDS) at byte level

## Layering: four separate questions

1. **Controller Area Network (CAN) physical and data link** (ISO 11898): which node wins arbitration, how bits are sent, frames checked/acknowledged and errors recovered?
2. **Signal packing** (application/AUTOSAR Communication module (Com), often documented by a DBC): how does a speed value become fields of one Controller Area Network (CAN) frame?
3. **ISO 15765-2 Transport Protocol (ISO-TP)** (ISO 15765-2): how do diagnostic PDUs longer than a single Controller Area Network (CAN) frame traverse Controller Area Network (CAN) as multiple segments with flow control?
4. **Unified Diagnostic Services (UDS)** (ISO 14229): which diagnostic service/request/response semantics are carried by that transport?

Controller Area Network (CAN) is **not** Unified Diagnostic Services (UDS). Unified Diagnostic Services (UDS) is not limited to Controller Area Network (CAN) (e.g. DoIP over IP/Ethernet). AUTOSAR CAN Transport Layer (CanTp) implements the transport function; Diagnostic Communication Manager (Dcm) handles diagnostics; CAN Interface (CanIf) abstracts Controller Area Network (CAN) driver details.

## Classical Controller Area Network (CAN) versus Controller Area Network Flexible Data-Rate (CAN FD)

| Property | Classical Controller Area Network (CAN) | Controller Area Network Flexible Data-Rate (CAN FD) |
| --- | --- | --- |
| Payload per frame | 0–8 bytes | 0–64 bytes |
| Controller Area Network (CAN) ID | 11-bit standard or 29-bit extended | Same |
| Nominal arbitration phase | commonly 500 kbit/s | commonly 500 kbit/s |
| Data phase | same bit rate as arbitration | optional faster data phase with Bit Rate Switch (BRS) |
| FD payload byte lengths | — | 0..8, 12, 16, 20, 24, 32, 48, 64 (Data Length Code (DLC) mapping) |
| Host Application Programming Interface (API) | Linux `struct can_frame` | Linux `struct canfd_frame` |

FD can transmit Classical frames if configured. An **FD frame on a bus with Classical-only controllers** can cause protocol errors; do not enable FD traffic until all active nodes can participate.

At 500 kbit/s nominal and 2 Mbit/s data phase, 1 data bit nominally takes 2 microseconds and 1 data-phase bit 0.5 microseconds. Actual frame time depends on arbitration bits, stuffing, Cyclic Redundancy Check (CRC), Acknowledgment (ACK), intermission and the Bit Rate Switch (BRS) boundary; estimating simply payload_bits/2M is insufficient.

### Differential transceivers matter

Microcontroller Unit (MCU) Controller Area Network (CAN) Transmit (TX)/Receive (RX) are **logic pins**. The transceiver drives CAN_H/CAN_L; the two bus ends typically each have 120-ohm termination, creating about 60 ohms between CAN_H and CAN_L with power off. All nodes require compatible electrical levels and shared reference ground. Do not connect Microcontroller Unit (MCU) General-Purpose Input/Output (GPIO) directly to the differential pair.

## Decode an actual diagnostic request, not just a protocol name

Run `bash tools/run-all.sh`. The diagnostic tester prints `REQ: 10 03` and `RES: 50 03 00 32 01 F4`. On a Classical CAN frame using 11-bit normal addressing the transport bytes are:

| Byte or field | Full name | Meaning in this example |
| --- | --- | --- |
| CAN identifier `0x7E0` | Controller Area Network (CAN) identifier | Tester-to-Electronic Control Unit (ECU) arbitration identifier; not an authentication token |
| `02` | Single Frame (SF) Protocol Control Information (PCI) | The diagnostic request has two bytes of Unified Diagnostic Services (UDS) payload |
| `10` | Unified Diagnostic Services (UDS) Service Identifier (SID) | DiagnosticSessionControl |
| `03` | Session subfunction | Request extended session |
| Reply `06` | Single Frame (SF) payload length | Six Unified Diagnostic Services (UDS) reply bytes follow |
| Reply `50` | Positive response Service Identifier (SID) | `0x10 + 0x40`, indicating the positive response to `0x10` |
| Reply `00 32 01 F4` | Session timing parameters | P2=50 ms and P2*=5000 ms for the example's encoded diagnostic-session response |

For a 20-byte VIN response, a First Frame (FF) announces the total payload length, a **Flow Control frame (FC)** from the tester grants permission to continue, and **Consecutive Frames (CFs)** carry the remaining bytes. **Block Size (BS)** limits how many Consecutive Frames can be sent before another Flow Control frame; **Minimum Separation Time (STmin)** limits how closely consecutive frames may be sent. The code in `src/core/lab_cantp.c` implements only a teaching subset: it deliberately omits many production transport states, error checks and incoming multi-frame-request handling.

The distinction is mechanical: **CAN transports frames**, **ISO 15765-2 Transport Protocol (ISO-TP) segments long diagnostic messages**, and **Unified Diagnostic Services (UDS) assigns meaning to diagnostic request/response bytes**. Successfully sending one CAN frame does not prove the diagnostic service or its multi-frame transport works.


## 0x201 VehicleSpeed signal: concrete encoding

`config/network.json` defines:
- ID 0x201; every 100 ms.
- Bytes 0–1: speed in cm/s (16-bit unsigned, little endian).
- Byte 2: rolling alive counter.
- Byte 3: active diagnostic session (1 or 3).
- Bytes 4..7: zero/reserved; optional FD format is 16 bytes.

At speed 50 cm/s (0.5 m/s), alive=0 and default session=1:

~~~text
ID=0x201 payload [0x32,0x00,0x00,0x01,0x00,0x00,0x00,0x00]
~~~

At speed 2000 cm/s, bytes 0,1 are 0xD0,0x07 because 2000 decimal = 0x07D0. This is signal serialization, not Controller Area Network (CAN) arbitration.

## ISO 15765-2 Transport Protocol (ISO-TP) framing on Classical Controller Area Network (CAN) (normal 11-bit addressing)

**Single Frame (SF):** first byte Protocol Control Information (PCI) 0x0L, where L is Unified Diagnostic Services (UDS) length up to 7.

~~~text
Tester → ECU 0x7E0:
  02 10 03 00 00 00 00 00
  │  │  └─ extended session
  │  └─ UDS 0x10 DiagnosticSessionControl
  └─ 2-byte UDS request length

ECU → Tester 0x7E8:
  06 50 03 00 32 01 F4 00
     └─ positive response SID = request SID + 0x40
~~~

**Negative response:** a Unified Diagnostic Services (UDS) response starting with 0x7F, followed by original Service Identifier (SID) and one-byte Negative Response Code (NRC).

~~~text
Tester: 03 22 F1 99 00 00 00 00   (unsupported DID)
ECU:    03 7F 22 31 00 00 00 00   (request out of range)
~~~

**Multi-frame example: VIN (Data Identifier (DID) F190), 20-byte Unified Diagnostic Services (UDS) payload.**

~~~text
Request (Single Frame):
  03 22 F1 90 00 00 00 00

Response starts with First Frame (FF), PCI 0x1xxx:
  10 14 62 F1 90 4C 41 42
     │  └─ total UDS payload length = 0x014 = 20 bytes
     └─ 0x1 = FF type

Tester must send Flow Control (FC):
  30 01 00 00 00 00 00 00
     │  └─ STmin = 0ms
     └─ BS = 1, so ECU must pause after each CF
ECU sends CF with sequence 1:
  21 xx xx xx xx xx xx xx
Tester sends FC again
ECU sends CF with sequence 2:
  22 xx xx xx xx xx xx xx
~~~

The real bytes, including ASCII characters of the dummy VIN, appear in the `tools/uds.py` output. The two consecutive frames contain 14 remaining bytes, and padding bytes at the end of the last frame are ignored according to the reassembled Unified Diagnostic Services (UDS) length.

**Implemented limitations:** The local tutorial handles only single-frame diagnostic *requests* and <=64-byte diagnostic *responses*, with limited Flow Control frame (FC) Block Size (BS)/Minimum Separation Time (STmin) handling and a basic 1000 ms sender timeout. It does not include robust multi-frame Receive (RX) reassembly, addressing modes, padding/flow-control policy profiles, full ISO 15765-2 Transport Protocol (ISO-TP) conformance tests, Unified Diagnostic Services (UDS) security access or flashing services. Use production-proven CAN Transport Layer (CanTp) and Diagnostic Communication Manager (Dcm) instead in real applications.

## AUTOSAR communication module routes

Signal (non-diagnostic) example:

~~~text
SWC → RTE → Com → PduR → CanIf → Can(MCAL) → physical bus
~~~

Diagnostic exchange:

~~~text
Tester → Can(MCAL) → CanIf → CanTp → PduR → Dcm
Tester ← Can(MCAL) ← CanIf ← CanTp ← PduR ← Dcm
~~~

In a full stack these paths involve configured Protocol Data Unit (PDU) IDs, buffer semantics, notifications and state management. Our code merges some routing into `lab_pdur_canif_transmit`; the path is illustrative rather than complete.

## Exercises

1. Work out the 0x201 payload for 1234 cm/s: decimal 1234 = 0x04D2, so first bytes D2 04.
2. Send Data Identifier (DID) F187 and explain why its 7-byte positive Unified Diagnostic Services (UDS) response fits one Classical Single Frame (SF).
3. Send Data Identifier (DID) F190. Why must Electronic Control Unit (ECU) stop after First Frame (FF) until tester sends Flow Control frame (FC)?
4. Compare Block Size (BS)=0 versus Block Size (BS)=1 in `tools/uds.py`; observe how the extra Flow Control frame (FC) is issued in the Block Size (BS)=1 case.
5. Explain why a frame ID 0x7E0 does not itself authenticate the tester.

References: https://docs.kernel.org/networking/can.html ; https://www.autosar.org/standards/classic-platform/ ; ISO standards are authoritative for exact wire behavior.
