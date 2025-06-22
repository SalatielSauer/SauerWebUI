## 🔍 SauerWebUI Patch Notes

### 22/06/2025 📌
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