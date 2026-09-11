import { expect, test } from '@playwright/test'

test.use({
  permissions: ['microphone'],
  launchOptions: { args: ['--use-fake-device-for-media-stream', '--use-fake-ui-for-media-stream'] },
})

async function openRecord(page) {
  const generation = await page.evaluate(() => globalThis.__picoTrackerViewsTest.generation())
  await page.evaluate(() => globalThis.__picoTrackerViewsTest.request(5))
  await expect.poll(() => page.evaluate(() => globalThis.__picoTrackerViewsTest.generation())).toBeGreaterThan(generation)
  await expect.poll(() => page.evaluate(() => globalThis.__picoTrackerViewsTest.current())).toBe(5)
  const tap = async (key) => {
    await page.keyboard.press(key, { delay: 80 })
    await page.waitForTimeout(80)
  }
  // The removed shortcut must leave us on Instrument.
  await page.keyboard.down('j')
  await tap('x')
  await page.keyboard.up('j')
  await tap('s') // Type
  await page.keyboard.down('k')
  await tap('d') // NONE -> SAMPLE
  await page.keyboard.up('k')
  await tap('s') // Sample actions
  await tap('d') // Import
  await tap('d') // Record
  await page.locator('#picotracker-canvas').screenshot({ path: 'test-results/sample-record-action.png' })
  await tap('k') // Open Record without capturing.
  await page.keyboard.down('c')
  await tap('a') // Return to Instrument with Record still selected.
  await page.keyboard.up('c')
  await tap('k') // Reopen Record.
  await page.locator('#picotracker-canvas').screenshot({ path: 'test-results/record-idle.png' })

}

test('Web Record uses Enter, names and loads a saved take, and releases the mic', async ({ page }) => {
  test.setTimeout(90000)
  await page.addInitScript(() => {
    window.capturedFrames = 0
    const Context = window.AudioContext
    window.AudioContext = class extends Context {
      createScriptProcessor(...args) {
        const node = super.createScriptProcessor(...args)
        node.addEventListener('audioprocess', (event) => { window.capturedFrames += event.inputBuffer.length })
        return node
      }
    }
    window.recordingNodes = []
    const Worklet = window.AudioWorkletNode
    window.AudioWorkletNode = class extends Worklet {
      constructor(context, name, options) { super(context, name, options); window.recordingNodes.push(options.processorOptions) }
    }
    window.recordingStreams = []
    window.microphoneRequests = 0
    const original = navigator.mediaDevices.getUserMedia.bind(navigator.mediaDevices)
    navigator.mediaDevices.getUserMedia = async (constraints) => {
      window.microphoneRequests++
      const stream = await original(constraints)
      window.recordingStreams.push(stream)
      return stream
    }
  })
  await page.goto('/?audio=disabled&storage-test=1&views-test=1')
  await expect(page.locator('[data-runtime-state="ready"]')).toBeVisible({ timeout: 20000 })
  await openRecord(page)
  expect(await page.evaluate(() => window.microphoneRequests)).toBe(0)
  await page.keyboard.press('k', { delay: 80 })
  await expect.poll(() => page.evaluate(() => window.recordingStreams[0]?.getTracks()[0].readyState)).toBe('live')
  await expect.poll(() => page.evaluate(() => window.recordingNodes[0]?.control ? Atomics.load(new Int32Array(window.recordingNodes[0].control), 0) : window.capturedFrames), { timeout: 12000 }).toBeGreaterThan(10000)
  await page.keyboard.press('k', { delay: 80 })
  await expect.poll(() => page.evaluate(() => window.recordingStreams[0].getTracks()[0].readyState)).toBe('ended')
  await expect.poll(() => page.evaluate(() => globalThis.__picoTrackerStorageTest.exists('/data/recordings/REC01.wav'))).toBe(true)
  const firstTake = await page.evaluate(() => {
    const bytes = Uint8Array.from(globalThis.__picoTrackerStorageTest.read('/data/recordings/REC01.wav'))
    const view = new DataView(bytes.buffer)
    let peak = 0
    for (let i = 44; i < bytes.length; i += 2) peak = Math.max(peak, Math.abs(view.getInt16(i, true)))
    return { length: bytes.length, channels: view.getUint16(22, true), rate: view.getUint32(24, true), frames: view.getUint32(40, true) / 2, peak }
  })
  expect(firstTake.rate).toBe(44100)
  expect(firstTake.channels).toBe(1)
  expect(firstTake.frames).toBeGreaterThan(10000)
  expect(firstTake.peak).toBeGreaterThan(0)
  await page.waitForTimeout(300)
  await page.locator('#picotracker-canvas').screenshot({ path: 'test-results/web-recording-editor.png' })
  await page.keyboard.down('c')
  await page.keyboard.press('a', { delay: 80 }) // Confirm leaving an unsaved take.
  await page.keyboard.up('c')
  await page.keyboard.press('d', { delay: 80 })
  await page.keyboard.press('k', { delay: 80 })
  await page.waitForTimeout(150)
  await page.keyboard.press('k', { delay: 80 }) // Instrument -> Record
  await page.waitForTimeout(150)
  expect(await page.evaluate(() => window.microphoneRequests)).toBe(1)
  await page.keyboard.press('k', { delay: 80 })
  await expect.poll(() => page.evaluate(() => window.recordingStreams[1]?.getTracks()[0].readyState)).toBe('live')
  await expect.poll(() => page.evaluate(() => window.recordingStreams[1].getTracks()[0].readyState), { timeout: 35000, intervals: [1000] }).toBe('ended')
  await expect.poll(() => page.evaluate(() => {
    const bytes = Uint8Array.from(globalThis.__picoTrackerStorageTest.read('/data/recordings/REC01.wav'))
    return new DataView(bytes.buffer).getUint32(40, true) / 2
  })).toBe(44100 * 30)
  await page.waitForTimeout(300)
  const tap = async (key) => {
    await page.keyboard.press(key, { delay: 80 })
    await page.waitForTimeout(100)
  }
  for (let i = 0; i < 3; i++) await tap('s') // Save
  await tap('k') // Name the recording
  await page.locator('#picotracker-canvas').screenshot({ path: 'test-results/recording-name.png' })
  await tap('w') // Naming actions; Save is selected.
  await tap('k')
  await expect.poll(() => page.evaluate(() => globalThis.__picoTrackerStorageTest.exists('/data/samples/Recording.wav'))).toBe(true)
  await expect.poll(() => page.evaluate(() => globalThis.__picoTrackerViewsTest.modelSnapshot().sampleCount)).toBe(1)
  const project = await page.evaluate(() => globalThis.__picoTrackerViewsTest.modelSnapshot().projectName)
  expect(await page.evaluate((p) => globalThis.__picoTrackerStorageTest.exists(`/data/projects/${p}/samples/Recording.wav`), project)).toBe(true)
  await page.locator('#picotracker-canvas').screenshot({ path: 'test-results/recording-saved-instrument.png' })
  await page.evaluate(() => globalThis.__picoTrackerStorageTest.flush())
  await page.reload()
  await expect(page.locator('[data-runtime-state="ready"]')).toBeVisible({ timeout: 20000 })
  expect(await page.evaluate(() => window.microphoneRequests)).toBe(0)
  expect(await page.evaluate(() => globalThis.__picoTrackerStorageTest.exists('/data/samples/Recording.wav'))).toBe(true)
})

test('stopping while microphone permission is pending releases a late stream', async ({ page }) => {
  await page.addInitScript(() => {
    const original = navigator.mediaDevices.getUserMedia.bind(navigator.mediaDevices)
    navigator.mediaDevices.getUserMedia = (constraints) => new Promise((resolve) => {
      window.grantPendingMicrophone = async () => {
        window.lateMicrophone = await original(constraints)
        resolve(window.lateMicrophone)
      }
    })
  })
  await page.goto('/?audio=disabled&storage-test=1&views-test=1')
  await expect(page.locator('[data-runtime-state="ready"]')).toBeVisible({ timeout: 20000 })
  await openRecord(page)
  await page.keyboard.press('k', { delay: 80 })
  await expect.poll(() => page.evaluate(() => typeof window.grantPendingMicrophone)).toBe('function')
  await page.keyboard.press('k', { delay: 80 })
  await page.evaluate(() => window.grantPendingMicrophone())
  await expect.poll(() => page.evaluate(() => window.lateMicrophone.getTracks()[0].readyState)).toBe('ended')
  expect(await page.evaluate(() => globalThis.__picoTrackerStorageTest.exists('/data/recordings/REC01.wav'))).toBe(false)
})

test('recording naming can be cancelled, then Discard returns without loading', async ({ page }) => {
  await page.addInitScript(() => {
    window.microphoneRequests = 0
    const original = navigator.mediaDevices.getUserMedia.bind(navigator.mediaDevices)
    navigator.mediaDevices.getUserMedia = (constraints) => {
      window.microphoneRequests++
      return original(constraints)
    }
  })
  await page.goto('/?audio=disabled&storage-test=1&views-test=1')
  await expect(page.locator('[data-runtime-state="ready"]')).toBeVisible({ timeout: 20000 })
  await openRecord(page)
  const tap = async (key) => {
    await page.keyboard.press(key, { delay: 80 })
    await page.waitForTimeout(100)
  }
  await tap('x') // PLAY no longer records.
  expect(await page.evaluate(() => window.microphoneRequests)).toBe(0)
  await tap('k')
  await expect.poll(() => page.evaluate(() => window.microphoneRequests)).toBe(1)
  await page.waitForTimeout(4000)
  await page.locator('#picotracker-canvas').screenshot({ path: 'test-results/record-active.png' })
  await tap('k')
  await expect.poll(() => page.evaluate(() => globalThis.__picoTrackerStorageTest.exists('/data/recordings/REC01.wav'))).toBe(true)
  await page.waitForTimeout(300)
  for (let i = 0; i < 3; i++) {
    await tap('s')
    if (i === 1) await page.locator('#picotracker-canvas').screenshot({ path: 'test-results/sample-operation-row.png' })
    if (i === 2) await page.locator('#picotracker-canvas').screenshot({ path: 'test-results/sample-save-row.png' })
  }
  await tap('k') // Save opens naming.
  await tap('w')
  await tap('a')
  await tap('a') // Cancel
  await tap('k')
  expect(await page.evaluate(() => globalThis.__picoTrackerStorageTest.exists('/data/samples/Recording.wav'))).toBe(false)
  expect(await page.evaluate(() => globalThis.__picoTrackerStorageTest.exists('/data/recordings/REC01.wav'))).toBe(true)
  await page.keyboard.down('c')
  await tap('a')
  await page.keyboard.up('c')
  await page.locator('#picotracker-canvas').screenshot({ path: 'test-results/sample-unsaved-confirm.png' })
  await tap('k') // No: keep the recording.
  expect(await page.evaluate(() => globalThis.__picoTrackerStorageTest.exists('/data/recordings/REC01.wav'))).toBe(true)
  await page.keyboard.down('c')
  await tap('a')
  await page.keyboard.up('c')
  await tap('d') // Yes
  await tap('k')
  await expect.poll(() => page.evaluate(() => globalThis.__picoTrackerStorageTest.exists('/data/recordings/REC01.wav'))).toBe(false)
  expect(await page.evaluate(() => globalThis.__picoTrackerViewsTest.modelSnapshot().sampleCount)).toBe(0)
  await page.locator('#picotracker-canvas').screenshot({ path: 'test-results/recording-discarded.png' })
})

for (const location of ['library', 'project']) {
  test(`recording Save rejects a case-insensitive name collision in the ${location}`, async ({ page }) => {
    await page.goto('/?audio=disabled&storage-test=1&views-test=1')
    await expect(page.locator('[data-runtime-state="ready"]')).toBeVisible({ timeout: 20000 })
    await openRecord(page)
    const tap = async (key) => {
      await page.keyboard.press(key, { delay: 80 })
      await page.waitForTimeout(100)
    }
    await tap('k')
    await page.waitForTimeout(4000)
    await page.locator('#picotracker-canvas').screenshot({ path: 'test-results/record-active.png' })
    await tap('k')
    await expect.poll(() => page.evaluate(() => globalThis.__picoTrackerStorageTest.exists('/data/recordings/REC01.wav'))).toBe(true)
    await page.waitForTimeout(300)
    const path = await page.evaluate((location) => {
      const project = globalThis.__picoTrackerViewsTest.modelSnapshot().projectName
      const path = location === 'library' ? '/data/samples/recording.WAV' : `/data/projects/${project}/samples/recording.WAV`
      globalThis.__picoTrackerStorageTest.write(path, [11, 22, 33])
      return path
    }, location)
    for (let i = 0; i < 3; i++) await tap('s')
    await tap('d') // Right on Save stays on Save.
    await tap('k')
    await tap('w')
    await tap('k') // Recording conflicts with recording.WAV.
    await page.waitForTimeout(300)
    expect(await page.evaluate((path) => globalThis.__picoTrackerStorageTest.read(path), path)).toEqual([11, 22, 33])
    expect(await page.evaluate(() => globalThis.__picoTrackerStorageTest.exists('/data/samples/Recording.wav'))).toBe(false)
    expect(await page.evaluate(() => globalThis.__picoTrackerStorageTest.exists('/data/recordings/REC01.wav'))).toBe(true)
    expect(await page.evaluate(() => globalThis.__picoTrackerViewsTest.modelSnapshot().sampleCount)).toBe(0)
    await page.locator('#picotracker-canvas').screenshot({ path: `test-results/record-name-collision-${location}.png` })
  })
}
