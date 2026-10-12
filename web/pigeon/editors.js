// SPDX-License-Identifier: GPL-3.0-only
import {
  inspect,
  component,
  encodedPad,
  replaceKit,
  crc,
  normalize,
} from "./bank.js";
import {
  songChops,
  saveChops,
  padSettings,
  savePadSettings,
  renameSong,
  scanChops,
  project,
  readStep,
  editStep,
  patternLength,
  updateObject,
} from "./editing.js";
import { unb64 } from "./device.js";
import { WaveEditor } from "./wave-editor.js";
export function editors(api) {
  const $ = (id) => document.getElementById(id);
  let loaded = null,
    slot = 1,
    chop = 0,
    chops = [],
    pcm = null,
    pad = 0,
    object = 0,
    track = 0,
    step = 0;
  const block = document.createElement("section");
  block.className = "kit";
  block.innerHTML = `<div class="section-heading"><h2>Chop editor</h2><label>Song<select id="chop-song"><option value="1">Song 1</option><option value="2">Song 2</option></select></label><button id="load-chops" data-bank>Load waveform</button></div><p>Drag either chop boundary, or drag a new range. Edits remain local until you send the bank.</p><canvas class="wave" id="chop-wave"></canvas><div class="edit-fields"><label>Chop<select id="chop-index"></select></label><label>Start (s)<input id="chop-start" type="number" min="0" step="0.001"></label><label>End (s)<input id="chop-end" type="number" min="0" step="0.001"></label><label>Choke group<input id="chop-choke" type="number" min="0" max="3" value="0"></label><label>Pitch<input id="chop-pitch" type="number" min="-24" max="24" value="0"></label><label>Attack (ms)<input id="chop-attack" type="number" min="0" max="155" step="5" value="0"></label><label>Release (ms)<input id="chop-release" type="number" min="0" max="310" step="10" value="0"></label><label>Trigger<select id="chop-mode"><option value="0">Hold</option><option value="1">One-shot</option><option value="2">Latch</option></select></label><label>Loop<select id="chop-loop"><option value="1">On</option><option value="0">Off</option></select></label><label>Zoom<input id="chop-zoom" type="range" min="0" max="8" value="0"></label><label>Scroll<input id="chop-scroll" type="range" min="0" max="1000" value="0"></label><label>Transient sensitivity<input id="chop-sensitivity" type="range" min="0" max="100" value="50"></label><label>Copy settings from<select id="chop-copy"></select></label></div><div class="song-actions"><button id="chop-apply" data-bank>Apply chop</button><button id="chop-play" data-bank>Preview chop</button><button id="chop-stop">Stop preview</button><button id="chop-auto" data-bank>Detect transients</button><button id="chop-duplicate" data-bank>Duplicate</button><button id="chop-split" data-bank>Split</button><button id="chop-merge" data-bank>Merge previous</button><button id="chop-paste" data-bank>Copy settings</button></div><p>Attack/release and explicit trigger modes require the updated firmware. Release 0 uses the track default.</p><div class="edit-fields"><label>Song name (device: 8 characters)<input id="rename-song" maxlength="8"></label><button id="rename-apply" data-bank>Rename song</button></div>`;
  $("songs").after(block);
  const wave = new WaveEditor($("chop-wave"), (a, b) => {
    $("chop-start").value = (a / component(api.bank(), slot).h[5]).toFixed(5);
    $("chop-end").value = (b / component(api.bank(), slot).h[5]).toFixed(5);
  });
  function select() {
    const c = chops[chop];
    if (!c) return;
    const rate = component(api.bank(), slot).h[5];
    $("chop-start").value = (c.a / rate).toFixed(5);
    $("chop-end").value = (c.b / rate).toFixed(5);
    $("chop-pitch").value = c.pitch;
    $("chop-choke").value = c.flags >> 14;
    $("chop-attack").value = ((c.flags >> 2) & 31) * 5;
    $("chop-release").value = ((c.flags >> 7) & 31) * 10;
    $("chop-mode").value =
      c.flags & 2
        ? 2
        : c.flags & 8192 || (c.flags & 1 && !(c.flags & 4096))
          ? 1
          : 0;
    $("chop-loop").value = c.flags & 1 ? 0 : 1;
    wave.set(pcm, c.a, c.b);
  }
  function refresh() {
    const image = api.bank();
    if (image !== loaded) {
      loaded = image;
      pcm = null;
      wave.set(null);
      if (image && component(image, slot)) {
        chops = songChops(image, slot);
        chop = Math.min(chop, chops.length - 1);
        $("chop-index").replaceChildren(
          ...chops.map((_, i) => new Option(`Chop ${i + 1}`, i)),
        );
        $("chop-index").value = chop;
        $("chop-copy").replaceChildren(
          ...chops.map((_, i) => new Option(`Chop ${i + 1}`, i)),
        );
        $("rename-song").value = inspect(image).songs[slot - 1].name;
        select();
      } else {
        chops = [];
        $("chop-index").replaceChildren();
      }
    }
    if (image) {
      const c = padSettings(image)[pad];
      $("pad-label").value = api.padName(pad);
      for (const k of ["pitch", "level", "decay", "choke"])
        $(`pad-${k}`).value = c[k];
    }
    renderPattern();
  }
  async function save(next, message) {
    const audio = pcm;
    await api.setBank(saveChops(api.bank(), slot, next));
    chops = next;
    pcm = audio;
    select();
    api.status(message);
  }
  $("chop-song").onchange = () => {
    slot = Number($("chop-song").value);
    chop = 0;
    loaded = null;
    refresh();
  };
  $("chop-index").onchange = () => {
    chop = Number($("chop-index").value);
    select();
  };
  $("load-chops").onclick = () =>
    api.run(async () => {
      const audio = await api.songAudio(slot);
      pcm = audio.pcm;
      select();
      api.status(`Loaded Song ${slot} waveform`);
    });
  for (const k of ["zoom", "scroll"])
    $(`chop-${k}`).oninput = () => {
      wave.zoom = Number($("chop-zoom").value);
      wave.scroll = Number($("chop-scroll").value) / 1000;
      wave.draw();
    };
  $("chop-apply").onclick = () =>
    api.run(async () => {
      if (!chops[chop]) throw Error("Song is empty");
      const c = { ...chops[chop] },
        rate = component(api.bank(), slot).h[5],
        mode = Number($("chop-mode").value);
      c.a = Math.round(Number($("chop-start").value) * rate);
      c.b = Math.round(Number($("chop-end").value) * rate);
      c.pitch = Number($("chop-pitch").value);
      for (const [id, max] of [
        ["chop-choke", 3],
        ["chop-attack", 155],
        ["chop-release", 310],
      ]) {
        const value = Number($(id).value);
        if (!Number.isFinite(value) || value < 0 || value > max)
          throw Error("Invalid chop envelope or choke group");
      }
      c.flags =
        (Number($("chop-choke").value) << 14) |
        (Number($("chop-loop").value) ? 0 : 1) |
        (mode === 2 ? 2 : mode === 1 ? 8192 : 4096) |
        (Math.round(Number($("chop-attack").value) / 5) << 2) |
        (Math.round(Number($("chop-release").value) / 10) << 7);
      const next = structuredClone(chops);
      next[chop] = c;
      await save(next, "Chop updated locally");
    });
  $("chop-play").onclick = () =>
    api.run(async () => {
      if (!pcm) pcm = (await api.songAudio(slot)).pcm;
      const c = chops[chop];
      if (!c) throw Error("Song is empty");
      await api.play(
        pcm.slice(c.a, c.b),
        component(api.bank(), slot).h[5],
        Number($("chop-loop").value) === 1 &&
          Number($("chop-mode").value) !== 1,
      );
      api.status(`Playing chop ${chop + 1}`);
    });
  $("chop-stop").onclick = api.stop;
  $("chop-auto").onclick = () =>
    api.run(async () => {
      if (!pcm) pcm = (await api.songAudio(slot)).pcm;
      if (
        !confirm(
          "Replace this song’s chop assignments with detected transients?",
        )
      )
        return;
      chop = 0;
      await save(
        scanChops(pcm, Number($("chop-sensitivity").value)),
        "Transient chops generated locally",
      );
    });
  for (const op of ["duplicate", "split", "merge", "paste"])
    $(`chop-${op}`).onclick = () =>
      api.run(async () => {
        if (!chops[chop]) throw Error("Song is empty");
        const next = structuredClone(chops),
          c = next[chop];
        if (op === "duplicate" || op === "split") {
          if (next.length >= 32) throw Error("32 chops maximum");
          const copy = { ...c };
          if (op === "split") {
            if (c.b - c.a < 128) throw Error("Chop is too short to split");
            copy.a = c.a + Math.floor((c.b - c.a) / 2);
            c.b = copy.a;
          }
          next.splice(chop + 1, 0, copy);
          chop++;
        } else if (op === "merge") {
          if (!chop) throw Error("Select a chop after the first");
          next[chop - 1].a = Math.min(next[chop - 1].a, c.a);
          next[chop - 1].b = Math.max(next[chop - 1].b, c.b);
          next.splice(chop, 1);
          chop--;
        } else {
          const source = next[Number($("chop-copy").value)];
          c.pitch = source.pitch;
          c.flags = source.flags;
        }
        await save(next, "Chops updated locally");
      });
  $("rename-apply").onclick = () =>
    api.run(async () => {
      await api.setBank(renameSong(api.bank(), slot, $("rename-song").value));
      api.status("Song renamed locally");
    });
  const pads = document.createElement("section");
  pads.className = "kit";
  pads.innerHTML = `<h2>Drum pad settings</h2><div class="edit-fields"><label>Pad<select id="pad-setting-index">${Array.from({ length: 16 }, (_, i) => `<option value="${i}">Pad ${i + 1}</option>`).join("")}</select></label><label>Name<input id="pad-label" maxlength="128"></label><label>Pitch<input id="pad-pitch" type="number" min="-24" max="24"></label><label>Level<input id="pad-level" type="number" min="0" max="127"></label><label>Decay<input id="pad-decay" type="number" min="0" max="127"></label><label>Choke group<input id="pad-choke" type="number" min="0" max="4"></label></div><div class="song-actions"><button id="pad-settings-apply" data-bank>Apply pad settings</button><button id="pad-remove" data-bank>Remove pad</button><button id="pad-left" data-bank>Move pad left</button><button id="pad-right" data-bank>Move pad right</button></div><p>Decay 0 plays the full one-shot. Choke group 0 allows overlap. Removing a pad leaves a silent hit so later assignments keep their key numbers.</p>`;
  $("drum-settings-host").append(pads);
  $("pad-setting-index").onchange = () => {
    pad = Number($("pad-setting-index").value);
    refresh();
  };
  $("pad-settings-apply").onclick = () =>
    api.run(async () => {
      const label = $("pad-label").value;
      const settings = padSettings(api.bank());
      settings[pad] = Object.fromEntries(
        ["pitch", "level", "decay", "choke"].map((k) => [
          k,
          Number($(`pad-${k}`).value),
        ]),
      );
      await api.setBank(savePadSettings(api.bank(), settings));
      await api.setPadName(pad, label);
      api.status("Pad settings updated locally");
    });
  for (const direction of ["left", "right", "remove"])
    $(`pad-${direction}`).onclick = () =>
      api.run(async () => {
        const b = api.bank(),
          info = inspect(b),
          entries = Array.from({ length: info.pads }, (_, i) =>
            encodedPad(b, i),
          ),
          settings = padSettings(b);
        if (pad >= entries.length) throw Error("Pad is empty");
        if (direction === "remove") {
          entries[pad] = new Int16Array(64);
          settings[pad] = { pitch: 0, level: 127, decay: 0, choke: 0 };
        } else {
          const target = pad + (direction === "left" ? -1 : 1);
          if (target < 0 || target >= entries.length)
            throw Error("Cannot move beyond loaded pads");
          [entries[pad], entries[target]] = [entries[target], entries[pad]];
          [settings[pad], settings[target]] = [settings[target], settings[pad]];
          pad = target;
          $("pad-setting-index").value = pad;
        }
        await api.setBank(savePadSettings(replaceKit(b, entries), settings));
        api.status("Kit updated locally");
      });
  const patterns = document.createElement("section");
  patterns.className = "kit";
  patterns.innerHTML = `<h2>Pattern editor</h2><p>Open a full .pigeon session or back up the connected FM1. Edits are included when saving or restoring the session.</p><div class="edit-fields"><label>Pattern<select id="pattern-object"><option value="0">Current project</option>${["A", "B", "C", "D"].map((n, i) => `<option value="${i + 2}">Pattern ${n}</option>`).join("")}</select></label><label>Track<select id="pattern-track"><option value="0">Song 1</option><option value="1">Drums</option><option value="2">Song 2</option></select></label><label>Length<input id="pattern-length" type="number" min="1" max="64" value="16"></label><button id="pattern-length-apply">Apply length</button></div><div class="pattern-grid" id="pattern-grid"></div><div class="edit-fields"><label>Step<input id="pattern-step" type="number" min="1" max="64" value="1"></label><label>Chop / pad (0 = rest)<input id="pattern-chop" type="number" min="0" max="32"></label><label>Velocity<input id="pattern-velocity" type="number" min="1" max="127" value="100"></label><label>Probability (%)<input id="pattern-probability" type="number" min="0" max="100" value="100"></label><label>Step pitch<input id="pattern-pitch" type="number" min="-24" max="24" value="0"></label><label>Nudge (step /16)<input id="pattern-nudge" type="number" min="-8" max="7" value="0"></label><label>Condition<select id="pattern-condition"><option value="0">Always / probability</option><option value="1">Every second pass</option><option value="2">Every fourth pass</option><option value="3">First pass</option><option value="4">After first pass</option></select></label><label>Repeats<input id="pattern-repeats" type="number" min="1" max="4" value="1"></label></div><div class="song-actions"><button id="pattern-apply">Apply step</button><button id="pattern-clone">Copy current project to selected slot</button><button id="session-save">Save full session</button></div>`;
  block.after(patterns);
  function bytes() {
    const a = api.archive();
    if (!a) throw Error("Open or back up a full session first");
    const b = unb64(a.objects[object].data);
    if (!b.length)
      throw Error(
        "Empty pattern: copy the current project into this slot first",
      );
    return b;
  }
  function renderPattern() {
    const grid = $("pattern-grid");
    if (!grid) return;
    grid.replaceChildren();
    try {
      const p = project(bytes()),
        len = p.b[68 + track * p.stride + 29] - 64;
      $("pattern-length").value = len;
      for (let i = 0; i < len; i++) {
        const s = readStep(p, track, i),
          button = document.createElement("button");
        button.className = i === step ? "" : "secondary";
        button.textContent = `${i + 1} · ${s.n && s.time === 0 ? s.notes[0] - 59 : "—"}`;
        button.onclick = () => {
          step = i;
          $("pattern-step").value = i + 1;
          renderPattern();
        };
        grid.append(button);
      }
      const s = readStep(p, track, step);
      $("pattern-chop").value = s.n && s.time === 0 ? s.notes[0] - 59 : 0;
      $("pattern-velocity").value = s.velocity || 100;
      $("pattern-probability").value = s.probability;
      $("pattern-repeats").value = s.repeats;
      $("pattern-pitch").value = s.pitch;
      $("pattern-nudge").value = s.nudge;
      $("pattern-condition").value = s.condition;
    } catch {}
  }
  $("pattern-object").onchange = () => {
    object = Number($("pattern-object").value);
    step = 0;
    renderPattern();
  };
  $("pattern-track").onchange = () => {
    track = Number($("pattern-track").value);
    renderPattern();
  };
  $("pattern-step").onchange = () => {
    step = Math.max(0, Math.min(63, Number($("pattern-step").value) - 1));
    renderPattern();
  };
  $("pattern-apply").onclick = () =>
    api.run(async () => {
      const change = {
        chop: Number($("pattern-chop").value),
        velocity: Number($("pattern-velocity").value),
        probability: Number($("pattern-probability").value),
        repeats: Number($("pattern-repeats").value),
        pitch: Number($("pattern-pitch").value),
        nudge: Number($("pattern-nudge").value),
        condition: Number($("pattern-condition").value),
      };
      if (track === 1 && change.chop > 16) throw Error("Drums have 16 pads");
      await api.setArchive(
        updateObject(
          api.archive(),
          object,
          editStep(bytes(), track, step, change),
        ),
      );
      renderPattern();
      api.status("Pattern step updated locally");
    });
  $("pattern-length-apply").onclick = () =>
    api.run(async () => {
      await api.setArchive(
        updateObject(
          api.archive(),
          object,
          patternLength(bytes(), track, Number($("pattern-length").value)),
        ),
      );
      renderPattern();
      api.status("Pattern length updated locally");
    });
  $("pattern-clone").onclick = () =>
    api.run(async () => {
      if (!object) throw Error("Choose destination A–D");
      const a = api.archive();
      if (!a) throw Error("Open a session first");
      if (
        !confirm("Replace the selected local pattern with the current project?")
      )
        return;
      await api.setArchive(updateObject(a, object, unb64(a.objects[0].data)));
      renderPattern();
      api.status("Pattern copied locally");
    });
  $("session-save").onclick = () =>
    api.run(async () => {
      if (!api.archive()) throw Error("Open or back up a session first");
      api.saveArchive("pigeon-edited-session.pigeon", api.archive());
      api.status("Full session saved");
    });
  return { refresh };
}
