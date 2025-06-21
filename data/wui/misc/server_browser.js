
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
		window.cubescript('json_listservers 1', (serversJson) => {
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
				window.cubescript(`connect ${server.ip} ${server.port}`);
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
	wui.ondisappear = () => {
		clearInterval(loop);
		server_browser_cache = [];
	}
}

window._wui_create_server_browser = _wui_create_server_browser;