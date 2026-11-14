# TERRAIN for Digitone II

An unofficial custom OS for the **Elektron Digitone II**, based on OS **1.12**.
It adds **TERRAIN**, a wave-terrain synth machine, plus a set of quality-of-life mods.

**Build it in your browser:** open the project page (GitHub Pages, `docs/`), load your own copy of
Elektron's official `Digitone_II_OS1.12.syx`, download the result. Nothing is uploaded; the file never
leaves your computer. This repository contains **no Elektron firmware**.

> Unofficial and experimental. Not made, endorsed or supported by Elektron. Back up your projects
> before installing. Use at your own risk.

*(Italiano più sotto.)*

## What's inside

- **TERRAIN** machine (listed after SWARMER): a point travels over a 2-D surface; its height is the
  output. 8 banks × 8 surfaces, warp, FM-style MOD, phase distortion, land shaping (aspect, rotation,
  erosion, ridge, wind, feedback), a DRUM page (pitch sweep, filtered noise with hold/decay), sync and
  key tracking. Five pages with native-looking graphics; press an encoder to see its value.
- **SETTINGS › PERSONALIZE**
  - **NEGATIVE SCREEN**: inverted display.
  - **MOD GRAPHICS**: TERRAIN graphics glide to the new value on knob turns, randomise and FUNC + turn.
- Mods ported from [dn2_firmware_explore](https://github.com/angellinares/dn2_firmware_explore):
  more LFO destinations (FX and voice parameters), LFO ONE/HALF that hold their value, arpeggiator on
  MIDI tracks, SHUF and RAND arp modes, arp offsets in scale, MIDI out of audio tracks, reload confirm.
- Custom boot animation.

## Install / uninstall

Send the built `.syx` to the Digitone II like an official OS update (e.g. with Elektron Transfer).
To go back, send the original OS 1.12 file the same way.

## How the web builder works

`docs/patch/patch-*.json` describes the two OS sections we change (main OS, section 3; DSP blob,
section 7) as a list of operations: *copy N bytes from your original section* or *insert these bytes
of ours*. The page:

1. loads your file with dnfw's container code and runs Elektron's own integrity checks on it;
2. checks that each input section matches the SHA-256 of OS 1.12 (any other file is refused);
3. rebuilds sections 3 and 7 and checks their SHA-256 against the tested build;
4. repacks the `.syx` (dnfw), loads it again, re-runs every integrity check, and only then offers the
   download.

The OS code inside the result is byte-identical to our tested build; only the `.syx` packing (done
by dnfw instead of digikit) differs.

## Building from source

Requirements: [digikit](https://github.com/ElekLabs/digikit) at commit
`5f6be8326147fe41cf06bb19358ae2916b2f3f63` and its toolchain.

```sh
git -C digikit apply ../digikit/digikit-terrain.patch   # our changes to digikit (see below)
./tools/build.sh /path/to/digikit /path/to/Digitone_II_OS1.12.syx out.syx
python3 tools/mkpatch.py Digitone_II_OS1.12.syx out.syx docs/patch/patch-new.json   # needs dnfw's python package
```

- `modules/` – our patchkit modules: `terreno` (TERRAIN DSP + machine), `pagine` (TERRAIN pages,
  NEGATIVE SCREEN, MOD GRAPHICS), `intro`, and the ported dnfw mods.
- `digikit/digikit-terrain.patch` – changes to digikit: 40 parameters per machine, voice adapter,
  machine list, port anchors for OS 1.12, patchkit page fix.
- `tools/mkpatch.py` – turns a built `.syx` into the web patch.
- `docs/` – the GitHub Pages builder. `docs/js/dnfw/` is dnfw's code, unchanged.

## A note on graphics

Fonts are read from your own firmware at run time. Some TERRAIN page glyphs (FM DRUM-style curves,
brackets) were captured from the stock pages so the machine looks native; they are part of
`modules/pagine` and of the patch. They will be replaced by calls to the firmware's own drawing
routines in a future version, and removed on request.

## Licence and credits

AGPL-3.0 (see `LICENSE`), with the additional terms in `NOTICE`.

- Derived from **digikit by Em D - https://github.com/ElekLabs/digikit** (modified; not the original
  digikit).
- Uses and ports code from **dn2_firmware_explore** by angellinares
  (https://github.com/angellinares/dn2_firmware_explore), AGPL-3.0.

`docs/img/oled.png` holds screenshots of the TERRAIN pages taken in the emulator.

Elektron and Digitone are trademarks of Elektron Music Machines. This project is not affiliated with
Elektron and gives no rights to Elektron software or firmware.

---

## Italiano

OS personalizzato non ufficiale per **Elektron Digitone II**, basato sull'OS **1.12**: aggiunge la
macchina **TERRAIN** (sintesi wave terrain) e varie migliorie.

**Costruiscilo nel browser:** apri la pagina del progetto, carica la tua copia ufficiale di
`Digitone_II_OS1.12.syx` presa dal sito Elektron e scarica il risultato. Il file non viene caricato da
nessuna parte. Questo repository non contiene firmware Elektron.

Installazione: manda il `.syx` al Digitone II come un normale aggiornamento (per esempio con Elektron
Transfer). Per tornare indietro, manda allo stesso modo l'OS 1.12 originale. Fai prima un backup dei
progetti. Progetto non ufficiale, non supportato da Elektron: usalo a tuo rischio.
