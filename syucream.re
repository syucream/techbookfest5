= Microservices frendly L7 proxy : Envoy 

== はじめに

こんにちは。 @syu_cream です。普段は Web サービスを陰から支えるインフラ周辺の構築をしています。

いやー、よく分からないけどマイクロサービス流行っていますね！
Netflix などの大型サービスの事例を鑑みて、俺も俺もと足を踏み出す方々も結構多いのではないかと見受けられます。
かくいう筆者も業務上マイクロサービスを意識したアーキテクチャと組織の構築に、まさに揉まれている最中でございます。

本記事ではそのマイクロサービスの文脈でよく出てくる技術要素の一つであるサービスメッシュに関わる部分と、 Envoy というリバースプロキシについて触れていきます。

本記事が読者の皆様にとっての、今後のマイクロサービス化の奔流を乗り越える道標のひとつとして機能することを願っています。


== マイクロサービス概要

まずは Envoy の話の前に、簡単に必要最低限のマイクロサービスアーキテクチャに関する説明をします。

マイクロサービスアーキテクチャは Netflix などの巨大なサービス群を支えるアーキテクチャの思想のひとつです。
具体的にはあるサービスの機能や責任を細分化し、小規模で自律するマイクロサービスを協調動作させて構築します。
各マイクロサービスは疎結合ゆえ他機能に非依存な技術選択を行うことができ、たとえばそのマイクロサービスが担う責務を担うのにもっとも適切なプログラミング言語やデータベースを選択できます。
また特定のマイクロサービスのみスケールさせて少数のリソースで提供するサービス全体を安定運用したり、サーキットブレイカーのような仕組みで動作しないマイクロサービスの機能だけ提供を諦めつつ全体のサービスとしては機能を維持する選択もできます。
加えてマイクロサービスの境界を適切に線引きすることで、各々のマイクロサービスの開発・デプロイ・運用を個別のチームで実施することができ、ビジネスをより早く進めることができるかも知れません。

これらの話は理想的であり、マイクロサービスアーキテクチャに舵切りすることでビジネスを成長させてきた企業の成功体験を聞くとそれに従いたくなるかも知れません。
しかしながらマイクロサービスアーキテクチャは、モノリスとして構築するのとは別にマイクロサービス間のメッセージングやサーキットブレイカーなどの共通機能の提供方法、構成が複雑化することによるデバッグや運用の難易度の向上、組織編成になじむかの問題など様々な障壁があると思われます。
筆者としてもマイクロサービスアーキテクチャは馴染まないケースが多々存在し、これに従うにしても組織としてある程度覚悟をしてリスクを承知の上で取り掛かる必要があると考えています。


マイクロサービスアーキテクチャに関して深く知るための資料のひとつとして @<href>{https://martinfowler.com/articles/microservices.html, Microservices - Martin Fowler} などの記事が参考になると思われます。


== マイクロサービスを支える Envoy と Istio

サービスメッシュやサイドカーパターンによるプロキシの注入について書く。
それを書いたあと、具体的なそれの実現方法である Envoy と Istio について概要説明する

マイクロサービスアーキテクチャに従って実世界でサービスを構築していくと、前述したものやそれ以外の複雑な問題に遭遇することかと思います。
その中でも本記事のテーマである Envoy は、よく発生する課題の共通部分を強くサポートするソフトウェアです。

=== Envoy Proxy とは

@<href>{https://www.envoyproxy.io/, Envoy Proxy} はライドシェアサービスを提供する Lyft によって作られた、マイクロサービスの世界の課題を解決するためのプロキシです。
Envoy は @<href>{https://www.cncf.io/, Cloud Native Computing Foundation} 参加のソフトウェアでもあります。

Envoy は C++11 で実装されており、ハイパフォーマンスでマイクロサービスの世界におけるネットワーキングとオブサーバビリティの問題を解決することを目指しています。
具体的に Envoy が持つ機能・特徴は以下のようになります。

- 省メモリで高性能
- HTTP/2 と gRPC のサポート (もちろん HTTP/1.1 もサポートしている)
- 自動リトライ
- サーキットブレイカー
- 複数種類のロードバランシング
- 動的に設定変更を可能にする API の提供
- 分散トレーシングや特定データベースに特化したオブザーバビリティの提供

この中でも動的に設定変更可能にしている API の存在はユニークであると思われます。
Envoy は複数種類からなる xDS(x Discovery Service) API をサポートしており、外部システムにプロキシとしての振る舞いの設定管理を委ねることができます。
こうした柔軟な設定変更を可能にしているからこそ、マイクロサービス間の通信においてデータのやり取りを担う Data Plane として Envoy を使い、データのやり取りの仕方の管理を担う Control Plane をそれ以外(例えば後述の Istio )に任せる構成が取れます。

また Envoy はコンテナ上で動作させることを想定して開発されており、よくあるパターンとして @<href>{https://kubernetes.io/, Kubernetes} のアプリケーションコンテナと同じ Pod で動作させる Sidecar コンテナとして使われることがあります。
Envoy の担う機能はアプリケーションとは別コンテナで動作し、通信は gRPC などで行うことで特定のプログラミング言語に依存することなくマイクロサービスの構成に柔軟性を与えることができます。

=== Istio とは

@<href>{https://istio.io/, Istio} は Envoy で Data Plane を提供しつつ Control Plane も別途提供することで、マイクロサービス間のコネクションを変更・制御したり認証認可や暗号化によるセキュリティ担保を行うソフトウェアです。
現状だとターゲットとするインフラとして Kubernetes を前提にしています。

ほげほげ〜〜


== Envoy 詳解

ここからはより Envoy の詳細について提供する機能と使用している技術の側面から掘り下げていこうと思います。

=== Envoy アーキテクチャ概要

Envoy は先述の通りハイパフォーマンスであることを目指しており、モダンなプロキシが取るようなマルチスレッド・イベントドリブンな I/O 処理を実装しています。
スレッドの実装としては実は pthread を利用していて、 C++11 の std::thread を使わずに実装されていたりします。
スレッドには幾つかの役割分担があり、単一の master スレッドと複数の workers スレッドが存在します。

（いい感じの図を描く）

またイベント処理に関しては @<href>{https://libevent.org/, libevent} を使っているようです。
Envoy では思想として 100 ％ノンブロッキングをうたっており、イベントドリブンな処理は非常に意識して実装されています。

（いい感じの図を描く）


その他のほげほげ〜〜

TODO
- スレッドローカルストレージ

=== Envoy の特徴的な機能説明

Envoy には先に挙げたようなユニークな機能がいくつか存在します。
ここでは xDS API と分散トレーシングに関して深掘りしてみようと思います。


ほげほげ〜〜
（サーキットブレイカーも気になる？）

=== nginx など従来のプロキシと何が違うのかについて

Envoy の技術的側面について考えていくと、 nginx などモダンでハイパフォーマンスなプロキシと何が違うのかと疑問を抱く方が現れるかと思います。
Envoy の公式ページでは、 Envoy は各アプリケーションとセットで動作し、オブザーバビリティなどマイクロサービスにおける問題を解くのに注力しているような記述が見受けられます。
筆者の個人の意見としては、 Envoy は nginx など既存のプロキシと比べて機能の充実具合でいうとまだ劣るものの、逆に機能が少ないゆえにターゲットとする課題を解くのに最低限のことができるものと思われます。
また Envoy では xDS API の提供により最初から動的に設定を変更していくことを強くサポートしている点も、大きな差分になるとも考えられます。

現状だと Istio との統合も考えると Envoy の今後が期待できるところですが、最近は nginx で gRPC のサポートが入ったり引き続き活発な開発が続いていくと、解決したい問題によって使用するプロキシを選択してサービスメッシュを構築できる未来が訪れるのかも知れないと期待しています。


== Envoy の試し方

=== Envoy を軽く動かす方法

そんなのある？
Docker image 使ったほうが早そう


=== Docker image 使ったり

MUST でやる
公式ドキュメントに記述がある

=== GCP 上で動かしてみたり（できる？

できたらやる、無理はしない


== まとめ

マイクロサービスはヤバイ変更だから覚悟を持ってやりましょう（仮）
たぶん Envoy はすごいアーキテクチャ。素敵。 nginx のファンやめます（仮）
