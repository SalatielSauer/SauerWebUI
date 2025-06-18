// Simple WebRTC zip file transfer without external servers.
// Creates a menu for exchanging SDP offers/answers manually
// and sending/receiving a single zip file via DataChannel.

function _wui_create_webrtc_menu() {
    const body = document.createElement('div');
    body.style.minWidth = '420px';
    body.style.minHeight = '380px';

    const status = document.createElement('div');
    status.textContent = 'Idle';
    body.appendChild(status);

    const progress = document.createElement('div');
    progress.textContent = '';
    body.appendChild(progress);

    const downloadLink = document.createElement('a');
    downloadLink.style.display = 'none';
    downloadLink.href = '#';
    body.appendChild(downloadLink);

    body.appendChild(document.createElement('hr'));

    const localSDP = window.wui.createTextArea('Local SDP');
    localSDP.style.height = '80px';
    body.appendChild(localSDP);

    const remoteSDP = window.wui.createTextArea('Remote SDP');
    remoteSDP.style.height = '80px';
    body.appendChild(remoteSDP);

    const btnCreateOffer = window.wui.createButton('Create Offer', async () => {
        setupConnection(true);
        status.textContent = 'creating offer...';
        const offer = await pc.createOffer();
        await pc.setLocalDescription(offer);
        status.textContent = 'awaiting answer...';
    });
    body.appendChild(btnCreateOffer);

    const btnCreateAnswer = window.wui.createButton('Create Answer', async () => {
        if (!remoteSDP.value) return;
        setupConnection(false);
        status.textContent = 'creating answer...';
        await pc.setRemoteDescription(JSON.parse(remoteSDP.value));
        const answer = await pc.createAnswer();
        await pc.setLocalDescription(answer);
        status.textContent = 'awaiting connection...';
    });
    body.appendChild(btnCreateAnswer);

    const btnSetRemote = window.wui.createButton('Set Remote Description', async () => {
        if (!remoteSDP.value) return;
        await pc.setRemoteDescription(JSON.parse(remoteSDP.value));
        status.textContent = 'remote set, connecting...';
    });
    body.appendChild(btnSetRemote);

    body.appendChild(document.createElement('hr'));

    const fileInput = document.createElement('input');
    fileInput.type = 'file';
    fileInput.accept = '.zip,.gz,.gzip,.ogz';
    body.appendChild(fileInput);

    const btnSend = window.wui.createButton('Send File', () => {
        if (!dataChannel || dataChannel.readyState !== 'open') return;
        const file = fileInput.files[0];
        if (!file) return;
        status.textContent = `Sending ${file.name}`;
        progress.textContent = `0/${file.size} (0%)`;
        dataChannel.send(JSON.stringify({type:'file-meta',name:file.name,size:file.size}));
        sendFile(file);
    });
    btnSend.disabled = true;
    body.appendChild(btnSend);

    const menu = window.wui.createMenu('p2pfileshare', body, '50%', '50%', 'P2P File Share', {allowExit:true, allowDrag:false});

    async function sendFile(file){
        const buffer = await file.arrayBuffer();
        const chunkSize = 16 * 1024;
        let offset = 0;
        dataChannel.bufferedAmountLowThreshold = 256 * 1024;
        function pump(){
            while (offset < buffer.byteLength && dataChannel.bufferedAmount < dataChannel.bufferedAmountLowThreshold) {
                let end = Math.min(offset + chunkSize, buffer.byteLength);
                dataChannel.send(buffer.slice(offset, end));
                offset = end;
                const pct = Math.floor(offset / buffer.byteLength * 100);
                progress.textContent = `Sending ${file.name}: ${offset}/${buffer.byteLength} (${pct}%)`;
            }
            if (offset < buffer.byteLength) {
                setTimeout(pump, 10);
            } else {
                dataChannel.send(JSON.stringify({type:'file-end'}));
            }
        }
        pump();
    }

    function setupConnection(initiator){
        if (pc) return;
        pc = new RTCPeerConnection({iceServers:[{urls:'stun:stun.l.google.com:19302'}]});
        status.textContent = pc.connectionState;
        pc.onicecandidate = ev => {
            if(!ev.candidate) localSDP.value = JSON.stringify(pc.localDescription);
        };
        pc.onconnectionstatechange = () => { status.textContent = pc.connectionState; };
        pc.oniceconnectionstatechange = () => { status.textContent = pc.iceConnectionState; };
        if (initiator){
            dataChannel = pc.createDataChannel('file');
            setupDataChannel();
        } else {
            pc.ondatachannel = ev => { dataChannel = ev.channel; setupDataChannel(); };
        }
    }

    function setupDataChannel(){
        dataChannel.binaryType = 'arraybuffer';
        dataChannel.onopen = () => { btnSend.disabled = false; status.textContent = 'Channel open'; };
        dataChannel.onclose = () => { btnSend.disabled = true; status.textContent = 'Channel closed'; };
        dataChannel.onerror = (err) => {
            console.error('[P2P] DataChannel error:', err.reason, err);
        };
        let incoming = [];
        let incomingSize = 0;
        let fileInfo = null;
        dataChannel.onmessage = ev => {
            if (typeof ev.data === 'string'){
                const msg = JSON.parse(ev.data);
                if (msg.type === 'file-meta'){
                    fileInfo = msg;
                    incoming = [];
                    incomingSize = 0;
                    status.textContent = `Receiving ${msg.name}`;
                    progress.textContent = `0/${msg.size} (0%)`;
                } else if (msg.type === 'recv-progress') {
                    const pct = Math.floor(msg.received / msg.total * 100);
                    progress.textContent = `Peer receiving: ${msg.received}/${msg.total} (${pct}%)`;
                } else if (msg.type === 'recv-complete') {
                    status.textContent = `Transfer complete: ${msg.name}`;
                    console.log('[P2P] Transfer complete:', msg);
                    progress.textContent = '';
                } else if (msg.type === 'file-end') {
                    if (fileInfo) finalizeIncomingFile();
                }
            } else if (fileInfo){
                incoming.push(ev.data);
                incomingSize += ev.data.byteLength;
                const pct = Math.floor(incomingSize / fileInfo.size * 100);
                progress.textContent = `Receiving ${fileInfo.name}: ${incomingSize}/${fileInfo.size} (${pct}%)`;
                dataChannel.send(JSON.stringify({type:'recv-progress',received:incomingSize,total:fileInfo.size}));
                if (incomingSize >= fileInfo.size) {
                    dataChannel.send(JSON.stringify({type:'recv-complete', name:fileInfo.name}));
                    finalizeIncomingFile();
                }
            }
        };

        function finalizeIncomingFile() {
            if (downloadLink.href && downloadLink.href.startsWith('blob:')) {
                URL.revokeObjectURL(downloadLink.href);
            }

            const blob = new Blob(incoming);
            const url = URL.createObjectURL(blob);
            
            downloadLink.href = url;
            downloadLink.download = fileInfo.name;
            downloadLink.textContent = `Save ${fileInfo.name}`;
            downloadLink.style.display = 'inline-block';
            console.log('[P2P] File received:', fileInfo.name, fileInfo.size, 'bytes');
            status.textContent = 'File ready to save.';
            progress.textContent = '';

            downloadLink.onclick = (e) => {
                status.textContent = 'Saving...';
                setTimeout(() => {
                    window.cubescript(`findfile ${e.target.download}`, (fileExists) => {
                        if (fileExists) {
                            status.textContent = `File "${e.target.download}" saved successfully!`;
                            downloadLink.style.display = 'none';
                        } else {
                            status.textContent = 'Failed to save, please try again.';
                            downloadLink.style.display = 'inline-block';
                        }
                    });
                }, 500);
            };

            incoming = [];
            incomingSize = 0;
            fileInfo = null;
        }

    }

    menu.onclear = () => {
        if (pc) pc.close();
        pc = null;
        dataChannel = null;
        status.textContent = 'Idle';
        progress.textContent = '';
    };
}

window._wui_create_webrtc_menu = _wui_create_webrtc_menu;

let pc = null;
let dataChannel = null;