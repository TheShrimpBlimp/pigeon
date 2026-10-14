import "./test-assembly.mjs";
// SPDX-License-Identifier: GPL-3.0-only
import assert from "node:assert/strict";
import fs from "node:fs";
import { spawnSync } from "node:child_process";
import "./test.mjs";
import { component, crc, inspect } from "./bank.js";
import {
  songChops,
  saveChops,
  padSettings,
  savePadSettings,
  renameSong,
  project,
  readStep,
  editStep,
  patternLength,
  scanChops,
} from "./editing.js";
const root = new URL("../../", import.meta.url).pathname;
const native = new URL(
  "../../build/gui-deps/pigeon-editor-native",
  import.meta.url,
).pathname;
const built = spawnSync(
  process.env.CC || "cc",
  [
    "-O1",
    "-w",
    "-DFELUCCA_SAMPLER_ONLY=1",
    "-DFELUCCA_KO_DUAL=1",
    "-Ibuild/gen",
    "-Ifirmware/src",
    "tests/pigeon_expansion_test.c",
    "-lm",
    "-o",
    native,
  ],
  { cwd: root, encoding: "utf8" },
);
assert.equal(built.status, 0, built.stdout + built.stderr);
const fixtureRun = spawnSync(native, [], { cwd: root, encoding: "utf8" });
assert.equal(fixtureRun.status, 0, fixtureRun.stdout + fixtureRun.stderr);
let bank = new Uint8Array(
    fs.readFileSync(
      new URL("../../build/gui-deps/dual-kit.kob", import.meta.url),
    ),
  ),
  before = component(bank, 2).song;
let chops = songChops(bank, 1);
chops[0] = { a: 0, b: 10000, pitch: -7, flags: 4096 | (10 << 2) | (20 << 7) };
bank = saveChops(bank, 1, chops);
assert.deepEqual(songChops(bank, 1), chops);
assert.deepEqual(component(bank, 2).song, before);
assert.throws(() =>
  saveChops(bank, 1, [{ a: 100, b: 150, pitch: 0, flags: 0 }]),
);
assert.throws(() =>
  saveChops(bank, 1, [{ a: 0, b: 10000, pitch: 0, flags: 65536 }]),
);
const settings = padSettings(bank);
settings[1] = { pitch: 5, level: 80, decay: 20, choke: 2 };
bank = savePadSettings(bank, settings);
assert.deepEqual(padSettings(bank), settings);
assert.equal(inspect(renameSong(bank, 2, "SECOND")).songs[1].name, "SECOND");
assert.deepEqual(component(renameSong(bank, 2, "SECOND"), 2).song, before);
assert.equal(scanChops(new Float32Array(1000), 50).length, 1);
const fixture = new URL("../../build/expansion-project.funa", import.meta.url);
let bytes = new Uint8Array(fs.readFileSync(fixture));
const p = project(bytes);
assert.equal(p.np, 111);
for (const probability of [0, 1, 70, 99, 100]) {
  const changed = editStep(bytes, 0, 5, {
      chop: 10,
      velocity: 95,
      probability,
      repeats: 3,
    }),
    s = readStep(project(changed), 0, 5);
  assert.equal(s.probability, probability);
  assert.equal(s.time, 0);
  assert.equal(s.n, 1);
  assert.equal(s.notes[0], 69);
  assert.equal(s.repeats, 3);
}
bytes = patternLength(
  editStep(bytes, 0, 5, {
    chop: 10,
    velocity: 95,
    probability: 70,
    repeats: 3,
  }),
  0,
  32,
);
assert.equal(project(bytes).b[68 + 29], 96);
assert.throws(() => editStep(bytes, 0, 5, { chop: 33 }));
assert.throws(() => editStep(bytes, 0, 5, { velocity: 70.5 }));
bytes = editStep(bytes, 0, 10, { chop: 2, pitch: 5, nudge: -4, condition: 2 });
const advanced = readStep(project(bytes), 0, 10);
assert.equal(advanced.pitch, 5);
assert.equal(advanced.nudge, -4);
assert.equal(advanced.condition, 2);
assert.equal(advanced.probability, 100);
assert.throws(() => editStep(bytes, 0, 10, { pitch: 25 }));
assert.throws(() => editStep(bytes, 0, 10, { nudge: 8 }));
assert.throws(() => editStep(bytes, 0, 10, { condition: 5 }));
const out = new URL("../../build/expansion-edited.funa", import.meta.url);
fs.writeFileSync(out, bytes);
const result = spawnSync(native, [out.pathname, "performance"], {
  env: { ...process.env, ASAN_OPTIONS: "detect_leaks=0" },
  encoding: "utf8",
});
assert.equal(result.status, 0, result.stdout + result.stderr);
fs.writeFileSync(
  new URL("../../build/gui-deps/edited-bank.kob", import.meta.url),
  bank,
);
console.log(
  "GUI editing: chop/kit metadata, audio preservation, all probability values, repeats/length, and actual firmware FUNA import passed",
);
