# JP (BZMJ) and Angel SP4 support

*Status (2026-08-18): clean JP and the exact Angel Team Chinese SP4 revision have
complete runtime profiles. All required runtime `RomOffsets` are populated;
SP4's injected text codec, 16-bank font table, wide glyph layout, remap table,
and palette rule are implemented natively. Generated JP asset-offset headers
remain a local build prerequisite and are not committed.*

The Minish Cap speedrun scene runs the **Japanese** version (RNG manipulations are
version-exclusive and were authored for JP). The decomp supports JP at the source
level (`#ifdef JP` throughout `src/`); this port now wires JP through the build and
runtime, and supplies the JP ROM data-table offsets.

## Build & run a JP port

1. **Provide a legal JP baserom** (`BZMJ`, SHA-1 `6c5404a1effb17f481f352181d0f1c61a2765c5d`,
   see `tmc_jp.sha1`) as `baserom_jp.gba` in the repo root. (Gitignored — never committed.)
2. **Build:** `python3 build.py --jp` (or `xmake f --game_version=JP -y && xmake build -y tmc_pc`).
3. **Run:** point the binary at the JP ROM. It auto-detects `JP (BZMJ)` and selects the
   JP offset table. Verified boot log:
   ```
   ROM region detected: JP (BZMJ)
   Using offsets for JP (game code: BZMJ)
   gMapData loaded (13482224 bytes from ROM offset 0x324710).
   Area data tables loaded (0x90 areas, 2-level pointers resolved).
   Entering AgbMain...
   ```

For a JP-baseline 3DS debug build, generate the local asset-offset headers
first, then invoke the cross build:

```sh
xmake f -y --game_version=JP
xmake build -y asset_processor
tools/bin/asset_processor extract JP build/JP/assets
TMC3DS_REGION=JP TMC3DS_BUILD_TYPE=Debug platform/3ds/build.sh
```

The 3DS launcher deliberately requires this JP build family for clean JP and
Angel SP4. The default USA/EU package rejects them before loading tables, and a
JP package likewise rejects USA/EU ROMs; this avoids running with incompatible
compile-time asset/layout choices.

## What's wired

- `ROM_REGION_JP`, `kRomOffsets_JP` (real values), `BZMJ` detection, JP/USA offset
  selection, JP-aware region messages — `port/port_config.h`, `port/port_rom.c`
- `port/port_offset_JP.h` (asset-blob offsets) + `#if defined(JP)` include — `port/port_main.c`
- `JP`/`JAPANESE` in `pc_versions` — `xmake.lua`; `--jp` + JP version entry — `build.py`
- JP-only symbol guards so the JP port links (`sub_0807FC24`, `sub_08088658` are
  USA-only in the decomp) — `port/port_script_funcs.c`
- `-I port` on the decomp ROM build's preprocess so committed `src/` port-includes
  resolve (header-resolution only; no PC code enters the ROM build) — `xmake.lua`
- Exact SHA-1/SHA-256 runtime profiles for clean JP and Angel SP4. Unknown
  `BZMJ` revisions fail closed and print their SHA-256.
- Collision, Kinstone/fusion, figurine, Lake Hylia, lilypad, song, guard, inn,
  Simon, Gust Jar, and multipart sprite offsets added after the original work.
- Region-native addresses for 26 additional room entity lists, validated for
  bounds, 16-byte records, and the final `0xFF` terminator.
- SP4 text profile: glyph root `0xDC9F00`, 16 banks, remap root `0xE4F000`,
  wide glyphs from bank 3, special palette bank 3, and the patch's variable
  two/three-byte decoder.
- Clean JP and SP4 use separate EEPROM saves, desktop save states, and
  profile-keyed extracted asset caches.

## How `kRomOffsets_JP` was derived (content-anchoring)

This tree's decomp ROM build is **non-matching** (port edits shift symbols ~0xC44),
so `build/JP/tmc_jp.map` is NOT a valid source — its addresses don't match the retail
ROM the port actually loads. Instead the offsets come straight from the **retail USA +
JP ROMs**: the USA addresses are known-correct, so each USA table is located in the JP
ROM. Per-field method (see the comment block in `port_rom.c`):

- **direct content-anchor** — unique 64-byte signature of the table start (version-stable
  tables: gfx/palette/map data);
- **pointer-table dereference / shift-search** — for tables of `0x08xxxxxx` pointers,
  find the single shift under which the whole pointer array is internally consistent;
- **text/translations cluster** — uniform `-0x33C` shift, fixed by 4 independent anchors.

Regional shifts grow monotonically with address (`0 → -0x260 → ~-0x338 → -0x33C →
-0x3D4`), each value corroborated by neighbours. The whole table is validated by the
boot test above (2-level area-pointer resolution across all 0x90 areas cannot succeed
with wrong offsets). `/tmp` scratch scripts that produced these aren't committed; the
final values + provenance live in `port_rom.c`. Count/size fields are content-invariant
(identical USA==EU) and carried over.

Later fields were recovered from unique retail signatures and JP asset-shift
markers, then checked against pointer-table structure and ROM bounds. The 26
room lists were additionally checked to end on a 16-byte entity record whose
kind is `0xFF`.

## Verification status and remaining hardware work

Earlier clean-JP boot testing established the core runtime path. The SP4 work
below is verified against the supplied ROM's structure and complete text corpus;
its rendering and gameplay still require device-level QA.

Resolved since the original writeup:

- **Script addresses (was: `port/port_scripts.h` hardcoded USA).**
  `Port_TranslateScriptAddr` (`port/port_script_addrs.c`) now remaps the
  **entire** script bytecode section — all 576 data scripts, USA→EU/JP by exact
  symbol lookup in the retail maps — not just the ~100 `GBA_script_*` macros.
  This covers scripts referenced only by raw entity-data blobs in
  `port/data_const_stubs.c` (e.g. `script_ZeldaOutsideLinksHouse`, the prologue
  Business Scrub orchestrators). Those were previously untranslated and ran
  garbage bytecode on JP/EU — the cause of the intro Zelda "wrong position" and
  the prologue scrub not spitting.
- **`port/port_script_funcs.c` native-call table.** Has per-region tables
  (`sScriptFuncTable_JP` / `_EU`, selected at runtime via `REGION_IS_*`); the
  two USA-only functions are excluded for JP.

- **Clean JP text compatibility.** The retail decoder path is unchanged and its
  prefix-to-bank mapping is covered by regression tests.
- **Angel SP4 text compatibility.** Both language slots were parsed from the
  exact supported ROM: 160 language/group combinations and 7,473 messages in
  total (3,776 in the Chinese slot). Every editable message round-trips to its
  original bytes; the variable-length decoder, 16-bank range, wide-glyph
  threshold, remap range, and palette bank also have focused native tests.
- **Runtime ROM structure.** Both supplied 16 MiB JP profiles pass full hash,
  required-offset, pointer-table, bounds, and text-table validation before the
  engine starts. Unknown `BZMJ` revisions fail closed.

The remaining work is device-level QA, not another inferred ROM layout: build a
JP-baseline 3DS binary, then audit dialogue, menus, line wrapping, save isolation,
room transitions, and cutscenes on real hardware. A Debug build emits symbols and
`tmc-3ds.map` for crash-address resolution. Pair clean JP testing with
`--console-parity` when checking speedrun behaviour on the desktop PC build;
the 3DS frontend does not expose that command-line option.

## Related

- Console-Parity mode (`--console-parity`) — run-integrity switch.
- Background + divergence audit: `docs/speedrun-and-rando-port-notes-2026-06-13.md`.
