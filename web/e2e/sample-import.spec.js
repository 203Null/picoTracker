import { expect, test } from '@playwright/test'

async function tap(page, key) {
  await page.keyboard.press(key, { delay: 80 })
  await page.waitForTimeout(80)
}
function wav() {
  const bytes = Buffer.alloc(44 + 128 * 2)
  bytes.write('RIFF', 0); bytes.writeUInt32LE(bytes.length - 8, 4)
  bytes.write('WAVEfmt ', 8); bytes.writeUInt32LE(16, 16)
  bytes.writeUInt16LE(1, 20); bytes.writeUInt16LE(1, 22)
  bytes.writeUInt32LE(44100, 24); bytes.writeUInt32LE(88200, 28)
  bytes.writeUInt16LE(2, 32); bytes.writeUInt16LE(16, 34)
  bytes.write('data', 36); bytes.writeUInt32LE(256, 40)
  return bytes
}

test('Sample Import opens OS picker, renames, persists and loads the chosen WAV', async ({ page }) => {
  await page.goto('/?audio=disabled&storage-test=1&views-test=1')
  await expect(page.locator('[data-runtime-state="ready"]')).toBeVisible({ timeout: 20000 })
  await page.evaluate(() => globalThis.__picoTrackerViewsTest.request(5))
  await expect.poll(() => page.evaluate(() => globalThis.__picoTrackerViewsTest.current())).toBe(5)
  await tap(page, 's') // instrument type
  await page.keyboard.down('k')
  await tap(page, 'd') // NONE -> SAMPLE
  await page.keyboard.up('k')
  await tap(page, 's') // sample field
  await tap(page, 'd') // Import
  // Cancelling either stage must release the native pending operation.
  const cancelledPicker = page.waitForEvent('filechooser')
  await tap(page, 'k')
  await (await cancelledPicker).setFiles([])
  await page.waitForTimeout(150)
  const cancelledRename = page.waitForEvent('filechooser')
  await tap(page, 'k')
  page.once('dialog', (dialog) => dialog.dismiss())
  await (await cancelledRename).setFiles({ name: 'cancelled.wav', mimeType: 'audio/wav', buffer: wav() })
  await page.waitForTimeout(150)
  expect(await page.evaluate(() => globalThis.__picoTrackerViewsTest.modelSnapshot().sampleCount)).toBe(0)
  expect(await page.evaluate(() => globalThis.__picoTrackerStorageTest.exists('/data/samples/cancelled.wav'))).toBe(false)
  const picker = page.waitForEvent('filechooser')
  await tap(page, 'k')
  page.once('dialog', (dialog) => dialog.accept('ImportedKick'))
  await (await picker).setFiles({ name: 'source.wav', mimeType: 'audio/wav', buffer: wav() })
  await expect.poll(() => page.evaluate(() => globalThis.__picoTrackerViewsTest.modelSnapshot().sampleCount)).toBe(1)
  const project = await page.evaluate(() => globalThis.__picoTrackerViewsTest.modelSnapshot().projectName)
  for (const path of ['/data/samples/ImportedKick.wav', `/data/projects/${project}/samples/ImportedKick.wav`]) {
    await expect.poll(() => page.evaluate((path) => globalThis.__picoTrackerStorageTest.exists(path), path)).toBe(true)
  }
  // Editor activation verifies that the imported pool entry is bound to this instrument.
  await tap(page, 'd') // Record
  await tap(page, 'd') // Edit
  await tap(page, 'k')
  await page.screenshot({ path: 'test-results/sample-import-editor.png' })
  const original = await page.evaluate((p) => globalThis.__picoTrackerStorageTest.read(`/data/projects/${p}/samples/ImportedKick.wav`), project)
  await page.locator('#picotracker-canvas').screenshot({ path: 'test-results/sample-start-edit.png' })
  for (let i = 0; i < 6; i++) await tap(page, 'd')
  await page.keyboard.down('k')
  await page.waitForTimeout(120)
  await page.locator('#picotracker-canvas').screenshot({ path: 'test-results/sample-start-digit-value.png' })
  await tap(page, 'w') // Trim one frame from the start.
  await page.keyboard.up('k')
  await page.keyboard.down('c')
  await tap(page, 'a') // Range-only edits also need an unsaved warning.
  await page.keyboard.up('c')
  await page.locator('#picotracker-canvas').screenshot({ path: 'test-results/sample-range-unsaved.png' })
  await tap(page, 'k') // No: retain the edited start point.
  await tap(page, 's') // END
  await tap(page, 's') // OPERATION
  await tap(page, 'k') // Trim starts immediately; no confirmation.
  await page.waitForTimeout(500)
  await page.keyboard.down('c')
  await tap(page, 'a') // Unsaved edits require confirmation.
  await page.keyboard.up('c')
  await page.locator('#picotracker-canvas').screenshot({ path: 'test-results/sample-edit-unsaved.png' })
  await tap(page, 'k') // No: keep editing.
  await tap(page, 's') // SAVE
  await tap(page, 'd') // Save As
  await page.locator('#picotracker-canvas').screenshot({ path: 'test-results/sample-save-as.png' })
  await tap(page, 'k')
  await tap(page, 'w') // Keep suggested ImportedKick-copy name.
  await tap(page, 'k')
  await expect.poll(() => page.evaluate(() => globalThis.__picoTrackerStorageTest.exists('/data/samples/ImportedKick-copy.wav'))).toBe(true)
  expect(await page.evaluate((p) => globalThis.__picoTrackerStorageTest.read(`/data/projects/${p}/samples/ImportedKick.wav`), project)).toEqual(original)
  const copy = await page.evaluate(() => globalThis.__picoTrackerStorageTest.read('/data/samples/ImportedKick-copy.wav'))
  expect(copy.length).toBe(original.length - 2)
  await expect.poll(() => page.evaluate(() => globalThis.__picoTrackerViewsTest.modelSnapshot().sampleCount)).toBe(2)

  await page.evaluate(() => globalThis.__picoTrackerStorageTest.flush())
  await page.reload()
  await expect(page.locator('[data-runtime-state="ready"]')).toBeVisible({ timeout: 20000 })
  expect(await page.evaluate(() => globalThis.__picoTrackerStorageTest.exists('/data/samples/ImportedKick.wav'))).toBe(true)
})
