# 02: Choose the icon for VITA+

Status: open
Type: grilling
Label: ready-for-human

## Question

Which of the five candidates in `../icons/` is the VITA+ icon? The overview
is `../icons/contact-sheet.png`. Each candidate is an SVG (`candidate-N-*.svg`,
512 x 512) and a preview PNG. Every SVG has two groups, `background` and
`foreground`, so it can become an Android adaptive icon. The foreground stays
inside a circle of radius 165 around the center (the launcher may crop the
rest).

| N | Idea | Colors | Notes |
| --- | --- | --- | --- |
| 1 | A big white V with a yellow plus at its top right | violet to blue | Clear at 48 px. The plus overlaps the V. |
| 2 | A flat handheld with a plus on its screen | dark blue, teal, yellow | Says "handheld" at once. More detail, so it is weaker at 48 px. |
| 3 | The four face-button shapes placed as a plus | dark violet, green, red, blue, pink | Thin lines, weak at 48 px. The shapes are the ones on the PlayStation buttons. Check that it is acceptable for the project. The overlay buttons of the app already use them. |
| 4 | A yellow plus with an orange and magenta edge (extruded) | purple, yellow | Keeps the palette of the current Vita3K and Vita3K+ icons. Best at 48 px. Has no V and no handheld. |
| 5 | Pixel art V and plus | dark slate, teal, yellow | Retro look. Weakest at 48 px. |

My ranking for the launcher: 4, 1, 2, 5, 3. Candidate 4 is the easiest to tell
apart from the Vita3K and Vita3K+ icons on the home screen and keeps the family
colors. Candidate 1 is the best choice if the icon should stop looking like
the old handheld drawings.

## Also decide

- Does the icon inside the app (welcome screen, apps list, documents provider
  roots) use the same picture? Default: yes. See issue 03.
- Is the plus yellow or another color? (Only if the answer changes the colors
  of candidate 1, 2 or 4.)

## Answer

(Fill in: the number, and any change, for example "4, with a pink plus".)

## Comments
