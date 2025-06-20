class MapAssetsMenu {
    constructor(assets, { onDownload, onFindFile, menuApi } = {}) {
        this.assets = assets.map(a => Object.assign({}, a));
        this.onDownload = onDownload;
        this.onFindFile = onFindFile;
        this.menuApi = menuApi || window.wui;
        this.id = 'mapassets';
        this.body = null;
        this.rowRefs = [];
        this.completedCount = 0;
    }

    /*async checkInstalled() {
        // returns a promise that sets installed flag for each asset
        const check = (a) => new Promise(res => {
            this.onFindFile(`assets/${a.assetname}`, (r) => res(r === '1'));
        });
        for (let i = 0; i < this.assets.length; ++i) {
            this.assets[i].installed = await check(this.assets[i]);
        }
    }*/

    async checkInstalled() {
        // returns a promise that sets installed and outdated flag for each asset
        const check = (a) => new Promise(res => {
            // get installed flag and version
            this.onFindFile(`assets/${a.assetname}`, (found) => {
                if (found !== '1') {
                    window.cubescript(`prunevarsbyprefix _mapassets_version_${a.assetname}`);
                    return res({ installed: false, outdated: false });
                }

                // now check version using cubescript variable
                window.cubescript(`nodebug [ result $_mapassets_version_${a.assetname} ]`, (ver) => {
                    let installedVersion = parseInt(ver, 10) || 0;
                    let targetVersion = parseInt(a.assetversion, 10) || 0;
                    res({
                        installed: true,
                        outdated: installedVersion < targetVersion,
                        installedVersion
                    });
                });
            });
        });
        for (let i = 0; i < this.assets.length; ++i) {
            let result = await check(this.assets[i]);
            this.assets[i].installed = result.installed;
            this.assets[i].outdated = result.outdated;
            this.assets[i].installedVersion = result.installedVersion;
        }
    }

    async show() {
        await this.checkInstalled();
        if (this.body) this.body.remove();
        this.completedCount = this.assets.filter(a => a.installed).length;
        this.body = this._render();
        this.menuApi.createMenu(this.id, this.body, '52%', '52%', 'Map Assets', { allowExit: true, allowDrag: false });
        this.menuApi.showMenu(this.id);
        window.cubescript('showcursor 1');
        this.menuApi.ondisappear = () => {
           
        }
    }

    // main download (or refresh) per row
    runDownloadForRow(idx) {
        const row = this.rowRefs[idx];
        const asset = row.asset;
        const { tr, tdStatus, tdAction } = row;

        if (!asset.assetname) {
            const parts = asset.assetsrc.split('/');
            asset.assetname = parts[parts.length - 1];
        }

        asset.assetname = asset.assetname.replace(/[^a-zA-Z0-9_\-\.]/g, '_');

        // reset
        tr.style.opacity = '';
        tr.style.filter = '';
        tdStatus.innerHTML = '';
        tdAction.innerHTML = '';

        // progress bar and status
        const progressBar = document.createElement('progress');
        progressBar.max = 100;
        progressBar.value = 0;
        progressBar.style.width = '120px';
        progressBar.style.display = '';
        tdStatus.appendChild(progressBar);

        const statusText = document.createElement('span');
        statusText.style.marginLeft = '8px';
        tdStatus.appendChild(statusText);

        // button (disabled while downloading)
        const btn = document.createElement('button');
        btn.textContent = 'Refresh';
        btn.disabled = true;
        tdAction.appendChild(btn);

        // save refs for this row
        row.progressBar = progressBar;
        row.statusText = statusText;
        row.btn = btn;

        let alreadyCompleted = false;

        this.onDownload(asset.assetsrc, `assets/${asset.assetname}`,
            (percent) => {
                progressBar.value = percent;
                statusText.textContent = `${percent.toFixed(1)}%`;
                if (percent >= 100 && !alreadyCompleted) {
                    alreadyCompleted = true;

                    // store asset version in config
                    window.cubescript(`_mapassets_version_${asset.assetname} = ${asset.assetversion || 0}`);

                    setTimeout(() => {
                        progressBar.style.display = 'none';
                        statusText.textContent = 'done';
                        tr.style.opacity = '0.5';
                        tr.style.filter = 'brightness(0.8)';
                        tdStatus.innerHTML = '';
                        tdAction.innerHTML = '';

                        // downloaded label
                        const downloadedLabel = document.createElement('span');
                        downloadedLabel.innerHTML = `✔️<br><span style="font-size: 7px;display: block;">version: ${asset.assetversion == undefined || asset?.assetversion == 0 ? 'initial' : asset.assetversion}</span>`;
                        downloadedLabel.style.color = "#36a833";
                        downloadedLabel.style.fontWeight = 'bold';
                        downloadedLabel.style.display = 'block';
                        tdStatus.appendChild(downloadedLabel);

                        // new Refresh button
                        const refreshBtn = document.createElement('button');
                        refreshBtn.textContent = 'Refresh';
                        refreshBtn.onclick = () => this.runDownloadForRow(idx);
                        tdAction.appendChild(refreshBtn);

                        // save updated btn
                        row.btn = refreshBtn;
                        row.progressBar = null;
                        row.statusText = null;

                        // only count the first time
                        if (!row.asset.installed) {
                            row.asset.installed = true;
                            this.completedCount++;
                            this.checkAllComplete();
                        }
                    }, 350);
                }
            },
            (success, msgOrPath) => {
                if (!success) {
                    statusText.textContent = `❌ Failed: ${msgOrPath}`;
                    btn.disabled = false;
                }
            }
        );
    }

    // "download all" or "refresh all"
    async downloadAllRows() {
        // only rows that are not installed or button is enabled (not downloading)
        const rowsToDownload = this.rowRefs.filter(r => r.btn && !r.btn.disabled);
        for (let i = 0; i < rowsToDownload.length; ++i) {
            await new Promise((resolve) => {
                // wrap runDownloadForRow so it resolves when done
                let origOnDownload = this.onDownload;
                // monkey-patch onDownload to resolve at 100%
                this.onDownload = (url, subdir, onProgress, onDone) => {
                    origOnDownload(url, subdir, (percent, received) => {
                        onProgress(percent, received);
                        if (percent >= 100) resolve();
                    }, (success, msgOrPath) => {
                        onDone(success, msgOrPath);
                        if (!success) resolve();
                    });
                };
                this.runDownloadForRow(this.rowRefs.indexOf(rowsToDownload[i]));
                // restore onDownload
                this.onDownload = origOnDownload;
            });
        }
    }

    checkAllComplete() {
        if (this.completedCount === this.rowRefs.length) {
            this.allReadyMsg.style.display = "";
            this.rowRefs.forEach(row => {
                if (row.btn) row.btn.style.display = "none";
            });
            this.btnAll.textContent = 'Refresh All';
        }
    }

    _render() {
        const body = document.createElement('div');
        body.style.minWidth = '520px';
        body.style.padding = '0';

        // scrollable wrapper
        const scrollArea = document.createElement('div');
        scrollArea.style.maxHeight = '340px';
        scrollArea.style.overflowY = 'auto';
        scrollArea.style.border = '1px solid #e3e3e3';
        scrollArea.style.marginBottom = '14px';
        body.appendChild(scrollArea);

        // table
        const table = document.createElement('table');
        table.style.width = '100%';
        table.style.borderCollapse = 'collapse';
        scrollArea.appendChild(table);

        // header
        const thead = document.createElement('thead');
        const headerRow = document.createElement('tr');
        ['Thumbnail', 'Name', 'Status', 'Action'].forEach(text => {
            const th = document.createElement('th');
            th.textContent = text;
            th.style.borderBottom = '1px solid #ccc';
            th.style.padding = '8px';
            th.style.textAlign = 'left';
            headerRow.appendChild(th);
        });
        thead.appendChild(headerRow);
        table.appendChild(thead);

        // body
        const tbody = document.createElement('tbody');
        table.appendChild(tbody);
        this.rowRefs = [];
        const total = this.assets.length;

        // message when all complete
        const allReadyMsg = document.createElement('div');
        allReadyMsg.textContent = "All assets are ready!";
        allReadyMsg.style.display = "none";
        allReadyMsg.style.marginTop = "12px";
        allReadyMsg.style.fontWeight = "bold";
        allReadyMsg.style.color = "#339933";
        body.appendChild(allReadyMsg);
        this.allReadyMsg = allReadyMsg;

        // for the download all / refresh all button
        let allInstalled = this.assets.every(asset => asset.installed);

        this.assets.forEach((asset, idx) => {
            const tr = document.createElement('tr');
            tr.style.verticalAlign = 'middle';

            // thumbnail
            const tdThumb = document.createElement('td');
            const img = document.createElement('img');
            img.src = asset.assetthumb;
            img.style.width = '64px';
            img.style.height = '48px';
            img.style.objectFit = 'cover';
            tdThumb.appendChild(img);
            tdThumb.style.padding = '8px';
            tr.appendChild(tdThumb);

            // name
            const tdName = document.createElement('td');
            const displayName = asset.assetname.length > 15 ? asset.assetname.slice(0, 13) + '…' : asset.assetname;
            tdName.textContent = displayName;
            tdName.title = asset.assetname;
            tdName.style.padding = '8px';
            tr.appendChild(tdName);

            // status
            const tdStatus = document.createElement('td');
            tdStatus.style.padding = '8px';

            let progressBar = null;
            let statusText = null;

            // action
            const tdAction = document.createElement('td');
            tdAction.style.padding = '8px';

            // initial state
            let btn;
            if (asset.installed) {
            if (asset.installedVersion > parseInt(asset.assetversion, 10)) {
                // installed is newer than target, offer "revert"
                tr.style.opacity = '';
                tr.style.filter = '';
                const revertLabel = document.createElement('span');
                revertLabel.innerHTML = `⬅️map uses older version<br><span style="font-size: 7px;display: block;">installed: v${asset.installedVersion} → map: v${asset.assetversion}</span>`;
                revertLabel.style.color = "#36a1e8";
                revertLabel.style.fontWeight = 'bold';
                revertLabel.style.display = 'block';
                revertLabel.style.fontSize = '8px';
                tdStatus.appendChild(revertLabel);

                btn = document.createElement('button');
                btn.textContent = 'Revert';
                btn.onclick = () => this.runDownloadForRow(idx);
                tdAction.appendChild(btn);
            }
            else if (asset.outdated) {
                    // show "update available"
                    tr.style.opacity = '';
                    tr.style.filter = '';
                    const updateLabel = document.createElement('span');
                    updateLabel.innerHTML = `⚠️map uses newer version<br><span style="font-size: 7px;display: block;">installed: v${asset.installedVersion} → map: v${asset.assetversion}</span>`;
                    updateLabel.style.color = "#e6b800";
                    updateLabel.style.fontWeight = 'bold';
                    updateLabel.style.marginRight = '8px';
                    updateLabel.style.display = 'block';
                    updateLabel.style.fontSize = '8px';
                    updateLabel.style.borderStyle = 'dashed';
                    updateLabel.style.borderWidth = '1px';
                    updateLabel.style.borderColor = 'yellow';
                    tdStatus.appendChild(updateLabel);

                    btn = document.createElement('button');
                    btn.textContent = 'Update';
                    btn.onclick = () => this.runDownloadForRow(idx);
                    tdAction.appendChild(btn);
                } else {
                    // show as downloaded
                    tr.style.opacity = '0.5';
                    tr.style.filter = 'brightness(0.8)';

                    const downloadedLabel = document.createElement('span');
                    downloadedLabel.innerHTML = `☑️<br><span style="font-size: 7px;display: block;">version: ${asset.installedVersion == 0 ? 'initial' : asset.installedVersion}</span>`;
                    downloadedLabel.style.color = "lightblue";
                    downloadedLabel.style.fontWeight = 'bold';
                    downloadedLabel.style.display = 'block';
                    tdStatus.appendChild(downloadedLabel);

                    btn = document.createElement('button');
                    btn.textContent = 'Refresh';
                    btn.style.backgroundColor = 'unset';
                    btn.onclick = () => this.runDownloadForRow(idx);
                    tdAction.appendChild(btn);
                }
            } else {
                progressBar = document.createElement('progress');
                progressBar.max = 100;
                progressBar.value = 0;
                progressBar.style.width = '120px';
                progressBar.style.display = 'none';
                tdStatus.appendChild(progressBar);

                statusText = document.createElement('span');
                statusText.style.marginLeft = '0px';
                statusText.innerHTML = `not downloaded<br><span style="font-size: 8px">version: ${asset.assetversion == undefined || asset?.assetversion == 0 ? 'initial' : asset.assetversion}</span>`;
                tdStatus.appendChild(statusText);

                btn = document.createElement('button');
                btn.textContent = 'Download';
                btn.style.backgroundColor = '#2798cbbd';
                btn.onclick = () => this.runDownloadForRow(idx);
                tdAction.appendChild(btn);
            }

            tr.appendChild(tdStatus);
            tr.appendChild(tdAction);
            tbody.appendChild(tr);

            // save refs for this row
            this.rowRefs.push({ btn, progressBar, statusText, asset, tr, tdStatus, tdAction });
        });

        // download all or refresh all
        const btnAll = document.createElement('button');
        btnAll.textContent = allInstalled ? '🔄Refresh All' : '🗃️Download All';
        btnAll.style.marginTop = '8px';
        btnAll.onclick = () => this.downloadAllRows();
        this.btnAll = btnAll;
        body.appendChild(btnAll);

        return body;
    }
}

function onDownload(url, subdir, onProgress, onDone) {
    const downloadId = Math.floor(Math.random() * 1e9).toString();
    window.cefQuery({
        request: `downloadfile:${url}|${subdir}|${downloadId}`,
        persistent: true,
        onSuccess: (result) => {
            try { var data = JSON.parse(result); } catch (e) { return; }
            if (data.status === 'progress' && onProgress) onProgress(data.percent, data.received);
            else if (data.status === 'complete') {
                if (onDone) onDone(true, data.path);
            }
        },
        onFailure: (code, msg) => {
            if (onDone) onDone(false, msg);
        }
    });
}

function onFindFile(path, cb) {
    window.cubescript(`findfile ${path}`, cb);
}

function checkMapAssets(onError = () => {}) {
    window.cubescript('result $mapassets', (json) => {
        if (!json) {
            onError('No map assets found.');
            return;
        }
        json = json.trim();
        if (!json) return;
        if (!json.startsWith('[')) json = `[${json}]`;
        let assets;
        try { assets = JSON.parse(json); } catch (e) { onError(e); return; }
        if (!Array.isArray(assets)) assets = [assets];

        // use the class, passing in the handler functions
        const menu = new MapAssetsMenu(assets, {
            onDownload,
            onFindFile
        });
        menu.show();
    });
}


window.checkMapAssets = checkMapAssets;

window.cubescript(`getmapassets = [ javascript [ window.checkMapAssets() ] ]`);
