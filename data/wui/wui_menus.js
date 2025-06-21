
const btn_leave = window.wui.createButton("Exit", () => {
	window.cubescript(`if $isconnected [ disconnect ] [ quit ]`);
})

function _create_menu_main() {
	const body = document.createElement('div');
	body.innerHTML = '<p style="font-size: 10px">Unfinished HTML/CSS/JS UI</p>';
	const menu = window.wui.createMenu('main', body, '50%', '50%', 'SauerWebUI Test', { allowDrag: false, allowExit: true });
	menu.style.zIndex = -1;
	body.appendChild(window.wui.createTextInput('name', (e) => {
		//console.log("test: hello", e.target.value);
		window.cubescript(`name ${e.target.value}`);
	}));

 	body.appendChild(window.wui.createButton("🛠️Check Updates", () => {
		window.showGithubUpdater();
	}))
	
	body.appendChild(window.wui.createButton("Map Assets", (event) => {
		window.checkMapAssets((callback) => {
			console.log("\f8Map assets:\f2", callback);
		});
	}))

	body.appendChild(window.wui.createButton("Server Browser", () => {
		_wui_create_server_browser();
		window.wui.showMenu('serverbrowser');
	}))
	
	body.appendChild(window.wui.createButton("Geometry Editor", () => {
		//window.wui.showMenu('mapeditor');
		window.cubescript(`newmap`);
		window.wui.hideMenu('main');
	}))

	body.appendChild(window.wui.createButton("Web Browser", (event) => {
		window.wui.showMenu('webbrowser', event);
	}))

	body.appendChild(window.wui.createButton("Dev Tools", (event) => {
		window.cefQuery({ request: 'openDevTools' });
	}))

	body.appendChild(window.wui.createButton("Text Editor", (event) => {
		
	}))

	body.appendChild(window.wui.createButton("SauerTracker", (event) => {

	}))

	body.appendChild(window.wui.createButton("P2P File Share", () => {
		window._wui_create_webrtc_menu();
		window.wui.showMenu('p2pfileshare');
	}))

	/*body.appendChild(window.wui.createButton("Map Upload & Manage", () => {
		window._wui_create_map_manage();
		window.wui.showMenu('mapmanage');
	}));*/

	
	body.appendChild(btn_leave);
	setInterval(() => {
		window.cubescript(`result $isconnected`, (result) => {
			//console.log(result);
			btn_leave.textContent = result == "1" ? "Disconnect" : "Exit";
		});
	}, 100);

	menu.ondisappear = () => {
		window.cubescript(`if $mainmenu [ showgui main ]`);
	}

}

function _create_menu_browser() {
	const iframe = document.createElement('iframe');
	const wui = window.wui.createMenu('webbrowser', iframe, '40%', '50%', '', { allowFullscreen: true, allowExit: true, clearOnExit: false });
	const hr = document.createElement('hr');

	const searchField = window.wui.createTextInput('Search URL', (e) => {
		iframe.src = e.target.value;
	});

	wui.overlay.appendChild(searchField);
	wui.overlay.appendChild(hr);

	iframe.src = "http://sauerbraten.org/";

}

window._create_menu_main = _create_menu_main;
window._create_menu_browser = _create_menu_browser;

// since WUI is unfinished, we will still use GUI by default.
//_create_menu_main();
//_create_menu_browser();
//window.wui.showMenu('main');

window.cubescript(`
newgui main [guistayopen [
	guitext "^<rgb:255/100/155>SauerWebUI (v0.1)" 0 0.5
	guistrut -0.5
	guialign 1 [
		guibutton "test WUI" [
			cleargui
			javascript [
				window._create_menu_main();
				window._create_menu_browser();
				window.wui.showMenu('main');
			]
			showcursor 1
		] "arrow_fw" 0.5
	]
	guistrut 0.1
    guilist [
        guiimage (concatword "packages/icons/" (playermodelicon) ".jpg") [chooseplayermodel] 1.15
        guistrut 0.25
        guilist [
            newname = (getname)
            guifield newname 15 [name $newname]
            guispring
            guilist [
                guibutton (playermodelname) [chooseplayermodel] 0
                guistrut 1
                guiimage (getcrosshair) [showgui crosshair] 0.5
            ]
        ]
    ]
    guibar
    guibutton "server browser.." [ showgui servers ]
    if (isconnected) [
        guibar
        if (|| $editing (m_edit (getmode))) [
            guibutton "editing.." [ showgui editing ]
        ]
        guibutton "vote game mode / map.." [ showgui gamemode ]
        guibutton "switch team" [ if (strcmp (getteam) "good") [team evil] [team good] ]
        guibutton "toggle spectator" [ spectator (! (isspectator (getclientnum))) ] "spectator"
        guibutton "master.." [ showgui master ]
        guibar
        guibutton "options.." [ showgui options ]
        guibutton "disconnect" [ disconnect ] "exit"
    ] [
        guibutton "bot match.." [ showgui botmatch ]
        guibutton "campaign.." [ showgui campaign ]
        guibar
        guibutton "options.." [ showgui options ]
        guibutton "about.." [ showgui about ]
        guibutton "quit" [ quit ] "exit"
    ]
]] 0
showgui main
`)