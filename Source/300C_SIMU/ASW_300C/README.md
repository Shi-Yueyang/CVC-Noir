# PDA (Persistent Data Area) API Guide

This document provides the API reference for PDA data storage services on the CVC-300C platform.

## Overview

PDA (Persistent Data Area) provides persistent storage for application data across system restarts. The platform provides four types of PDA data with different access patterns and storage media.

## PDA Data Types

| Type | Single System | Dual System | Storage Media | Size per Core |
|------|---------------|-------------|---------------|---------------|
| 1st (NRNW) | Not readable/writable | Readable/writable | NVRAM + Dataplug | 1KB |
| 2nd (ARAW) | Readable/writable | Readable/writable | Dataplug | 1KB |
| 3rd (Mass) | Readable/writable | Readable/writable | Flash | 7 areas × 15MB |
| 4th (ARNW) | Readable only | Readable/writable | Dataplug or NVRAM | 1KB |

## Storage Media Values

| Media | Value |
|-------|-------|
| Dataplug + NVRAM | 1 |
| Dataplug only | 2 |
| NVRAM only | 3 |

## Operation States

```c
#define CVC_PDA_NO_OPERATION      0U  // Ready for new operation
#define CVC_PDA_OPERATION_SUCCEED 1U  // Operation completed successfully
#define CVC_PDA_OPERATION_PENDING 2U  // Operation in progress
#define CVC_PDA_OPERATION_FAILED    3U  // Operation failed
#define CVC_PDA_NO_AVAILABLE        4U  // Three consecutive failures
```

## Data Structures

### Message Header

```c
typedef struct {
    INT8U  AreaIdex;   // Area index: mass data [0-6]; vital: 7,9; nvital: 8,10
    INT32U datasize;   // Data size in bytes
    INT32U crc;        // CRC32 with polynomial 0x04C11DB7
} PDA_T_PdaMsgHeader;
```

## First Type Data API (NRNW)

### ReadNRNWData

Read first type PDA data (NVRAM/Non-Readable-Non-Writable for single system).

```c
CVC_PDA_STATE_t API_ReadNRNWData(
    void*   opData,        // [out] Data buffer
    INT16U  iSize,         // [in] Buffer size (max 1KB)
    INT32U* opActualSize,  // [out] Actual data size read
    INT8U   media          // [in] Storage medium (fixed: 1)
);
```

**Returns:** `CVC_PDA_OPERATION_SUCCEED`, `CVC_PDA_OPERATION_PENDING`, `CVC_PDA_OPERATION_FAILED`, `CVC_PDA_NO_AVAILABLE`

**Notes:**
- Check `API_GetNRNWStatus()` returns `CVC_PDA_NO_OPERATION` before reading
- Poll every cycle until status is `SUCCEED` or `FAILED`
- If `iSize < opActualSize`, returns `FAILED`
- Invalid `opData` address causes platform shutdown

### WriteNRNWData

Write first type PDA data.

```c
CVC_PDA_STATE_t API_WriteNRNWData(
    void*  ipSource,  // [in] Data buffer pointer
    INT16U iSize,     // [in] Data size (max 1KB)
    INT8U  media      // [in] Storage medium (fixed: 1)
);
```

**Returns:** `CVC_PDA_OPERATION_PENDING`, `CVC_PDA_OPERATION_FAILED`, `CVC_PDA_NO_AVAILABLE`

**Notes:**
- Check `API_GetNRNWStatus()` returns `CVC_PDA_NO_OPERATION` before writing
- Poll status after write using `API_GetNRNWStatus()`
- If `iSize > 1KB`, returns `FAILED`
- Invalid `ipSource` address causes platform shutdown

### GetNRNWStatus

Get status of first type PDA read/write operations.

```c
CVC_PDA_STATE_t API_GetNRNWStatus(INT8U media);  // [in] Storage medium (fixed: 1)
```

**Returns:**
- `CVC_PDA_NO_OPERATION` - Ready for new operation (previous completed/failed/none)
- `CVC_PDA_OPERATION_SUCCEED` - Last operation succeeded
- `CVC_PDA_OPERATION_PENDING` - Operation in progress
- `CVC_PDA_OPERATION_FAILED` - Last operation failed
- `CVC_PDA_NO_AVAILABLE` - Three consecutive read/write failures

## Second Type Data API (ARAW)

### ReadARAWData

Read second type PDA data (Always Readable Always Writable).

```c
CVC_PDA_STATE_t API_ReadARAWData(
    void*   opData,        // [out] Data buffer
    INT16U  iSize,         // [in] Buffer size (max 1KB)
    INT32U* opActualSize,  // [out] Actual data size read
    INT8U   media          // [in] Storage medium (1, 2, or 3)
);
```

**Returns:** Same as `API_ReadNRNWData`

**Notes:**
- Similar behavior to `API_ReadNRNWData` with media parameter support

### WriteARAWData

Write second type PDA data.

```c
CVC_PDA_STATE_t API_WriteARAWData(
    void*  ipSource,  // [in] Data buffer pointer
    INT16U iSize,     // [in] Data size (max 1KB)
    INT8U  media      // [in] Storage medium (1, 2, or 3)
);
```

**Returns:** Same as `API_WriteNRNWData`

### GetARAWStatus

Get status of second type PDA operations.

```c
CVC_PDA_STATE_t API_GetARAWStatus(INT8U media);  // [in] Storage medium (1, 2, or 3)
```

## Third Type Data API (Mass Data)

### ReadPDAMass

Read third type PDA data (mass data from Flash).

```c
CVC_PDA_STATE_t API_ReadPDAMass(
    INT8U   iFileIndex,       // [in] Flash area index: 0-6 (each area is 15MB)
    INT8U*  opBufferAddress,  // [out] Data address for this index
    INT32U* opLength          // [out] Buffer size for this area
);
```

**Returns:** `CVC_PDA_OPERATION_SUCCEED`, `CVC_PDA_OPERATION_PENDING`, `CVC_PDA_OPERATION_FAILED`, `CVC_PDA_NO_AVAILABLE`

**Notes:**
- R500 and R501 must not use the same flash simultaneously
- Invalid `iFileIndex` (>6) or invalid addresses cause `FAILED` or platform shutdown
- If flash area is empty, returns `FAILED`
- Data consistency guaranteed within dual CPUs or between dual MPUs

### WritePDAMass

Write third type PDA data to Flash.

```c
CVC_PDA_STATE_t API_WritePDAMass(
    INT8U  iFileIndex,      // [in] Flash area index: 0-6
    INT8U* ipBufferAddress  // [in] Data to write
);
```

**Returns:** `CVC_PDA_OPERATION_PENDING`, `CVC_PDA_OPERATION_FAILED`, `CVC_PDA_NO_AVAILABLE`

**Notes:**
- Must check `API_GetPDAMassStatus()` returns `CVC_PDA_NO_OPERATION` before writing
- Poll status after write using `API_GetPDAMassStatus()`
- Invalid `iFileIndex` (>6) or invalid address causes `FAILED` or platform shutdown
- Flash full causes `FAILED`

### GetPDAMassStatus

Get status of third type PDA operations.

```c
CVC_PDA_STATE_t API_GetPDAMassStatus(INT8U iFileIndex);  // [in] Flash area index: 0-6
```

## Fourth Type Data API (ARNW)

### ReadARNWData

Read fourth type PDA data (Always Readable Not Writable for single system).

```c
CVC_PDA_STATE_t API_ReadARNWData(
    void*   opData,        // [out] Data buffer
    INT16U  iSize,         // [in] Buffer size (max 1KB)
    INT32U* opActualSize,  // [out] Actual data size read
    INT8U   media          // [in] Storage medium (2 or 3)
);
```

**Returns:** Same as `API_ReadNRNWData`

**Notes:**
- Check `API_GetARNWStatus()` returns `CVC_PDA_NO_OPERATION` before reading
- Poll every cycle until status is `SUCCEED` or `FAILED`

### WriteARNWData

Write fourth type PDA data.

```c
CVC_PDA_STATE_t API_WriteARNWData(
    void*  ipSource,  // [in] Data buffer pointer
    INT16U iSize,     // [in] Data size (max 1KB)
    INT8U  media      // [in] Storage medium (2 or 3)
);
```

**Returns:** Same as `API_WriteNRNWData`

**Notes:**
- Check `API_GetARNWStatus()` returns `CVC_PDA_NO_OPERATION` before writing
- Poll status after write using `API_GetARNWStatus()`

### GetARNWStatus

Get status of fourth type PDA operations.

```c
CVC_PDA_STATE_t API_GetARNWStatus(INT8U media);  // [in] Storage medium (2 or 3)
```

## Time Service API

### ReadUTCTime

Get current UTC time.

```c
CVC_T_Status API_ReadUTCTime(INT32U* const opValue);  // [out] Current time in seconds
```

**Returns:**
- `CVC_C_NO_ERROR` (0) - Success
- `CVC_C_ERROR` (201) - Error

## Shutdown Services API

### WriteAppShutdownMsg

Store application error information during system downtime to Flash.

```c
CVC_T_Status API_WriteAppShutdownMsg(
    void*   ipData,     // [in] Downtime data buffer
    INT32U  iDataSize   // [in] Data size (max 1MB per application core)
);
```

## Usage Patterns

### Read Pattern

```c
// 1. Check status before reading
if (API_GetNRNWStatus(1) == CVC_PDA_NO_OPERATION) {
    // 2. Start read operation
    API_ReadNRNWData(buffer, sizeof(buffer), &actualSize, 1);
}

// 3. Poll every cycle until complete
CVC_PDA_STATE_t status = API_GetNRNWStatus(1);
if (status == CVC_PDA_OPERATION_SUCCEED) {
    // Read completed successfully, data is in buffer
} else if (status == CVC_PDA_OPERATION_FAILED) {
    // Handle read failure
}
```

### Write Pattern

```c
// 1. Check status before writing
if (API_GetNRNWStatus(1) == CVC_PDA_NO_OPERATION) {
    // 2. Start write operation
    API_WriteNRNWData(data, dataSize, 1);
}

// 3. Poll every cycle until complete
CVC_PDA_STATE_t status = API_GetNRNWStatus(1);
if (status == CVC_PDA_OPERATION_SUCCEED) {
    // Write completed successfully
} else if (status == CVC_PDA_OPERATION_FAILED) {
    // Handle write failure
}
```

## Caller/Implementer Matrix

| API | Callers | Implementer |
|-----|---------|-------------|
| All PDA Read/Write APIs | ASW1, ASW2 | BSW |
| ReadUTCTime | ASW1, ASW2 | BSW |
| WriteAppShutdownMsg | ASW1, ASW2 | BSW |

## Dataplug Memory Layout

| Address Range | Content |
|---------------|---------|
| 0-1855 | Dataplug configuration (platform reserved) |
| 1856-2891 | 1st type PDA data, R50-ASW |
| 2912-3947 | 1st type PDA data, R51-ASW |
| 3968-5003 | 2nd type PDA data, R50-ASW |
| 5024-6059 | 2nd type PDA data, R51-ASW |
| 6080-7115 | 4th type PDA data, R50-ASW |
| 7136-8171 | 4th type PDA data, R51-ASW |

Each 1KB data block structure:
- 12 bytes: Header (`AreaIdex`, `datasize`, `crc`)
- 1024 bytes: Data
- 20 bytes: Reserved

## Error Handling

- **CRC Error:** Data is erased, operation returns `FAILED`
- **Inconsistency:** Dual CPU inconsistency or dual MPU inconsistency causes data erasure and `FAILED`
- **Invalid Address:** Invalid buffer addresses cause platform shutdown (safety-critical)
- **Size Violation:** Exceeding max size (1KB for types 1/2/4, 15MB for type 3) returns `FAILED`
- **Consecutive Failures:** Three consecutive failures on same operation returns `NO_AVAILABLE`

## See Also

- `pda.h` - Internal PDA structures and platform functions
- `interface_p2a.h` - Platform-to-application interface definitions
