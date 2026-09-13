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

/** ダイアログ側からの返答。code=入力された6桁 / retry=期限切れで再申請 / cancel=中断 */
export type CodeAnswer =
  | { kind: 'code'; code: string }
  | { kind: 'retry' }
  | { kind: 'cancel' };

/**
 * コードを人に尋ねる処理。error は前回の失敗理由（初回は null）。
 * 入力UI（PairingDialog）を開き、submit/cancel/retry で resolve する。
 */
export type CodePrompt = (secondsValid: number, error: string | null) => Promise<CodeAnswer>;

export type PairFailure = 'start_failed' | 'cancelled' | 'invalid_code' | 'network';
export type PairResult = { ok: true } | { ok: false; reason: PairFailure };

const PAIR_TIMEOUT_MS = 5000;
const DEFAULT_WINDOW_SEC = 120;
const INVALID_CODE_MESSAGE = 'コードが違います。画面のコードをもう一度確認してください。';

/** POST /api/pair。デバイス画面にコードを出し、有効秒数を返す。 */
async function startPairing(address: string): Promise<number | null> {
  const start = await fetch(`http://${address}/api/pair`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    signal: AbortSignal.timeout(PAIR_TIMEOUT_MS),
  });
  if (!start.ok && start.status !== 202) return null;
  const info = await start.json().catch(() => ({}));
  return Number(info?.expires_in ?? DEFAULT_WINDOW_SEC);
}

/** POST /api/pair/confirm。合っていればトークン文字列、違えば null。 */
async function confirmPairing(address: string, code: string): Promise<string | null> {
  const confirm = await fetch(`http://${address}/api/pair/confirm`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ code: code.trim() }),
    signal: AbortSignal.timeout(PAIR_TIMEOUT_MS),
  });
  if (!confirm.ok) return null;
  const data = await confirm.json().catch(() => ({}));
  return typeof data?.token === 'string' && data.token ? data.token : null;
}

/**
 * ペアリングを実行する。askForCode にはデバイス画面のコードを人に尋ねる処理を渡す。
 * - コードが違えば同じ申請のまま再入力を求める（error 付きで askForCode を再呼び出し）
 * - 期限切れ（retry）のときだけ /api/pair を申請し直す
 * 成功したらトークンを保存して {ok:true} を返す。
 */
export async function pairWithDevice(
  address: string,
  askForCode: CodePrompt,
): Promise<PairResult> {
  try {
    let secondsValid = await startPairing(address);
    if (secondsValid === null) return { ok: false, reason: 'start_failed' };

    let error: string | null = null;
    for (;;) {
      const answer = await askForCode(secondsValid, error);
      if (answer.kind === 'cancel') return { ok: false, reason: 'cancelled' };
      if (answer.kind === 'retry') {
        secondsValid = await startPairing(address);
        if (secondsValid === null) return { ok: false, reason: 'start_failed' };
        error = null;
        continue;
      }

      const token = await confirmPairing(address, answer.code);
      if (token) {
        setDeviceToken(token);
        return { ok: true };
      }
      error = INVALID_CODE_MESSAGE;
    }
  } catch {
    return { ok: false, reason: 'network' };
  }
}
