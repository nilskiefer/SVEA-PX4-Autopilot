<script setup>
import { useRouter, withBase } from "vitepress"
import { VPButton } from "vitepress/theme"
import { computed, onBeforeUnmount, onMounted, ref, shallowRef, watch } from "vue"
import {
  backupParameters, CODEGRIP, FIRMWARE_SIZE_LIMIT, flashFirmware, inspectTarget, PARAMETER_BACKUP_SIZE,
  prepareFirmwareImage, restoreParameters, validateParameterBackup
} from "../flash/clicker4.mjs"

const releaseTagUrl = "https://github.com/nilskiefer/SVEA-PX4-Autopilot/releases/tag/"
const channels = [
  { key: "release", label: "Latest release", manifest: "release.json" },
  { key: "prerelease", label: "Latest prerelease", manifest: "manifest.json" }
]
const stageLabels = {
  downloading: "Downloading firmware…",
  preparing: "Preparing to flash…",
  preparingRestore: "Preparing to restore…",
  readingParameters: "Reading saved parameters…",
  checkingBackup: "Checking the parameter backup…",
  checkingInstalledFirmware: "Checking the installed firmware…",
  erasing: "Erasing flash…",
  programming: "Writing firmware…",
  verifying: "Verifying firmware…",
  resettingParameters: "Erasing saved parameters…",
  restoringParameters: "Writing saved parameters…",
  verifyingParameters: "Verifying saved parameters…",
  rebooting: "Rebooting the Clicker4…"
}
const startupLabels = {
  running: "Running",
  halted: "CPU halted",
  locked: "CPU lockup",
  unconfirmed: "Startup unconfirmed"
}
const interruptedMessages = {
  backup: "Backup interrupted. Reconnect CN2 and check the board again.",
  flashing: "Flash interrupted. Reconnect CN2 and check the board again.",
  restoring: "Restore interrupted. Reconnect CN2 and check the board again."
}

const router = useRouter()
const hid = shallowRef(null)
const device = shallowRef(null)
const board = shallowRef(null)
const connectionStatus = ref("Not connected")
const releases = shallowRef(null)
const channel = ref("prerelease")
const factoryReset = ref(false)
const firmwareInput = ref(null)
const customFirmware = shallowRef(null)
const firmwareFileError = ref("")
const backupInput = ref(null)
const backupFile = shallowRef(null)
const lastFlashedDigest = ref("")
const progress = shallowRef(null)
const browserError = ref("")
const releaseError = ref("")
const feedback = shallowRef(null)
const operation = ref("")
let firmwareDownload

const release = computed(() => releases.value?.[channel.value])
const customSelected = computed(() => channel.value === "custom")
const firmware = computed(() => customSelected.value ? customFirmware.value : release.value)
const installed = computed(() => board.value?.installed)
const loadingReleases = computed(() => !releases.value && !releaseError.value)
const writing = computed(() => operation.value === "flashing" || operation.value === "restoring")
const canCheck = computed(() => Boolean(hid.value && !operation.value &&
  (!loadingReleases.value || customSelected.value)))
const canBackup = computed(() => Boolean(device.value && board.value && canCheck.value))
const canChooseFirmware = computed(() => Boolean(hid.value && customSelected.value && !operation.value))
const canFlash = computed(() => Boolean(canBackup.value && firmware.value &&
  (lastFlashedDigest.value !== firmware.value.sha256 || factoryReset.value)))
const canChooseBackup = computed(() => canBackup.value && Boolean(installed.value))
const canRestore = computed(() => canChooseBackup.value && Boolean(backupFile.value))
const boardStatus = computed(() => {
  if (operation.value === "choosing") return "Choose CODEGRIP in the browser"
  if (operation.value === "checking") return "Checking the board…"
  if (device.value && writing.value) {
    if (progress.value?.stage === "rebooting") return "Rebooting…"
    return operation.value === "flashing" ? "Flashing…" : "Restoring…"
  }
  if (board.value) return startupLabels[board.value.state]
  return connectionStatus.value
})
const statusText = computed(() => {
  if (progress.value) return stageLabels[progress.value.stage]
  return feedback.value?.type === "error" ? "" : feedback.value?.text
})

watch(channel, () => {
  clearWriteChoices()
  customFirmware.value = null
  firmwareFileError.value = ""
})

onMounted(() => {
  if (!window.isSecureContext) {
    browserError.value = "Open this page over HTTPS to connect to CODEGRIP."
  } else if (typeof navigator.hid?.requestDevice !== "function") {
    browserError.value = "Open this page in desktop Chrome or Edge with WebHID enabled."
  } else {
    hid.value = navigator.hid
    hid.value.addEventListener("disconnect", handleDisconnect)
  }
  void loadReleases()
})

onBeforeUnmount(() => {
  firmwareDownload?.abort()
  hid.value?.removeEventListener("disconnect", handleDisconnect)
  hid.value = null
  device.value = null
})

async function loadReleases() {
  releaseError.value = ""
  try {
    const versions = await Promise.all(channels.map(async ({ key, label, manifest: file }) => {
      const response = await fetch(withBase(`/flash-firmware/${file}`), { cache: "no-cache" })
      if (!response.ok) {
        throw new Error(`${label} metadata request failed with HTTP ${response.status}.`)
      }
      const manifest = await response.json()
      const identity = manifest?.identity
      if (
        typeof manifest?.tag !== "string" || !manifest.tag.trim() ||
        manifest.releaseUrl !== releaseTagUrl + encodeURIComponent(manifest.tag) ||
        typeof manifest.sha256 !== "string" || !/^[a-f0-9]{64}$/.test(manifest.sha256) ||
        !Number.isInteger(manifest.size) || manifest.size < 8 || manifest.size > FIRMWARE_SIZE_LIMIT ||
        !Number.isInteger(identity?.offset) || identity.offset < 0 || identity.offset + 41 > manifest.size ||
        typeof identity.commit !== "string" || !/^[a-f0-9]{40}$/.test(identity.commit)
      ) {
        throw new Error(`${label} metadata is invalid.`)
      }
      return [key, manifest]
    }))
    releases.value = Object.fromEntries(versions)
    if (device.value && board.value && !board.value.installed && !customSelected.value) void checkBoard()
  } catch (error) {
    releaseError.value = `Could not load the firmware versions. ${error.message}`
  }
}

async function checkBoard(changeProbe = false) {
  if (!canCheck.value) return
  const api = hid.value
  let selected = device.value
  feedback.value = null
  operation.value = "checking"

  try {
    if (changeProbe || !selected) {
      const permitted = changeProbe ? [] : (await api.getDevices()).filter(probe =>
        probe.vendorId === CODEGRIP.vendorId && probe.productId === CODEGRIP.productId)
      if (hid.value !== api) return
      if (permitted.length === 1) {
        selected = permitted[0]
      } else {
        operation.value = "choosing"
        const chosen = await api.requestDevice({ filters: [CODEGRIP] })
        if (hid.value !== api) return
        if (chosen.length === 0) {
          if (!device.value) connectionStatus.value = "No probe selected"
          return
        }
        if (chosen.length !== 1) {
          throw new Error("The browser returned more than one CODEGRIP interface.")
        }
        selected = chosen[0]
      }
    }

    operation.value = "checking"
    device.value = selected
    board.value = null
    feedback.value = null
    lastFlashedDigest.value = ""
    clearWriteChoices()
    const releaseData = releases.value
    const snapshot = await inspectTarget(selected, Object.values(releaseData ?? {}))
    if (device.value === selected) board.value = { ...snapshot, releaseDataChecked: Boolean(releaseData) }
  } catch (error) {
    if (hid.value === api && (!selected || device.value === selected)) {
      if (!board.value) connectionStatus.value = device.value ? "Board check failed" : "Not connected"
      feedback.value = { type: "error", text: error.message }
    }
  } finally {
    operation.value = ""
  }
}

function clearWriteChoices() {
  factoryReset.value = false
  backupFile.value = null
}

function handleDisconnect({ device: disconnected }) {
  if (device.value !== disconnected) return
  firmwareDownload?.abort()
  device.value = null
  board.value = null
  connectionStatus.value = "CODEGRIP disconnected"
  lastFlashedDigest.value = ""
  clearWriteChoices()

  const interrupted = interruptedMessages[operation.value]
  if (interrupted && progress.value?.stage !== "rebooting") {
    feedback.value = { type: "error", text: interrupted }
  }
}

function warnBeforeUnload(event) {
  event.preventDefault()
  event.returnValue = true
}

async function loadFirmware(event) {
  const file = event.target.files[0]
  event.target.value = ""
  if (!file || !canChooseFirmware.value) return
  customFirmware.value = null
  firmwareFileError.value = ""
  clearWriteChoices()
  feedback.value = null
  operation.value = "loadingFirmware"

  try {
    if (!/\.bin$/i.test(file.name)) {
      throw new Error("Choose a Clicker4 firmware file with a .bin extension.")
    }
    if (file.size < 8 || file.size > FIRMWARE_SIZE_LIMIT) {
      throw new Error(`Choose a firmware file between 8 bytes and ${FIRMWARE_SIZE_LIMIT / 1024} KiB.`)
    }
    const image = new Uint8Array(await file.arrayBuffer())
    const sha256 = await prepareFirmwareImage(image)
    if (hid.value) customFirmware.value = { tag: file.name, sha256, size: image.byteLength, image }
  } catch (error) {
    if (hid.value) firmwareFileError.value = error.message
  } finally {
    operation.value = ""
  }
}

async function downloadBackup() {
  if (!canBackup.value) return
  const selected = device.value
  operation.value = "backup"
  feedback.value = null
  progress.value = { stage: "readingParameters" }

  try {
    const bytes = await backupParameters(selected, (update) => {
      if (device.value === selected) progress.value = update
    })
    if (device.value !== selected) return

    const url = URL.createObjectURL(new Blob([bytes], { type: "application/octet-stream" }))
    const link = document.createElement("a")
    const timestamp = new Date().toISOString().replace(/[:.]/g, "-")
    link.href = url
    link.download = `clicker4-parameters-${timestamp}.bin`
    document.body.append(link)
    link.click()
    link.remove()
    setTimeout(() => URL.revokeObjectURL(url), 10_000)
    feedback.value = { text: "Parameter backup download started." }
  } catch (error) {
    if (device.value === selected) feedback.value = { type: "error", text: `Backup failed. ${error.message}` }
  } finally {
    progress.value = null
    operation.value = ""
  }
}

async function loadBackup(event) {
  const file = event.target.files[0]
  event.target.value = ""
  if (!file || !canChooseBackup.value) return
  const selected = device.value
  clearWriteChoices()
  feedback.value = null
  operation.value = "loadingBackup"

  try {
    if (file.size !== PARAMETER_BACKUP_SIZE) {
      throw new Error("Choose a 256 KiB Clicker4 parameter backup.")
    }
    const bytes = new Uint8Array(await file.arrayBuffer())
    validateParameterBackup(bytes)
    if (device.value === selected) backupFile.value = { name: file.name, bytes }
  } catch (error) {
    if (device.value === selected) feedback.value = { type: "error", text: error.message }
  } finally {
    operation.value = ""
  }
}

function flash() {
  if (!canFlash.value) return
  void writeBoard({ kind: "flashing", manifest: firmware.value, eraseParameters: factoryReset.value })
}

function restore() {
  if (!canRestore.value) return
  void writeBoard({ kind: "restoring", manifest: installed.value, bytes: backupFile.value.bytes })
}

async function writeBoard({ kind, manifest, eraseParameters = false, bytes }) {
  // An earlier navigation can still be loading its page while this button is visible.
  if (window.location.pathname.replace(/\.html$/, "") !== router.route.path.replace(/\.html$/, "")) return
  const selected = device.value
  const flashing = kind === "flashing"
  const downloadImage = flashing && !manifest.image
  operation.value = kind
  feedback.value = null
  progress.value = { stage: downloadImage ? "downloading" : flashing ? "preparing" : "preparingRestore" }

  const previousRouteGuard = router.onBeforeRouteChange
  const previousPageGuard = router.onBeforePageLoad
  const blockNavigation = () => false
  const writeUrl = window.location.href
  const blockPageLoad = () => {
    // Browser Back and Forward change the URL before VitePress asks to load the page.
    if (window.location.href !== writeUrl) window.history.pushState({}, "", writeUrl)
    return false
  }
  router.onBeforeRouteChange = blockNavigation
  router.onBeforePageLoad = blockPageLoad

  let programmingStarted = false
  let verified = false
  let rebootConfirmed = false
  const onProgress = (update) => {
    if (update.stage === "rebooting") verified = true
    if (device.value !== selected) return
    if (update.stage === "rebooting") board.value = { installed: manifest, state: "unconfirmed" }
    progress.value = update
  }

  try {
    let image = manifest.image
    if (downloadImage) {
      firmwareDownload = new AbortController()
      const response = await fetch(withBase(`/flash-firmware/${manifest.sha256}.bin`), {
        signal: firmwareDownload.signal
      })
      if (!response.ok) {
        throw new Error(`Firmware download failed with HTTP ${response.status}.`)
      }
      image = new Uint8Array(await response.arrayBuffer())
      if (image.byteLength !== manifest.size) {
        throw new Error("The firmware download does not match the size in the manifest.")
      }
      firmwareDownload = undefined
    }
    if (device.value !== selected) return

    programmingStarted = true
    window.addEventListener("beforeunload", warnBeforeUnload)
    if (flashing) {
      progress.value = { stage: "preparing" }
      board.value = null
      await flashFirmware(selected, image, manifest.sha256, onProgress, { factoryReset: eraseParameters })
    } else {
      await restoreParameters(selected, bytes, manifest, onProgress)
    }
    verified = true
    rebootConfirmed = device.value === selected
  } catch (error) {
    if (!verified && device.value === selected) {
      if (programmingStarted) {
        board.value = null
        connectionStatus.value = "Check the board again"
      }
      feedback.value = { type: "error", text: `${flashing ? "Flashing" : "Restore"} failed. ${error.message}` }
    }
  } finally {
    if (verified) {
      if (device.value === selected) {
        board.value = { installed: manifest, state: rebootConfirmed ? "running" : "unconfirmed" }
        if (flashing) lastFlashedDigest.value = manifest.sha256
      }
      const result = flashing
        ? `Firmware ${manifest.tag} verified. ${eraseParameters ? "Saved parameters cleared." : "Saved parameters preserved."}`
        : "Parameters restored and verified."
      const checkStartup = device.value === selected
        ? "Select Check board to read the startup status again."
        : "Reconnect CN2 to check startup."
      feedback.value = rebootConfirmed
        ? { text: `${result} The Clicker4 has rebooted.` }
        : { type: "warning", text: `${result} Startup unconfirmed. ${checkStartup}` }
    }
    firmwareDownload = undefined
    if (programmingStarted) window.removeEventListener("beforeunload", warnBeforeUnload)
    if (router.onBeforeRouteChange === blockNavigation) {
      router.onBeforeRouteChange = previousRouteGuard
    }
    if (router.onBeforePageLoad === blockPageLoad) {
      router.onBeforePageLoad = previousPageGuard
    }
    progress.value = null
    clearWriteChoices()
    operation.value = ""
  }
}
</script>

<template>
  <div class="firmware-flasher">
    <section class="board-strip" aria-label="CODEGRIP connection">
      <dl class="board-summary" aria-live="polite">
        <div>
          <dt>Installed firmware</dt>
          <dd>
            <code v-if="installed">{{ installed.tag }}</code>
            <span v-else-if="device && operation === 'flashing'">Updating…</span>
            <span v-else-if="!board">Not checked</span>
            <span v-else-if="!board.releaseDataChecked">
              {{ releases ? "Check again to identify" : "Release data unavailable" }}
            </span>
            <span v-else>Unknown version</span>
          </dd>
        </div>
        <div>
          <dt>Board status</dt>
          <dd>{{ boardStatus }}</dd>
        </div>
      </dl>
      <div class="board-actions">
        <VPButton :text="device ? 'Check board' : 'Connect CODEGRIP'" type="button"
          :theme="device ? 'alt' : 'brand'" :disabled="!canCheck" @click="checkBoard()" />
        <VPButton v-if="device" text="Change probe" type="button" theme="alt"
          :disabled="!canCheck" @click="checkBoard(true)" />
      </div>
      <p v-if="browserError" class="error" role="alert">{{ browserError }}</p>
    </section>

    <div class="flasher-sections">
      <section class="flasher-section" aria-labelledby="firmware-heading">
        <h2 id="firmware-heading">Firmware</h2>
        <fieldset class="firmware-versions" aria-label="Firmware version" :disabled="Boolean(operation)">
          <template v-if="releases">
            <label v-for="option in channels" :key="option.key" class="firmware-choice">
              <input v-model="channel" type="radio" name="firmware-version" :value="option.key" />
              <span class="version-label">{{ option.label }} <code>{{ releases[option.key].tag }}</code></span>
            </label>
          </template>
          <label class="firmware-choice">
            <input v-model="channel" type="radio" name="firmware-version" value="custom" />
            <span>Custom <code>.bin</code></span>
          </label>
        </fieldset>
        <p v-if="release">
          <a :href="release.releaseUrl" target="_blank" rel="noopener noreferrer">Release notes</a>
        </p>
        <p v-else-if="loadingReleases" class="help" role="status">Loading firmware versions…</p>
        <div v-else-if="releaseError" class="release-error">
          <p class="error" role="alert">{{ releaseError }}</p>
          <VPButton text="Retry loading firmware" theme="alt" type="button"
            :disabled="Boolean(operation)" @click="loadReleases" />
        </div>
        <div v-if="customSelected" class="firmware-upload">
          <input id="custom-firmware-file" ref="firmwareInput" type="file" accept=".bin" hidden
            @change="loadFirmware" />
          <VPButton :text="customFirmware ? 'Choose another firmware file' : 'Choose firmware file'"
            :theme="board && !customFirmware && !backupFile ? 'brand' : 'alt'" type="button"
            :disabled="!canChooseFirmware" @click="firmwareInput.click()" />
          <p v-if="customFirmware" class="selected-file" role="status">Selected file <code>{{ customFirmware.tag }}</code></p>
          <p v-else-if="operation === 'loadingFirmware'" class="help" role="status">Checking the firmware file…</p>
          <p v-else-if="firmwareFileError" class="error" role="alert">{{ firmwareFileError }}</p>
        </div>

        <div v-if="releases || customSelected" class="factory-reset">
          <label class="reset-choice">
            <input v-model="factoryReset" type="checkbox" :disabled="Boolean(operation) || !firmware"
              aria-describedby="factory-reset-help" />
            <span>Factory reset parameters when flashing</span>
          </label>
          <p id="factory-reset-help" class="help">Erases saved settings and calibration after firmware verification.</p>
        </div>
        <div v-if="releases || customSelected">
          <VPButton :text="factoryReset ? 'Flash and erase parameters' : 'Flash firmware'" type="button"
            :theme="canFlash && !backupFile ? 'brand' : 'alt'" :disabled="!canFlash" @click="flash" />
        </div>
      </section>

      <section class="flasher-section parameters-section" aria-labelledby="parameters-heading">
        <h2 id="parameters-heading">Parameters</h2>
        <div>
          <VPButton text="Back up parameters" type="button" theme="alt" :disabled="!canBackup"
            @click="downloadBackup" />
        </div>
        <div class="restore-tools">
          <p id="restore-help" class="help">
            Restore a 256 KiB <code>.bin</code> backup from this board and firmware version.
            <span v-if="board && !installed"> Restore requires a recognized firmware version.</span>
          </p>
          <input id="parameter-backup-file" ref="backupInput" type="file" accept=".bin" hidden @change="loadBackup" />
          <div>
            <VPButton :text="backupFile ? 'Choose another file' : 'Choose backup file'" type="button" theme="alt"
              :disabled="!canChooseBackup" aria-describedby="restore-help" @click="backupInput.click()" />
          </div>
          <template v-if="backupFile">
            <p class="selected-file">Selected file <code>{{ backupFile.name }}</code></p>
            <p id="restore-confirmation" class="help">Restore replaces the saved settings and calibration.</p>
            <div>
              <VPButton text="Restore parameters" type="button" theme="brand" :disabled="!canRestore"
                aria-describedby="restore-confirmation" @click="restore" />
            </div>
          </template>
        </div>
      </section>
    </div>

    <div class="operation-status" :class="{ 'has-status': progress || feedback }">
      <p id="flash-progress-label" role="status" :class="{ warning: feedback?.type === 'warning' }">{{ statusText }}</p>
      <p v-if="feedback?.type === 'error'" class="error" role="alert">{{ feedback.text }}</p>
      <template v-if="progress">
        <progress aria-labelledby="flash-progress-label" :max="progress.total || 1"
          :value="progress.total ? progress.completed : undefined"></progress>
        <p v-if="writing" class="help">Keep this page open and CN2 connected until the operation finishes.</p>
      </template>
    </div>
  </div>
</template>

<style scoped>
.firmware-flasher {
  margin: 24px 0;
  container-type: inline-size;
  overflow-wrap: anywhere;
}

.firmware-flasher p {
  margin: 0;
  line-height: 1.5;
}

.firmware-flasher :is(button, input, a):focus-visible {
  outline: 2px solid var(--vp-c-brand-1);
  outline-offset: 2px;
}

.firmware-flasher button:disabled {
  cursor: not-allowed;
  opacity: 0.55;
}

.board-strip {
  display: grid;
  align-items: center;
  gap: 12px 24px;
  padding: 16px;
  border: 1px solid var(--vp-c-divider);
  border-radius: 8px;
  background: var(--vp-c-bg-soft);
}

.board-summary {
  display: grid;
  grid-template-columns: minmax(0, 2fr) minmax(0, 1fr);
  gap: 12px;
  margin: 0;
}

.board-summary dt {
  color: var(--vp-c-text-2);
  font-size: 0.875rem;
  line-height: 1.5;
}

.board-summary dd {
  margin: 4px 0 0;
  font-weight: 600;
  line-height: 1.5;
}

.board-actions {
  display: flex;
  flex-wrap: wrap;
  gap: 8px;
}

.board-strip > .error {
  grid-column: 1 / -1;
}

.flasher-sections {
  margin-top: 24px;
}

.flasher-section {
  display: grid;
  align-content: start;
  gap: 12px;
  min-width: 0;
}

.flasher-section h2 {
  margin: 0;
  padding: 0;
  border: 0;
  font-size: 1.25rem;
  line-height: 1.4;
}

.parameters-section {
  margin-top: 24px;
  padding-top: 24px;
  border-top: 1px solid var(--vp-c-divider);
}

.firmware-versions {
  display: grid;
  gap: 8px;
  min-width: 0;
  margin: 0;
  padding: 0;
  border: 0;
}

.firmware-choice,
.reset-choice {
  display: flex;
  align-items: flex-start;
  gap: 12px;
  cursor: pointer;
  font-size: 0.875rem;
  line-height: 1.5;
}

.firmware-choice {
  padding: 12px;
  border: 1px solid var(--vp-c-divider);
  border-radius: 8px;
}

.firmware-choice:hover {
  background: var(--vp-c-bg-soft);
}

.firmware-choice:has(input:checked) {
  border-color: var(--vp-c-brand-1);
  background: var(--vp-c-brand-soft);
}

.firmware-choice:has(input:disabled),
.reset-choice:has(input:disabled) {
  cursor: default;
}

:is(.firmware-choice, .reset-choice) input {
  flex: none;
  margin-top: 4px;
  accent-color: var(--vp-c-brand-1);
}

.version-label {
  display: flex;
  flex: 1;
  flex-wrap: wrap;
  align-items: baseline;
  justify-content: space-between;
  gap: 4px 12px;
  min-width: 0;
}

.factory-reset {
  padding-top: 12px;
  border-top: 1px solid var(--vp-c-divider);
}

.reset-choice {
  font-weight: 600;
}

.firmware-flasher .help {
  color: var(--vp-c-text-2);
  font-size: 0.875rem;
}

.factory-reset .help {
  margin-top: 4px;
  margin-left: 25px;
}

.firmware-upload,
.release-error,
.restore-tools {
  display: grid;
  justify-items: start;
  gap: 8px;
}

.restore-tools {
  margin-top: 4px;
}

.selected-file {
  font-size: 0.875rem;
  font-weight: 600;
}

.error {
  color: var(--vp-c-danger-1);
}

.warning {
  color: var(--vp-c-warning-1);
}

.operation-status.has-status {
  margin-top: 24px;
  padding: 12px 16px;
  border: 1px solid var(--vp-c-divider);
  border-radius: 8px;
  background: var(--vp-c-bg-soft);
}

#flash-progress-label {
  font-weight: 600;
}

progress {
  display: block;
  width: 100%;
  margin: 8px 0;
  accent-color: var(--vp-c-brand-1);
}

@container (min-width: 42rem) {
  .board-strip {
    grid-template-columns: minmax(0, 1fr) auto;
  }

  .board-summary {
    grid-template-columns: minmax(0, 1.2fr) minmax(0, 1fr);
  }

  .flasher-sections {
    display: grid;
    grid-template-columns: minmax(0, 1.3fr) minmax(0, 1fr);
    gap: 24px;
  }

  .parameters-section {
    margin: 0;
    padding: 0 0 0 24px;
    border-top: 0;
    border-left: 1px solid var(--vp-c-divider);
  }
}
</style>
