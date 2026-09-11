export const RecordingState = Object.freeze({ idle: 0, requesting: 1, capturing: 2, saving: 3, saved: 4, failed: 5 })
const SAMPLE_RATE = 44100
const MAX_DURATION_MS = 30000

export function encodeRecordingWav(samples) {
  const bytes = new Uint8Array(44 + samples.length * 2)
  const view = new DataView(bytes.buffer)
  const text = (offset, value) => bytes.set(new TextEncoder().encode(value), offset)
  text(0, 'RIFF'); view.setUint32(4, bytes.length - 8, true)
  text(8, 'WAVEfmt '); view.setUint32(16, 16, true)
  view.setUint16(20, 1, true); view.setUint16(22, 1, true)
  view.setUint32(24, SAMPLE_RATE, true); view.setUint32(28, SAMPLE_RATE * 2, true)
  view.setUint16(32, 2, true); view.setUint16(34, 16, true)
  text(36, 'data'); view.setUint32(40, samples.length * 2, true)
  for (let index = 0; index < samples.length; index++) {
    const sample = Number.isFinite(samples[index]) ? Math.max(-1, Math.min(1, samples[index])) : 0
    view.setInt16(44 + index * 2, Math.round(sample * (sample < 0 ? 32768 : 32767)), true)
  }
  return bytes
}

export function createRecordingHandle(files, environment = globalThis) {
  let active = null
  let disposed = false
  const publish = (session, state, count = 0, peak = 0) => session.publish(state, Math.floor(count * 1000 / SAMPLE_RATE), peak)
  function release(session) {
    if (session.control) Atomics.store(session.control, 2, 1)
    environment.clearInterval(session.timer)
    environment.clearTimeout(session.setupTimer)
    environment.clearTimeout(session.workletTimer)
    environment.clearTimeout(session.deadline)
    for (const track of session.stream?.getTracks() ?? []) track.stop()
    session.source?.disconnect()
    session.node?.disconnect()
    session.node?.port?.close()
    if (session.context) void session.context.close().catch(() => {})
  }
  async function finish(session, save) {
    if (active !== session || session.finishing) return session.completion
    session.finishing = true
    // Close hardware before encoding, waiting for disk, or handling an error.
    release(session)
    const count = session.control ? Atomics.load(session.control, 0) : 0
    session.completion = (async () => {
      if (!save || count === 0) { publish(session, RecordingState.failed); return }
      publish(session, RecordingState.saving, count)
      try {
        const bytes = encodeRecordingWav(session.samples.subarray(0, count))
        await files.saveRecording(session.path, bytes)
        publish(session, RecordingState.saved, count)
      } catch (error) {
        console.error('[Recording] Save failed', error)
        publish(session, RecordingState.failed, count)
      }
    })()
    await session.completion
    if (active === session) active = null
  }
  async function start(path, durationMs, onUpdate) {
    if (disposed || active) { onUpdate(RecordingState.failed, 0, 0); return }
    const session = { path: `/data${path}`, publish: onUpdate, finishing: false }
    active = session
    publish(session, RecordingState.requesting)
    try {
      if (path !== '/recordings/REC01.wav') throw new Error('Invalid recording destination')
      session.context = new environment.AudioContext({ sampleRate: SAMPLE_RATE })
      if (session.context.sampleRate !== SAMPLE_RATE) throw new Error('44.1 kHz recording is unavailable')
      // Resume in the initiating gesture, before the permission prompt can outlive it.
      const resumed = session.context.resume()
      resumed.catch(() => {})
      const stream = await environment.navigator.mediaDevices.getUserMedia({
        audio: { channelCount: 1, sampleRate: SAMPLE_RATE, echoCancellation: false, noiseSuppression: false, autoGainControl: false },
        video: false,
      })
      if (active !== session || session.finishing || disposed) {
        for (const track of stream.getTracks()) track.stop()
        return
      }
      session.stream = stream
      session.setupTimer = environment.setTimeout(() => { void finish(session, false) }, 10000)
      for (const track of stream.getTracks()) track.addEventListener('ended', () => { void finish(session, true) }, { once: true })
      // A few browser/audio-driver combinations leave addModule pending.
      // Keep a raw-PCM compatibility path rather than holding the mic open.
      let workletReady = false
      if (session.context.audioWorklet && environment.AudioWorkletNode) {
        try {
          workletReady = await Promise.race([
            session.context.audioWorklet.addModule('/worklets/microphone.js').then(() => true),
            new Promise(resolve => { session.workletTimer = environment.setTimeout(() => resolve(false), 2000) }),
          ])
        } catch { /* Use the bounded PCM callback below. */ }
      }
      environment.clearTimeout(session.workletTimer)
      await resumed
      if (active !== session || session.finishing || disposed) return
      const capacity = Math.max(1, Math.floor(SAMPLE_RATE * Math.min(MAX_DURATION_MS, Math.max(1, durationMs || MAX_DURATION_MS)) / 1000))
      session.samples = new Float32Array(new SharedArrayBuffer(capacity * 4))
      session.control = new Int32Array(new SharedArrayBuffer(3 * 4))
      if (workletReady) {
        session.node = new environment.AudioWorkletNode(session.context, 'tracker-microphone', {
          numberOfInputs: 1, numberOfOutputs: 1, outputChannelCount: [1],
          processorOptions: { samples: session.samples.buffer, control: session.control.buffer },
        })
        session.node.port.onmessage = () => { void finish(session, true) }
        session.node.onprocessorerror = () => { void finish(session, false) }
      } else {
        session.node = session.context.createScriptProcessor(2048, 1, 1)
        session.node.onaudioprocess = (event) => {
          if (active !== session || session.finishing) return
          const input = event.inputBuffer.getChannelData(0)
          const offset = Atomics.load(session.control, 0)
          const count = Math.min(input.length, capacity - offset)
          let peak = 0
          for (let index = 0; index < count; index++) {
            const value = Math.max(-1, Math.min(1, input[index]))
            session.samples[offset + index] = value
            peak = Math.max(peak, Math.abs(value))
          }
          Atomics.store(session.control, 1, Math.round(peak * 32767))
          Atomics.store(session.control, 0, offset + count)
          // Never monitor the microphone through the speakers.
          event.outputBuffer.getChannelData(0).fill(0)
          if (offset + count === capacity) void finish(session, true)
        }
      }
      environment.clearTimeout(session.setupTimer)
      session.deadline = environment.setTimeout(() => { void finish(session, true) }, capacity * 1000 / SAMPLE_RATE + 1000)
      session.source = session.context.createMediaStreamSource(stream)
      session.source.connect(session.node)
      session.node.connect(session.context.destination)
      publish(session, RecordingState.capturing)
      session.timer = environment.setInterval(() => {
        if (active !== session || session.finishing) return
        const count = Atomics.load(session.control, 0)
        publish(session, RecordingState.capturing, count, Atomics.load(session.control, 1))
        if (count >= capacity) void finish(session, true)
      }, 50)
    } catch (error) {
      if (active === session && !session.finishing) {
        console.error('[Recording] Start failed', error)
        await finish(session, false)
      }
    }
  }
  const stop = () => active ? finish(active, true) : Promise.resolve()
  const dispose = () => {
    environment.removeEventListener?.('pagehide', dispose)
    disposed = true
    return active ? finish(active, false) : Promise.resolve()
  }
  environment.addEventListener?.('pagehide', dispose)
  return Object.freeze({ start, stop, dispose })
}
