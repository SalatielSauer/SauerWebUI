## 🔍 SauerWebUI Patch Notes

### 02/07/2025 📌
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
