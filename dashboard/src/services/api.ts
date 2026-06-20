import type { Snapshot } from '../types/snapshot';

export async function getSnapshot(signal?: AbortSignal): Promise<Snapshot> {
  const response = await fetch('/api/status', {
    cache: 'no-store',
    signal,
  });

  if (!response.ok) {
    throw new Error(`Falha ao consultar /api/status (${response.status})`);
  }

  return response.json() as Promise<Snapshot>;
}
