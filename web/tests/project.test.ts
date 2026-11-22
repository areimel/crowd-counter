import { test } from "node:test";
import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import { createHash } from "node:crypto";
import { mkdirSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import { readIndex, validateIndex, withBase } from "../src/lib/project";

const fixtures = resolve(import.meta.dirname, "fixtures");
const fixtureJson = () => JSON.parse(readFileSync(join(fixtures, "index.json"), "utf8"));

test("readIndex returns null for a missing directory", () => {
  assert.equal(readIndex(join(tmpdir(), "no-such-firmware-dir")), null);
});

test("readIndex returns the board", () => {
  const index = readIndex(fixtures);
  assert.ok(index);
  assert.deepEqual(
    index.boards.map((b) => b.id),
    ["sticks3"],
  );
  assert.equal(index.project.name, "Fixture Counter");
});

test("withBase normalizes slashes for any base", () => {
  for (const base of ["/", "/repo", "/repo/", ""]) {
    const prefix = base.replace(/\/+$/, "");
    assert.equal(withBase("firmware/index.json", base), `${prefix}/firmware/index.json`);
    assert.equal(withBase("/firmware/index.json", base), `${prefix}/firmware/index.json`);
  }
  assert.equal(withBase("", "/repo"), "/repo/");
});

test("validateIndex rejects malformed indexes", () => {
  const cases: Array<[string, (i: any) => void]> = [
    ["bad commit", (i) => (i.commit = "ABC")],
    ["uppercase commit", (i) => (i.commit = i.commit.toUpperCase())],
    ["missing tagline", (i) => delete i.project.tagline],
    ["non-string description", (i) => (i.project.description = 5)],
    ["missing label", (i) => delete i.boards[0].label],
    ["missing chip", (i) => delete i.boards[0].chip],
    ["schemaVersion 2", (i) => (i.schemaVersion = 2)],
    ["traversal part", (i) => (i.boards[0].parts[0].path = "../x.bin")],
    ["absolute part", (i) => (i.boards[0].parts[0].path = "/x.bin")],
    ["backslash part", (i) => (i.boards[0].parts[0].path = "a\\b.bin")],
    ["drive part", (i) => (i.boards[0].parts[0].path = "C:/x.bin")],
    ["traversal manifest", (i) => (i.boards[0].manifest = "../m.json")],
    ["absolute manifest", (i) => (i.boards[0].manifest = "/m.json")],
  ];
  for (const [name, mutate] of cases) {
    const index = fixtureJson();
    mutate(index);
    assert.throws(() => validateIndex(index), /Invalid firmware index/, name);
  }
  assert.doesNotThrow(() => validateIndex(fixtureJson()));
});

const script = resolve(import.meta.dirname, "../scripts/check-firmware.ts");
const run = (dir: string) =>
  spawnSync(process.execPath, ["--import", "tsx", script, dir], {
    encoding: "utf8",
    cwd: resolve(import.meta.dirname, ".."),
  });

type Mutate = (index: any, dir: string) => void;

/** Copy the fixture index and write matching part files into a temp dir. */
function packaged(opts: { bytes?: number[]; mutate?: Mutate } = {}): string {
  const dir = mkdtempSync(join(tmpdir(), "fw-"));
  const index = fixtureJson();
  const good = Buffer.from([1, 2, 3, 4]);
  const written = Buffer.from(opts.bytes ?? [1, 2, 3, 4]);
  for (const board of index.boards) {
    mkdirSync(join(dir, board.id), { recursive: true });
    for (const part of board.parts) {
      part.size = good.length;
      part.sha256 = createHash("sha256").update(good).digest("hex");
      writeFileSync(join(dir, part.path), written);
    }
  }
  opts.mutate?.(index, dir);
  writeFileSync(join(dir, "index.json"), JSON.stringify(index));
  return dir;
}

test("check-firmware builds without a release when no index exists", () => {
  const r = run(mkdtempSync(join(tmpdir(), "fw-empty-")));
  assert.equal(r.status, 0);
  assert.match(r.stdout, /No packaged firmware; building without a release/);
});

test("check-firmware accepts matching hashes", () => {
  const r = run(packaged());
  assert.equal(r.status, 0, r.stderr);
});

test("check-firmware fails on a hash mismatch", () => {
  const r = run(packaged({ bytes: [9, 9, 9, 9] }));
  assert.equal(r.status, 1);
  assert.match(r.stderr, /mismatch/i);
});

test("check-firmware fails on a size mismatch", () => {
  const r = run(packaged({ bytes: [1, 2, 3, 4, 5] }));
  assert.equal(r.status, 1);
  assert.match(r.stderr, /mismatch/i);
});

test("check-firmware rejects a traversal path", () => {
  const r = run(packaged({ mutate: (i) => (i.boards[0].parts[0].path = "../evil.bin") }));
  assert.equal(r.status, 1);
  assert.match(r.stderr, /Invalid firmware index/);
});

test("check-firmware rejects a bad commit", () => {
  const r = run(packaged({ mutate: (i) => (i.commit = "nothex") }));
  assert.equal(r.status, 1);
  assert.match(r.stderr, /commit/);
});
