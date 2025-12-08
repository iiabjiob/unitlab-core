export function buildQuery(baseUrl: string, params?: Record<string, any>) {
  if (!params) return baseUrl;

  const queryString = Object.entries(params)
    .filter(([, value]) => value !== undefined && value !== null)
    .map(([key, value]) => `${encodeURIComponent(key)}=${encodeURIComponent(value)}`)
    .join("&");

  return queryString ? `${baseUrl}?${queryString}` : baseUrl;
}

export const API_V1 = "/api/v1";
