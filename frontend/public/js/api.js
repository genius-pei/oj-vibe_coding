export class ApiError extends Error {
  constructor(message, status) {
    super(message);
    this.name = 'ApiError';
    this.status = status;
  }
}

async function readJson(response) {
  try {
    return await response.json();
  } catch (_) {
    return null;
  }
}

function buildError(message, status) {
  return new ApiError(message || `HTTP ${status}`, status);
}

export async function apiGet(path) {
  const response = await fetch(path, {
    credentials: 'same-origin',
    headers: { Accept: 'application/json' }
  });
  if (!response.ok) {
    const body = await readJson(response);
    throw buildError(body && body.error, response.status);
  }
  return response.json();
}

export async function apiPost(path, payload) {
  const response = await fetch(path, {
    method: 'POST',
    credentials: 'same-origin',
    headers: {
      Accept: 'application/json',
      'Content-Type': 'application/json'
    },
    body: JSON.stringify(payload)
  });
  if (!response.ok) {
    const body = await readJson(response);
    throw buildError(body && body.error, response.status);
  }
  return response.json();
}

export async function apiPut(path, payload) {
  const response = await fetch(path, {
    method: 'PUT',
    credentials: 'same-origin',
    headers: {
      Accept: 'application/json',
      'Content-Type': 'application/json'
    },
    body: JSON.stringify(payload)
  });
  if (!response.ok) {
    const body = await readJson(response);
    throw buildError(body && body.error, response.status);
  }
  if (response.status === 204) {
    return null;
  }
  return response.json();
}

export async function apiDelete(path) {
  const response = await fetch(path, {
    method: 'DELETE',
    credentials: 'same-origin',
    headers: { Accept: 'application/json' }
  });
  if (!response.ok) {
    const body = await readJson(response);
    throw buildError(body && body.error, response.status);
  }
  if (response.status === 204) {
    return null;
  }
  return response.json();
}
