/** Calls the C++ API and converts HTTP failures into readable UI errors. */
export async function api<T>(path: string, payload?: unknown, signal?: AbortSignal): Promise<T> {
  // A same-origin proxy keeps local browser traffic and WebSockets on one origin.
  const response = await fetch(`/api/v1${path}`, {
    method: payload === undefined ? 'GET' : 'POST',
    headers: payload === undefined ? undefined : { 'Content-Type': 'application/json' },
    body: payload === undefined ? undefined : JSON.stringify(payload),
    signal,
  });
  // Error bodies share the JSON API contract; proxy failures may return plain text.
  const text = await response.text();
  let data: T & { error?: string }; // Parsed response after defensive JSON handling.
  try {
    data = JSON.parse(text);
  } catch {
    throw new Error('C++ engine is unreachable. Start the backend on port 8080.');
  }
  // A successful run snapshot may contain its own execution error; render it instead of hiding the run.
  if (!response.ok) throw new Error(data.error || `Request failed (${response.status})`);
  return data;
}

/** Downloads a self-contained audit snapshot without sending evidence externally. */
export function downloadJson(filename: string, value: unknown): void {
  const blob = new Blob([JSON.stringify(value, null, 2)], { type: 'application/json' }); // Local evidence file.
  const url = URL.createObjectURL(blob); // Temporary browser-owned download URL.
  const anchor = document.createElement('a'); // Ephemeral link triggers the native download flow.
  anchor.href = url;
  anchor.download = filename;
  anchor.click();
  window.setTimeout(() => URL.revokeObjectURL(url), 1000);
}
