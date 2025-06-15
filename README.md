# Sauerbraten + JavaScript = Wonderfulness

## What is this?
**SauerWebUI** is a mod for [Sauerbraten](http://sauerbraten.org/) that brings web technologies to the [Cube Engine](http://cubeengine.com/), allowing users to create interfaces entirely in HTML, CSS, and JavaScript, and to use any library available for those environments.

And of course, you can execute [CubeScript](https://github.com/CubeScript) from JavaScript and vice versa!

Also, for now, there are some miscellaneous experiments that you will see below.

## Installation
There is currently a pre-compiled binary available for Windows x64 only (tested with Windows 10 22H2):
[Download SauerWebUI JSquare Edition (15/06/2025)]()

## Building
If you want to build, there is a Visual Studio 2022 project file in `src/vs/` that you can use.

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

## Misc
- ### `guiimagestring <base64 string> [command] <size> <overlay 0/1>`
	Works similarly to `guiimage`, but instead of specifying a file path for the image, you can provide a string containing an image encoded in base64.

- ### `guitext <text> <icon> <size>`
- ### `guibutton <text> [command] <icon> <size>`
	It is now possible to set the size of a guitext using the third parameter of the command, or the fourth parameter in the case of guibutton (1 is the default size).

- ### Custom RGB text support
	Text supports a new color marker in addition to the traditional `^f0` - `^f8`. Simply insert `^<rgb:255/0/255>` before any text to specify a custom color using the RGB format.

- ### `addselection`
	Allows adding a new selection to the existing ones, enabling manipulation of multiple geometry or texture regions at once, including non-contiguous areas. (requires `multiselmode 1`)
- ### `multiselmode <0/1>`
	Enables experimental multi-selection mode (`addselection`).

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