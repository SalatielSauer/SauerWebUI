# Sauerbraten + JavaScript = Wonderfulness

![](https://raw.githubusercontent.com/SalatielSauer/misc/refs/heads/master/sauerwui_2.png)

## What is this?
**SauerWebUI** is a mod for [Sauerbraten](http://sauerbraten.org/) that brings web technologies to the [Cube Engine](http://cubeengine.com/), allowing users to create interfaces entirely in HTML, CSS, and JavaScript, and to use any library available for those environments.

And of course, you can execute [CubeScript](https://github.com/CubeScript) from JavaScript and vice versa!

Also, for now, there are some miscellaneous experiments that you will see below.

## Installation
There is currently a pre-compiled binary available for Windows x64 only (tested with Windows 10 22H2):

[Releases page](https://github.com/SalatielSauer/SauerWebUI/releases/latest)

[Download SauerWebUI (07-07-2025) ZIP](https://github.com/SalatielSauer/SauerWebUI/releases/download/v07-07-2025/sauerwebui_jsquare_07-07-2025.zip)

[Download SauerWebUI (07-07-2025) installer](https://github.com/SalatielSauer/SauerWebUI/releases/download/v07-07-2025/sauerwebui_jsquare_07-07-2025_installer.exe)

The installation is meant to go alongside the vanilla client, that's why essential folders like packages are not included.

When running the installer, select the folder where your Sauerbraten is (make sure you are not accidentally creating a directory inside it).

If you are using the ZIP, just extract it to your Sauerbraten installation folder, no files will be overwritten.

## Building
If you want to build, there is a Visual Studio 2022 project file in `src/vs/`.

If you can help with a setup to build on Linux, it would be appreciated.

## Known Issues
It won't work if you install it side by side with a P1xbraten installation (reasons I still don't know), so I recommend installing it alongside the vanilla installation.

<hr>

## Functions

- [WUI](#wui)
  - [`javascript [ ... ]`](#javascript---)
  - [WUI Standard Library](#wui-standard-library)
  - [`exec <file.js>`](#exec-filejs)
- [Miscellaneous Commands](#misc)
  - [`guiimagestring <base64 string> [command] <size> <overlay 0/1>`](#guiimagestring-base64-string-command-size-overlay-01)
  - [`guiimageurl <url> [command] <size> <overlay 0/1>`](#guiimageurl-url-command-size-overlay-01)
  - [`guitext <text> <icon> <size>`](#guitext-text-icon-size)
  - [`guibutton <text> [command] <icon> <size>`](#guibutton-text-command-icon-size)
  - [Custom RGB text support](#custom-rgb-text-support)
  - [`addselection`](#addselection)
  - [`multiselmode <0/1>`](#multiselmode-01)
  - [`loopvarsbyprefix <var> <prefix> [command]`](#loopvarsbyprefix-var-prefix-command)
  - [`prunevarsbyprefix <prefix>`](#prunevarsbyprefix-prefix)
  - [`mapassets [json]`](#mapassets-json)
  - [`mapcfg [command]`](#mapcfg-command)
  - [`safedo [command]`](#safedo-command)
  - [`setmapvar <var> <value>`](#setmapvar-var-value)
  - [`getmapvar <var>`](#getmapvar-var)
  - [`setmapvar particletex_ID [command]`](#setmapvar-particletex_id-command)
  - [`importobj <file> <size>`](#importobj-file-size)
  - [`importlms`](#importlms)
  - [`writeobjuvmap <name> <dump lightmap texture 1/0>`](#writeobjuvmap-name-dump-lightmap-texture-10)
  - [`dumpmmodels <name> <optional texture path>`](#dumpmmodels-name-optional-texture-path)
  - [`dumpmaterials <name>`](#dumpmaterials-name)
  - [`loadmonster <config>`](#loadmonster-config)
  - [`clearmonsters`](#clearmonsters)
  - <details>
	<summary>Monster Commands</summary>

	- [`monstername <text>`](#monstername-text)
	- [`monstermodel <modelpath>`](#monstermodel-modelpath)
	- [`monstervwep <modelpath>`](#monstervwep-modelpath)
	- [`monsterweapon <value>`](#monsterweapon-value)
	- [`monsterspeed <value>`](#monsterspeed-value)
	- [`monsterhealth <value>`](#monsterhealth-value)
	- [`monsterfreq <value>`](#monsterfreq-value)
	- [`monsterlag <ms>`](#monsterlag-ms)
	- [`monsterrate <ms>`](#monsterrate-ms)
	- [`monsterpain <ms>`](#monsterpain-ms)
	- [`monsterloyalty <value>`](#monsterloyalty-value)
	- [`monsterbscale <value>`](#monsterbscale-value)
	- [`monsterweight <value>`](#monsterweight-value)
	- [`monsterpainsound <value>`](#monsterpainsound-value)
	- [`monsterdiesound <value>`](#monsterdiesound-value)
	- [`monsterpuppet <1/0>`](#monsterpuppet-10)
	- [`level_monsterai = [command]`](#level_monsterai--command)
	</details>

## WUI

- ### `javascript [ ... ]`

	This command can be executed within a CubeScript environment, allowing you to run JavaScript code in its own function scope (so variables won’t interfere with others unless you assign them to global objects).

	**Example:**
	```js
	newgui test [
		guibutton "say hello" [
			javascript [
				console.log("hello from JS! 2 + 2 =", 2+2);
			]
		]
	]
	showgui test
	```

	To share functions between different scopes, simply define them on the global `window.` object (once it's available, i.e., after `data/wui/wui.cfg` is loaded).

	**Example:**
	```js
	javascript [
		function say_hello(name) {
			console.log(`hello, ${name}! from another scope`);
		}

		window.say_hello = say_hello; // Share function globally
	]

	newgui test [
		guibutton "say hello" [
			javascript [
				say_hello(@(escape (getname))); // Now accessible
			]
		]
	]
	showgui test
	```
	As you can see, you can use some escape tricks to pass data from CubeScript into JavaScript functions before they are executed.

- ## WUI Standard Library
	There is an (incomplete) library available at `data/wui/wui_lib.js` that contains some JavaScript functions for creating interfaces and communicating with CubeScript. Everything is still experimental and rough, but it can serve as a starting point if you want to help :)

	- `window.cubescript(code, callback);`
		Executes CubeScript from the JavaScript environment. Example:
		```js
		let cubescript = `
			echo "^f8Hello from ^f2CubeScript!"
			result (concat "five times two =" (* 5 2))
		`;

		window.cubescript(cubescript, (result => {
			console.log(`CubeScript result is: \f0${result}`);
		}))
		```
	- `window.wui.createMenu(id, element, x, y, title, options);`
		
		Creates a menu positioned at x y with an optional title. `element` is a document element containing the menu body, example:

		```js
		function _create_menu() {
			const body = document.createElement('div');
			body.innerHTML = "<h2>hello</h2>";

			window.wui.createMenu('mainmenu', body, '50%', '50%', 'Title', { allowExit: true });

			window.wui.showMenu('mainmenu');
		}
		```
		`50%` means the menu will be displayed at the center of the screen if a mouse event is not specified.

		Possible `options` include:
		- allowDrag
		- allowExit
		- allowFullscreen

	- `window.wui.showMenu(id, event);`
	- `window.wui.hideMenu(id, keep_cursor);`
	- `window.wui.clearMenu(id, keep_cursor);`

		Displays, hides, or deletes a specific menu by its id. If an event is obtained through something like a button click, it can be passed so that the menu is positioned at the mouse location. If keep_cursor is set to 1, the cursor will be kept visible when the menu disappears.

	- `window.wui.createButton(text, onClick);`

		Creates a button with the specified text. When clicked, it triggers the `onClick` callback.

	- `window.wui.createTextInput(placeholder, onChange);`

		Creates a single-line text input field with a placeholder. Triggers the `onChange` callback when the text changes.

	- `window.wui.createTextArea(placeholder, onChange);`

		Creates a multi-line text area with a placeholder. Triggers the `onChange` callback when the text changes.

	- `window.wui.clearAll();`

		Removes all menus currently created by WUI.

	- `window.gameassets`

		Just a quick and provisional class to hold local paths relative to the installation folder and other useful things.
		- `window.gameassets.path_packages`
		- `window.gameassets.path_maps`
		- `window.gameassets.allmapnames`
---

- ### `exec <file.js>`

	Basically the same as the `javascript` command, but allows you to execute any JavaScript file from a folder (and CubeScript files as originally supported).

---

If you have a better idea for an interface, or improvements for the current one, feel free to suggest them. While much of the current functionality revolves around the default WUI, the goal is to allow multiple interfaces to coexist, so players can choose which one to use at any given time.

There are some experimental files in the `data/wui` folder, some of which don’t lead to anything yet. Play around with them and see if you can get anything useful :)

![](https://raw.githubusercontent.com/SalatielSauer/misc/refs/heads/master/sauerwui_3.png)
*an experimental server browser packed with workarounds*

## Misc
- ### `guiimagestring <base64 string> [command] <size> <overlay 0/1>`
	Works similarly to `guiimage`, but instead of specifying a file path for the image, you can provide a string containing an image encoded in base64.

- ### `guiimageurl <url> [command] <size> <overlay 0/1>`
	Just like `guiimagestring`, but allows loading an image from a url.

- ### `guitext <text> <icon> <size>`
- ### `guibutton <text> [command] <icon> <size>`
	It is now possible to set the size of a guitext using the third parameter of the command, or the fourth parameter in the case of guibutton (1 is the default size).

- ### Custom RGB text support
	Text supports a new color marker in addition to the traditional `^f0` - `^f8`. Simply insert `^<rgb:255/0/255>` before any text to specify a custom color using the RGB format.

- ### `addselection`
	Allows adding a new selection to the existing ones, enabling manipulation of multiple geometry or texture regions at once, including non-contiguous areas. (requires `multiselmode 1`)
- ### `multiselmode <0/1>`
	Enables experimental multi-selection mode (`addselection`).

- ### `loopvarsbyprefix <var> <prefix> [command]`
	Iterate through all existing variables that match 'prefix' and store them in 'var' when executing the command, similar to `looplist`.

- ### `prunevarsbyprefix <prefix>`
	Remove all variables that match 'prefix', preventing them from being saved in the config.cfg file.

- ### `mapassets [json]`
	Map variable to create an automatic menu with download buttons for external assets. It is sent via sendmap and can go in your map.cfg by following the json format:
	```json
	mapassets [
		{
			"assetname": "My_Custom_Asset",
			"assetsrc": "https://github.com/CubeScript/Sauer-Vslot-Text-Sender/releases/download/v0.1/vsts.zip",
			"assetthumb": "https://avatars.githubusercontent.com/u/9287152?s=40&v=4",
			"assetversion": 1
		},
		{
			"assetname": "Skin-Colorizer",
			"assetsrc": "https://github.com/CubeScript/Sauer-Skin-Colorizer/releases/download/v0.4-lite/skincolorizer.zip",
			"assetthumb": "https://github.com/SalatielSauer/misc/raw/master/skincolorizerlite_demo3.gif?raw=true",
			"assetversion": 1
		}
	]
	```
	As soon as the map loads (`mapstart`), the player will be prompted with a menu allowing them to download each asset, which will be added to the `home/assets/<asset name>` folder. Assets must have a unique name. You can suggest asset updates by bumping the `assetversion`.

	![](https://raw.githubusercontent.com/SalatielSauer/misc/refs/heads/master/sauerwui_1.png)

- ### `mapcfg [command]`
	Allows sharing map cfgs with other players via sendmap; the commands will be executed automatically during getmap.
	
	If the current `mapcfg` map variable is empty and you have a local map.cfg file, the contents of your local map.cfg will automatically be added to the `mapcfg` map variable during sendmap.

 	Only a limited list of commands is available, and you can see them below.

- ### `safedo [command]`
	Similar to `do`, but executes CubeScript with a limited list of allowed commands.
	Some of the available commands are:
	```
    "texture", "safemmodel", "addzip", "removezip",
    "shader", "setshader", "defuniformparam", "findfile",
    "texturereset", "mapmodelreset", "maptitle", "echo",
    "sub*", "str*", "at" "indexof", "if",
    "concat*","format", "get*", "ent*", "timeremaining",
    "mdl*", "md2*", "md3*", "md5*", "obj*", "smd*", "iqm*"
	```

- ### `setmapvar <var> <value>`
	Create custom map variables that are stored along with the map file.

- ### `getmapvar <var>`
	Retrieve the value of a map variable (such as those created with `setmapvar`).

- ### `setmapvar particletex_ID [command]`
	Defines a CubeScript that must return a string to be displayed on particle number 14 (`/newent particles 14 2`), example:
	```
	setmapvar particletex_2 [ format "hello %1" (getname) ]
	```
	The second attribute of the particle (after its name) defines the mapvar ID.

	The third attribute defines the size of the text.

	The fourth attribute defines the color.

	The fifth atrribute defines the orientation (0 = camera, 1 = left, 2 = right, 3 = back, 4 = front, 5 = bottom, 6 = top).

	![](https://raw.githubusercontent.com/SalatielSauer/misc/refs/heads/master/sauerwui_5.png)


- ### `importobj <file> <size>`
	Imports an .obj model file as cubes in a voxelized style and places it in the current selection, the larger the size, the more detailed the geometry will be.

	![](https://raw.githubusercontent.com/SalatielSauer/misc/refs/heads/master/sauerwui_4.png)

- ### `importlms`
	Replaces the current map's lightmap with an external image (the image must be "indexed colors" type; see below for how to convert it using GIMP).
	It will look for lightmaps with the same names generated by `/dumplms`.

- ### `writeobjuvmap <name> <dump lightmap texture 1/0>`
	Similar to `writeobj`, but exports a map as an .obj file containing lightmap coordinates. You can then use Blender to import the obj file with UV coordinates.

- ### `dumpmmodels <name> <optional texture path>`
	Exports all mapmodels of the current map as a single .obj including their textures in a .mtl file.

	![](https://raw.githubusercontent.com/SalatielSauer/misc/refs/heads/master/sauerwui_lightmaps_models_thumb.png)

- ### `dumpmaterials <name>`
	Exports all materials of the current map as a single .obj, each with a different color.


<details>
  <summary>how to process lightmaps using GIMP</summary>
  <img src="https://github.com/user-attachments/assets/19733866-b470-4e9f-a3de-186460216bcd">
</details>

- ### `loadmonster <config>`
	Load a monster definition from a cfg file. The cfg should use the `monster` commands below to describe the monster.

- ### `clearmonsters`
	Clear previously loaded monster definitions (and any spawned monsters).
<details>
<summary>Show monster* commands</summary>

- ### `monstername <text>`
	Sets the display name for the monster.

- ### `monstermodel <modelpath>`
	Model used when rendering the monster.

- ### `monstervwep <modelpath>`
	Optional weapon model attached for third person view.

- ### `monsterweapon <value>`
	Weapon used when attacking. Provide the integer value of the weapon
enum. Values are:

	`0` GUN_FIST, `1` GUN_SG, `2` GUN_CG, `3` GUN_RL, `4` GUN_RIFLE,
	`5` GUN_GL, `6` GUN_PISTOL, `7` GUN_FIREBALL, `8` GUN_ICEBALL,
	`9` GUN_SLIMEBALL, `10` GUN_BITE, `11` GUN_BARREL.
	
- ### `monsterspeed <value>`
- ### `monsterhealth <value>`
- ### `monsterfreq <value>`
	Spawn frequency used by DMSP.

- ### `monsterlag <ms>`
	Delay between starting to attack and firing.

- ### `monsterrate <ms>`
	Delay between decision updates when pursuing a target.

- ### `monsterpain <ms>`
	Time the monster remains in the pain state after being hit.

- ### `monsterloyalty <value>`
	Higher values make it less likely to retaliate against allied monsters.

- ### `monsterbscale <value>`
	Bounding box scale (affects the monster's size).

- ### `monsterweight <value>`
	Weight used in physics calculations.

- ### `monsterpainsound <value>`
	Integer sound id played when the monster is hurt.
	
	Example values:
	`32` S_PAINO, `33` S_PAINR, `35` S_PAINE, `37` S_PAINS ...

- ### `monsterdiesound <value>`
	Integer sound id played upon death.
	
	Example values:
	`23` S_DIE1, `34` S_DEATHR, `36` S_DEATHE, `38` S_DEATHS,
	`40` S_DEATHB.

- ### `monsterpuppet <1/0>`
	Enables experimental `level_monsterai`:
- ### `level_monsterai = [command]`
	Experimental command to get SP data and run monster actions.
	```
	level_monsterai = [
		local id etype edist evis egun ehealth earmour estate eyawpitch myawpitch mpos mgun mhealth mstate mmillis mmove manger mblocked

		id        = $arg1     // monster tag
		etype     = $arg2     // enemy type
		edist     = $arg3     // enemy distance
		evis      = $arg4     // enemy visible
		egun      = $arg5     // enemy gun
		ehealth   = $arg6     // enemy health
		earmour   = $arg7     // enemy armour
		estate    = $arg8     // enemy state
		eyawpitch = $arg9     // enemy yaw pitch
		myawpitch = $arg10    // monster yaw pitch
		mpos      = $arg11    // monster position
		mgun      = $arg12    // monster weapon
		mhealth   = $arg13    // monster health
		mstate    = $arg14    // monster state
		mmillis   = $arg15    // lastmillis
		mmove     = $arg16    // monster movement
		manger    = $arg17    // monster anger
		mblocked  = $arg18    // monster blocked

		// result 0 				// default AI
		// result [1 "x y z"] 		// attack position (or enemy by default)
		// result [2 move strafe] 	// apply monster movement
		// result [3 jumping] 		// monster jump
		// result [4 yaw pitch] 	// monster yaw / pitch
		// result [5 sound]			// play sound ID

		// push the monster when it is closer
		result (? (< $edist 20) (? $evis [2 -1 -1] [2 1 1]) [2 0 0])
	]
	```

</details>

<hr>

### Visual Studio Code Syntax Highlighting for CubeScript + JavaScript
If you use VSCode, there is an extension available that handles syntax highlighting for JavaScript embedded in CubeScript (inside `javascript [ ]` blocks): [CS+JS syntax highlighting for VSCode](https://gist.github.com/SalatielSauer/ecdd6c8fd8a5f2dfb5835ac273fe21db).


<hr>

**SauerWebUI** is a mod of [Cube 2 Sauerbraten](http://sauerbraten.org/) and uses [CEF (Chromium Embedded Framework)](https://github.com/chromiumembedded/cef).

by [@SalatielSauer](https://github.com/SalatielSauer)

<hr>

```
Chromium Embedded Framework
Copyright (c) 2008-2020 Marshall A. Greenblatt. Portions Copyright (c)
2006-2009 Google Inc. All rights reserved.
-------------------------------------------
Cube 2 Sauerbraten
Copyright (c) 2001-2020 Wouter van Oortmerssen, Lee Salzman, Mike Dysart, Robert Pointon, and Quinton Reeves.
```