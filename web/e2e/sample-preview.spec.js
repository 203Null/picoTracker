import { expect, test } from '@playwright/test'
import { stopWorkbench } from './helpers/runtime.js'

function previewWav() {
  const frames = 44100 * 4
  const wav = Buffer.alloc(44 + frames * 2)
  wav.write('RIFF', 0)
  wav.writeUInt32LE(wav.length - 8, 4)
  wav.write('WAVEfmt ', 8)
  wav.writeUInt32LE(16, 16)
  wav.writeUInt16LE(1, 20)
  wav.writeUInt16LE(1, 22)
  wav.writeUInt32LE(44100, 24)
  wav.writeUInt32LE(88200, 28)
  wav.writeUInt16LE(2, 32)
  wav.writeUInt16LE(16, 34)
  wav.write('data', 36)
  wav.writeUInt32LE(frames * 2, 40)
  for (let i = 0; i < frames; i += 1) {
    wav.writeInt16LE(Math.round(10000 * Math.sin(2 * Math.PI * 440 * i / 44100)), 44 + i * 2)
  }
  return wav
}

async function tap(page, key) {
  const generation = () => page.evaluate(() => globalThis.__picoTrackerViewsTest.inputGeneration())
  const before = await generation()
  await page.keyboard.down(key)
  await expect.poll(generation).toBeGreaterThanOrEqual(before + 1)
  await page.keyboard.up(key)
  await expect.poll(generation).toBeGreaterThanOrEqual(before + 2)
}

async function playheadX(page) {
  return page.locator('#picotracker-canvas').evaluate((canvas) => {
    // The tracker transfers its canvas to the rendering worker. Read a copy
    // of the presented bitmap instead of taking ownership of that context.
    const copy = document.createElement('canvas')
    copy.width = copy.height = 240
    const context = copy.getContext('2d')
    context.drawImage(canvas, 0, 0, 240, 240)
    const { data, width } = context.getImageData(0, 0, 240, 240)
    // A playhead is a full-height white line. The cyan waveform and endpoint
    // markers must not satisfy this, including after the preview stops.
    for (let x = 10; x < 230; x += 1) {
      let white = 0
      for (let y = 60; y < 132; y += 1) {
        const index = (y * width + x) * 4
        const channels = [data[index], data[index + 1], data[index + 2]]
        if (Math.min(...channels) > 150 && Math.max(...channels) - Math.min(...channels) < 40) white += 1
      }
      if (white >= 68) return x
    }
    return -1
  })
}

test('sample editor playhead advances after Play release and clears on the next tap', async ({ page }) => {
  await page.goto('/?views-test=1')
  await expect(page.locator('[data-runtime-state="ready"]')).toBeVisible({ timeout: 20_000 })
  try {
    await page.evaluate(() => globalThis.__picoTrackerViewsTest.request(5))
    await expect.poll(() => page.evaluate(() => globalThis.__picoTrackerViewsTest.current())).toBe(5)
    await tap(page, 's')
    await page.keyboard.down('k')
    await tap(page, 'd') // NONE -> SAMPLE
    await page.keyboard.up('k')
    await tap(page, 's')
    await tap(page, 'd') // IMPORT
    page.on('dialog', (dialog) => dialog.accept('PREVIEW_TEST'))
    const chooser = page.waitForEvent('filechooser')
    await tap(page, 'k')
    await (await chooser).setFiles({ name: 'PREVIEW_TEST.wav', mimeType: 'audio/wav', buffer: previewWav() })
    await expect.poll(() => page.evaluate(() => globalThis.__picoTrackerViewsTest.modelSnapshot().sampleCount)).toBe(1)
    await tap(page, 'd')
    await tap(page, 'd') // EDIT
    await tap(page, 'k')

    // This checks the application clock and rendered cursor, independently
    // of whether the test machine has an available audio output device.
    await tap(page, 'x')
    await expect.poll(() => playheadX(page)).toBeGreaterThan(9)
    const first = await playheadX(page)
    await expect.poll(() => playheadX(page)).toBeGreaterThan(first + 10)
    await tap(page, 'x')
    await expect.poll(() => playheadX(page)).toBe(-1)
  } finally {
    await stopWorkbench(page)
  }
})
