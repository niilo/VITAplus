# 08: Jak in-game music bank never starts on either driver

Status: open
Type: research
Label: ready-for-agent
Blocked by: 09

## Goal

Stock in-game mixes effects but no background music. Turnip in-game mixes
nothing (issue 09). Once issue 09 is fixed, the music path still needs its
own fix. This ticket tracks the music path.

## Evidence

Device log `/tmp/vitalogs/vita3k.log`, build `v1.2.1-dev.18` (`00514fb7`),
PCSA00080, Pocket S.

Menu boots (0, 2, 4) on both drivers log:

- `FluxIO WITH FIOS MUST BE INITIALIZED BEFORE AUDIO or you will only get SFX.`
- `SCREAM streaming intialized.`
- `AAC Decoder Started ...`
- `FluxFOpen: /audio/Menu.bnk`
- `StartMenuMusic()...>>>>Starting music via SCREAM...<<<`
- `MusicHandle = 85000002`

In-game boots (1, 3, 5) on both drivers log none of these. No `.bnk` open.
No `SCREAM` line. No `StartMenuMusic`. Boot 1 (stock in-game) creates 12,424
`NGSRATE` resamplers from `PCMDecoderState`, so effects mix. The music bank
never starts in `Jak1.self` in these logs.

Menu TTY also logs `Music delayed since it is played via SCREAM and bank is
not loaded yet...`. In-game never reaches even that line.

## What to check

1. Which file or stream the game opens for in-game music. Trace `sceIoOpen`
   after `sceAppMgrLoadExec "app0:Jak1.self"` past `jak1.psarc` and the
   savedata opens. The menu opens `/audio/Menu.bnk` through Flux. Find the
   in-game equivalent.
2. Whether FluxIO init order is the cause. The menu warns init must run
   before audio or only SFX plays. Check if `Jak1.self` changes the order.
3. Whether the decoder differs. Menu music uses the AAC decoder and
   `avPlayer AudioDec`. In-game effects use `PCMDecoderState` resamplers.
   Find which decoder the in-game music needs and whether its init is
   missing.
4. Whether this reproduces on desktop Linux with the same title. If yes, no
   device is needed for the fix loop.

## Acceptance

- In-game boot logs a music bank or stream start after `Jak1.self` loads.
- `NGSRATE` or decoder lines cover the music voices, not only PCM effects.
- Stock in-game plays effects and music. Turnip in-game is covered by issue
  09 plus this one.
- `./format.sh` is clean if code changed.

## Comments
