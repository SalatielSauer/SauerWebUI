class GithubUpdater {
    constructor() {
        this.config = {
            repository: 'https://api.github.com/repos/SalatielSauer/SauerWebUI',
            branch: 'main',
            name: 'SauerWebUI'
        };
        const repoMatch = this.config.repository.match(/repos\/([^/]+\/[^/]+)/);
        this.repoPath = repoMatch ? repoMatch[1] : '';
        this.rawBase = `https://raw.githubusercontent.com/${this.repoPath}`;
        this.storageKey = `lastsha_${this.repoPath}_${this.config.branch}`;
        this.lastAutoCheckKey = `lastautocheck_${this.repoPath}_${this.config.branch}`;

    }

    async getLatestCommit() {
        const url = `${this.config.repository}/commits/${this.config.branch}`;
        const resp = await fetch(url);
        const data = await resp.json();
        return {
            sha: data.sha,
            date: data.commit.author.date,
            author: data.commit.author.name
        };
    }

    async getChangedFiles(baseSha) {
        const url = `${this.config.repository}/compare/${baseSha}...${this.config.branch}`;
        const resp = await fetch(url);
        const data = await resp.json();
        return data.files || [];
    }

    async check() {
        const latestCommit = await this.getLatestCommit();
        const latestSha = latestCommit.sha;
        const lastSha = localStorage.getItem(this.storageKey);
        //console.log(`Latest SHA: ${latestSha}, Last SHA: ${lastSha}`);
        if (!lastSha) {
            // first run, set lastSha to latest and return no changes
            localStorage.setItem(this.storageKey, latestSha);
            return { changed: [], latestSha, lastSha: latestSha, latestCommit };
        }
        if (lastSha === latestSha) {
            return { changed: [], latestSha, lastSha, latestCommit };
        }
        let files = await this.getChangedFiles(lastSha);

        files = files.filter(f => !f.filename.startsWith('src/'));

        return { changed: files, latestSha, lastSha, latestCommit };
    }

    async autoCheckForUpdates() {
        const now = Date.now();
        const lastCheck = parseInt(localStorage.getItem(this.lastAutoCheckKey) || '0', 10);

        // 2 hours = 7.200.000 milliseconds
        if (now - lastCheck < 7200000) {
            // less than 2 hours since last auto-check, skip.
            return;
        }
        localStorage.setItem(this.lastAutoCheckKey, now.toString());

        const result = await this.check();
        if (result.changed.length > 0) {
            const { author, date } = result.latestCommit || {};
            const formattedDate = date ? new Date(date).toLocaleString() : '';
            console.log(`\f7[${formattedDate}] --------------------\n\f8A new version of ${this.config.name} is available!\n\f0Use the menu to update your client.\n\f7--------------------`);
            window.cubescript(`cleargui; showcursor 1`);
            this.showMenu();
        }
    }

    async downloadFile(file, li) {
        const url = `${this.rawBase}/${this.config.branch}/${file.filename}`;
        const parts = file.filename.split('/');
        const name = parts.pop();
        const dir = parts.join('/');
        return new Promise((resolve, reject) => {
            const token = Date.now().toString();
            let completed = false;
            let completeTimeout = null;

            function markComplete() {
                if (completed) return;
                completed = true;
                li.innerHTML = `✔️ <span style="color:#bfffae;">${file.filename}</span> <span style="font-size:8px;color:#888;">(${file.status})</span>`;
                resolve();
            }

            window.cefQuery({
                request: `downloadfile:${url}|../${dir}|${token}`,
                persistent: true,
                onSuccess: (r) => {
                    try { var d = JSON.parse(r); } catch (e) { return; }
                    if (d.status === 'progress') {
                        li.innerHTML = `${file.filename} (${file.status}) <span style="color:#aaa;">${d.percent.toFixed(1)}%</span>`;
                        if (d.percent >= 100 && !completed) {
                            // wait briefly for 'complete', otherwise force mark as complete
                            if (completeTimeout) clearTimeout(completeTimeout);
                            completeTimeout = setTimeout(markComplete, 700); // 700ms grace period
                        }
                    }
                    if (d.status === 'complete' && !completed) {
                        if (completeTimeout) clearTimeout(completeTimeout);
                        markComplete();
                    }
                },
                onFailure: (c, m) => {
                    if (completeTimeout) clearTimeout(completeTimeout);
                    li.innerHTML = `❌ ${file.filename} <span style="color:#ff4d4d;">(failed)</span>`;
                    reject(m);
                }
            });
        });
    }


    async updateAll(files, latestSha, listItems) {
        for (let i = 0; i < files.length; ++i) {
            try {
                await this.downloadFile(files[i], listItems[i]);
            } catch(e) {
                console.error(`\f7[${this.config.name} updater] \f3Download failed`, e);
            }
        }
        localStorage.setItem(this.storageKey, latestSha);
    }

    async showMenu() {
        const body = document.createElement('div');
        body.style.minWidth = '320px';
        body.innerHTML = '<p>Checking for updates...</p>';
        const menu = window.wui.createMenu('githubupdater', body, '50%', '50%', `🛠️${this.config.name} Updates`, { allowExit: true, allowDrag: false });
        
        menu.ondisappear = () => {
            if (window.wui.openMenuStack.length === 0) {
                window.cubescript(`if $mainmenu [ showgui main ]`);
            }
        }

        window.wui.showMenu('githubupdater');

        const result = await this.check();
        const { author, date } = result.latestCommit || {};
        const formattedDate = date ? new Date(date).toLocaleString() : '';
        //console.log('Update check result:', result);
        body.innerHTML = '';
        if (result.changed.length === 0) {
            // show latest commit info and a "check for updates" button
            const infoDiv = document.createElement('div');
            infoDiv.style.fontSize = '10px';
            infoDiv.style.marginBottom = '10px';
            infoDiv.innerHTML = `
                <div style="margin-bottom:5px;">${this.config.name} is up to date ✔️</div>
                <b>Latest commit:</b><br>
                <span style="color:#aaa;">${formattedDate}${author ? ` by ${author}` : ''}</span>
            `;

            const checkBtn = document.createElement('button');
            checkBtn.textContent = '🔄Check for Updates';
            checkBtn.onclick = async () => {
                checkBtn.disabled = true;
                body.innerHTML = '<p>Re-checking for updates...</p>';
                await this.showMenu();
            };

            body.appendChild(infoDiv);
            body.appendChild(checkBtn);
            localStorage.setItem(this.storageKey, result.latestSha);
            return;
        }


        // count file status
        let added = 0, modified = 0, removed = 0;
        let totalAdditions = 0, totalDeletions = 0;
        result.changed.forEach(f => {
            if (f.status === 'added') added++;
            else if (f.status === 'modified') modified++;
            else if (f.status === 'removed') removed++;
            if (typeof f.additions === 'number') totalAdditions += f.additions;
            if (typeof f.deletions === 'number') totalDeletions += f.deletions;
        });

        // info summary
        const summary = document.createElement('div');
        summary.style.marginBottom = '8px';
        summary.style.padding = '5px';
        summary.style.fontSize = '9px';
        summary.style.background = 'black';
        summary.innerHTML = `
            <b>${result.changed.length} file(s) changed:</b>
            <span style="color:#40ff80;">+${added} added</span>,
            <span style="color:#60a0ff;">~${modified} modified</span>,
            <span style="color:#ff4040;">-${removed} removed</span><br>
            <span style="color:#2ea043;">+${totalAdditions} additions</span>,
            <span style="color:#d73a49;">-${totalDeletions} deletions</span><br>
            <span style="color:#aaa;">Last update: ${formattedDate}${author ? ` by ${author}` : ''}</span>
        `;
        body.appendChild(summary);

        // scrollable file list
        const scrollDiv = document.createElement('div');
        scrollDiv.style.maxHeight = '200px';
        scrollDiv.style.overflowY = 'auto';
        scrollDiv.style.fontSize = '8px';
        scrollDiv.style.border = '1px dashed rgb(221, 221, 221)';
        scrollDiv.style.padding = '4px';
        scrollDiv.style.borderRadius = '4px';
        scrollDiv.style.background = 'rgb(13 41 25)';

        const list = document.createElement('ul');
        list.style.margin = 0;
        list.style.padding = '0 0 0 18px';
        // for referencing <li> nodes later:
        const listItems = [];
        result.changed.forEach(f => {
            const li = document.createElement('li');
            li.innerHTML = `
                ${f.filename} 
                <span style="color:#888;">(${f.status}</span>${
                    (typeof f.additions === 'number' && typeof f.deletions === 'number')
                        ? `, <span style="color:#2ea043;">+${f.additions}</span> <span style="color:#d73a49;">-${f.deletions}</span>`
                        : ''
                }<span style="color:#888;">)</span>`;
            li.style.marginBottom = '2px';
            list.appendChild(li);
            listItems.push(li);
        });
        scrollDiv.appendChild(list);
        body.appendChild(scrollDiv);

        // update button
        const btn = document.createElement('button');
        btn.textContent = '🌐Update All';
        btn.style.marginTop = '10px';
        btn.onclick = async () => {
            btn.disabled = true;
            await this.updateAll(result.changed, result.latestSha, listItems);
            scrollDiv.innerHTML = 'All files are up to date ✔️<br>⚠️You may have to restart the client to apply the changes.';
        };
        body.appendChild(btn);
    }

}

window.githubUpdater = new GithubUpdater();
window.showGithubUpdater = () => window.githubUpdater.showMenu();
window.githubUpdater.autoCheckForUpdates();