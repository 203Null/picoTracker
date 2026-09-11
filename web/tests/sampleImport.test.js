import { describe, expect, it, vi } from 'vitest'
import { sampleImportName, saveImportedSample } from '../src/handles/sampleImport.js'

const wav = () => new File(['RIFF0000WAVEdata'], 'source.wav', { type: 'audio/wav' })
describe('sample import', () => {
  it('normalizes the extension and rejects unsafe names', () => {
    expect(sampleImportName(' Kick.WAV ')).toBe('Kick.wav')
    for (const name of ['', '../kick', 'a/b', 'a\\b', '.hidden', 'a\0b']) {
      expect(() => sampleImportName(name)).toThrow()
    }
  })
  it('persists the renamed file before returning the native library path', async () => {
    const uploadFiles = vi.fn(async ([file], directory) => {
      expect(file.name).toBe('Kick.wav')
      expect(directory).toBe('/data/samples')
      expect(await file.text()).toBe('RIFF0000WAVEdata')
    })
    await expect(saveImportedSample({ uploadFiles }, wav(), 'Kick')).resolves.toBe('/samples/Kick.wav')
    expect(uploadFiles).toHaveBeenCalledOnce()
  })
  it('does not load when a collision or storage failure prevents saving', async () => {
    const uploadFiles = vi.fn().mockRejectedValue(new Error('Destination already exists'))
    await expect(saveImportedSample({ uploadFiles }, wav(), 'Kick')).rejects.toThrow('already exists')
  })
  it('rejects a project collision before saving the library file', async () => {
    const files = { listDirectory: vi.fn().mockResolvedValue([{ name: 'Kick.wav' }]), uploadFiles: vi.fn() }
    await expect(saveImportedSample(files, wav(), 'kick', '.untitled')).rejects.toThrow('already has')
    expect(files.uploadFiles).not.toHaveBeenCalled()
  })
  it('rejects fake WAVs before writing', async () => {
    const uploadFiles = vi.fn()
    await expect(saveImportedSample({ uploadFiles }, new File(['not audio'], 'bad.wav'), 'bad')).rejects.toThrow('not a WAV')
    expect(uploadFiles).not.toHaveBeenCalled()
  })
})
