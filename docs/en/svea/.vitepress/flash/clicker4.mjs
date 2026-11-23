import dapjs from "dapjs/dist/dap.umd.js";
import { LOADER } from "./stm32f745-loader.mjs";
import { CmsisDapHidTransport } from "./webhid.mjs";

export const CODEGRIP = { vendorId: 0x2dbc, productId: 0x1924 };

const FLASH_BASE = 0x08000000;
const PARAMETER_SECTOR = 0x080c0000;
const PARAMETER_SECTOR_SIZE = 0x40000;
const SECTORS = [0x00000, 0x08000, 0x10000, 0x18000, 0x20000, 0x40000, 0x80000];
const PAGE_SIZE = 1024;
const FLASH_OPTIONS = 0x40023c14;
const DBGMCU_IDCODE = 0xe0042000;
const DBGMCU_APB1_FZ = 0xe0042008;
const FLASH_SIZE_WORD = 0x1ff0f440;

const VTOR = 0xe000ed08;
const AIRCR = 0xe000ed0c;
const DFSR = 0xe000ed30;
const DHCSR = 0xe000edf0;
const DEMCR = 0xe000edfc;
const DEBUG_KEY = 0xa05f0000;
const DEBUG_ENABLE = 1;
const HALT = 2;
const S_HALT = 1 << 17;
const S_LOCKUP = 1 << 19;
const S_RESET = 1 << 25;
const RESET_REQUEST = 0x05fa0004;
const VECTOR_CATCH_RESET = 1;
const DFSR_BREAKPOINT = 1 << 1;
const DFSR_VECTOR_CATCH = 1 << 3;
const DFSR_CLEAR = 0x1f;

// Use SRAM1 while the application is halted at reset.
const BREAKPOINT = 0x20020000;
const LOADER_BASE = BREAKPOINT + 4;
const BUFFER = 0x20021000;
const STACK = 0x20024000;
const INIT = 0x22;
const UNINIT = 0x50;
const ERASE_SECTOR = 0x8a;
const PROGRAM_PAGE = 0xda;
const ERASE_MODE = 1;
const PROGRAM_MODE = 2;

const SP = 13;
const LR = 14;
const PC = 15;
const XPSR = 16;
const CONTROL_MASKS = 20;

async function withTarget(device, action) {
  if (device.vendorId !== CODEGRIP.vendorId || device.productId !== CODEGRIP.productId) {
    throw new Error("Choose the Clicker4 CODEGRIP probe connected through CN2.");
  }
  return navigator.locks.request("svea-clicker4-codegrip", { ifAvailable: true }, async (lock) => {
    if (!lock) {
      throw new Error("The flasher is already using CODEGRIP in another tab.");
    }
    const transport = new CmsisDapHidTransport(device);
    const core = new dapjs.CortexM(transport, 1, 1_000_000);
    try {
      transport.connectDeadline = performance.now() + 6000;
      await core.connect();
      transport.connectDeadline = Infinity;
      const deviceId = await core.readMem32(DBGMCU_IDCODE);
      const flashKiB = (await core.readMem32(FLASH_SIZE_WORD)) >>> 16;
      if ((deviceId & 0xfff) !== 0x449 || flashKiB !== 1024) {
        throw new Error("CODEGRIP did not identify the expected 1 MiB STM32F7 target.");
      }
      const result = await action(core);
      await core.disconnect();
      return result;
    } finally {
      await transport.close();
    }
  });
}

export async function inspectTarget(device) {
  await withTarget(device, async () => {});
}

async function readParameterSector(core, stage, onProgress) {
  const sector = new Uint8Array(PARAMETER_SECTOR_SIZE);
  onProgress({ stage, completed: 0, total: sector.length });
  // CSW auto-increment can wrap at 1 KiB, so each read starts at its own page.
  for (let offset = 0; offset < sector.length; offset += PAGE_SIZE) {
    const words = await core.readBlock(PARAMETER_SECTOR + offset, PAGE_SIZE / 4);
    sector.set(new Uint8Array(words.buffer, words.byteOffset, words.byteLength), offset);
    onProgress({ stage, completed: offset + PAGE_SIZE, total: sector.length });
  }
  return sector;
}

export async function backupParameters(device, onProgress) {
  return withTarget(device, async (core) => {
    const snapshot = await readParameterSector(core, "readingParameters", onProgress);
    const confirmed = await readParameterSector(core, "checkingBackup", onProgress);
    if (snapshot.some((value, index) => value !== confirmed[index])) {
      throw new Error("Saved parameters changed during the backup. Let the board finish saving and try again.");
    }
    return snapshot;
  });
}

async function verifyImage(image, expectedSha256) {
  if (image.length < 8 || image.length > PARAMETER_SECTOR - FLASH_BASE) {
    throw new Error("The firmware size is outside the Clicker4 application flash area.");
  }
  const hash = new Uint8Array(await crypto.subtle.digest("SHA-256", image));
  const sha256 = Array.from(hash, (byte) => byte.toString(16).padStart(2, "0")).join("");
  if (sha256 !== expectedSha256) {
    throw new Error("The firmware SHA-256 checksum does not match the published release.");
  }

  const vector = new DataView(image.buffer, image.byteOffset, image.byteLength);
  const stack = vector.getUint32(0, true) & ~3;
  const reset = vector.getUint32(4, true);
  if (stack <= 0x20000000 || stack > 0x20050000 || !(reset & 1) ||
      reset < FLASH_BASE || (reset & ~1) >= FLASH_BASE + image.length ||
      !new TextDecoder().decode(image).includes("MIKROE_CLICKER4_STM32F7\0")) {
    throw new Error("The firmware image does not match the Clicker4 startup layout.");
  }
}

async function waitForCore(check, timeout, error) {
  const deadline = performance.now() + timeout;
  do {
    if (await check()) return;
    await new Promise((resolve) => setTimeout(resolve, 10));
  } while (performance.now() < deadline);
  throw new Error(error);
}

async function resetTarget(core, halt) {
  await core.writeMem32(DEMCR, halt ? VECTOR_CATCH_RESET : 0);
  await core.writeMem32(DFSR, DFSR_CLEAR);
  await core.readMem32(DHCSR);
  await core.writeMem32(AIRCR, RESET_REQUEST);
  let resetSeen = false;
  await waitForCore(async () => {
    const status = await core.readMem32(DHCSR);
    if (status & S_LOCKUP) return false;
    if (halt) {
      resetSeen ||= Boolean(status & S_RESET);
      return resetSeen && Boolean(status & S_HALT) && Boolean((await core.readMem32(DFSR)) & DFSR_VECTOR_CATCH);
    }
    // VTOR resets to zero. NuttX sets it to the noboot application's flash base during startup.
    return !(status & S_HALT) && (await core.readMem32(VTOR)) === FLASH_BASE;
  }, halt ? 2000 : 5000, halt ? "The STM32 did not halt at reset." : "The browser could not confirm that the STM32 restarted.");
}

async function prepareLoader(core) {
  const watchdogs = await core.readMem32(DBGMCU_APB1_FZ);
  await core.writeMem32(DBGMCU_APB1_FZ, watchdogs | 0x1800);
  await core.writeMem32(DHCSR, DEBUG_KEY | DEBUG_ENABLE | HALT);
  await resetTarget(core, true);
  await core.writeMem32(DEMCR, 0);

  const code = new Uint8Array(4 + LOADER.length);
  code.set([0x00, 0xbe], 0); // Thumb BKPT, where each flash function returns.
  code.set(LOADER, 4);
  await core.writeBlock(BREAKPOINT, new Uint32Array(code.buffer));
  await core.writeCoreRegister(CONTROL_MASKS, 1); // Privileged Thread mode, MSP, IRQs masked.
  await core.writeCoreRegister(SP, STACK);
  await core.writeCoreRegister(9, LOADER_BASE + 0x164);
}

async function callLoader(core, entry, args, timeout = 1000) {
  for (const [register, value] of args.entries()) {
    await core.writeCoreRegister(register, value);
  }
  await core.writeCoreRegister(LR, BREAKPOINT | 1);
  await core.writeCoreRegister(XPSR, 0x01000000);
  await core.writeCoreRegister(PC, (LOADER_BASE + entry) | 1);
  await core.writeMem32(DFSR, DFSR_CLEAR);
  await core.writeMem32(DHCSR, DEBUG_KEY | DEBUG_ENABLE);
  await waitForCore(() => core.isHalted(), timeout, "The STM32 flash loader timed out.");

  const pc = await core.readCoreRegister(PC);
  const reason = await core.readMem32(DFSR);
  if ((pc & ~1) !== BREAKPOINT || !(reason & DFSR_BREAKPOINT) || await core.readCoreRegister(0) !== 0) {
    throw new Error("The STM32 flash loader did not return successfully.");
  }
}

export async function flashFirmware(device, image, expectedSha256, onProgress, { factoryReset = false } = {}) {
  await verifyImage(image, expectedSha256);
  await withTarget(device, async (core) => {
    const sectors = SECTORS.filter((offset) => offset < image.length);
    const options = await core.readMem32(FLASH_OPTIONS);
    const appMask = (1 << sectors.length) - 1;
    const resetMask = factoryReset ? 1 << SECTORS.length : 0;
    const requiredWritable = (appMask | resetMask) << 16;
    if ((options & 0xff00) !== 0xaa00 || (options & requiredWritable) !== requiredWritable) {
      throw new Error("The STM32 flash is protected. All selected sectors must be writable.");
    }

    await prepareLoader(core);
    const savedParameters = factoryReset
      ? new Uint8Array(PARAMETER_SECTOR_SIZE).fill(0xff)
      : await readParameterSector(core, "readingParameters", onProgress);
    await callLoader(core, INIT, [FLASH_BASE, 16_000_000, ERASE_MODE]);
    onProgress({ stage: "erasing", completed: 0, total: sectors.length });
    for (const [index, offset] of sectors.entries()) {
      await callLoader(core, ERASE_SECTOR, [FLASH_BASE + offset], 6000);
      onProgress({ stage: "erasing", completed: index + 1, total: sectors.length });
    }
    await callLoader(core, UNINIT, [ERASE_MODE]);

    await callLoader(core, INIT, [FLASH_BASE, 16_000_000, PROGRAM_MODE]);
    onProgress({ stage: "programming", completed: 0, total: image.length });
    for (let offset = 0; offset < image.length; offset += PAGE_SIZE) {
      const page = image.subarray(offset, offset + PAGE_SIZE);
      const padded = new Uint8Array((page.length + 3) & ~3).fill(0xff);
      padded.set(page);
      await core.writeBlock(BUFFER, new Uint32Array(padded.buffer));
      await callLoader(core, PROGRAM_PAGE, [FLASH_BASE + offset, padded.length, BUFFER]);
      onProgress({ stage: "programming", completed: offset + page.length, total: image.length });
    }
    await callLoader(core, UNINIT, [PROGRAM_MODE]);

    onProgress({ stage: "verifying", completed: 0, total: image.length });
    for (let offset = 0; offset < image.length; offset += PAGE_SIZE) {
      const page = image.subarray(offset, offset + PAGE_SIZE);
      const words = await core.readBlock(FLASH_BASE + offset, Math.ceil(page.length / 4));
      const bytes = new Uint8Array(words.buffer, words.byteOffset, page.length);
      const mismatch = bytes.findIndex((value, index) => value !== page[index]);
      if (mismatch !== -1) {
        const address = (FLASH_BASE + offset + mismatch).toString(16);
        throw new Error(`Firmware read-back differed at 0x${address}.`);
      }
      onProgress({ stage: "verifying", completed: offset + page.length, total: image.length });
    }

    if (factoryReset) {
      onProgress({ stage: "resettingParameters", completed: 0, total: 1 });
      await callLoader(core, INIT, [FLASH_BASE, 16_000_000, ERASE_MODE]);
      await callLoader(core, ERASE_SECTOR, [PARAMETER_SECTOR], 6000);
      await callLoader(core, UNINIT, [ERASE_MODE]);
    }
    const verifiedParameters = await readParameterSector(core, "verifyingParameters", onProgress);
    if (verifiedParameters.some((value, index) => value !== savedParameters[index])) {
      throw new Error(factoryReset
        ? "The parameter sector did not erase completely."
        : "The saved-parameter sector changed during the flash.");
    }

    onProgress({ stage: "rebooting", completed: 0, total: 1 });
    await resetTarget(core, false);
  });
}
