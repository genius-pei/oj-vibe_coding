export class ApiError extends Error {
  constructor(message, status) {
    super(message);
    this.status = status;
  }
}

export async function apiGet(path) {
  const response = await fetch(path, {
    credentials: 'same-origin',
    headers: { Accept: 'application/json' }
  });
  if (!response.ok) {
    let detail = '';
    try {
      const body = await response.json();
      detail = body.error || '';
    } catch (_) {
      detail = '';
    }
    throw new ApiError(detail || `HTTP ${response.status}`, response.status);
  }
  return response.json();
}