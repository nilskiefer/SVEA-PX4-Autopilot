import { createHash } from "node:crypto";
import { mkdir, writeFile } from "node:fs/promises";

const repository = "nilskiefer/SVEA-PX4-Autopilot";
const prereleaseTag = /^v\d+\.\d+\.\d+-\d+\.\d+\.\d+-(?:rc|beta|alpha)\d+$/;
const clicker4Image = /^mikroe_clicker4-stm32f7_noboot-[a-f0-9]{12}\.bin$/;
const maxImageSize = 0xc0000; // 768 KiB of application sectors, before the saved parameters.
const output = new URL("./dist/flash-firmware/", import.meta.url);

const headers = { Accept: "application/vnd.github+json" };
if (process.env.GITHUB_TOKEN) {
  headers.Authorization = `Bearer ${process.env.GITHUB_TOKEN}`;
}

const prereleases = [];
for (let page = 1; ; page++) {
  const url = `https://api.github.com/repos/${repository}/releases?per_page=100&page=${page}`;
  const response = await fetch(url, { headers, signal: AbortSignal.timeout(30_000) });
  if (!response.ok) {
    throw new Error(`GitHub release lookup failed with HTTP ${response.status}.`);
  }
  const releases = await response.json();
  prereleases.push(...releases.filter((release) =>
    release.prerelease && !release.draft && release.published_at && prereleaseTag.test(release.tag_name)
  ));
  if (releases.length < 100) break;
}

prereleases.sort((a, b) => b.published_at.localeCompare(a.published_at));
const [release] = prereleases;
if (!release) {
  throw new Error("No versioned SVEA prerelease is published.");
}

const images = release.assets.filter((asset) => asset.state === "uploaded" && clicker4Image.test(asset.name));
if (images.length !== 1) {
  throw new Error(`Expected exactly one Clicker4 noboot binary in ${release.tag_name}.`);
}
const [image] = images;
if (image.size < 8 || image.size > maxImageSize || !/^sha256:[a-f0-9]{64}$/.test(image.digest)) {
  throw new Error(`Invalid firmware size or missing GitHub SHA-256 digest in ${release.tag_name}.`);
}

const response = await fetch(image.browser_download_url, { signal: AbortSignal.timeout(60_000) });
if (!response.ok) {
  throw new Error(`GitHub firmware download failed with HTTP ${response.status}.`);
}
const bytes = Buffer.from(await response.arrayBuffer());
const sha256 = createHash("sha256").update(bytes).digest("hex");
if (bytes.length !== image.size || `sha256:${sha256}` !== image.digest) {
  throw new Error("Downloaded Clicker4 firmware does not match the GitHub release digest.");
}

await mkdir(output, { recursive: true });
await writeFile(new URL(`${sha256}.bin`, output), bytes);
await writeFile(new URL("manifest.json", output), JSON.stringify({
  tag: release.tag_name,
  releaseUrl: release.html_url,
  sha256,
  size: bytes.length,
}, null, 2) + "\n");
console.log(`Staged ${release.tag_name} firmware for GitHub Pages.`);
