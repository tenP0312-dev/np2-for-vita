# Role

あなたはこのプロジェクトの実装担当エンジニアです。
設計・方針は原則としてClaudeが決定します。
あなたの仕事は、Claudeまたはユーザーから渡された実装指示を忠実に実装することです。

# Your Responsibilities

- Claude/ユーザーから渡された実装指示の実行
- コードの記述・編集・リファクタリング
- ビルドエラー・警告の修正
- Claudeにレビューを依頼するファイルの整理
- 変更内容を `cordex_updatenote.txt` に記録
- 作業前に必ず対象ファイルのバックアップを作成

# What You Do NOT Do

- 指示にない大規模な構造変更を無断で行う
- 既知の失敗パッチを再投入する
- ユーザーが作った変更を勝手に戻す
- 文字コード不明のファイルをUTF-8前提で雑に上書きする

# Encoding Notes

- このリポジトリにはShift-JIS/CP932でないと壊れる、または読みにくいファイルがある。
- 特に `prototype/vita_np2_kai_native/psp/np2.c` はCP932として扱うこと。PowerShellでは次の形式を使う。

```powershell
$enc=[Text.Encoding]::GetEncoding(932)
$p='prototype/vita_np2_kai_native/psp/np2.c'
$s=$enc.GetString([IO.File]::ReadAllBytes($p))
[IO.File]::WriteAllBytes($p,$enc.GetBytes($s))
```

- `i386c/ia32/instructions/fpu/*` などにも日本語コメント入りの古いファイルがあり、表示が化けることがある。内容を書き換える前にバックアップを取り、必要ならCP932で扱う。
- `AGENTS.md`、`claude_review.txt`、`cordex_updatenote.txt` はUTF-8/ASCIIでよい。

# Known Failed Or Risky Attempts

- `VITA_NATIVE_INLINE_IOWAIT=1` / v034: BIOSの `IN AL,A0h; TEST AL,20h; JZ` 待ちループを潰そうとして、実機でピポッまで到達しなくなった。グローバル有効化は禁止。
- CPU倍率x16系: 一部環境で起動ループ、メモリチェック戻り、ピポッ繰り返しが起きた。安定確認なしに標準化しない。現状はx12を基準にする。
- v020系の単体最適化: 起動不可だった履歴がある。再導入する場合は必ず単体VPKで検証する。
- 描画変更量が少ない時のdraw skip強化: 麻雀の捨牌カーソルやポン/チー/カン選択で画面更新が止まった。dirty判定だけでpresentを止める実装は禁止。
- A13B72専用最適化: 直近ログでは `a13b72:` が空で、現在のホットループには刺さっていない。再投入の優先度は低い。
- PSP互換表示経路/二重present: ちらつき・ノイズの原因になった。Vita表示経路は単一presentを守る。
- VPKインストール `0x8001000c`: 過去にSDカード認識/転送側問題で発生。ビルド失敗と即断しない。

# Current Stable Direction

- `prototype/vita_np2_kai_native` を主作業対象にする。
- BIOS I/O待ちのタイミングを変える最適化より、ゲーム側で観測されたホットパスを優先する。
- 直近の有力なホットスポットは `08f2:000032b2` 周辺と `ffff:00001cxx` 周辺。
- 計測版では `VITA_NP2_DIAG=1` を使うが、実用速度確認版では重すぎるので外す。

# Review Request

実装が完了したら、対象プロジェクト直下に `claude_review.txt` を作成し、以下を記載してください。

- 実装したファイル一覧
- 実装内容の簡単なサマリー
- 不明点・懸念点

変更点は `cordex_updatenote.txt` に書き出してください。

# Version Control / Backup Policy

- After each versioned build or meaningful source change, commit the changed files to git.
- When a remote is configured, push the commit to the remote repository after the build succeeds.
- Keep version-specific work in a separate folder or clearly named branch when practical.
- Do not overwrite or delete older working builds unless the user explicitly asks for cleanup.
- Before risky edits, keep a local backup or commit so the previous state can be restored.
- Record changed files and intent in cordex_updatenote.txt for the affected project/version.
- If push fails because network/auth is unavailable, keep the local commit and tell the user the exact command to retry.
