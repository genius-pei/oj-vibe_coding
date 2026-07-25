import {
  validateUsernameInput,
  validatePasswordInput,
  validateConfirmInput,
  USERNAME_RE
} from '../public/js/validation.js';

import { test } from 'node:test';
import assert from 'node:assert/strict';

test('USERNAME_RE accepts valid names', () => {
  assert.ok(USERNAME_RE.test('abc'));
  assert.ok(USERNAME_RE.test('alice'));
  assert.ok(USERNAME_RE.test('User_123'));
  assert.ok(USERNAME_RE.test('123'));
  assert.ok(USERNAME_RE.test('a'.repeat(20)));
});

test('USERNAME_RE rejects invalid names', () => {
  assert.ok(!USERNAME_RE.test('ab'));
  assert.ok(!USERNAME_RE.test('a'.repeat(21)));
  assert.ok(!USERNAME_RE.test('hello-world'));
  assert.ok(!USERNAME_RE.test('hello world'));
  assert.ok(!USERNAME_RE.test('用户名'));
  assert.ok(!USERNAME_RE.test('a@b'));
});

test('validateUsernameInput: empty shows neutral hint', () => {
  const r = validateUsernameInput('');
  assert.equal(r.ok, true);
  assert.match(r.message, /3-20/);
});

test('validateUsernameInput: trims whitespace before validation', () => {
  const r = validateUsernameInput('  alice  ');
  assert.equal(r.ok, true);
  assert.match(r.message, /✓/);
});

test('validateUsernameInput: too short is invalid', () => {
  const r = validateUsernameInput('ab');
  assert.equal(r.ok, false);
  assert.match(r.message, /长度/);
});

test('validateUsernameInput: too long is invalid', () => {
  const r = validateUsernameInput('a'.repeat(21));
  assert.equal(r.ok, false);
  assert.match(r.message, /长度/);
});

test('validateUsernameInput: invalid character is rejected even when length ok', () => {
  const r = validateUsernameInput('hello-world');
  assert.equal(r.ok, false);
  assert.match(r.message, /仅允许/);
});

test('validateUsernameInput: CJK rejected', () => {
  assert.equal(validateUsernameInput('用户名').ok, false);
});

test('validatePasswordInput: empty shows neutral hint', () => {
  const r = validatePasswordInput('');
  assert.equal(r.ok, true);
  assert.match(r.message, /8-64/);
});

test('validatePasswordInput: too short invalid', () => {
  const r = validatePasswordInput('abc12');
  assert.equal(r.ok, false);
  assert.match(r.message, /长度/);
});

test('validatePasswordInput: too long invalid', () => {
  const r = validatePasswordInput('a'.repeat(65));
  assert.equal(r.ok, false);
  assert.match(r.message, /长度/);
});

test('validatePasswordInput: exactly 64 valid', () => {
  const ok = validatePasswordInput('a'.repeat(62) + '1b');
  assert.equal(ok.ok, true);
});

test('validatePasswordInput: digits only invalid', () => {
  const r = validatePasswordInput('12345678');
  assert.equal(r.ok, false);
  assert.match(r.message, /同时包含/);
});

test('validatePasswordInput: letters only invalid', () => {
  const r = validatePasswordInput('abcdefgh');
  assert.equal(r.ok, false);
  assert.match(r.message, /同时包含/);
});

test('validatePasswordInput: mixed alnum ok', () => {
  const r = validatePasswordInput('P4ssword!');
  assert.equal(r.ok, true);
  assert.match(r.message, /✓/);
});

test('validateConfirmInput: empty neutral', () => {
  const r = validateConfirmInput('', 'p4ssword');
  assert.equal(r.ok, true);
});

test('validateConfirmInput: matches password ok', () => {
  const r = validateConfirmInput('p4ssword', 'p4ssword');
  assert.equal(r.ok, true);
});

test('validateConfirmInput: differs from password invalid', () => {
  const r = validateConfirmInput('p4ssworD', 'p4ssword');
  assert.equal(r.ok, false);
  assert.match(r.message, /一致/);
});

test('validateConfirmInput: trailing whitespace considered mismatch', () => {
  const r = validateConfirmInput('p4ssword ', 'p4ssword');
  assert.equal(r.ok, false);
});
