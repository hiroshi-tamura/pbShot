# pbShot

LightShot 風のタスクトレイ常駐スクリーンショットツール（Windows, Qt 6 / C++17）。

## 機能

- タスクトレイ常駐、ホットキーで即起動（設定画面から変更可）
- 範囲選択キャプチャ、全画面即保存
- 範囲選択後のインライン注釈
  - ペン / 直線 / 矢印 / 四角 / マーカー / テキスト
  - 色選択、Undo
- 青枠のドラッグで矩形移動、角／辺ハンドルでリサイズ
- 保存ダイアログ経由で保存（PNG / JPEG / BMP）
- 画像そのものまたは「キャッシュ保存後のファイルパス」をクリップボードへコピー
- マルチモニタ対応（カーソルがあるモニタを対象）
- 設定ウィンドウ
  - 保存先フォルダ、キャッシュフォルダ
  - 既定フォーマット、画質
  - キャッシュ上限（MB）。超過時に古いものから自動削除
  - ホットキー（範囲選択 / 全画面）
  - 保存時のクリップボードコピー
  - Windows 起動時の自動起動
- 多重起動防止

## ビルド

前提: Qt 6.9.1 (mingw_64) / MinGW / CMake

```
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
```

ビルド後、`build/pbShot.exe` は windeployqt と MinGW ランタイム DLL が自動同梱され、そのまま単独起動できます。

## ホットキー（デフォルト）

- `Ctrl+Shift+A` : 範囲選択
- `Ctrl+Shift+S` : 全画面即保存
- `PrintScreen` / `Shift+PrintScreen` : 副次（Win11 の切り取りツールと衝突する場合あり）

## 範囲選択中

- `Enter` / `Ctrl+S` : 保存ダイアログ
- `Ctrl+C` : クリップボードコピー
- `ESC` : キャンセル
- テキストツール中: `Enter` 改行 / `Ctrl+Enter` 確定 / `ESC` キャンセル

## サードパーティライセンス

OCR 機能のために以下のサードパーティ成果物を同梱しています。各ライセンス全文は `LICENSES/` フォルダに収録。

- **PaddleOCR PP-OCRv4 モデル**（Baidu 著作） — Apache License 2.0
- **RapidOCR**（RapidAI） — Apache License 2.0
- **ONNX Runtime**（Microsoft） — MIT License
