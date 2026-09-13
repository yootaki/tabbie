// デバイス側の本人承認（ペアリング）に対応するための小さなヘルパ。
//
// ファームウェアは状態を変えるエンドポイント(animation/servo/reset/debug)に
// `X-Tabbie-Token` を要求する。未登録だと 403 {"error":"unpaired"} が返る。
//
// ペアリングの流れ:
//   1. POST /api/pair          → デバイスの画面に6桁コードが出る（2分有効）
//   2. そのコードを人が読む     → 物理的にデバイスの前にいる人だけが読める
//   3. POST /api/pair/confirm  → トークンが発行される
//
// トークンはこのブラウザの localStorage にだけ置く。

const TOKEN_KEY = 'tabbie_device_token';

export function getDeviceToken(): string {
  try {
    return localStorage.getItem(TOKEN_KEY) ?? '';
  } catch {
    return '';
  }
}

export function setDeviceToken(token: string): void {
  try {
    localStorage.setItem(TOKEN_KEY, token);
  } catch {
    /* プライベートウィンドウ等では保存できない。動作は継続する */
  }
}

export function clearDeviceToken(): void {
  try {
    localStorage.removeItem(TOKEN_KEY);
  } catch {
    /* noop */
  }
}

/** 書き込み系リクエスト用のヘッダ。トークンが無ければ付けない。 */
export function authHeaders(): Record<string, string> {
  const token = getDeviceToken();
  return token
    ? { 'Content-Type': 'application/json', 'X-Tabbie-Token': token }
    : { 'Content-Type': 'application/json' };
}

/** 403 が「未ペアリング」由来かどうか。 */
export async function isPairingError(response: Response): Promise<boolean> {
  if (response.status !== 403) return false;
  try {
    const body = await response.clone().json();
    return body?.error === 'unpaired' || body?.error === 'forbidden';
  } catch {
    return false;
  }
}

export type CodePrompt = (secondsValid: number) => Promise<string | null>;

/**
 * ペアリングを実行する。askForCode にはデバイス画面のコードを人に尋ねる処理を渡す。
 * 成功したらトークンを保存して true を返す。
 */
export async function pairWithDevice(
  address: string,
  askForCode: CodePrompt,
): Promise<boolean> {
  const start = await fetch(`http://${address}/api/pair`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    signal: AbortSignal.timeout(5000),
  });
  if (!start.ok && start.status !== 202) return false;

  const info = await start.json().catch(() => ({}));
  const code = await askForCode(Number(info?.expires_in ?? 120));
  if (!code) return false;

  const confirm = await fetch(`http://${address}/api/pair/confirm`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ code: code.trim() }),
    signal: AbortSignal.timeout(5000),
  });
  if (!confirm.ok) return false;

  const data = await confirm.json().catch(() => ({}));
  if (!data?.token) return false;

  setDeviceToken(data.token);
  return true;
}
