// TERRAIN for Digitone II: builds the custom OS in the browser from the owner's own Elektron OS 1.12 file.
// The patch (patch/*.json) holds only our own bytes; everything else is copied from the file you load.
// Loading, rebuilding and verifying the .syx container is done by dnfw's code (./dnfw, AGPL-3.0).
// AGPL-3.0. Derived from digikit by Em D - https://github.com/ElekLabs/digikit and dn2_firmware_explore by angellinares.
import * as fw from "./dnfw/firmware.js";

export async function sha256(bytes) {
  const h = await crypto.subtle.digest("SHA-256", bytes);
  return [...new Uint8Array(h)].map((b) => b.toString(16).padStart(2, "0")).join("");
}

function fromB64(s) {
  const bin = atob(s);
  const u = new Uint8Array(bin.length);
  for (let i = 0; i < bin.length; i++) u[i] = bin.charCodeAt(i);
  return u;
}

function applyOps(orig, sec) {
  const out = new Uint8Array(sec.length);
  let p = 0;
  for (const op of sec.ops) {
    if (op[0] === "c") {
      if (op[1] + op[2] > orig.length) throw new Error("patch: copy past the end of the original section");
      out.set(orig.subarray(op[1], op[1] + op[2]), p); p += op[2];
    } else {
      const l = fromB64(op[1]); out.set(l, p); p += l.length;
    }
  }
  if (p !== sec.length) throw new Error(`patch: built ${p} bytes, expected ${sec.length}`);
  return out;
}

/** raw: Uint8Array of the owner's .syx; patch: the parsed JSON; log(msg): progress. -> { syx, sha } */
export async function buildFirmware(raw, patch, log = () => {}) {
  const fileSha = await sha256(raw);
  log(`Your file: SHA-256 ${fileSha.slice(0, 16)}…`);
  const firmware = await fw.load(raw);
  const inputChecks = await fw.verify(firmware);
  const bad = inputChecks.filter((c) => !c.ok);
  if (bad.length) throw new Error("This file does not pass Elektron's own checks: " + bad.map((c) => c.name).join(", "));
  for (const [id, expected] of Object.entries(patch.input_sections)) {
    const s = firmware.container.find(Number(id));
    const c = s && s.unpack();
    if (!c || (await sha256(c)) !== expected)
      throw new Error("This is not the Elektron Digitone II OS 1.12 file this build was made for (section " + id + " differs). Nothing was built.");
  }
  log("Elektron Digitone II OS 1.12: recognised.");
  const repl = new Map();
  for (const [id, sec] of Object.entries(patch.sections)) {
    const orig = firmware.container.find(Number(id)).unpack();
    const content = applyOps(orig, sec);
    if ((await sha256(content)) !== sec.sha256) throw new Error(`Section ${id}: the result is not the tested build. Nothing was built.`);
    repl.set(Number(id), fw.replacement(firmware, Number(id), content));
    log(`Section ${id}: rebuilt and identical to the tested build.`);
  }
  const syx = await fw.build(firmware, repl);
  const again = await fw.load(syx);
  const checks = await fw.verify(again);
  const failed = checks.filter((c) => !c.ok);
  if (failed.length) throw new Error("The rebuilt file failed a check: " + failed.map((c) => c.name).join(", ") + ". Nothing to download.");
  for (const [id, sec] of Object.entries(patch.sections)) {
    const c = again.container.find(Number(id)).unpack();
    if ((await sha256(c)) !== sec.sha256) throw new Error(`Section ${id} changed while packing. Nothing to download.`);
  }
  log(`All ${checks.length} integrity checks pass.`);
  return { syx, sha: await sha256(syx) };
}
