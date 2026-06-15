import { useCallback, useEffect, useRef, useState } from 'react';
import { getSnapshot } from '../services/api';
import type { Snapshot } from '../types/snapshot';

const POLLING_INTERVAL_MS = 10_000;

export function useSnapshot() {
  const [snapshot, setSnapshot] = useState<Snapshot | null>(null);
  const [isLoading, setIsLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const activeRequest = useRef<AbortController | null>(null);

  const refresh = useCallback(async () => {
    activeRequest.current?.abort();
    const controller = new AbortController();
    activeRequest.current = controller;

    setIsLoading(true);
    setError(null);

    try {
      setSnapshot(await getSnapshot(controller.signal));
    } catch (requestError) {
      if (requestError instanceof DOMException && requestError.name === 'AbortError') {
        return;
      }

      console.error(requestError);
      setError('Falha de comunicação');
    } finally {
      if (activeRequest.current === controller) {
        activeRequest.current = null;
        setIsLoading(false);
      }
    }
  }, []);

  useEffect(() => {
    void refresh();
    const intervalId = window.setInterval(() => void refresh(), POLLING_INTERVAL_MS);

    return () => {
      window.clearInterval(intervalId);
      activeRequest.current?.abort();
    };
  }, [refresh]);

  return { snapshot, isLoading, error, refresh };
}
