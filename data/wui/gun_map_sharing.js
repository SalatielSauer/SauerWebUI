const gun = Gun('https://gunjs.herokuapp.com/gun');
const gun_server = gun.get('sauerwebui_assets');

// upload asset
function gun_uploadAsset({ author, assetName, thumbnail, gzipFile, deletePassword }, cb) {
	const deleteKey = md5(deletePassword);
	gun_server.get(assetName).put({
		author,
		assetName,
		thumbnail,
		gzipFile,
		deleteKey, // store hash, not the password
		timestamp: Date.now()
	}, cb);
}

// list all assets
function gun_listassets(callback) {
	let assets = [];
	gun_server.map().once((data, key) => {
		if (data && data.assetName) assets.push(data);
		clearTimeout(window._mapListTimeout);
		window._mapListTimeout = setTimeout(() => callback(assets), 400);
	});
}

// delete asset (check password)
function gun_deleteAssetWithPassword(assetName, password, cb) {
	const inputHash = md5(password);
	gun_server.get(assetName).once(asset => {
		if (asset && asset.deleteKey === inputHash) {
			gun_server.get(assetName).put(null, cb ? () => cb(true, "Asset deleted.") : undefined);
		} else {
			cb && cb(false, "Wrong password or asset not found.");
		}
	});
}


function _wui_create_map_manage() {
		const body = document.createElement('div');
		body.style.minWidth = '600px';
		body.style.minHeight = '500px';

		const wui = window.wui.createMenu('mapmanage', body, '52%', '52%', 'Asset Upload & Manage', { allowExit: true, allowDrag: false });

		// upload Form
		const form = document.createElement('div');
		form.innerHTML = `
			<b>Upload Asset</b><br>
			<input id="map_author" placeholder="Author" style="width:90%"><br>
			<input id="map_name" placeholder="Asset Name" style="width:90%"><br>

			<label>Thumbnail Image:</label><br>
			<input type="file" id="map_thumbnail_file" accept="image/*"><br>
			<img id="map_thumbnail_preview" style="max-width:90px;max-height:60px;display:none"><br>

			<label>Asset File (GZIP/ZIP):</label><br>
			<input type="file" id="map_gzip_file" accept=".gz,.gzip,.zip"><br>

			<input id="delete_password" placeholder="Delete Password" type="password" style="width:90%"><br>
			<button id="uploadasset">Upload</button>
			<div id="uploadmsg"></div>
			<!-- Hidden fields for base64 -->
			<input type="hidden" id="map_thumbnail">
			<textarea id="map_gzip" style="display:none"></textarea>

		`;
		body.appendChild(form);

		// handle thumbnail image selection
		document.getElementById('map_thumbnail_file').onchange = function(e) {
				const file = e.target.files[0];
				if (!file) return;
				const reader = new FileReader();
				reader.onload = function(ev) {
						document.getElementById('map_thumbnail').value = ev.target.result;
						// show preview
						const img = document.getElementById('map_thumbnail_preview');
						img.src = ev.target.result;
						img.style.display = "block";
				};
				reader.readAsDataURL(file); // produces base64 image data
		};

		// handle asset file selection (GZIP/ZIP)
		document.getElementById('map_gzip_file').onchange = function(e) {
				const file = e.target.files[0];
				if (!file) return;
				const reader = new FileReader();
				reader.onload = function(ev) {
						// strip the data:...base64, part if any, keep only base64
						let base64data = ev.target.result.split(',')[1] || ev.target.result;
						document.getElementById('map_gzip').value = base64data;
				};
				reader.readAsDataURL(file); // get base64 encoding of any file
		};


		document.getElementById('uploadasset').onclick = () => {
				const author = document.getElementById('map_author').value;
				const assetName = document.getElementById('map_name').value;
				const thumbnail = document.getElementById('map_thumbnail').value; 
				const gzipFile = document.getElementById('map_gzip').value;
				const deletePassword = document.getElementById('delete_password').value;
				if (!author || !assetName || !thumbnail || !gzipFile || !deletePassword) {
						document.getElementById('uploadmsg').textContent = "All fields are required!";
						return;
				}

				gun_uploadAsset({ author, assetName, thumbnail, gzipFile, deletePassword }, () => {
						document.getElementById('uploadmsg').textContent = "Asset uploaded!";
						document.getElementById('delete_password').value = "";
						listAndShowAssets();
				});
		};

		// asset List UI
		const listDiv = document.createElement('div');
		body.appendChild(document.createElement('hr'));
		body.appendChild(listDiv);

		function listAndShowAssets() {
				listDiv.innerHTML = "<p>Loading assets...</p>";
				gun_listassets((assets) => {
						if (!assets.length) {
								listDiv.innerHTML = "<p>No assets uploaded yet.</p>";
								return;
						}
						const table = document.createElement('table');
						table.innerHTML = `<thead>
						<tr><th>Thumbnail</th><th>Name</th><th>Author</th><th>Download</th><th>Delete</th></tr>
						</thead>`;
						const tbody = document.createElement('tbody');
						assets.forEach(asset => {
								const row = document.createElement('tr');
								row.innerHTML = `
										<td><img src="${asset.thumbnail}" style="max-width:64px;max-height:48px"></td>
										<td>${asset.assetName}</td>
										<td>${asset.author}</td>
										<td><button>Download</button></td>
										<td><button>Delete</button></td>
								`;
								// download
								row.querySelector('button').onclick = () => {
										const link = document.createElement('a');
										link.href = 'data:application/gzip;base64,' + asset.gzipFile;
										link.download = `${asset.author}\\/${asset.assetName}.zip`;
										link.click();
								};
								// delete
								row.querySelectorAll('button')[1].onclick = () => {
										const pw = prompt("Enter the delete password for this asset:");
										if (!pw) return;
										gun_deleteAssetWithPassword(asset.assetName, pw, (ok, msg) => {
												alert(msg);
												if (ok) listAndShowAssets();
										});
								};
								tbody.appendChild(row);
						});
						table.appendChild(tbody);
						listDiv.innerHTML = '';
						listDiv.appendChild(table);
				});
		}

		listAndShowAssets();
}

window._wui_create_map_manage = _wui_create_map_manage;

function cs_gun_list_assets() {
	gun_listassets((assets) => {
			if (!assets.length) {
					console.log("No assets uploaded yet.");
					return;
			}
			console.log("Assets:", assets);
	});
}

window.cs_gun_list_assets = cs_gun_list_assets;