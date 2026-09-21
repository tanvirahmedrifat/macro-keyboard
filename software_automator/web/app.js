const video = document.getElementById('videoFeed');
const overlayCanvas = document.getElementById('overlayCanvas');
const captureCanvas = document.getElementById('captureCanvas');
const overlayCtx = overlayCanvas.getContext('2d');
const captureCtx = captureCanvas.getContext('2d', { willReadFrequently: true });
const logDiv = document.getElementById('statusLog');

let ws = null;
let autoPilotRunning = false;
let autoPilotInterval = null;

function log(msg) {
    const time = new Date().toLocaleTimeString();
    logDiv.innerHTML += `<div>[${time}] ${msg}</div>`;
    logDiv.scrollTop = logDiv.scrollHeight;
}

function connectWebSocket() {
    ws = new WebSocket('ws://localhost:8081');
    
    ws.onopen = () => {
        log('Connected to backend server.');
        ws.send(JSON.stringify({type: 'get_ports'}));
    };
    
    ws.onmessage = (event) => {
        const data = JSON.parse(event.data);
        if (data.type === 'log') {
            log(data.msg);
            if (data.msg.includes('auto-connected to ESP32') || data.msg.includes('Successfully connected')) {
                document.getElementById('espStatus').className = 'status-indicator green';
            } else if (data.msg.includes('Failed to connect') || data.msg.includes('Disconnected')) {
                document.getElementById('espStatus').className = 'status-indicator red';
            }
        } else if (data.type === 'status') {
            document.getElementById('espStatus').className = data.esp_connected ? 'status-indicator green' : 'status-indicator red';
        } else if (data.type === 'trigger_fired') {
            const btn = document.getElementById('btn-macro-' + data.key);
            if (btn) {
                // Add glow effect
                btn.classList.add('glow-green');
                // Remove glow effect after 1 second
                setTimeout(() => {
                    btn.classList.remove('glow-green');
                }, 1000);
            }
        } else if (data.type === 'frame_processed') {
            if (autoPilotRunning) {
                // Send next frame after 500ms delay to keep CPU usage low
                setTimeout(sendFrame, 500);
            }
        }
    };
    
    ws.onclose = () => {
        log('Disconnected from backend server. Reconnecting in 3s...');
        setTimeout(connectWebSocket, 3000);
    };
}

function sendTrigger(key) {
    if (ws) ws.send(JSON.stringify({type: 'trigger', key: key}));
}

document.getElementById('btnShare').onclick = async () => {
    try {
        const stream = await navigator.mediaDevices.getDisplayMedia({
            video: { cursor: "always" },
            audio: false
        });
        video.srcObject = stream;
        
        // Match canvas size to video aspect ratio
        video.onloadedmetadata = () => {
            overlayCanvas.width = video.videoWidth;
            overlayCanvas.height = video.videoHeight;
            captureCanvas.width = video.videoWidth;
            captureCanvas.height = video.videoHeight;
        };
        log('Screen stream started. Ready for Auto-Pilot.');
        document.getElementById('screenStatus').className = 'status-indicator green';
        
        // Handle screen share stop
        stream.getVideoTracks()[0].onended = () => {
            document.getElementById('screenStatus').className = 'status-indicator red';
            log('Screen stream stopped.');
            if (autoPilotRunning) {
                document.getElementById('btnAutoPilot').click(); // Stop autopilot
            }
        };
    } catch (err) {
        log('Screen sharing cancelled or failed: ' + err);
        document.getElementById('screenStatus').className = 'status-indicator red';
    }
};

function sendFrame() {
    if (!autoPilotRunning || !video.srcObject || video.readyState !== video.HAVE_ENOUGH_DATA) return;
    
    captureCtx.drawImage(video, 0, 0, captureCanvas.width, captureCanvas.height);
    const frameData = captureCanvas.toDataURL('image/jpeg', 0.5);
    if (ws) {
        ws.send(JSON.stringify({type: 'frame', image: frameData}));
    }
}

document.getElementById('btnAutoPilot').onclick = () => {
    if (autoPilotRunning) {
        // Stop
        autoPilotRunning = false;
        if (ws) ws.send(JSON.stringify({type: 'stop_autopilot'}));
        document.getElementById('btnAutoPilot').innerHTML = '▶ Start Auto-Pilot';
        document.getElementById('btnAutoPilot').classList.remove('active');
        document.getElementById('screenStatus').className = 'status-indicator red';
    } else {
        // Start
        if (!video.srcObject) {
            alert('Please select a window to share first!');
            return;
        }
        autoPilotRunning = true;
        if (ws) ws.send(JSON.stringify({type: 'start_autopilot'}));
        document.getElementById('btnAutoPilot').innerHTML = '⏹ Stop Auto-Pilot';
        document.getElementById('btnAutoPilot').classList.add('active');
        document.getElementById('screenStatus').className = 'status-indicator green';
        sendFrame();
    }
};

// Initialize
connectWebSocket();
