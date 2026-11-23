import CRC32 from "crc-32";

// Sector 7 begins at 0x080c0000. Clicker4 noboot maps flashfs to its upper 128 KiB.
export const PARAMETER_BACKUP_SIZE = 0x40000;
export const PARAMETER_FILE_OFFSET = 0x20000;
const FLASHFS_MAGIC = 0xaa553cc3;
const PARAMETER_TOKEN = 0x6d726170; // "parm" in little-endian order.
const HEADER_SIZE = 16;
const CRC_START = 10;
const BSON_VALUE_SIZE = { 0x01: 8, 0x10: 4 }; // Double and int32 are PX4's saved parameter types.

function validateDocument(data, view, start, end) {
  const length = end - start;
  const terminator = end - 1;
  if (length < 5 || data[terminator] !== 0) {
    throw new Error("The backup contains an invalid PX4 parameter document.");
  }
  const bsonLength = view.getUint32(start, true);
  // The PX4 buffer encoder can leave the BSON length word at zero.
  if (bsonLength !== 0 && bsonLength !== length) {
    throw new Error("The PX4 parameter document length is invalid.");
  }
  for (let cursor = start + 4; cursor < terminator;) {
    const valueSize = BSON_VALUE_SIZE[data[cursor]];
    const keyEnd = data.indexOf(0, cursor + 1);
    const next = keyEnd + 1 + valueSize;
    if (!valueSize || keyEnd <= cursor + 1 || next > terminator) {
      throw new Error("The backup contains an invalid PX4 parameter entry.");
    }
    cursor = next;
  }
}

export function validateParameterBackup(bytes) {
  if (!(bytes instanceof Uint8Array) || bytes.length !== PARAMETER_BACKUP_SIZE) {
    throw new Error("Select a 256 KiB Clicker4 parameter backup.");
  }

  const data = bytes.subarray(PARAMETER_FILE_OFFSET);
  const view = new DataView(data.buffer, data.byteOffset, data.byteLength);
  // Match flashfs.c's search and its final-word boundary.
  const limit = data.length - 4;
  for (let offset = 0; offset + HEADER_SIZE <= limit;) {
    if (view.getUint32(offset, true) !== FLASHFS_MAGIC) {
      offset += 4;
      continue;
    }

    const flags = view.getUint16(offset + 4, true);
    const size = view.getUint16(offset + CRC_START, true);
    const end = offset + size;
    if (size < HEADER_SIZE || size % 4 || end > limit) {
      break;
    }

    const active = (flags & 0xfffc) === 0xa5ac && view.getUint32(offset + 12, true) === PARAMETER_TOKEN;
    // PX4 starts the reflected CRC with zero and uses no final XOR.
    const crc = (CRC32.buf(data.subarray(offset + CRC_START, end), -1) ^ -1) >>> 0;
    if (crc !== view.getUint32(offset + 6, true)) {
      if (active) throw new Error("The saved PX4 parameter record has an invalid checksum.");
      offset += 4;
      continue;
    }

    if (active) {
      validateDocument(data, view, offset + HEADER_SIZE, end - (flags & 3));
      return;
    }

    offset = end;
    if (offset + HEADER_SIZE <= limit &&
        view.getUint32(offset, true) === 0xffffffff && view.getUint16(offset + 4, true) === 0xffff) {
      break;
    }
  }
  throw new Error("The backup has no readable saved PX4 parameter record.");
}
