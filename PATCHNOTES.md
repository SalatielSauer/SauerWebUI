## 🔍 SauerWebUI Patch Notes

### 01/11/2025 📌
- Map .cfg generator

  For a mapper, the most tedious step is probably setting up the .cfg file, all those unused default textures, all those manual replaces... not anymore.

  SauerWUI now have edit commands to make it easy to remove unused assets and generate a custom cfg:
  - `removeunusedtextures`
  - `removeunusedmmodels`

    As the name suggests, they remove all unused content from the map, giving you a clean texture browser and a clean mapmodel browser.

  - `writemapcfg <name>`

    A clean list of textures and models requires an appropriate cfg file; this command will take care of that for you, including not only textures and mapmodels, but also mapvars and mapsounds. The file is created in the `packages/base` folder using the map name by default if not provided.


- Emoji Support 💪

  If words aren’t enough, now you can add an emoji to any text input using the <e:> marker with a Unicode value, like:
  `<e:1F438>`.
  <img width="256px" src="https://raw.githubusercontent.com/SalatielSauer/misc/refs/heads/master/sauerwui_emojisupport.png">
  
- `radaroverlay <1/0>`

  Option to display a copy of the radar in the center of the screen; you can adjust the size and transparency with `radaroverlaysize` and `radaroverlayopacity`.

- Frag Announcer sounds

  Now you can feel more hyped while fragging! If you score the first blood or pull off a combo of 2 to 5 kills, you'll hear encouraging messages.
  There’s also an Intermission announcement.

- Scoreboard Flag

  Players carrying the flag are visible on the scoreboard accompanied by a flag icon, just to satisfy your curiosity.

- Controller Support

  If you're tired of blaming your keyboard or mouse, now you can blame your controller too; just plug it in and you should be ready to ~~suffer~~ go.

  You can adjust the sensitivity and binds in your `autoexec.cfg` file (see `data/wui/joystickbinds.cfg`, note: changes to this file will only take effect if you run it manually with `/exec`).

- Keep command history

  You can use `maxhistorykeep` to control how many commands will be retained across different sessions in your command history.



### 27/07/2025
A new cutscene system with a bunch of commands for all your filmmaker needs:
<iframe width="460" height="215" src="https://www.youtube.com/embed/k9ephMR1d5U" frameborder="0" allowfullscreen></iframe>

<details>
  <summary>show cutscene commands</summary>

  - `playcutsceneat <file> <start> <end>`
  - `cutsceneplaybackstart <file> <start> <end>`
  - `cutscenerecordstart <file>`
  - `cutscenerecordover <file>`
  - `cutscenerecordcontinue`
  - `cutscenerecordpause`
  - `cutscenerecordend`
  - `cutscenerecordload <file>`
  - `cutscenerecordrestart`
  - `cutscenerecordsettime <minute:second>`
  - `cutscenerecordsetframe <frame>`
  - `cutscenecamdebug <1/0>`
  - `cutscenecamdebugsize <0.25/4.0>`
  - `cutscenecamdebugpath <1/0>`
  - `cutscenecamdebugpathstep <1/100>`
  - `cutscenecamlerpfrom`
  - `cutscenecamlerpto <ms>`
  - `cutscenecamclear <direction>`
  - `cutsceneactorclear <id>`
  - `cutscenecurrentframe`
  - `cutsceneframeslen`
  - `cutscenecurrentfile`
  - `cutscenestate <state> <time>`
</details>

Not only that, cutscenes also allow dynamic display of subtitles, audio, images, and mapmodels!

All part of a custom format supported by SauerWUI: `.ctscn`. See more about cutscenes in the [readme](https://github.com/SalatielSauer/SauerWebUI?tab=readme-ov-file#cutscene-playback).

There is also a new `.obpy` format for exporting maps to Blender, including textures, vcommands, lightmaps, mapvars, and skybox:
- `writeobpy <name>` (map)
- `writemmobpy <name>` (mapmodels)

Since it’s a custom format based on .obj, it requires an add-on to be imported into Blender:
[Blender .obpy Importer](https://gist.github.com/SalatielSauer/397881f744c69688b644d4efffe2ce25).

<hr>

### 07/07/2025
- `dumpmmodels <name> <optional texture path>`
- `dumpmaterials <name>`

  commands to export all materials and mapmodels (with textures) of the current map as a single .obj and .mtl file;

- `particletex_ID` new mapvar-based particle that allows displaying dynamic texts with CubeScript. (`/newent particle 14 <id> <size> <color> <orientation>`).

  <img width="512px" src="https://raw.githubusercontent.com/SalatielSauer/misc/refs/heads/master/sauerwui_5.png">

### 02/07/2025
- `importobj <file> <size>` now supports material colors (not all colors may be displayed depending on the size parameter you set);
- to make things easier, the binary can now be downloaded with the updater; just restart the client as soon as an update finishes;
- new `writeobjuvmap <name> <dump lightmap texture 1/0>` to export a map as a .obj file containing lightmap coordinates;
- new `importlms` to replace the current map's lightmap with an external image (the image must be "indexed colors" type; see below for how to convert it using GIMP).
<details>
  <summary>how to process lightmaps using GIMP</summary>
  <img src="https://github.com/user-attachments/assets/19733866-b470-4e9f-a3de-186460216bcd">
</details>

### 28/06/2025
- the updater can now ignore specific files;
- new experimental `level_monsterai` for custom ai scripting;
- new `loadmonster` command to load custom monsters from .cfg files;
- `safedo` can no longer assign aliases;
- new `safemmodel` command to load model cfgs with whitelisted commands.

### 22/06/2025
- new /importobj command;
- crosshair no longer appears in the main menu;
- when running multiple processes, each will use a separate cef cache, no longer breaking cef.

### 19/06/2025
- experimental safedo to run only whitelisted commands;
- new setmapvar and getmapvar for custom mapvars;
- new mapassets mapvar for defining downloadable assets via JSON;
- mapvars no longer adhere to maxstrlen limit;
- new loopvarsbyprefix and prunevarsbyprefix commands.

### 15/06/2025
- new guiimageurl command to load images from the web into gui menus;
- new guiimagestring command to load images from a Base64 string;
- new text prefix for customizable rgb colors with "^<rgb:255/255/255>text";
- guibutton and guitext now supports a size parameter.

### 04/06/2025
- experimental multiselection mode (multiselmode, addselection, clearselections, multiselcount).
