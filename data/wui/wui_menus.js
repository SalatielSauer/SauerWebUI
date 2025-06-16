
const btn_leave = window.wui.createButton("Exit", () => {
	cubescript(`if $isconnected [ disconnect ] [ quit ]`);
})

function _createMainMenu() {
	const body = document.createElement('div');
	body.innerHTML = '<p style="font-size: 10px">Unfinished HTML/CSS/JS UI</p>';
	const menu = window.wui.createMenu('main', body, '50%', '50%', 'SauerWebUI Test', { allowDrag: false, allowExit: true });
	menu.style.zIndex = -1;
	body.appendChild(window.wui.createTextInput('name', (e) => {
		//console.log("test: hello", e.target.value);
		cubescript(`name ${e.target.value}`);
	}));
	
	body.appendChild(window.wui.createButton("Server Browser", () => {
		_wui_create_server_browser();
		window.wui.showMenu('serverbrowser');
	}))
	
	body.appendChild(window.wui.createButton("Geometry Editor", () => {
		//window.wui.showMenu('mapeditor');
		cubescript(`newmap`);
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
		cubescript(`result $isconnected`, (result) => {
			//console.log(result);
			btn_leave.textContent = result == "1" ? "Disconnect" : "Exit";
		});
	}, 100);

	menu.onclear = () => {
		cubescript(`showgui main`);
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

let server_browser_cache = [];
let all_servers = [];
let all_servers_raw = "";

function _wui_create_server_browser() {
	const body = document.createElement('div');
	const container = document.createElement('div');
	
	body.style.minWidth = '600px';
	body.style.minHeight = '400px';

	const wui = window.wui.createMenu('serverbrowser', body, '50%', '50%', 'Server Browser', { allowExit: true, allowDrag: false });

	wui.onshow = () => {
		if (all_servers.length == 0) {
			container.innerHTML = "<p>Loading servers...</p>";
		}
	}

	let refresh = () => {
		cubescript('json_listservers 1', (serversJson) => {
			try {
				all_servers_raw = serversJson;
				all_servers = JSON.parse(serversJson);
			} catch (e) {
				console.error("Error parsing server list:", e);
				return;
			}
		})
	}

	body.appendChild(window.wui.createButton("Refresh", () => {
		refresh();
		draw_servers();
	}));

	function draw_servers() {
		if (all_servers.length == 0) {
			container.innerHTML = "<p>No servers found.</p>";
			return;
		}

		if (server_browser_cache == all_servers_raw) {
			//console.log("no changes")
			return;
		}
		//console.log("drawing server")
		const scrollTop = container.scrollTop;

		let table = container.querySelector('table.server-table');
		if (!table) {
			table = document.createElement('table');
			table.className = 'server-table';
			table.innerHTML = `<thead>
				<tr>
					<th></th>
					<th>Players</th>
					<th>Description</th>
					<th>Map</th>
					<th>IP</th>
					<th>Port</th>
					<th>Ping</th>
				</tr>
			</thead>`;
			container.innerHTML = '';
			container.appendChild(table);
		}

		const tbody = document.createElement('tbody');
		all_servers.forEach(server => {
			const row = document.createElement('tr');
			let map_thumbnail = '';
			if (window.gameassets.allmapnames.includes(server.map)) {
				map_thumbnail = `${window.gameassets.path_maps}/${server.map}.jpg`;
			} else {
				map_thumbnail = `${window.gameassets.path_packages}/textures/default.png`;
			}
			row.innerHTML = `
				<img src="${map_thumbnail}" class="icon" alt="${server.map}" onerror="this.onerror=null;this.src='${window.gameassets.path_packages}/textures/notexture.png'">
				<td>${server.numplayers}</td>
				<td>${server.desc}</td>
				<td>${server.map}</td>
				<td>${server.ip}</td>
				<td>${server.port}</td>
				<td>${server.ping}</td>
			`;
			row.onclick = () => {
				cubescript(`connect ${server.ip} ${server.port}`);
				window.wui.hideMenu('serverbrowser');
			};
			tbody.appendChild(row);
		});

		const oldTbody = table.querySelector('tbody');
		if (oldTbody) {
			table.replaceChild(tbody, oldTbody);
		} else {
			table.appendChild(tbody);
		}

		container.scrollTop = scrollTop;

		server_browser_cache = all_servers_raw;
	}

	body.appendChild(container);
	
	// loop
    let firstRefresh = true;
	let loop;
    function do_refresh() {
        refresh();
        draw_servers();
        if (firstRefresh) {
            firstRefresh = false;
            // Now start interval for subsequent refreshes
        	loop = setInterval(() => {
                refresh();
                draw_servers();
            }, 5000); // 5 seconds
        }
    }

    // do instant first refresh
	refresh();
	draw_servers();
    setTimeout(do_refresh, 1000);
	wui.onclear = () => {
		clearInterval(loop);
		server_browser_cache = [];
	}
}

window._createMainMenu = _createMainMenu;
window._create_menu_browser = _create_menu_browser;
// since WUI is unfinished, we will still use GUI by default.
//_createMainMenu();
//_create_menu_browser();
//window.wui.showMenu('main');

cubescript(`
newgui main [guistayopen [
	guitext "^<rgb:255/100/155>SauerWebUI (v0.1)" 0 0.5
	guistrut -0.5
	guialign 1 [
		guibutton "test WUI" [
			cleargui
			javascript [
				window._createMainMenu();
				window._create_menu_browser();
				window.wui.toggleMenu('main');
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