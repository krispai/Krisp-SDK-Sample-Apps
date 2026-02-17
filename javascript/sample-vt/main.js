import { KrispVTSDK, LogLevel } from "@anthropic/krisp-vt-sdk";

const API_KEY = "your-api-key";
let vtSession = null;

function initSDK() {
  const sdk = new KrispVTSDK({
    apiKey: API_KEY,
    logLevel: LogLevel.DEBUG,
  });

  sdk.setHooks({
    onConnected: () => console.log("Session ready"),

    // Translated audio arrives here as a MediaStream
    onProcessedAudio: async (translatedStream) => {
      const audio = new Audio();
      audio.srcObject = translatedStream;
      audio.autoplay = true;
      audio.play();
    },

    onMessage: (event) => console.log("Message:", event),
    onError: (error) => console.error("Error:", error.message),
    onDisconnected: () => console.log("Disconnected"),
  });

  return sdk;
}

async function main() {
  vtSession = initSDK();

  const audioStream = await navigator.mediaDevices.getUserMedia({
    audio: { autoGainControl: true, sampleRate: 48000 },
    video: false,
  });

  await vtSession.start({
    from: "en-US",
    to: "es-ES",
    gender: "female",
  });

  // Audio processing is continuous; translated output via onProcessedAudio
  await vtSession.process(audioStream);
}

async function cleanup() {
  if (vtSession) {
    await vtSession.stop();
    vtSession = null;
  }
}

main();