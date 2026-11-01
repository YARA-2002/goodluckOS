# Puppy N64-inspired multi-system theme (experimental)

This modification changes **Puppy**, not RetroArch or the console's operating system.

## Included behavior
- A common retro-console design across **all** Puppy game sections, including sections defined in /home/player/apps.puppy.
- A distinct accent color for each section, using a blue/green/red/yellow 1990s console-inspired palette.
- Optional full-screen 640x480 background and optional logo per section.
- Box covers shown in full without cropping (within the existing 192x128 cells).
- Existing icons, commands, emulation cores, controller input, and game launching preserved.
- Missing images cause a fallback to the built-in colored background and category title.

## HOME (SD card) paths

The HOME partition appears as D: in Windows when it is assigned that drive letter.

Place your own PNG artwork here:

```
D:\.config\puppy\themes\n64-unified\backgrounds\nintendo-64.png
D:\.config\puppy\themes\n64-unified\logos\nintendo-64.png
D:\.config\puppy\themes\n64-unified\backgrounds\playstation.png
D:\.config\puppy\themes\n64-unified\logos\playstation.png
D:\.config\puppy\themes\n64-unified\backgrounds\psp.png
D:\.config\puppy\themes\n64-unified\logos\psp.png
```

On the device, the corresponding root is
`/home/player/.config/puppy/themes/n64-unified/`.

Examples of category slug mapping:

| Category name in apps.puppy | Image stem |
|---|---|
| Nintendo 64 | nintendo-64 |
| Playstation | playstation |
| PSP | psp |
| Super Nintendo | super-nintendo |
| Game Boy (Color) | game-boy-color |
| Sega Genesis | sega-genesis |

Each game system can use `roms/<system>/covers/<exact ROM filename without extension>.png` (or jpg/jpeg) on HOME. The existing `roms/<system>/icons/` directory remains supported.

A custom [ARCHIVE] defined in HOME's `apps.puppy` is automatically included. No individual section needs recompilation.

## Safe testing

Only test on the **secondary / experimental SD card**. Preserve the original Puppy binary and normal launch path. This branch builds a standalone experimental `puppy` executable; it is not a verified replacement until tested on hardware.

The repository ships no copyrighted Nintendo, Sony, Sega or game artwork. Users can supply their own permitted artwork.

## Known scope limitations

This is a first-stage visual enhancement, not a full 3D frontend. The 640x480 card layout and existing category navigation remain. Future design iterations can add a dedicated console selection screen and a larger artwork panel after hardware testing.
