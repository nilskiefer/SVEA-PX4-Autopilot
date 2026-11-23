<script setup>
import { useRouter, withBase } from "vitepress"
import { VPButton } from "vitepress/theme"
import { computed, onBeforeUnmount, onMounted, ref, shallowRef, watch } from "vue"
import { backupParameters, CODEGRIP, flashFirmware, inspectTarget } from "../flash/clicker4.mjs"

const releaseTagUrl = "https://github.com/nilskiefer/SVEA-PX4-Autopilot/releases/tag/"
const firmwareSizeLimit = 0xc0000
const channels = [
  { key: "release", label: "Latest release", manifest: "release.json" },
  { key: "prerelease", label: "Latest prerelease", manifest: "manifest.json" }
]
const stageLabels = {
  downloading: "Downloading firmware…",
  preparing: "Preparing to flash…",
  readingParameters: "Reading saved parameters…",
  checkingBackup: "Checking the parameter backup…",
  erasing: "Erasing flash…",
  programming: "Writing firmware…",
  verifying: "Verifying firmware…",
  resettingParameters: "Erasing saved parameters…",
  verifyingParameters: "Verifying the parameter sector…",
  rebooting: "Rebooting the Clicker4…"
}

const router = useRouter()
const hid = shallowRef(null)
const device = shallowRef(null)
const releases = shallowRef(null)
const channel = ref("prerelease")
const factoryReset = ref(false)
const progress = shallowRef(null)
const browserError = ref("")
const releaseError = ref("")
const operationError = ref("")
const operation = ref("")
const boardStatus = ref("No board checked.")
const boardChecked = ref(false)
const flashResult = shallowRef(null)
const backupMessage = ref("")
let firmwareDownload

const release = computed(() => releases.value?.[channel.value])
const complete = computed(() => Boolean(flashResult.value && flashResult.value.tag === release.value?.tag &&
  !factoryReset.value))
const canConnect = computed(() => Boolean(hid.value && !operation.value))
const canBackup = computed(() => Boolean(device.value && boardChecked.value && !operation.value))
const canFlash = computed(() => Boolean(canBackup.value && release.value && !complete.value))
const flashStatus = computed(() => {
  if (progress.value) return stageLabels[progress.value.stage]
  if (operationError.value) return ""
  if (backupMessage.value) return backupMessage.value
  if (complete.value) {
    return flashResult.value.factoryReset
      ? "Firmware verified. Saved parameters cleared. The Clicker4 has rebooted."
      : "Firmware verified. Saved parameters matched before reboot. The Clicker4 has rebooted."
  }
  return ""
})

watch(channel, () => {
  factoryReset.value = false
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
      if (
        typeof manifest?.tag !== "string" || !manifest.tag.trim() ||
        manifest.releaseUrl !== releaseTagUrl + encodeURIComponent(manifest.tag) ||
        typeof manifest.sha256 !== "string" || !/^[a-f0-9]{64}$/.test(manifest.sha256) ||
        !Number.isInteger(manifest.size) || manifest.size < 8 || manifest.size > firmwareSizeLimit
      ) {
        throw new Error(`${label} metadata is invalid.`)
      }
      return [key, manifest]
    }))
    releases.value = Object.fromEntries(versions)
  } catch (error) {
    releaseError.value = `Could not load the firmware versions. ${error.message}`
  }
}

async function connectCodegrip() {
  if (!canConnect.value) return
  operation.value = "connecting"
  device.value = null
  boardChecked.value = false
  factoryReset.value = false
  flashResult.value = null
  backupMessage.value = ""
  operationError.value = ""
  boardStatus.value = "Choose CODEGRIP in the browser’s USB device chooser."

  try {
    const devices = await hid.value.requestDevice({ filters: [CODEGRIP] })
    if (!hid.value) return
    if (devices.length === 0) {
      boardStatus.value = "No CODEGRIP selected. Select Connect CODEGRIP to try again."
      return
    }
    if (devices.length !== 1) {
      throw new Error("The browser returned more than one CODEGRIP interface.")
    }

    const selected = devices[0]
    device.value = selected
    boardStatus.value = "Checking the board…"
    await inspectTarget(selected)
    if (device.value === selected) {
      boardChecked.value = true
      boardStatus.value = "STM32F745/F75x detected with 1 MiB of flash."
    }
  } catch (error) {
    if (!operationError.value) {
      device.value = null
      boardStatus.value = "Board check failed."
      operationError.value = `${error.message} Select Connect CODEGRIP to try again.`
    }
  } finally {
    operation.value = ""
  }
}

function handleDisconnect({ device: disconnected }) {
  if (device.value !== disconnected) return
  firmwareDownload?.abort()
  device.value = null
  boardChecked.value = false
  boardStatus.value = "CODEGRIP disconnected."
  if (operation.value === "backup") {
    operationError.value = "The backup was interrupted. Reconnect CN2 and check the board again."
  } else if (progress.value?.stage === "rebooting") {
    operationError.value = unconfirmedRestart(factoryReset.value)
  } else if (!complete.value) {
    operationError.value = "Reconnect CN2 and select Connect CODEGRIP to check the board again."
  }
  factoryReset.value = false
}

function unconfirmedRestart(erased) {
  const parameters = erased ? "Saved parameters were erased." : "Saved parameters matched before reboot."
  return `Firmware written and verified. ${parameters} The browser could not confirm the reboot. Check runtime USB through CN1.`
}

function warnBeforeUnload(event) {
  event.preventDefault()
  event.returnValue = true
}

async function downloadBackup() {
  if (!canBackup.value) return
  const selected = device.value
  operation.value = "backup"
  operationError.value = ""
  backupMessage.value = ""
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
    backupMessage.value = "Parameter backup download started."
  } catch (error) {
    if (device.value === selected) operationError.value = `Backup failed. ${error.message}`
  } finally {
    progress.value = null
    operation.value = ""
  }
}

async function flash() {
  if (!canFlash.value) return
  // An earlier navigation can still be loading its page while this button is visible.
  if (window.location.pathname.replace(/\.html$/, "") !== router.route.path.replace(/\.html$/, "")) return
  const selected = device.value
  const manifest = release.value
  const eraseParameters = factoryReset.value
  operation.value = "flashing"
  flashResult.value = null
  operationError.value = ""
  backupMessage.value = ""
  progress.value = { stage: "downloading" }
  firmwareDownload = new AbortController()
  const previousRouteGuard = router.onBeforeRouteChange
  const previousPageGuard = router.onBeforePageLoad
  const blockNavigation = () => false
  const flashUrl = window.location.href
  const blockPageLoad = () => {
    // Browser Back and Forward change the URL before VitePress asks to load the page.
    if (window.location.href !== flashUrl) window.history.pushState({}, "", flashUrl)
    return false
  }
  let programmingStarted = false
  router.onBeforeRouteChange = blockNavigation
  router.onBeforePageLoad = blockPageLoad

  try {
    const response = await fetch(withBase(`/flash-firmware/${manifest.sha256}.bin`), {
      signal: firmwareDownload.signal
    })
    if (!response.ok) {
      throw new Error(`Firmware download failed with HTTP ${response.status}.`)
    }
    const image = new Uint8Array(await response.arrayBuffer())
    if (image.byteLength !== manifest.size) {
      throw new Error("The firmware download does not match the size in the manifest.")
    }
    if (device.value !== selected) return

    firmwareDownload = undefined
    programmingStarted = true
    progress.value = { stage: "preparing" }
    window.addEventListener("beforeunload", warnBeforeUnload)
    await flashFirmware(selected, image, manifest.sha256, (update) => {
      if (device.value === selected) progress.value = update
    }, { factoryReset: eraseParameters })
    if (device.value === selected) flashResult.value = { tag: manifest.tag, factoryReset: eraseParameters }
  } catch (error) {
    if (device.value === selected) {
      if (programmingStarted) {
        boardChecked.value = false
        boardStatus.value = "Check the board again before flashing."
      }
      operationError.value = progress.value?.stage === "rebooting"
        ? unconfirmedRestart(eraseParameters)
        : `Flashing failed. ${error.message}`
    }
  } finally {
    firmwareDownload = undefined
    if (programmingStarted) window.removeEventListener("beforeunload", warnBeforeUnload)
    if (router.onBeforeRouteChange === blockNavigation) {
      router.onBeforeRouteChange = previousRouteGuard
    }
    if (router.onBeforePageLoad === blockPageLoad) {
      router.onBeforePageLoad = previousPageGuard
    }
    progress.value = null
    factoryReset.value = false
    operation.value = ""
  }
}
</script>

<template>
  <div class="firmware-flasher">
    <fieldset v-if="releases" class="firmware-versions" :disabled="Boolean(operation)">
      <legend>Firmware version</legend>
      <label v-for="option in channels" :key="option.key" class="firmware-choice">
        <input v-model="channel" type="radio" name="firmware-version" :value="option.key" />
        <span>{{ option.label }} <code>{{ releases[option.key].tag }}</code></span>
      </label>
    </fieldset>
    <p v-if="release">
      <a :href="release.releaseUrl" target="_blank" rel="noopener noreferrer">Release notes</a>
    </p>
    <p v-else-if="!releaseError" role="status">Loading firmware versions…</p>
    <div v-if="releaseError">
      <p class="error" role="alert">{{ releaseError }}</p>
      <VPButton class="retry" text="Retry loading firmware" theme="alt" type="button"
        @click="loadReleases" />
    </div>

    <p v-if="browserError" class="error" role="alert">{{ browserError }}</p>
    <VPButton text="Connect CODEGRIP" type="button" :theme="canFlash ? 'alt' : 'brand'"
      :disabled="!canConnect" @click="connectCodegrip" />
    <p role="status"><strong>Board</strong> · {{ boardStatus }}</p>

    <VPButton text="Back up parameters" type="button" theme="alt" :disabled="!canBackup"
      @click="downloadBackup" />
    <div>
      <label class="firmware-choice">
        <input v-model="factoryReset" type="checkbox" :disabled="Boolean(operation)"
          aria-describedby="factory-reset-help" />
        <span>Factory reset parameters when flashing</span>
      </label>
      <p id="factory-reset-help">Erases saved PX4 settings and calibration. Back them up first to keep a copy.</p>
    </div>
    <VPButton :text="factoryReset ? 'Flash and erase parameters' : 'Flash firmware'" type="button"
      :theme="canFlash ? 'brand' : 'alt'"
      :disabled="!canFlash" @click="flash" />
    <p v-if="operationError" class="error" role="alert">{{ operationError }}</p>
    <div v-if="flashStatus" class="flash-status">
      <p id="flash-progress-label" role="status">{{ flashStatus }}</p>
      <template v-if="progress">
        <progress aria-labelledby="flash-progress-label" :max="progress.total || 1"
          :value="progress.total ? progress.completed : undefined"></progress>
        <p v-if="operation === 'flashing'">Keep this page open and CN2 connected until the Clicker4 reboots.</p>
      </template>
    </div>
  </div>
</template>

<style scoped>
.firmware-flasher {
  display: grid;
  justify-items: start;
  gap: 12px;
  margin: 24px 0;
  padding: 16px;
  border: 1px solid var(--vp-c-divider);
  border-radius: 8px;
  background: var(--vp-c-bg-soft);
  overflow-wrap: anywhere;
}

.firmware-flasher p {
  margin: 0;
}

.firmware-flasher :is(button, input):focus-visible {
  outline: 2px solid var(--vp-c-brand-1);
  outline-offset: 2px;
}

.firmware-flasher button:disabled {
  cursor: not-allowed;
  opacity: 0.55;
}

.firmware-flasher .retry {
  margin-top: 8px;
}

.firmware-versions {
  display: grid;
  gap: 8px;
  min-width: 0;
  padding: 0;
  border: 0;
}

.firmware-versions legend {
  margin-bottom: 8px;
  font-weight: 600;
}

.firmware-choice {
  display: flex;
  align-items: baseline;
  gap: 8px;
}

.firmware-choice input {
  flex: none;
  accent-color: var(--vp-c-brand-1);
}

.error {
  color: var(--vp-c-danger-1);
}

.flash-status {
  width: 100%;
}

progress {
  display: block;
  width: 100%;
  margin: 8px 0;
  accent-color: var(--vp-c-brand-1);
}
</style>
