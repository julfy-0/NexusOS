# NexusOS — 日本語

NexusOS は x86_64 向けの軽量な 64 ビット OS です。C 言語でゼロから開発されており、独自の UEFI ブートローダーと freestanding カーネルを使用します。

## 機能
- UEFI/GOP ブートと framebuffer コンソール
- x86_64 long mode
- GDT、IDT、PIC、PIT、例外処理
- PS/2 キーボードとマウス対応
- GUI 基盤とコマンドライン
- AHCI/FAT32 対応
- PCI と USB xHCI の基盤
- 起動モード選択: Graphic / Command Line
- Kernel Panic 画面、ログ、30 秒の再起動カウントダウン

## ビルド
```bash
make clean
make iso
make run
```
