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

Istio では Envoy を拡張一部拡張して Data Plane を実現するのに使います。
Envoy は Kubernetes でデプロイする Pod 全てに Sidecar として導入され、マイクロサービス間の通信時に Sidecar の Envoy 同士が通信処理を仲介するような動作になります。
マイクロサービスの世界では Pod の生き死にが頻繁に起こり、 Envoy のルーティングの設定を動的に更新できなければなりません。
Istio ではこの設定変更を実現するために Pilot というコンポーネントを持ち、 Envoy に対するルーティングルールを設定して Envoy に伝えるようにしています。

本記事では Envoy に主眼を起きたいため Istio については深く触れません。
詳しく知りたい方は先述の公式ページのリンクを辿ったり、実際の導入事例などを探してみることをおすすめいたします。


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

=== Envoy のリソース抽象化

Envoy ではネットワーク通信やプロキシ処理における様々なリソースを抽象化しています。
全体像は以下の図の通りになります。

（いい感じの図）

ここでは主要な、抽象化されたリソースを解説していきます。

==== Listener

Envoy がクライアント、 Envoy の用語としては downstream から受け付けるコネクションを受け付けるネットワークロケーションです。
現在は TCP listener のみサポートしているようです。
Envoy では複数の Listener に対応しており、この Listener に対して後述の Filter を設定して通信制御や Cluster への転送を行います。

==== Listener Filter

Envoy の Listener に対応してコネクションのメタデータを修正したりするのに使われる Filter です。
主に他のシステムとの連携に使用するのに必要なメタデータを付与したりするのに使う想定のようです。

==== Network(L3/L4) Filter

L3, L4 レベルの生データを触れて制御することができる Filter です。
そして Envoy においてプロキシの制御のコアの部分はこの Network Filter として実装されているといえます。

HTTP リクエストに関してフィルタやルーティングなどを行う、恐らく Envoy を利用する上でお世話になることが多々ある HTTP connection manager もこの Network Filter の一種です。
その他にも TCP Proxy 機能の提供やレートリミットも Network Filter の一種として提供されます。

==== HTTP connection manager 

HTTP connection manager は Network Filter の一種であり生データを処理して HTTP として解釈した上で様々な機能を提供します。
Envoy は HTTP に関しては HTTP/2, HTTP/1.1 はもちろんのこと WebSocket もサポートします。(ちなみに公式ドキュメントでは SPDY のサポートはしていない旨の明記がされています。このご時世ならこのサポートは不要でしょうが)
HTTP connection manager がサポートする機能としては以下の通りです。

- HTTP Filter のサポート
- ルーティング
- アクセスログの記録
- トレーシングのためのリクエスト ID 発行
- リクエスト・レスポンスヘッダの修正

HTTP Filter というのは Network Filter の HTTP 版であるようなイメージを浮かべていただけるといいと思います。
HTTP Filter として標準でサポートされている機能も多々あり、バッファリングや gzip 圧縮など nginx などの他のプロキシ実装でも広く存在するものや、 gRPC-HTTP/1.1 bridge など gRPC のサポートを厚くしている Envoy の特色が出ているものなど多岐にわたります。
また HTTP Filter では Lua スクリプトによる機能拡張もサポートされています。

ルーティングは HTTP リクエストに対して適切な upstream Cluster を決定してリクエストを転送する機能を提供します。
それの伴いバーチャルホストの提供やホストの書き換え、リトライ、優先度の解釈などの機能も提供します。

==== Cluster

Envoy におけるプロキシ先の upstream ホストをグループ化したものです。
upstream ホストはヘルスチェックされ生死判定をされ、 Envoy が実際に転送処理を行う際は生きている upstream ホストに対して、ロードバランシングポリシーを加味して転送先を決定することになります。

ちなみに Envoy が転送処理を行う際に upstream Cluster を探す必要があるのですが、これを service discovery と呼んでいます。

=== Envoy の特徴的な機能説明

Envoy には先に挙げたようなユニークな機能がいくつか存在します。
ここでは xDS API と分散トレーシングに関して深掘りしてみようと思います。

==== xDS API

少しだけ先述しましたが、 Envoy は xDS API という多種にわたる動的設定変更のための API をサポートしています。
具体的には以下の API がサポートされています。(Envoy の API には v1 と v2 の 2 バージョンがあるのですが、ここでは v2 のみ触れます)

- LDS(Listener Discovery Service)
- RDS(Route Discovery Service)
- CDS(Cluster Discovery Service)
- EDS(Endpoint Discovery Service)

EDS(Endpoint Discovery Service) は xDS API の中でもよく使われる類のものかも知れません。 Cluster のメンバーとなる upstream ホストを検出します。この API は v1 では SDS(Service Discovery Service) という名前だったようです。
CDS(Cluster Discovery Service) は upstream Cluster を探す際に使われます。
RDS(Route Discovery Service) は HTTP connection manager に関連する xDS API であり、ルーティングの設定を変更することができます。
LDS(Listener Discovery Service) は〜ほげほげ

xDS API の定義は @<href>{https://developers.google.com/protocol-buffers/, Protocol Buffer} によって @<href>{https://github.com/envoyproxy/data-plane-api/blob/master/XDS_PROTOCOL.md, 定義されて} います。
xDS API にアクセスすることを考える際は、この定義をよく見るのが良さそうです。

==== トレーシング

Envoy が提供する大きな機能であり、マイクロサービスの世界で問題になることとしてオブザーバビリティが挙げられます。
例えばこれが 1 つのモノリスで構築されたシステムであれば開発時にデバッグをしたければ従来のデバッガを使ったり、静的解析してコールグラフを出したりスタックトレースを出力してみたりすることができます。
しかし境界を明確にして自立分散して動作するマイクロサービスの世界では、従来は出来ていたこれらも対応するのが困難になります。
代表的な問題として、多量のマイクロサービスが存在してそれらを連携する際に、あるリクエストがどういう経路を辿って関連しているのか紐づけが難しいことが挙げられます。

Envoy ではこの関連付けを行ってオブザーバビリティを向上するための幾つかの仕組みが提供されています。
ちなみにこれらの仕組みは Envoy では HTTP connection manager によって提供されます。

- リクエスト ID の生成
- 外部トレーシングサービスとの連携
- クライアントトレース ID の結合

リクエスト ID は実態は UUID です。 Envoy ではこの生成した UUID を x-request-id HTTP ヘッダに付与してくれます。
このリクエスト ID をログに記録しておくことで、後で複数のマイクロサービスのログを x-request-id で突き合わせてリクエストのフローを確認することができます。

TODO 詳細を書くと結構面倒臭そう

==== サーキットブレイカー

==== ロードバランサ

=== nginx など従来のプロキシと何が違うのかについて

Envoy の技術的側面について考えていくと、 nginx などモダンでハイパフォーマンスなプロキシと何が違うのかと疑問を抱く方が現れるかと思います。
Envoy の公式ページでは、 Envoy は各アプリケーションとセットで動作し、オブザーバビリティなどマイクロサービスにおける問題を解くのに注力しているような記述が見受けられます。
また nginx を比較の対象とするなら、 Envoy はそれに比べて以下のような長所が存在します。

- downstream だけでなく upstream への通信も HTTP/2 に対応している
- nginx plus でサポートされるようなロードバランサの仕組みを Envoy では標準で搭載している

筆者の個人の意見としては、 Envoy は nginx など既存のプロキシと比べて機能の充実具合や拡張性でいうとまだ劣ると考えています。
しかし逆に機能が少ないゆえにターゲットとする課題を解くのに最低限のことができるものと思われます。
加えて Envoy では xDS API の提供により最初から動的に設定を変更していくことをサポートしている点も、大きな差分になるとも考えられます。

現状だと Istio との統合も考えると Envoy の今後が期待できるところですが、最近は nginx で gRPC のサポートが入ったり引き続き活発な開発が続いていくと、解決したい問題によって使用するプロキシを選択してサービスメッシュを構築できる未来が訪れるのかも知れないと期待しています。


== Envoy の試し方

ここまで解説してきた Envoy ですが、まずは実際の動作や設定値を確認してみた方がイメージもつかみやすいでしょう。
ここでは公式で提供されている Docker image を使って手軽に動作を確認しつつ、設定項目を追ってみようと思います。

=== Docker image 使ったり

TODO Envoy はマイクロサービス前提以外でも動かせるような説明をする

Envoy 公式提供の Docker image ですが、以下のように普通に docker pull して使用することができます。
この image では 10000 番ポートでプロキシリクエストの、 9901 番ポートで管理用リクエストの受け付けをしており、動作時にポートマッピングをすることで気軽に動作確認することができます。

//cmd{
$ docker pull envoyproxy/envoy
$ docker run -it -p 10000:10000 -p 9901:9901 envoyproxy/envoy
//}

（いい感じの図）

動作している Envoy に対してリクエストを発行すると、このイメージにおけるデフォルトの upstream である google.com にプロキシされ、無事にレスポンスが得られる事が確認できます。
また server ヘッダが envoy となっており、 Envoy からレスポンスが返ってきたであろうことも確認できます。

//cmd{
$ curl -I http://localhost:10000/
HTTP/1.1 200 OK
date: Mon, 17 Sep 2018 06:38:02 GMT
expires: -1
cache-control: private, max-age=0
content-type: text/html; charset=ISO-8859-1
p3p: CP="This is not a P3P policy! See g.co/p3phelp for more info."
server: envoy
x-xss-protection: 1; mode=block
...
//}

この Docker image に含まれるデフォルトの設定ファイルは Envoy の GitHub のリポジトリの configs/google_com_proxy.v2.yaml の内容になります。
ここでは一部エッセンスを抜粋して上記で発生した動作を追ってみます。

//source[/etc/envoy/envoy.yaml]{
...
static_resources:
  listeners:
  - name: listener_0
    address:
      sockert_address:
        protocol: TCP
        address: 0.0.0.0
        port_value: 10000
    filter_chains:
    - filters:
      - name: envoy.http_connection_manager
        config:
          stat_prefix: ingress_http
          route_config:
            name: local_route
            virtual_hosts:
            - name: local_service
              domains: ["*"]
              routes:
              - match:
                  prefix: "/"
                route:
                  host_rewrite: www.google.com
                  cluster: google.com
          http_filters:
          - name: envoy.router
  clusters:
  - name: service_google
    connect_timeout: 0.25s
    type: LOCAL_DNS
    dns_lookup_family: V4_ONLY
    lb_policy: ROUND_ROBIN
    hosts:
      - socket_address:
        address: google.com
        port_value: 443
    tls_context: { sni: www.google.com }
//}

TODO upstream を xDS API 経由で切り替えてみる


== まとめ

マイクロサービスはヤバイ変更だから覚悟を持ってやりましょう（仮）
たぶん Envoy はすごいアーキテクチャ。素敵。 nginx のファンやめます（仮）
