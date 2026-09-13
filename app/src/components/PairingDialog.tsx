import React, { useEffect, useState } from 'react';
import {
  Dialog,
  DialogContent,
  DialogDescription,
  DialogHeader,
  DialogTitle,
} from '@/components/ui/dialog';
import { Input } from '@/components/ui/input';
import { Button } from '@/components/ui/button';

// デバイス画面に出る6桁コードを入力してもらうダイアログ。
// 状態は持たず、submit/cancel/retry を親（TabbieContext）に返すだけ。

const CODE_LENGTH = 6;

interface PairingDialogProps {
  open: boolean;
  /** コードの有効残り秒数（開いた時点の値。カウントダウンはこの中で行う） */
  secondsValid: number;
  error: string | null;
  /** 確認リクエスト送信中 */
  busy: boolean;
  theme?: 'clean' | 'retro';
  onSubmit: (code: string) => void;
  onCancel: () => void;
  /** 期限切れ後の「もう一度」 */
  onRetry: () => void;
}

const retroCard = 'border-2 border-black rounded-2xl shadow-[4px_4px_0_0_rgba(0,0,0,0.3)]';

export const PairingDialog: React.FC<PairingDialogProps> = ({
  open,
  secondsValid,
  error,
  busy,
  theme = 'clean',
  onSubmit,
  onCancel,
  onRetry,
}) => {
  const [code, setCode] = useState('');
  const [remaining, setRemaining] = useState(secondsValid);

  // 開くたび・再申請のたびに入力とカウントダウンをリセットする
  useEffect(() => {
    if (!open) return;
    setCode('');
    setRemaining(secondsValid);
  }, [open, secondsValid]);

  useEffect(() => {
    if (!open || remaining <= 0) return;
    const timer = setInterval(() => setRemaining((s) => Math.max(0, s - 1)), 1000);
    return () => clearInterval(timer);
  }, [open, remaining]);

  const expired = remaining <= 0;
  const canSubmit = code.length === CODE_LENGTH && !busy && !expired;

  const handleChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    setCode(e.target.value.replace(/\D/g, '').slice(0, CODE_LENGTH));
  };

  const handleKeyDown = (e: React.KeyboardEvent<HTMLInputElement>) => {
    if (e.key === 'Enter' && canSubmit) onSubmit(code);
  };

  return (
    <Dialog open={open} onOpenChange={(next) => { if (!next && !busy) onCancel(); }}>
      <DialogContent className={theme === 'retro' ? `sm:max-w-md ${retroCard}` : 'sm:max-w-md'}>
        <DialogHeader>
          <DialogTitle>Pair this browser with Tabbie</DialogTitle>
          <DialogDescription>
            Tabbie の画面に6桁のコードが出ています。物理的にデバイスの前にいる人だけが読めます。
          </DialogDescription>
        </DialogHeader>

        <div className="space-y-3">
          <Input
            autoFocus
            inputMode="numeric"
            pattern="[0-9]*"
            maxLength={CODE_LENGTH}
            placeholder="000000"
            value={code}
            onChange={handleChange}
            onKeyDown={handleKeyDown}
            disabled={busy || expired}
            className="text-center text-2xl tracking-[0.5em] font-mono"
          />

          <p className={`text-sm ${expired ? 'text-red-600 dark:text-red-400' : 'text-muted-foreground'}`}>
            {expired
              ? 'コードの期限が切れました。もう一度申請してください。'
              : `残り ${remaining} 秒`}
          </p>

          {error && (
            <p className="text-sm text-red-600 dark:text-red-400">{error}</p>
          )}
        </div>

        <div className="flex justify-end gap-2">
          <Button variant="ghost" onClick={onCancel} disabled={busy}>
            Cancel
          </Button>
          {expired ? (
            <Button onClick={onRetry} disabled={busy}>
              もう一度
            </Button>
          ) : (
            <Button onClick={() => onSubmit(code)} disabled={!canSubmit}>
              {busy ? 'Confirming...' : 'Pair'}
            </Button>
          )}
        </div>
      </DialogContent>
    </Dialog>
  );
};

export default PairingDialog;
