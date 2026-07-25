import { apiPost } from './api.js';
import {
  validateUsernameInput,
  validatePasswordInput,
  validateConfirmInput
} from './validation.js';

const form = document.getElementById('register-form');
const usernameInput = document.getElementById('reg-username');
const passwordInput = document.getElementById('reg-password');
const confirmInput = document.getElementById('reg-confirm');
const hintUsername = document.getElementById('hint-username');
const hintPassword = document.getElementById('hint-password');
const hintConfirm = document.getElementById('hint-confirm');
const submitBtn = document.getElementById('submit-btn');
const errorEl = document.getElementById('form-error');

function setHint(el, message, invalid) {
  el.textContent = message;
  el.classList.toggle('invalid', Boolean(invalid));
}

function setInvalid(input, invalid) {
  input.classList.toggle('invalid', Boolean(invalid));
}

function applyHint(el, result, input) {
  setHint(el, result.message, !result.ok);
  setInvalid(input, !result.ok);
  return result.ok;
}

function validateUsername() {
  return applyHint(hintUsername, validateUsernameInput(usernameInput.value), usernameInput);
}

function validatePassword() {
  return applyHint(hintPassword, validatePasswordInput(passwordInput.value), passwordInput);
}

function validateConfirm() {
  return applyHint(
    hintConfirm,
    validateConfirmInput(confirmInput.value, passwordInput.value),
    confirmInput
  );
}

function refreshSubmitState() {
  const ok = validateUsername() && validatePassword() && validateConfirm();
  submitBtn.disabled = !ok;
  errorEl.textContent = '';
}

usernameInput.addEventListener('input', () => {
  validateUsername();
  validateConfirm();
  refreshSubmitState();
});

passwordInput.addEventListener('input', () => {
  validatePassword();
  validateConfirm();
  refreshSubmitState();
});

confirmInput.addEventListener('input', () => {
  validateConfirm();
  refreshSubmitState();
});

form.addEventListener('submit', async (event) => {
  event.preventDefault();
  if (submitBtn.disabled) {
    return;
  }
  errorEl.textContent = '';
  submitBtn.disabled = true;
  submitBtn.textContent = '注册中…';

  try {
    await apiPost('/api/auth/register', {
      username: usernameInput.value.trim(),
      password: passwordInput.value
    });
    window.location.href = '/';
  } catch (error) {
    if (error && error.status === 409) {
      setHint(hintUsername, '该用户名已被占用', true);
      setInvalid(usernameInput, true);
      errorEl.textContent = '该用户名已被占用';
    } else if (error && error.status === 400) {
      errorEl.textContent = error.message || '注册信息不合法';
    } else {
      errorEl.textContent = `注册失败：${error && error.message ? error.message : '网络异常'}`;
    }
    submitBtn.disabled = false;
    submitBtn.textContent = '注 册';
    usernameInput.focus();
  }
});

refreshSubmitState();
