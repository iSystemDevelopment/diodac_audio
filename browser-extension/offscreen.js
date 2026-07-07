let audioCtx = null;
let sourceNode = null;
let compressorNode = null;
let gainNode = null;
let stream = null;

chrome.runtime.onMessage.addListener(async (request, sender, sendResponse) => {
    
    if (request.action === 'start_audio_capture') {
        try {
            // Get the media stream using the streamId provided by the background script
            stream = await navigator.mediaDevices.getUserMedia({
                audio: {
                    mandatory: {
                        chromeMediaSource: 'tab',
                        chromeMediaSourceId: request.streamId
                    }
                },
                video: false
            });

            audioCtx = new AudioContext();
            
            // Create Audio Graph
            sourceNode = audioCtx.createMediaStreamSource(stream);
            
            // Compressor (Normalizer)
            compressorNode = audioCtx.createDynamicsCompressor();
            compressorNode.threshold.setValueAtTime(-24, audioCtx.currentTime); // Start compressing early
            compressorNode.knee.setValueAtTime(30, audioCtx.currentTime);
            compressorNode.ratio.setValueAtTime(12, audioCtx.currentTime); // Hard compression
            compressorNode.attack.setValueAtTime(0.003, audioCtx.currentTime);
            compressorNode.release.setValueAtTime(0.25, audioCtx.currentTime);

            // Volume Control
            gainNode = audioCtx.createGain();
            gainNode.gain.value = 1.0; // Default 100%

            // Connect
            sourceNode.connect(compressorNode);
            compressorNode.connect(gainNode);
            gainNode.connect(audioCtx.destination);

            console.log("Audio Normalizer running in offscreen document.");

        } catch (e) {
            console.error("Failed to capture tab audio:", e);
        }
    }

    if (request.action === 'stop_audio_capture') {
        if (stream) {
            stream.getTracks().forEach(track => track.stop());
        }
        if (audioCtx) {
            audioCtx.close();
        }
    }

    if (request.action === 'set_volume') {
        if (gainNode) {
            // volume is 0.0 to 2.0 (0% to 200%)
            gainNode.gain.linearRampToValueAtTime(request.volume, audioCtx.currentTime + 0.1);
        }
    }
});
