import { sha256hex } from '../fm1pkg.js';
import { recordInstall } from './install-count.js';
// SPDX-License-Identifier: GPL-3.0-only
import {
  inspect,
  component,
  repack,
  padAudio,
  encodedPad,
  replaceKit,
  normalize,
  HEAD,
  join,
} from "./bank.js";
import { editors as createEditors } from "./editors.js";
import { WaveEditor } from "./wave-editor.js";
import { Package, logicalImage } from "../fm1pkg.js";
import { Updater } from "../fm1ota.js";
import { crc } from "./bank.js";
import { Device, validateArchive, b64 } from "./device.js";
import { BackupConnection } from "../fm1backup.js";
const $ = (id) => document.getElementById(id);
let image = null,
  archive = null,
  recovery = null,
  busy = false,
  initializing = true,
  midi = null,
  connection = null,
  device = null,
  audio = null,
  playing = null;
const sources = [null, null],
  pendingPads = Array(16).fill(null);
let labels = {};
let kitReplaceAll = false,
  editors = null;
const trimWaves = [null, null];
const db = new Promise((resolve, reject) => {
  const r = indexedDB.open("pigeon-library", 1);
  r.onupgradeneeded = () => r.result.createObjectStore("files");
  r.onsuccess = () => resolve(r.result);
  r.onerror = () => reject(r.error);
});
async function store(key, value) {
  const d = await db;
  return new Promise((resolve, reject) => {
    const t = d.transaction("files", "readwrite");
    t.objectStore("files").put(value, key);
    t.oncomplete = resolve;
    t.onerror = () => reject(t.error);
    t.onabort = () => reject(t.error || Error("Local save cancelled"));
  });
}
async function load(key) {
  const d = await db;
  return new Promise((resolve, reject) => {
    const r = d.transaction("files").objectStore("files").get(key);
    r.onsuccess = () => resolve(r.result);
    r.onerror = () => reject(r.error);
  });
}
function status(text, error = false) {
  $("status").textContent = text;
  $("transfer-status").textContent = text;
  $("transfer-status").classList.toggle("error",error);
  $("status").parentElement.classList.toggle("error", error);
}
function progress(label, done, total) {
  status(`${label} • ${Math.round((done / total) * 100)}%`);
  $("job").value = done / total;
  $("transfer-progress").value = done / total;
}
function controls() {
  document.querySelectorAll("button,input,select").forEach((e) => {
    e.disabled =
      initializing ||
      busy ||
      (e.hasAttribute("data-bank") && !image) ||
      (e.hasAttribute("data-device") && !device);
  });
  $("send-help").textContent=initializing?"Preparing the tool…":busy?"Operation in progress — keep USB connected.":!image?"Open a .kob bank or import Song 1 before sending.":!device?"Bank ready. Connect the FM1 and select matching MIDI ports before sending.":"Bank and FM1 ready. Press Send bank to review the transfer.";
  $("restore").disabled = initializing || busy || !device || !archive;
  $("recovery").disabled = initializing || busy || !recovery;
}
async function run(fn) {
  if (busy || initializing) return;
  busy = true;
  controls();
  $("job").value = 0;
  try {
    await fn();
    $("job").value = 1;
  } catch (e) {
    status(e.message || String(e), true);
  } finally {
    busy = false;
    controls();
  }
}
function download(name, data, type = "application/octet-stream") {
  const blob = new Blob([data], { type }),
    url = URL.createObjectURL(blob),
    a = document.createElement("a");
  a.href = url;
  a.download = name;
  a.click();
  setTimeout(() => URL.revokeObjectURL(url), 10000);
}
const date = () => new Date().toISOString().replace(/[:.]/g, "-");
function saveArchive(name, a) {
  download(name, JSON.stringify(a) + "\n", "application/json");
}
async function context() {
  audio ??= new AudioContext();
  if (audio.state === "suspended") await audio.resume();
  return audio;
}
async function readAudio(file) {
  if (!file || file.size > 256 * 1024 * 1024)
    throw Error("Choose an audio file smaller than 256 MiB");
  const c = await context();
  let b;
  try {
    b = await c.decodeAudioData(await file.arrayBuffer());
  } catch {
    throw Error("Browser cannot decode this file. Try a PCM WAV or MP3.");
  }
  if (!b.length || b.duration > 600)
    throw Error(
      "Choose audio shorter than 10 minutes, then trim the section to fit",
    );
  const render = new OfflineAudioContext(
      1,
      Math.ceil(b.duration * 22050),
      22050,
    ),
    s = render.createBufferSource();
  s.buffer = b;
  s.connect(render.destination);
  s.start();
  const mono = (await render.startRendering()).getChannelData(0);
  return {
    pcm: mono.slice(),
    filename: file.name,
    name: file.name.replace(/\.[^.]+$/, ""),
    duration: mono.length / 22050,
  };
}
function wave(canvas, pcm, starts = []) {
  const w = (canvas.width = 1000),
    h = (canvas.height = 140),
    c = canvas.getContext("2d");
  c.clearRect(0, 0, w, h);
  c.fillStyle = "#b9ef77";
  if (!pcm?.length) {
    c.fillRect(0, h / 2, w, 1);
    return;
  }
  for (let i = 0; i < w; i++) {
    let lo = 1,
      hi = -1;
    const a = Math.floor((i * pcm.length) / w),
      b = Math.max(a + 1, Math.floor(((i + 1) * pcm.length) / w));
    for (let j = a; j < Math.min(b, pcm.length); j++) {
      lo = Math.min(lo, pcm[j]);
      hi = Math.max(hi, pcm[j]);
    }
    c.fillRect(i, ((1 - hi) * h) / 2, 1, Math.max(1, ((hi - lo) * h) / 2));
  }
  c.strokeStyle = "#f0efd6";
  for (const p of starts) {
    const x = (p / pcm.length) * w;
    c.beginPath();
    c.moveTo(x, 0);
    c.lineTo(x, h);
    c.stroke();
  }
}
async function play(pcm, rate = 22050, loop = false) {
  const c = await context();
  playing?.stop();
  const b = c.createBuffer(1, pcm.length, rate);
  b.copyToChannel(pcm, 0);
  playing = c.createBufferSource();
  playing.buffer = b;
  playing.connect(c.destination);
  playing.loop = loop;
  playing.start();
}
async function songAudio(slot) {
  const s = component(image, slot);
  if (!s) throw Error("Empty song slot");
  const h = s.h;
  if (h[4] === 1) {
    const { imaDecode } = await import("./bank.js");
    return { pcm: imaDecode(s.song, h[6]), rate: h[5] };
  }
  const c = await context(),
    raw = s.song.slice((h[12] + 1) * 4),
    b = await c.decodeAudioData(raw.buffer);
  let pcm = b.getChannelData(0);
  if (b.sampleRate !== 22050) {
    const o = new OfflineAudioContext(1, Math.ceil(b.duration * 22050), 22050),
      n = o.createBufferSource();
    n.buffer = b;
    n.connect(o.destination);
    n.start();
    pcm = (await o.startRendering()).getChannelData(0);
  }
  const trimmed = pcm.slice(h[13], h[13] + h[6]);
  if (!trimmed.length) throw Error("Browser MP3 preview failed");
  return { pcm: trimmed, rate: 22050 };
}
function encode(pcm, bitrate, name) {
  return new Promise((resolve, reject) => {
    const worker = new Worker("encode-worker.js");
    worker.onerror = (e) => {
      worker.terminate();
      reject(Error(e.message || "Encoder failed"));
    };
    worker.onmessage = ({ data }) => {
      if (data.error) {
        worker.terminate();
        reject(Error(data.error));
      } else if (data.image) {
        worker.terminate();
        resolve(data.image);
      } else progress("Encoding locally", data.progress, 1);
    };
    const copy = pcm.slice();
    worker.postMessage({ pcm: copy, bitrate, name }, [copy.buffer]);
  });
}
async function updateBank(next) {
  inspect(next);
  await store("bank", next);
  image = next;
  if (archive) {
    archive = { ...archive, bank: b64(next), bank_crc: crc(next) };
    await store("session", archive);
  }
  render();
}
async function remember(key, name) {
  labels[key] = name;
  await store("labels", labels);
  if (archive) {
    archive = { ...archive, labels };
    await store("session", archive);
  }
}
function render() {
  editors?.refresh();
  const info = image ? inspect(image) : null;
  $("used").textContent = info
    ? `${(info.payloadUsed / 1024).toFixed(1)} / 408 KiB used`
    : "No bank loaded";
  $("free").textContent = info
    ? `${(info.free / 1024).toFixed(1)} KiB free`
    : "Import Song 1 to start, or open a bank.";
  $("capacity").value = info?.payloadUsed || 0;
  for (let i = 0; i < 2; i++) {
    const s = info?.songs[i];
    $(`name${i}`).textContent = s?.name || "Empty";
    $(`info${i}`).textContent = s
      ? `${s.seconds.toFixed(2)} seconds • ${s.codec} • ${(s.bytes / 1024).toFixed(1)} KiB • ${s.fields[16]} initial chops`
      : "Import your audio below.";
    if (sources[i])
      trimWaves[i]?.set(
        sources[i].pcm,
        Math.floor(Number($(`start${i}`).value) * 22050),
        Math.floor(Number($(`end${i}`).value) * 22050),
      );
    else if (s) {
      const comp = component(image, i + 1),
        w = comp.wave,
        pcm = Float32Array.from(w, (n) => (n >= 128 ? n - 256 : n) / 64);
      wave($(`wave${i}`), pcm);
    } else wave($(`wave${i}`), null);
  }
  for (let i = 0; i < 2; i++) {
    const c = image && component(image, i + 1);
    if (!sources[i])
      $(`source-name${i}`).textContent = c
        ? labels["song:" + crc(c.song)] || ""
        : "";
  }
  for (let p = 0; p < 16; p++) {
    const has = info && p < info.pads;
    $(`padname${p}`).textContent =
      pendingPads[p]?.filename ||
      (has
        ? labels["pad:" + crc(encodedPad(image, p).encoded)] || "Loaded"
        : "Empty");
    $(`pad${p}`).classList.toggle("secondary", !has && !pendingPads[p]);
  }
  controls();
}
for (let i = 0; i < 2; i++) {
  const section = document.createElement("article");
  section.className = "song";
  section.innerHTML = `<div class="song-header"><p class="eyebrow">SONG ${i + 1}</p><strong class="song-name" id="name${i}">Empty</strong></div><p class="song-info" id="info${i}"></p><canvas class="wave" id="wave${i}" aria-label="Song ${i + 1} waveform"></canvas><label>Choose audio<input id="file${i}" type="file" accept="audio/*"></label><div class="trim"><label>Start (seconds)<input id="start${i}" type="number" min="0" step="0.01" value="0"></label><label>End (seconds)<input id="end${i}" type="number" min="0" step="0.01" value="0"></label><label>Quality<select id="rate${i}">${[24, 32, 40, 48, 56, 64].map((n) => `<option value="${n}" ${n === 64 ? "selected" : ""}>${n} kbps</option>`).join("")}</select></label></div><p id="source-name${i}"></p><div class="edit-fields"><label>Zoom<input id="trim-zoom${i}" type="range" min="0" max="8" value="0"></label><label>Scroll<input id="trim-scroll${i}" type="range" min="0" max="1000" value="0"></label><label>Loop preview<select id="trim-loop${i}"><option value="0">Off</option><option value="1">On</option></select></label><button id="trim-stop${i}">Stop preview</button></div><p id="estimate${i}">Choose a file to trim and estimate its size.</p><div class="song-actions"><button id="preview${i}" class="secondary">Preview section</button><button id="stored${i}" class="secondary" data-bank>Preview stored song</button><button id="apply${i}">Import into Song ${i + 1}</button></div>`;
  $("songs").append(section);
  trimWaves[i] = new WaveEditor($(`wave${i}`), (a, b) => {
    $(`start${i}`).value = (a / 22050).toFixed(5);
    $(`end${i}`).value = (b / 22050).toFixed(5);
    estimate();
  });
  for (const k of ["zoom", "scroll"])
    $(`trim-${k}${i}`).oninput = () => {
      trimWaves[i].zoom = Number($(`trim-zoom${i}`).value);
      trimWaves[i].scroll = Number($(`trim-scroll${i}`).value) / 1000;
      trimWaves[i].draw();
    };
  $(`trim-stop${i}`).onclick = () => {
    playing?.stop();
    status("Preview stopped");
  };
  const estimate = () => {
    const seconds = Number($(`end${i}`).value) - Number($(`start${i}`).value),
      br = Number($(`rate${i}`).value);
    $(`estimate${i}`).textContent =
      `Section: ${Math.max(0, seconds).toFixed(2)} s • about ${((Math.max(0, seconds) * (br * 125 + (22050 / 576) * 4)) / 1024 + 8.5).toFixed(1)} KiB. Final size is checked before changing the bank.`;
  };
  for (const k of ["start", "end", "rate"])
    $(`${k}${i}`).addEventListener("input", estimate);
  $(`file${i}`).onchange = () =>
    run(async () => {
      const source = await readAudio($(`file${i}`).files[0]);
      sources[i] = source;
      $(`source-name${i}`).textContent = $(`file${i}`).files[0].name;
      $(`start${i}`).value = 0;
      $(`end${i}`).value = source.duration.toFixed(3);
      estimate();
      render();
      status(`Selected ${source.name}. Trim the section, then import it.`);
    });
  function sectionPcm() {
    const s = sources[i],
      a = Number($(`start${i}`).value),
      b = Number($(`end${i}`).value);
    if (!s) throw Error("Choose an audio file first");
    if (
      !Number.isFinite(a) ||
      !Number.isFinite(b) ||
      a < 0 ||
      b <= a ||
      b > s.duration + 0.001
    )
      throw Error("Choose valid start and end points inside the audio");
    return s.pcm.slice(
      Math.floor(a * 22050),
      Math.min(s.pcm.length, Math.floor(b * 22050)),
    );
  }
  $(`preview${i}`).onclick = () =>
    run(async () => {
      await play(sectionPcm(), 22050, Number($(`trim-loop${i}`).value) === 1);
      status(`Playing Song ${i + 1} import section`);
    });
  $(`stored${i}`).onclick = () =>
    run(async () => {
      const { pcm, rate } = await songAudio(i + 1);
      await play(pcm, rate);
      status(`Playing stored Song ${i + 1}`);
    });
  $(`apply${i}`).onclick = () =>
    run(async () => {
      if (!image && i === 1) throw Error("Import Song 1 first");
      const encoded = await encode(
        sectionPcm(),
        Number($(`rate${i}`).value),
        sources[i].name,
      );
      await updateBank(image ? repack(image, encoded, i + 1) : encoded);
      await remember(
        "song:" + crc(component(image, i + 1).song),
        sources[i].filename,
      );
      render();
      status(
        `Song ${i + 1} imported locally. Save the bank or send it to the FM1.`,
      );
    });
}
for (let p = 0; p < 16; p++) {
  const cell = document.createElement("div");
  cell.className = "pad";
  cell.innerHTML = `<button id="pad${p}"><strong>${String(p + 1).padStart(2, "0")}</strong><span id="padname${p}">Empty</span></button><label class="pad-import">Choose hit<input id="padfile${p}" type="file" accept="audio/*"></label>`;
  $("pads").append(cell);
  $(`padfile${p}`).onchange = () =>
    run(async () => {
      pendingPads[p] = await readAudio($(`padfile${p}`).files[0]);
      render();
      status(
        `Pad ${p + 1} staged. Click Replace kit to apply all staged hits.`,
      );
    });
  $(`pad${p}`).onclick = () =>
    run(async () => {
      const pcm = pendingPads[p]?.pcm || (image && padAudio(image, p));
      if (!pcm) throw Error("Pad is empty");
      await play(pcm);
      status(`Playing pad ${p + 1}`);
    });
}
$("kit-files").onchange = () =>
  run(async () => {
    const files = [...$("kit-files").files];
    if (!files.length || files.length > 16)
      throw Error("Choose 1–16 one-shots");
    const next = Array(16).fill(null);
    for (let p = 0; p < files.length; p++) {
      next[p] = await readAudio(files[p]);
      progress("Preparing drums", p + 1, files.length);
    }
    pendingPads.splice(0, 16, ...next);
    kitReplaceAll = true;
    render();
    status("Kit staged in file-picker order. Replace kit to apply it.");
  });
$("apply-kit").onclick = () =>
  run(async () => {
    const info = inspect(image),
      last = Math.max(
        kitReplaceAll ? 0 : info.pads,
        pendingPads.reduce((n, p, i) => (p ? i + 1 : n), 0),
      );
    if (!last) throw Error("Choose some one-shots first");
    const pcm = [];
    for (let p = 0; p < last; p++)
      pcm.push(
        pendingPads[p]
          ? normalize(pendingPads[p].pcm)
          : !kitReplaceAll && encodedPad(image, p)
            ? encodedPad(image, p)
            : new Int16Array(64),
      );
    await updateBank(replaceKit(image, pcm));
    for (let p = 0; p < last; p++)
      if (pendingPads[p])
        await remember(
          "pad:" + crc(encodedPad(image, p).encoded),
          pendingPads[p].filename,
        );
    pendingPads.fill(null);
    kitReplaceAll = false;
    render();
    status("Drum kit replaced locally. Songs preserved.");
  });
$("clear-kit").onclick = () =>
  run(async () => {
    if (!confirm("Remove all 16 drum pads from this local bank?")) return;
    await updateBank(replaceKit(image, []));
    pendingPads.fill(null);
    kitReplaceAll = false;
    render();
    status("Kit cleared locally. Songs preserved.");
  });
$("open").onchange = () =>
  run(async () => {
    const file = $("open").files[0];
    if (!file || file.size > 2 * 1024 * 1024)
      throw Error(
        "Choose a Pigeon .kob bank or .pigeon session smaller than 2 MiB",
      );
    if (file.name.toLowerCase().endsWith(".pigeon")) {
      const a = JSON.parse(await file.text()),
        v = validateArchive(a);
      await updateBank(v.bank);
      archive = a;
      if (a.labels && typeof a.labels === "object") {
        labels = { ...labels, ...a.labels };
        await store("labels", labels);
      }
      await store("session", a);
      controls();
      status(
        "Session opened and validated locally. Restore sends its projects, settings, and samples.",
      );
    } else {
      archive = null;
      await store("session", null);
      await updateBank(new Uint8Array(await file.arrayBuffer()));
      status("Sample bank opened locally.");
    }
    sources.fill(null);
    pendingPads.fill(null);
    kitReplaceAll = false;
    render();
  });
$("save").onclick = () => download(`pigeon-bank-${date()}.kob`, image);
function ports() {
  for (const [id, list] of [
    ["input", midi.inputs],
    ["output", midi.outputs],
  ]) {
    const previous = $(id).value;
    $(id).replaceChildren();
    for (const port of list.values())
      if (port.state !== "disconnected") {
        const o = new Option(port.name || port.id, port.id);
        $(id).append(o);
      }
    if (
      [...list.values()].some(
        (p) => p.id === previous && p.state !== "disconnected",
      )
    )
      $(id).value = previous;
    else {
      const fm = [...list.values()].find((p) =>
        /fm.?1|felucca|pigeon/i.test(p.name || ""),
      );
      if (fm) $(id).value = fm.id;
    }
  }
}
function attach() {
  connection?.close();
  const input = midi?.inputs.get($("input").value),
    output = midi?.outputs.get($("output").value);
  if (
    !input ||
    !output ||
    input.state === "disconnected" ||
    output.state === "disconnected"
  ) {
    connection = null;
    device = null;
    $("connection").textContent = "Device disconnected";
  } else {
    connection = new BackupConnection(input, output);
    device = new Device((...args) => connection.request(...args), progress);
    $("connection").textContent = `${input.name} / ${output.name}`;
  }
  controls();
}
$("connect").onclick = () =>
  run(async () => {
    if (!isSecureContext) throw Error("Open this tool over HTTPS or localhost");
    if (!navigator.requestMIDIAccess)
      throw Error("Use Chrome or Edge for USB MIDI transfers");
    midi = await navigator.requestMIDIAccess({ sysex: true });
    ports();
    attach();
    midi.onstatechange = () => {
      if (busy) {
        const i = midi.inputs.get($("input").value),
          o = midi.outputs.get($("output").value);
        if (
          !i ||
          !o ||
          i.state === "disconnected" ||
          o.state === "disconnected"
        ) {
          connection?.close();
          device = null;
          $("connection").textContent = "Device disconnected";
        }
        return;
      }
      ports();
      attach();
    };
    if (!device) throw Error("No MIDI ports found. Connect the FM1 by USB.");
    await device.negotiate();
    const info = await device.ask(1);
    $("firmware-version").textContent =
      "Installed: " + String.fromCharCode(...info.slice(0, info.indexOf(0)));
    status("Pigeon connected. Stop playback before reading or sending.");
  });
$("input").onchange = $("output").onchange = attach;
$("read").onclick = () =>
  run(async () => {
    if (
      image &&
      !confirm("Replace the local working bank with the device bank?")
    )
      return;
    await updateBank(await device.readBank());
    sources.fill(null);
    pendingPads.fill(null);
    kitReplaceAll = false;
    render();
    status("Device bank read and saved locally.");
  });
async function protect() {
  status("Saving recovery backup before any device write…");
  const a = await device.capture();
  await store("recovery", a);
  recovery = a;
  controls();
  saveArchive(`pigeon-recovery-${date()}.pigeon`, a);
  return a;
}
$("upload").onclick = () => {
  if(busy || initializing || !image || !device){controls();return;}
  const info=inspect(image);
  $("send-summary").textContent=`Replace both FM1 songs and the kit with ${info.songs.map(s=>s.name).join(" + ")} and ${info.pads} drum pads (${(info.payloadUsed/1024).toFixed(1)} KiB)?`;
  $("send-review").hidden=false;
  $("send-review").scrollIntoView({block:"nearest"});
  $("send-confirm").focus();
};
$("send-cancel").onclick=()=>{$("send-review").hidden=true;status("Bank transfer cancelled. Nothing was written.");};
$("send-confirm").onclick=()=>run(async()=>{
  if(!image || !device)throw Error("Reconnect the FM1 and select a bank before sending.");
  $("send-review").hidden=true;
  const stage="Writing and verifying the bank";
  try{
    status("Starting sample-bank transfer…");
    const actual=await device.writeBank(image);
    await updateBank(actual);
    status("Bank sent and verified by reading it back.");
  }catch(e){throw Error(`${stage}: ${e.message||String(e)}`);}
});
$("backup").onclick = () =>
  run(async () => {
    const a = await device.capture();
    archive = a;
    await store("backup", a);
    await store("session", a);
    render();
    saveArchive(`pigeon-session-${date()}.pigeon`, a);
    status(
      "Complete session backed up: projects, settings, samples, and chop metadata.",
    );
  });
$("restore").onclick = () =>
  run(async () => {
    validateArchive(archive);
    if (
      !confirm(
        "Restore the opened session, replacing the FM1 projects, settings, and samples? Stop playback and release all keys. A recovery session will be saved first. Keep USB connected until verification finishes.",
      )
    )
      return;
    const a = archive;
    await protect();
    await device.restore(a);
    status("Full session restored and verified. Recovery backup retained.");
  });
$("recovery").onclick = () =>
  saveArchive(`pigeon-recovery-${date()}.pigeon`, recovery);
async function offline() {
  if (!("serviceWorker" in navigator) || !isSecureContext) {
    $("offline").textContent = "Offline reload needs HTTPS or localhost";
    return false;
  }
  try {
    const reg = await navigator.serviceWorker.register("sw.js");
    await navigator.serviceWorker.ready;
    const worker = reg.active;
    if (!worker) throw Error("Cache not active");
    const result = await new Promise((resolve, reject) => {
      const channel = new MessageChannel(),
        timer = setTimeout(
          () => reject(Error("Offline cache check timed out")),
          10000,
        );
      channel.port1.onmessage = (e) => {
        clearTimeout(timer);
        resolve(e.data);
      };
      worker.postMessage("CHECK_OFFLINE", [channel.port2]);
    });
    $("offline").textContent = result.ready
      ? "Offline ready"
      : "Offline cache incomplete";
    return result.ready;
  } catch (e) {
    $("offline").textContent = "Offline cache unavailable";
    console.error(e);
    return false;
  }
}
editors = createEditors({
  bank: () => image,
  archive: () => archive,
  setBank: updateBank,
  setArchive: async (a) => {
    validateArchive(a);
    await store("session", a);
    archive = a;
    render();
  },
  run,
  status,
  play,
  songAudio,
  padName: (p) => {
    const pad = image && encodedPad(image, p);
    return pad ? labels["pad:" + crc(pad.encoded)] || "" : "";
  },
  setPadName: async (p, name) => {
    const pad = encodedPad(image, p);
    if (!pad) throw Error("Pad is empty");
    await remember("pad:" + crc(pad.encoded), name.slice(0, 128));
    render();
  },
  saveArchive,
  stop: () => {
    playing?.stop();
    status("Preview stopped");
  },
});
const updatePanel = document.createElement("section");
updatePanel.className = "kit";
updatePanel.innerHTML =
  '<h2>Firmware update</h2><p id="firmware-version">Connect the FM1 to read its firmware version.</p><label>Choose Pigeon firmware<input id="firmware-file" type="file" accept=".fwsc"></label><p id="firmware-package">Choose a local .fwsc package. The GUI does not download firmware.</p><button id="firmware-install" data-device>Back up and install firmware</button><p>Installing reboots the FM1. Keep USB connected throughout. A full recovery session is saved first.</p><p>Successful browser installs report an anonymous completion event to the Pigeon counter; no device identity or sample data is sent.</p>';
document.querySelector(".status-box").before(updatePanel);
let firmwarePackage = null;
$("firmware-file").onchange = () =>
  run(async () => {
    firmwarePackage = null;
    const file = $("firmware-file").files[0];
    if (!file || file.size > 2 * 1024 * 1024)
      throw Error("Choose a Pigeon .fwsc package under 2 MiB");
    const bytes = new Uint8Array(await file.arrayBuffer()),
      pkg = new Package(bytes);
    if (
      !/^FM-1_9\d\d$/.test(pkg.product) ||
      !new TextDecoder().decode(pkg.dec).includes("FELUCCA-LOADER-1") ||
      !new TextDecoder()
        .decode(pkg.dec.subarray(pkg.appOff, pkg.appOff + pkg.app.size))
        .includes("Pigeon")
    )
      throw Error(
        "Package is not Pigeon firmware with the supported update loader",
      );
    firmwarePackage = { bytes, product: pkg.product, release: await sha256hex(bytes) };
    $("firmware-package").textContent =
      `${file.name} • ${pkg.product} • ${(bytes.length / 1024).toFixed(1)} KiB`;
    status("Firmware package validated locally");
  });
$("firmware-install").onclick = () =>
  run(async () => {
    if (!firmwarePackage) throw Error("Choose a firmware package first");
    if (
      !confirm(
        `Install ${firmwarePackage.product}? Stop playback and release all keys. A full recovery backup will be saved before rebooting. Keep USB connected.`,
      )
    )
      return;
    await protect();
    connection?.close();
    device = null;
    const updater = new Updater(midi);
    try {
      await updater.install(
        logicalImage(firmwarePackage.bytes),
        firmwarePackage.product,
        (stage, value) =>
          status(`Firmware update: ${stage}${value ? " • " + value : ""}`),
      );
      void recordInstall(firmwarePackage.release,"sample-tool");
      ports();
      attach();
      status(
        "Firmware installed and identity verified. Recovery session retained.",
      );
    } finally {
      ports();
      attach();
    }
  });
render();
status("Preparing your offline workspace…");
const caching = offline();
try {
  labels = (await load("labels")) || {};
  const saved = await load("bank");
  if (saved) {
    inspect(saved);
    image = saved;
    render();
    status("Restored your last local bank.");
  }
  const session = await load("session");
  if (session) {
    validateArchive(session);
    archive = session;
    render();
  }
  recovery = await load("recovery");
  if (recovery) validateArchive(recovery);
  controls();
} catch (e) {
  status(
    `Local storage unavailable: ${e.message}. Download banks to preserve your work.`,
    true,
  );
}

const offlineReady = await caching;
initializing = false;
controls();
if (!offlineReady)
  status(
    "Offline cache is unavailable. Keep the connection active; reload to try caching again.",
    true,
  );
else if ($("status").textContent === "Preparing your offline workspace…")
  status("Ready. Open a bank or import your first song.");
