# 02: Choose the icon for VITA+

Status: resolved
Type: grilling
Label: ready-for-agent

## Question

Which candidate is the VITA+ icon? There are two sets of five.

- **Set B (new, based on the current icon):** `../icons/set-b/`. The current
  handheld drawing, recolored in PlayStation blue (bright `#0070D1`, dark
  `#003791`), with the plus on the screen. The overview is
  `../icons/set-b/contact-sheet.png`. The files are 512 x 512 PNG with a
  transparent background. `recolor.py` makes them from
  `android/app/src/main/res/mipmap/ic_launcher_plus.png`, so a color can change
  in one line.
- **Set A (new drawings):** `../icons/set-a/`. The overview is
  `../icons/set-a/contact-sheet.png`. Each candidate is an SVG (512 x 512) and a
  preview PNG. Every SVG has two groups, `background` and `foreground`, so it
  can become an Android adaptive icon. The foreground stays inside a circle of
  radius 165 around the center (the launcher may crop the rest).

## Set B

| N | Colors | Plus | Notes |
| --- | --- | --- | --- |
| B1 | PlayStation blue screen and rim, navy body | Dark blue outline only, as in the current Vita3K+ icon | Most faithful. The plus is weak at 48 px. |
| B2 | Same as B1 | White plus | The plus reads at once, also at 48 px. Plain and strong. |
| B3 | Ice white screen, light blue rim, navy body | PlayStation blue plus | The most different from the old icons (light rim). Good on a dark home screen. |
| B4 | Cyan screen and rim (`#00A8E8`), navy body | White plus | Brighter and more cyan than the PlayStation logo blue. |
| B5 | Vertical blue gradient on the screen and rim | White plus | Looks the most finished. The gradient costs some clarity at 48 px. |

My ranking for set B: B2, B5, B4, B3, B1. B2 keeps the old drawing and gets a
clear plus with two colors.

## Set A

| N | Idea | Colors | Notes |
| --- | --- | --- | --- |
| A1 | A big white V with a yellow plus at its top right | violet to blue | Clear at 48 px. The plus overlaps the V. |
| A2 | A flat handheld with a plus on its screen | dark blue, teal, yellow | Says "handheld" at once. More detail, so it is weaker at 48 px. |
| A3 | The four face-button shapes placed as a plus | dark violet, green, red, blue, pink | Thin lines, weak at 48 px. The shapes are the ones on the PlayStation buttons. Check that it is acceptable for the project. The overlay buttons of the app already use them. |
| A4 | A yellow plus with an orange and magenta edge (extruded) | purple, yellow | Keeps the palette of the current Vita3K and Vita3K+ icons. Best at 48 px. Has no V and no handheld. |
| A5 | Pixel art V and plus | dark slate, teal, yellow | Retro look. Weakest at 48 px. |

My ranking for set A: A4, A1, A2, A5, A3. Candidate A4 is the easiest to tell
apart from the Vita3K and Vita3K+ icons on the home screen and keeps the family
colors. Candidate A1 is the best choice if the icon should stop looking like
the old handheld drawings.

## Also decide

- Does the icon inside the app (welcome screen, apps list, documents provider
  roots) use the same picture? Default: yes. See issue 03.
- Do you want another color for the plus or the body? (For set B, change the
  hex values at the end of `recolor.py` and run it.)

## Answer

The user chose **B3 (ice screen)** on 2026-09-30, without changes:
`icons/set-b/ps-blue-3-ice-screen.png`. Ice-white screen (`#CFE8FF`), light blue
rim, navy body (`#001A4D`), PlayStation blue plus (`#0070D1`).

Background for the adaptive icon: solid navy `#0A1633`. I rendered B3 on navy,
a PlayStation blue gradient, a dark gradient and white
(`icons/set-b/b3-backgrounds.png`, circle and rounded square masks, and 48 px).
Navy and the dark gradient show the light rim best. The blue gradient makes the
blue body blend into the background, and on white the rim fades. Use navy.

## Comments
