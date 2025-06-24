# Sauerbraten + JavaScript = Wonderfulness

![](https://raw.githubusercontent.com/SalatielSauer/misc/refs/heads/master/sauerwui_2.png)

## What is this?
**SauerWebUI** is a mod for [Sauerbraten](http://sauerbraten.org/) that brings web technologies to the [Cube Engine](http://cubeengine.com/), allowing users to create interfaces entirely in HTML, CSS, and JavaScript, and to use any library available for those environments.

And of course, you can execute [CubeScript](https://github.com/CubeScript) from JavaScript and vice versa!

Also, for now, there are some miscellaneous experiments that you will see below.

## Installation
There is currently a pre-compiled binary available for Windows x64 only (tested with Windows 10 22H2):

[Download SauerWebUI (21-06-2025) ZIP](https://github.com/SalatielSauer/SauerWebUI/releases/download/v0.1-test/sauerwebui_jsquare_21-06-2025.zip)

[Download SauerWebUI (21-06-2025) installer](https://github.com/SalatielSauer/SauerWebUI/releases/download/v0.1-test/sauerwebui_jsquare_21-06-2025_installer.exe)

The installation is meant to go alongside the vanilla client, that's why essential folders like packages are not included.

When running the installer, select the folder where your Sauerbraten is (make sure you are not accidentally creating a directory inside it).

If you are using the ZIP, just extract it to your Sauerbraten installation folder, no files will be overwritten.

## Building
If you want to build, there is a Visual Studio 2022 project file in `src/vs/`.

If you can help with a setup to build on Linux, it would be appreciated.

## Known Issues
It won't work if you install it side by side with a P1xbraten installation (reasons I still don't know), so I recommend installing it alongside the vanilla installation.

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
	The available commands are:
	```
    "texture", "safemmodel", "addzip", "removezip",
    "shader", "setshader", "defuniformparam", "findfile",
    "texturereset", "mapmodelreset", "maptitle", "echo",
    "concat", "concatword", "getmillis",
    "mdl*", "md2*", "md3*", "md5*", "obj*", "smd*", "iqm*"
	```

- ### `setmapvar <var> <value>`
	Create custom map variables that are stored along with the map file.

- ### `getmapvar <var>`
	Retrieve the value of a map variable (such as those created with `setmapvar`).

- ### `importobj <file> <size>`
	Imports an .obj model file as cubes in a voxelized style and places it in the current selection, the larger the size, the more detailed the geometry will be.

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