export function sampleImportName(value) {
  const stem = String(value ?? '').trim().replace(/\.wav$/i, '')
  if (!stem || stem.startsWith('.') || /[\\/:\x00-\x1f]/.test(stem) ||
      new TextEncoder().encode(stem).length > 20) {
    throw new Error('Use a name of 1–20 bytes without slashes or a leading dot.')
  }
  return `${stem}.wav`
}

export async function saveImportedSample(files, file, name, projectName) {
  if (!file || !/\.wav$/i.test(file.name)) throw new Error('Choose a WAV file.')
  const header = new Uint8Array(await file.slice(0, 12).arrayBuffer())
  const text = new TextDecoder().decode(header)
  if (text.length !== 12 || text.slice(0, 4) !== 'RIFF' || text.slice(8) !== 'WAVE') {
    throw new Error('This file is not a WAV sample.')
  }
  const leaf = sampleImportName(name)
  if (projectName) {
    const entries = await files.listDirectory(`/data/projects/${projectName}/samples`)
    if (entries.some((entry) => entry.name.toLowerCase() === leaf.toLowerCase())) {
      throw new Error('This project already has a sample with that name. Choose another name.')
    }
  }
  // uploadFiles rejects collisions and waits for the storage transaction.
  await files.uploadFiles([new File([file], leaf, { type: 'audio/wav' })], '/data/samples')
  return `/samples/${leaf}`
}

export async function importSample(files, projectName) {
  const file = await new Promise((resolve) => {
    const input = document.createElement('input')
    input.type = 'file'
    input.accept = '.wav,audio/wav,audio/x-wav'
    input.hidden = true
    const finish = (file) => { input.remove(); resolve(file) }
    input.addEventListener('change', () => finish(input.files?.[0] ?? null), { once: true })
    input.addEventListener('cancel', () => finish(null), { once: true })
    document.body.append(input)
    input.click()
  })
  if (!file) return null
  let draft = file.name.replace(/\.wav$/i, '')
  for (;;) {
    const name = window.prompt('Import sample — name', draft)
    if (name === null) return null
    draft = name
    try { return await saveImportedSample(files, file, name, projectName) }
    catch (error) { window.alert(error.message); if (!/\.wav$/i.test(file.name) || error.message.includes('not a WAV')) return null }
  }
}
