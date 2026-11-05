# GA36 Puppy — editable themes

This experimental theme engine is based on Puppy (SDL2) and keeps the existing `apps.puppy` game launcher, archive settings and ROM commands intact.

**Important:** The format is **theme.ini**, not EmulationStation `theme.xml`. It offers comparable editable layout/color/background controls, but is **not compatible with EmulationStation XML themes**.

## Install (HOME partition, no OS replacement)

1. Keep the original Puppy and GoodLuckOS unchanged.
2. Copy `themes/n64-unified/theme.ini` into `A:\.config\puppy\themes\n64-unified\theme.ini` alongside the 20 wallpapers already installed.
3. Copy `themes/arcade-blue/theme.ini` to `A:\.config\puppy\themes\arcade-blue\theme.ini`.
4. Install the newly built experimental `puppy` in `A:\puppy_theme_test\puppy` (not in the original OS or binaries) and use the existing preview app.
5. In the Systems page press **X** to cycle themes, **A** to enter games; in Games press **A** to launch and **B** to return. Keyboard: T switches theme, Enter chooses, Escape goes back.
6. The active name is stored in `A:\.config\puppy\active_theme.txt`.
7. Edit theme.ini on Windows and reopen preview (or switch away and back with X) to apply.

## What can be edited without rebuilding?

* `[systems]`: left panel and list position, item spacing, text width, visible list range.
* `[games]`: list panel and selected game position.
* `[art]`: box art panel and image location/size; `fit=contain` or `fit=cover`.
* `[fonts]`: list and selected font sizes; `[theme] font` supplies an installed TTF path.
* `[colors]`: text, highlight, panels, bars, fallback background. Hex `#RRGGBB` or `#RRGGBBAA`.
* `[theme] backgrounds`: relative to the theme folder; e.g., `../n64-unified/backgrounds`.
* `[bars]`: header/footer positioning.

Wallpaper names follow the sanitized `NAME` from `apps.puppy`, e.g. `Game Boy Advance` -> `game-boy-advance.png`, `Playstation` -> `playstation.png`. PNG/JPG/JPEG accepted. Wallpapers fill 640x480. Existing ROM thumbnail files under `roms/<system>/icons` remain the Games images.

To add another theme, create `A:\.config\puppy\themes\my-theme\theme.ini`, give it a [theme] name and backgrounds path. It appears as an X option.

These files do not change SD boot partitions, /usr/bin/puppy, or the installed emulators.

## Current limitations

* No EmulationStation theme.xml interpreter. No animations, video preview, or nested theme variables.
* The UI is fixed at logical 640x480 (GA36 framebuffer); avoid rectangles outside the screen.
* Changes to theme.ini reload when the theme is reactivated or Puppy restarts, not automatically at each file save.
* This is an experimental build until the GitHub ARM32 workflow succeeds and it is tested on GA36.
