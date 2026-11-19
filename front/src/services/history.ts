import type { AnimationFrame, Matrix, ProcessedResult } from '@/types'

export type HistoryItem = {
  id: string
  name: string
  createdAt: number
  original: Blob
  result?: Blob
  frames?: AnimationFrame[]
  matrix?: Matrix
}

const DB_NAME = 'pixelforge-history'
const STORE = 'images'

function openDb(): Promise<IDBDatabase> {
  return new Promise((resolve, reject) => {
    const request = indexedDB.open(DB_NAME, 1)
    request.onupgradeneeded = () => request.result.createObjectStore(STORE, { keyPath: 'id' })
    request.onsuccess = () => resolve(request.result)
    request.onerror = () => reject(request.error)
  })
}

export async function listHistory(): Promise<HistoryItem[]> {
  const db = await openDb()
  return new Promise((resolve, reject) => {
    const request = db.transaction(STORE).objectStore(STORE).getAll()
    request.onsuccess = () => resolve((request.result as HistoryItem[]).sort((a, b) => b.createdAt - a.createdAt))
    request.onerror = () => reject(request.error)
  })
}

export async function saveHistory(item: Omit<HistoryItem, 'id' | 'createdAt'>) {
  const db = await openDb()
  const value: HistoryItem = { ...item, id: crypto.randomUUID(), createdAt: Date.now() }
  return new Promise<void>((resolve, reject) => {
    const request = db.transaction(STORE, 'readwrite').objectStore(STORE).put(value)
    request.onsuccess = () => resolve()
    request.onerror = () => reject(request.error)
  })
}

export async function removeHistory(id: string) {
  const db = await openDb()
  db.transaction(STORE, 'readwrite').objectStore(STORE).delete(id)
}

export function resultFromHistory(item: HistoryItem): ProcessedResult | null {
  if (item.frames) return { animated: true, frames: item.frames }
  if (item.result) return { animated: false, blob: item.result }
  return null
}
