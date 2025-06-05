
const btn_leave = window.wui.createButton("Exit", () => {
	cubescript(`if $isconnected [ disconnect ] [ quit ]`);
})

function _createMainMenu() {
	const body = document.createElement('div');
	const menu = window.wui.createMenu('main', body, '50%', '50%', 'Main Menu', { allowDrag: false });
	menu.style.zIndex = -1;
	body.appendChild(window.wui.createTextInput('name', (e) => {
		console.log("test: hello", e.target.value);
		cubescript(`name ${e.target.value}`);
	}));
	
	body.appendChild(window.wui.createButton("Server Browser", () => {
		window.wui.showMenu('serverbrowser');
	}))
	
	body.appendChild(window.wui.createButton("Geometry Editor", () => {
		//window.wui.showMenu('mapeditor');
		cubescript(`map "abcdef"`);
		window.wui.hideMenu('main');
	}))

	body.appendChild(window.wui.createButton("Web Browser", () => {
		window.wui.showMenu('webbrowser');
	}))
	
	body.appendChild(btn_leave);
	setInterval(() => {
		cubescript(`result $isconnected`, (result) => {
			//console.log(result);
			btn_leave.textContent = result == "1" ? "Disconnect" : "Exit";
		});
	}, 100);

}

function _create_menu_browser() {
	const iframe = document.createElement('iframe');
	const wui = window.wui.createMenu('webbrowser', iframe, '40%', '50%', '', { allowFullscreen: true, allowExit: true });
	const hr = document.createElement('hr');

	const searchField = window.wui.createTextInput('Search URL', (e) => {
		iframe.src = e.target.value;
	});

	wui.overlay.appendChild(searchField);
	wui.overlay.appendChild(hr);

	iframe.src = "http://sauerbraten.org/";

}


function _wui_serverbrowser() {
	const body = document.createElement('div');
	window.wui.createMenu('serverbrowser', body, '50%', '50%', 'Server Browser', { allowExit: true });
	body.style.minWidth = '512px';
	body.style.minHeight = '332px';

	const container = document.createElement('div');

	function refreshServerList() {
		container.innerHTML = "";

		cubescript('json_listservers 1', (serversJson) => {
			let servers = [];
			try {
				servers = JSON.parse(serversJson);
			} catch (e) {
				container.innerHTML += "<p>Error parsing server list.</p>";
				return;
			}

			if (servers.length === 0) {
				container.innerHTML += "<p>No servers found.</p>";
				return;
			}

			const table = document.createElement('table');
			table.className = 'server-table';

			table.innerHTML = `<thead>
				<tr>
					<th>Description</th>
					<th>IP</th>
					<th>Port</th>
					<th>Ping</th>
					<th>Map</th>
					<th>Players</th>
				</tr>
			</thead>`;

			const tbody = document.createElement('tbody');
			servers.forEach(server => {
				const row = document.createElement('tr');
				row.innerHTML = `
					<td>${server.desc}</td>
					<td>${server.ip}</td>
					<td>${server.port}</td>
					<td>${server.ping}</td>
					<td>${server.map}</td>
					<td>${server.numplayers}</td>
				`;
				row.onclick = () => {
					cubescript(`connect ${server.ip} ${server.port}`);
					window.wui.hideMenu('serverbrowser');
				};
				tbody.appendChild(row);
			});

			table.appendChild(tbody);
			container.appendChild(table);
		});
	}

	const refreshBtn = window.wui.createButton("Refresh", refreshServerList);
	body.appendChild(refreshBtn);
	body.appendChild(container);

	refreshServerList();
}


_wui_serverbrowser();
_createMainMenu();
_create_menu_browser();

window.wui.showMenu('main');
