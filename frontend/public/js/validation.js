// 纯函数校验，浏览器与 Node 测试均可复用。
// 返回 `{ok: boolean, message: string}`：ok=true 不显示错误，message 用作红色 hint。

export const USERNAME_RE = /^[A-Za-z0-9_]{3,20}$/;

export function validateUsernameInput(value) {
  const trimmed = (value == null ? '' : String(value)).trim();
  if (trimmed === '') {
    return { ok: true, message: '3-20 位字母、数字或下划线' };
  }
  if (trimmed.length < 3 || trimmed.length > 20) {
    return { ok: false, message: '用户名长度需在 3 到 20 个字符之间' };
  }
  if (!USERNAME_RE.test(trimmed)) {
    return { ok: false, message: '仅允许字母、数字与下划线' };
  }
  return { ok: true, message: '✓ 用户名可用' };
}

export function validatePasswordInput(value) {
  const v = value == null ? '' : String(value);
  if (v === '') {
    return { ok: true, message: '8-64 位，至少包含一个字母与一个数字' };
  }
  if (v.length < 8 || v.length > 64) {
    return { ok: false, message: '密码长度需在 8 到 64 个字符之间' };
  }
  let hasLetter = false;
  let hasDigit = false;
  for (const ch of v) {
    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')) {
      hasLetter = true;
    } else if (ch >= '0' && ch <= '9') {
      hasDigit = true;
    }
    if (hasLetter && hasDigit) break;
  }
  if (!hasLetter || !hasDigit) {
    return { ok: false, message: '密码必须同时包含字母与数字' };
  }
  return { ok: true, message: '✓ 密码强度符合要求' };
}

export function validateConfirmInput(value, password) {
  const v = value == null ? '' : String(value);
  const p = password == null ? '' : String(password);
  if (v === '') {
    return { ok: true, message: '再次输入以确认' };
  }
  if (v !== p) {
    return { ok: false, message: '两次密码不一致' };
  }
  return { ok: true, message: '✓ 密码一致' };
}
