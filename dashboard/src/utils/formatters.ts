export function formatNumber(value: number | null | undefined, digits = 1) {
  if (value === null || value === undefined || Number.isNaN(Number(value))) {
    return '--';
  }

  return Number(value).toFixed(digits);
}

export function formatEta(value: number | null | undefined) {
  return value === null || value === undefined ? '--' : `${formatNumber(value, 1)} min`;
}

export function movementClass(value = '') {
  return value.toLowerCase().replace(/\s+/g, '-').replace(/[^a-z-]/g, '');
}
