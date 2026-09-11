import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { readFileSync } from 'node:fs'
import { runInNewContext } from 'node:vm'
import { createRecordingHandle, encodeRecordingWav, RecordingState as State } from '../src/handles/recording.js'

function deferred() { let resolve; let reject; const promise = new Promise((a, b) => { resolve = a; reject = b }); return { promise, resolve, reject } }
function fixture() {
  const track = { stop: vi.fn(), addEventListener: vi.fn() }
  const stream = { getTracks: () => [track] }
  const context = { sampleRate: 44100, resume: vi.fn().mockResolvedValue(), close: vi.fn().mockResolvedValue(), audioWorklet: { addModule: vi.fn().mockResolvedValue() }, destination: {} }
  const source = { connect: vi.fn(), disconnect: vi.fn() }
  context.createMediaStreamSource = () => source
  context.createGain = () => ({ gain: { value: 1 }, connect: vi.fn(), disconnect: vi.fn() })
  let node
  const environment = {
    navigator: { mediaDevices: { getUserMedia: vi.fn().mockResolvedValue(stream) } },
    AudioContext: class { constructor() { return context } },
    AudioWorkletNode: class { constructor(c, n, options) { node = this; this.options = options; this.port = { close: vi.fn() }; this.connect = vi.fn(); this.disconnect = vi.fn() } },
    setInterval, clearInterval, setTimeout, clearTimeout,
    addEventListener: vi.fn(), removeEventListener: vi.fn(),
  }
  const files = { saveRecording: vi.fn().mockResolvedValue() }
  const publish = vi.fn()
  const handle = createRecordingHandle(files, environment)
  return { handle, files, environment, publish, context, stream, track, node: () => node,
    capture(values) {
      const options = node.options.processorOptions
      new Float32Array(options.samples).set(values)
      Atomics.store(new Int32Array(options.control), 0, values.length)
    } }
}
beforeEach(() => { vi.useFakeTimers(); vi.spyOn(console, 'error').mockImplementation(() => {}) })
afterEach(() => { vi.useRealTimers(); vi.restoreAllMocks() })

describe('Web recording lifecycle', () => {
  it('does not request a mic on construction and closes it before durable saving', async () => {
    const f = fixture()
    expect(f.environment.navigator.mediaDevices.getUserMedia).not.toHaveBeenCalled()
    await f.handle.start('/recordings/REC01.wav', 30000, f.publish)
    expect(f.publish).toHaveBeenLastCalledWith(State.capturing, 0, 0)
    f.capture([0, .5, -.5])
    const saving = deferred()
    f.files.saveRecording.mockReturnValue(saving.promise)
    const stopped = f.handle.stop()
    expect(f.track.stop).toHaveBeenCalledOnce()
    expect(f.context.close).toHaveBeenCalledOnce()
    expect(f.publish).toHaveBeenLastCalledWith(State.saving, 0, 0)
    saving.resolve()
    await stopped
    expect(f.publish).toHaveBeenLastCalledWith(State.saved, 0, 0)
    expect(f.files.saveRecording.mock.calls[0][0]).toBe('/data/recordings/REC01.wav')
  })
  it('stops late permission streams after cancellation without starting a take', async () => {
    const f = fixture(); const permission = deferred()
    f.environment.navigator.mediaDevices.getUserMedia.mockReturnValue(permission.promise)
    const starting = f.handle.start('/recordings/REC01.wav', 30000, f.publish)
    await f.handle.stop()
    permission.resolve(f.stream); await starting
    expect(f.track.stop).toHaveBeenCalledOnce()
    expect(f.files.saveRecording).not.toHaveBeenCalled()
    expect(f.node()).toBeUndefined()
  })
  it('permission denial and processor failure release resources', async () => {
    const f = fixture()
    f.environment.navigator.mediaDevices.getUserMedia.mockRejectedValueOnce(new Error('Denied'))
    await f.handle.start('/recordings/REC01.wav', 30000, f.publish)
    expect(f.context.close).toHaveBeenCalledOnce()
    expect(f.publish).toHaveBeenLastCalledWith(State.failed, 0, 0)
    await f.handle.start('/recordings/REC01.wav', 30000, f.publish)
    f.node().onprocessorerror()
    expect(f.track.stop).toHaveBeenCalledOnce()
    expect(f.files.saveRecording).not.toHaveBeenCalled()
  })
  it('full takes stop hardware and save, and storage failure never reports success', async () => {
    const f = fixture()
    await f.handle.start('/recordings/REC01.wav', 1, f.publish)
    f.capture(new Float32Array(44))
    f.files.saveRecording.mockRejectedValueOnce(new Error('Quota exceeded'))
    f.node().port.onmessage({ data: 'full' })
    await vi.runAllTimersAsync()
    expect(f.track.stop).toHaveBeenCalledOnce()
    expect(f.publish).toHaveBeenLastCalledWith(State.failed, 0, 0)
  })
  it('dispose cancels capture and ignores old processor messages', async () => {
    const f = fixture()
    await f.handle.start('/recordings/REC01.wav', 30000, f.publish)
    f.capture([.5])
    const oldNode = f.node()
    await f.handle.dispose()
    oldNode.port.onmessage({ data: 'full' })
    expect(f.track.stop).toHaveBeenCalledOnce()
    expect(f.files.saveRecording).not.toHaveBeenCalled()
  })
  it('hung setup closes the mic and a late module load cannot restart it', async () => {
    const f = fixture(); const setup = deferred()
    f.context.audioWorklet.addModule.mockReturnValue(setup.promise)
    const starting = f.handle.start('/recordings/REC01.wav', 30000, f.publish)
    await vi.advanceTimersByTimeAsync(10000)
    expect(f.track.stop).toHaveBeenCalledOnce()
    setup.resolve(); await starting
    expect(f.node()).toBeUndefined()
  })
})

it('encodes mono 44.1k PCM16 with saturation and correct RIFF sizes', () => {
  const bytes = encodeRecordingWav(new Float32Array([-2, -.5, 0, .5, 2, NaN]))
  const view = new DataView(bytes.buffer)
  expect(view.getUint32(24, true)).toBe(44100)
  expect(view.getUint16(22, true)).toBe(1)
  expect(view.getUint32(4, true)).toBe(bytes.length - 8)
  expect(view.getUint32(40, true)).toBe(12)
  expect(Array.from({ length: 6 }, (_, i) => view.getInt16(44 + i * 2, true))).toEqual([-32768, -16384, 0, 16384, 32767, 0])
})

it('the production worklet commits only bounded mono frames and honors stop', () => {
  let Processor
  runInNewContext(readFileSync(new URL('../public/worklets/microphone.js', import.meta.url), 'utf8'), {
    AudioWorkletProcessor: class { constructor() { this.port = { postMessage: vi.fn() } } },
    registerProcessor: (_, implementation) => { Processor = implementation },
    Float32Array, Int32Array, Atomics, Math,
  })
  const samples = new Float32Array(new SharedArrayBuffer(3 * 4))
  const control = new Int32Array(new SharedArrayBuffer(3 * 4))
  const processor = new Processor({ processorOptions: { samples: samples.buffer, control: control.buffer } })
  expect(processor.process([])).toBe(true)
  expect(processor.process([[new Float32Array([1, 0]), new Float32Array([0, 1])]])).toBe(true)
  expect(control[0]).toBe(2)
  expect(processor.process([[new Float32Array([-2, 1])]])).toBe(false)
  expect(Array.from(samples)).toEqual([.5, .5, -1])
  expect(control[0]).toBe(3)
  expect(processor.port.postMessage).toHaveBeenCalledWith('full')
  Atomics.store(control, 2, 1)
  expect(processor.process([[new Float32Array([1])]])).toBe(false)
  expect(Array.from(samples)).toEqual([.5, .5, -1])
})

it('uses bounded raw PCM when worklet loading fails, without speaker monitoring', async () => {
  const f = fixture()
  f.context.audioWorklet.addModule.mockRejectedValue(new Error('Worklet unavailable'))
  const fallback = { connect: vi.fn(), disconnect: vi.fn() }
  f.context.createScriptProcessor = vi.fn(() => fallback)
  await f.handle.start('/recordings/REC01.wav', 1, f.publish)
  const output = new Float32Array(128).fill(1)
  fallback.onaudioprocess({
    inputBuffer: { getChannelData: () => new Float32Array(128).fill(.5) },
    outputBuffer: { getChannelData: () => output },
  })
  await vi.runAllTimersAsync()
  expect(output.every(value => value === 0)).toBe(true)
  expect(f.track.stop).toHaveBeenCalledOnce()
  const bytes = f.files.saveRecording.mock.calls[0][1]
  expect(new DataView(bytes.buffer).getUint32(40, true)).toBe(44 * 2)
  expect(f.publish).toHaveBeenLastCalledWith(State.saved, 0, 0)
})
