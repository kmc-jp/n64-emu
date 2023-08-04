# コントリビューションについて

このエミュレータに関するいろんな情報はScrapboxに載っています。
https://scrapbox.io/kmc-n64/

質問はKMC Slackのtamaronまでお願いします。
Slackチャンネル: #n64-emu

## コントリビューションの方法

Issueにも載っていますが、例えば以下の形で貢献してほしいです。

- エミュレーションの実装をすすめる
- テストの追加、整備
- デバッガの整備
- バグ検証
- 各プラットフォームの動作確認、ビルド環境整備

## フォーマッタ

clang-formatを使用してください


## コーディング規約

命名規則
- 普通の変数, 関数名, typedefした型の名前: snake_case, snake_case_t
- Class, Struct, Enum: PascalCase
- 定数, Enumのケース: UPPER_SNAKE_CASE

その他言及されていない箇所は変更するファイルに従ってください

## リファレンス

基本的な64のハードウェア固有の仕様はn64brewに載っています。
https://n64brew.dev/wiki/Main_Page

プロセッサの仕様は仕様書を参照してください。

また、実装は基本的に、Project64、n64、Kaizenを参考にしています
- https://github.com/project64/project64
- https://github.com/Dillonb/n64
- https://github.com/SimoneN64/Kaizen

Project64はおそらく最も正確です。できる限り、こちらを参照してください。
Kaizenはn64を参考にして作られたため、ほとんど同じです。(違いはCかC++か)
実装する際は、この二種類の実装を確認したのち、ソースコード中に該当箇所のパーマリンクを貼ってください。あとから確認しやすくするためです。
