= Envoy Proxy 入門

== はじめに

こんにちは。 @syu_cream です。普段は Web サービスを陰から支えるインフラ周辺の構築をしています。

よく分からないけどマイクロサービス流行っていますね！
Netflix などの大型サービスの事例を鑑みて、俺も俺もと足を踏み出す方々も結構多いのではないかと見受けられます。
国内でも例えば Cookpad が 2014 年にマイクロサービスを意識し始めたらしいブログ記事 @<fn>{cookpad_techblog} を公開しています。
かくいう筆者も業務上マイクロサービスを意識したアーキテクチャとソフトウェア開発・組織の構築に、まさに揉まれている最中でございます。

本記事ではそんなマイクロサービスの文脈でよく出てくる技術要素の一つであるサービスメッシュに関わる部分と、 Envoy というプロキシサーバについて触れていきます。

マイクロサービスは真面目に取り組むのなら、単にソフトウェア開発の仕方を少し変えるだとかツールを導入する程度では済まない変更が起こりうり、しばしば痛みを伴うものかと筆者は考えています。
本記事が読者の皆様にとっての、今後のマイクロサービス化の奔流を乗り越える道標のひとつとして機能することを願っています。

//footnote[cookpad_techblog][クックパッドとマイクロサービス: https://techlife.cookpad.com/entry/2014/09/08/093000]


== マイクロサービスアーキテクチャ概要

まずは Envoy の話の前に、簡単にマイクロサービスアーキテクチャに関して確認してみましょう。
マイクロ（小さな）サービスとはそもそも何でしょうか？
それを知るためには、まず対となる概念「モノリシックな（一枚岩な）アーキテクチャ」について考えてみるのがよいでしょう。

=== モノリシックアーキテクチャ

モノリシックアーキテクチャでは、様々な機能が一つのアプリケーションに集約されて構成されます。
ここでは例として @<img>{syucream_monolith_overview} のようなクライアント向け UI とサーバサードアプリケーション、データベースがそれぞれ 1 つずつ存在する Web アプリケーションを考えてみます。

//image[syucream_monolith_overview][モノリスのイメージ][scale=0.7]

このような構成は今日において珍しくなく、実現するためのライブラリやフレームワークは充実していると言えるでしょう。
ビジネスロジックを実現するシステムも一箇所に集中しているため、設計がしっかりしていれば重要な箇所が把握しやすく、また共通ロジックは気軽にライブラリ化して開発効率を上げることもできます。

しかしながらアプリケーションが実現するビジネスロジックが複雑化し、実装に関わるエンジニアが増員されるにつれて統制を取って開発を進めていくのが難しくなりがちです。
またモノリスであるがゆえ、エラーハンドリング漏れや遅延の発生がアプリケーション全体に影響しかねません。

=== マイクロサービスアーキテクチャ

これに対するマイクロサービスアーキテクチャですが、 @<img>{syucream_microservices_overview} のようにあるサービスの機能や責任を細分化し、小規模で自律するマイクロサービスを協調動作させることで Web サービスなどの機能提供を行います。
各マイクロサービスとしては自身の責任範囲に集中することになります。

//image[syucream_microservices_overview][マイクロサービスのイメージ][scale=0.7]

各マイクロサービスは疎結合ゆえ他機能に非依存な技術選択を行うことができ、たとえばそのマイクロサービスが担う責務を実現するのにもっとも適切なプログラミング言語やデータベースを選択できます。
また特定のマイクロサービスのみスケールさせて少数のリソースで提供するサービス全体を安定運用したり、 @<kw>{サーキットブレーカー, 家庭にある電源のブレーカーを思い出すとイメージしやすいでしょう} のような仕組みを導入することで、動作しないあるいは動作が遅延している機能の提供を諦めつつ全体のサービスとしては機能を維持する選択もできます。
加えてマイクロサービスの境界を適切に線引きすることで、各々のマイクロサービスの開発・デプロイ・運用を個別のチームで実施することができ、ビジネスをより早く進めることができるかも知れません。
もちろんマイクロサービスアーキテクチャに沿う際にどこまで自由度を与えるか、どこまで細分化するかは議論の余地があります。例えば Netflix では主に Java でマイクロサービスを実装することが多いようで、 Java 実装のマイクロサービス向けの共有ライブラリが少なくとも数年前は存在したようです。

これらの話は理想的であり、マイクロサービスアーキテクチャに舵切りすることでビジネスを成長させてきた企業の成功体験を聞くとそれに従いたくなるかも知れません。
しかしながらマイクロサービスアーキテクチャでは、複数のマイクロサービス間で通信することによる遅延などの発生や障害発生時のリトライなど考える必要のある課題が数多く発生します。
さらに構成が複雑化することで、マイクロサービス間がどう連携していて遅延やリソース使用の肥大化などが発生した際にどこが問題か発見しにくくなる、オブザーバビリティの確保をしなければならない課題も発生します。
もしかしたら組織編成になじむかの問題など、技術的でない障壁につまづく可能性もあります。
筆者としてもマイクロサービスアーキテクチャは馴染まないケースが多々存在し、これに従うにしても組織としてある程度覚悟をしてリスクを承知の上で取り掛かる必要があると考えています。

マイクロサービスアーキテクチャはそれがもたらすメリットやデメリット、成功事例や技術スタックなどここでは語りきれないほど多くの要素をもちます。
より深く知るための資料としてまずは Martin Fowler 氏の記事 @<fn>{microservices} などが参考になると思われます。

//footnote[microservices][Microservices - Martin Fowler: https://martinfowler.com/articles/microservices.html]


== マイクロサービス、そして Envoy と Istio

マイクロサービスアーキテクチャに従って実世界でサービスを構築していくと、前述したものやそれ以外の複雑な問題に遭遇します。
そして幸いなことに、それらの問題を解決するソフトウェアも世に多く公開されています。
この文脈でもっとも著名なソフトウェアは Kubernetes @<fn>{kubernetes} でしょう。
Kubernetes はインフラを抽象化して、アプリケーションコンテナや周辺システムを制御連携可能にし、コンテナの生死とセフルヒーリングも行ってくれます。
その他マイクロサービス間のメッセージングのために gRPC @<fn>{grpc} とそのエコシステムを使う事例も多く出てきています。
更に非同期通信のために Apache Kafka @<fn>{apachekafka} などのメッセージキューを使う場面も生じるでしょう。

その中でも本記事のテーマである Envoy は、上記とは異なるマイクロサービスの世界における課題を強くバックアップしてくれるソフトウェアです。
ここではまず Envoy と、それを利用したソフトウェアである Istio について触れます。

//footnote[kubernetes][Kubernetes: https://kubernetes.io/]
//footnote[grpc][gRPC: https://grpc.io/]
@<comment>{textlint-disable}
//footnote[apachekafka][Apache Kafka: https://kafka.apache.org/]
@<comment>{textlint-enable}

=== Envoy とは

Envoy Proxy @<fn>{envoyproxy} は Cloud Native Computing Foundation @<fn>{cncf} 配下のソフトウェアでもある、ライドシェアサービスを提供する Lyft によって作られた、マイクロサービスの世界の課題を解決するためのプロキシサーバです。
プロキシサーバとしては @<img>{syucream_proxyserver} のような、 nginx をリバースプロキシとして動作させる時のイメージを持っていただけると、役割が掴みやすいでしょう。

//image[syucream_proxyserver][プロキシサーバを使った構成例][scale=0.8]

Envoy はハイパフォーマンスでマイクロサービスの世界におけるネットワーク通信とオブサーバビリティの問題を解決することを目指しています。
具体的に Envoy が持つ機能・特徴は以下のようになります。

 * C++11 で実装されたモダンなプロキシサーバ
 * 省メモリで高性能
 * HTTP/2 と gRPC のサポート (もちろん HTTP/1.1 もサポートしている)
 * 自動リトライ
 * サーキットブレーカー
 * 複数種類のロードバランシング
 * 動的に設定変更を可能にする API の提供
 * 分散トレーシングや MongoDB, DynamoDB などのデータベースに対応したオブザーバビリティの提供

この中でも動的に設定変更可能にしている API の存在はユニークであると思われます。
Envoy は複数種類からなる xDS(x Discovery Service) API をサポートしており、外部システムにプロキシとしての振る舞いの設定管理を委ねることができます。
こうした柔軟な設定変更を可能にしているからこそ、マイクロサービス間の通信においてデータのやり取りを担う Data Plane として Envoy を使い、データのやり取りの仕方の管理を担う Control Plane をそれ以外(例えば後述の Istio )に任せる構成が取れます。

また Envoy はコンテナ上で動作させることを想定して開発されています。
よくある利用パターンとして @<img>{syucream_envoy_sidecar} に示すようなイメージで Kubernetes のアプリケーションコンテナとセットで Envoy を動作させる別コンテナもデプロイして、これらのコンテナを連携して使うことがあります。
Envoy の担う機能はアプリケーションとはコンテナレベルで独立して、かつ通信は gRPC などで行うことで特定のプログラミング言語に依存することなく、マイクロサービスの構成に柔軟性を与えることができます。

//image[syucream_envoy_sidecar][Envoy の利用イメージ][scale=0.7]
//footnote[envoyproxy][Envoy Proxy: https://www.envoyproxy.io//]
//footnote[cncf][Cloud Native Computing Foundation: https://www.cncf.io/]

=== Istio とは

Istio @<fn>{istio} は Envoy で Data Plane を提供しつつ Control Plane も別途提供することで、マイクロサービス間のコネクションを変更・制御したり認証認可や暗号化によるセキュリティ担保を行う、マイクロサービスの世界でサービスメッシュと呼ばれる機能を提供するソフトウェアです。
現状だとターゲットとするインフラとして Kubernetes を前提にしています。

Istio では Envoy を一部拡張して Data Plane を実現するのに使います。
Kubernetes の用語を混ぜての説明になりますが、 Istio において Envoy は Kubernetes でデプロイする、 @<kw>{Pod, いくつかのコンテナをまとめたもの} の全てに @<kw>{Sidecar コンテナ, アプリケーションのコンテナとは独立した、同一 Pod に同梱されるコンテナ} として導入されます。
マイクロサービス間の通信時はこの Sidecar の Envoy 同士が通信処理を仲介するように動作します。
Kubernetes の世界では Pod の生き死にが頻繁に起こり、 Envoy のルーティングの設定を動的に更新できなければなりません。
Istio ではこの設定変更を実現するために Pilot というコンポーネントを持ち、 Envoy に対するルーティングルールを設定して Envoy に伝えるようにしています。

本記事では Envoy に主眼を起きたいためあまり Istio については深く触れません。
詳しく知りたい方は先述の公式ページのリンクを辿ったり、実際の導入事例などを探してみることをおすすめいたします。

//footnote[istio][Istio: https://istio.io/]

== Envoy 詳解

ここからはより Envoy の詳細について提供する機能と使用している技術の側面から掘り下げていこうと思います。

=== Envoy アーキテクチャ概要

Envoy は先述の通りハイパフォーマンスであることを目指しており、モダンなプロキシが取るようなマルチスレッド・イベントドリブンな並列 I/O 処理を実装しています。
Envoy のアーキテクチャの概要を図示したものが @<img>{syucream_envoy_eventhandling} になります。

//image[syucream_envoy_eventhandling][Envoy のイベントハンドリング]

Envoy のスレッドには役割分担があり、 main() から開始された単一の main スレッドとネットワーク I/O などのイベントを処理する複数の worker スレッドが存在します。
worker スレッドの制御には pthread API を利用しています。 C++11 でサポートが入った std::thread の機能はほとんど使われておりません。
なおこの worker スレッドの数は Envoy のコマンドラインオプション --concurrency で指定可能であり、指定しない場合はハードウェアスレッド数 ( std::thread::hardware_concurrency() から与えられる)分実行されるようです。
@<comment>{textlint-disable}
master, worker スレッド以外にも実は、アクセスログの出力などファイルフラッシュ時に含まれるブロッキング処理をオフロードするためのファイルフラッシュスレッドなども存在します。
@<comment>{textlint-enable}

またイベント処理に関しては libevent @<fn>{libevent} を使っています。
Envoy では思想として 100 ％ノンブロッキングをうたっており、ネットワークやファイルの I/O 、内部的な処理をなるべくイベントドリブンで処理可能にしています。
@<comment>{textlint-disable}
ブロッキング処理を伴うファイルのフラッシュを worker スレッドから切り離されたファイルフラッシュスレッドを用意するあたり、この思想は実装に色濃く反映されていると言えるでしょう。
@<comment>{textlint-enable}
各ワーカスレッドはそれぞれ libevent でイベントループを回すための、 Envoy 内部で Dispatcher と呼ばれる構造を持ち、これを介してイベントのハンドリングを可能にしています。

Envoy では更にスレッドローカルストレージを抽象化した実装を持ち、スレッド間の共有データを排除してロックなどによるパフォーマンス低下を回避しています。
スレッドローカルストレージでは C++11 からサポートが入った thread_local キーワードを用いて、スレッド毎に割り当てられた記憶領域に任意の動的生成されたオブジェクトを格納できます。
どうやらここでは pthread API の pthread_get_specific() などは使っていないようです。
またスレッドローカルストレージでは、 slot という main スレッドからイベントループを介して（具体的には Dispatcher がサポートする、 0 秒後にタイマーイベントを発火させるメンバー関数を使って）値の更新が可能な領域も持ちます。

Envoy のアーキテクチャに関してより深く知りたい方は、 Kubecon EU 2018 の資料 @<fn>{kubecon_eu_2018} を参照してみると良いかもしれません！

//footnote[libevent][libevent: https://libevent.org/]
//footnote[kubecon_eu_2018][Kubecon EU 2018: https://speakerdeck.com/mattklein123/kubecon-eu-2018]

=== Envoy のリソース抽象化

Envoy ではネットワーク通信やプロキシ処理における様々なリソースを抽象化しています。
全体像は以下 @<img>{syucream_envoy_resources} の通りになります。
ここではイメージを掴みやすいよう、クライアントのリクエストを受け付けてから転送するまでのフローを想定して図示しています。

//image[syucream_envoy_resources][Envoy の各種リソース][scale=0.8]

以降ではこれらのリソースについて、リクエストが転送されるまでのフローの順で詳細を掘り下げていきます。

==== Listener

Envoy が @<kw>{downstream, Envoy に対してリクエストを送るクライアント} から受け付けるコネクションを受け付けるネットワークロケーションです。
現在は TCP listener のみサポートしています。
Envoy では複数の Listener に対応しており、この Listener に対して後述の Filter を設定して通信制御や Cluster への転送を行います。

==== Listener Filter

@<img>{syucream_envoy_resources} には含めていないのですが、 Envoy では Listener に対応してコネクションのメタデータを修正したりするのに使われる Filter をサポートしています。
主に他のシステムとの連携に使用するのに必要なメタデータを付与したりするのに使う想定のようです。
Envoy では現在エクステンションという立ち位置で、後ほど service discovery の Original Destination という機能でも触れるメタデータの読み込みなどの Listener Filter を提供しています。

==== Network(L3/L4) Filter

L3, L4 レベルの生データに触れたりコネクションレベルのイベントハンドリングをすることができる Filter です。
そして Envoy においてプロキシの制御のコアの部分はこの Network Filter として実装されています。

後述する、 Envoy を利用する上でお世話になることが多いであろう HTTP connection manager もこの Network Filter の一種です。
その他にも TCP Proxy 機能の提供やレートリミットも Network Filter の一種として提供されます。

Network Filter には downstream から送られてくるデータを解釈する際に呼ばれる ReadFilter と downstream にデータを送る際に呼ばれる WriteFilter 、その両方が行える Filter と、大きく分けて三種類あります。

==== HTTP connection manager 

HTTP connection manager は Network Filter の一種です。
生データを処理して HTTP として解釈した上で様々な機能を提供します。
Envoy は HTTP/2, HTTP/1.1 はもちろんのこと WebSocket もサポートします。
ちなみに公式ドキュメントでは SPDY のサポートはしていない旨の明記がされています。このご時世ならこのサポートは不要でしょうが。

HTTP connection manager がサポートする機能としては以下の通りです。

 * HTTP Filter のサポート
 * ルーティング
 * アクセスログの記録
 * トレーシングのためのリクエスト ID 発行
 * リクエスト・レスポンスヘッダの修正

HTTP Filter というのは Network Filter の HTTP 版であるようなイメージを浮かべていただけるといいと思います。
こちらも Network Filter と同様に 3 種類、 HTTP リクエストストリームをデコードする際に呼ばれる Stream Decode Filter と HTTP レスポンスをエンコードする際に呼ばれる Stream Encode Filter 、その両方が行える Stream Filter があります。
HTTP Filter として標準でサポートされている機能も多々あり、バッファリングや gzip 圧縮など nginx などの他のプロキシ実装でも広く存在するものや、 gRPC-HTTP/1.1 bridge など gRPC のサポートを厚くしている Envoy の特色が出ているものなど多岐にわたります。
また HTTP Filter では Lua スクリプトによる機能拡張もサポートされています。

ルーティングは HTTP リクエストに対して適切な upstream Cluster を決定してリクエストを転送する機能を提供します。
それに伴い、以下のような複数の機能も提供します。

 * バーチャルホストの提供
 * ホストやパスの書き換え
 * リクエストのリトライ
 * リクエストのタイムアウト制御
 * HTTP/2 の優先度による制御

==== Cluster

Envoy における、プロキシ先の upstream ホストをグループ化したものです。
Envoy は upstream ホストはヘルスチェックを通して生死判定をして、転送処理を行う際に生きているホストに対してロードバランシングポリシーを加味して転送先を決定します。

ちなみに Envoy が転送処理を行う際に upstream Cluster の host を探す必要があるのですが、これを service discovery と呼びます。

==== Upstream

Envoy におけるプロキシ先のホストです。
upstream のホストへの接続はコネクションプールという機構によって抽象化されており、 L7 の使用プロトコルが HTTP/1.1 なのか HTTP/2 なのかの差異などをここで吸収しています。
また Envoy ではリトライの処理など何かと upstream 単位で制御してくれたりします。

=== Envoy の特徴的な機能説明

Envoy には先に挙げたようなユニークな機能がいくつか存在します。
ここでは xDS API と分散トレーシング、そしてネットワーク通信に関わる主要な機能について深掘りしてみようと思います。

==== xDS API

少しだけ先述しましたが、 Envoy は xDS API という多種にわたる動的設定変更のための API をサポートしています。
具体的には以下の API がサポートされています。(Envoy の API には v1 と v2 の 2 バージョンがあるのですが、ここでは v2 のみ触れます)

 * LDS(Listener Discovery Service)
 * RDS(Route Discovery Service)
 * CDS(Cluster Discovery Service)
 * EDS(Endpoint Discovery Service)

EDS(Endpoint Discovery Service) は xDS API の中でもよく使われる類のものかも知れません。 Cluster のメンバーとなる upstream ホストを検出します。ちなみにこの API は v1 では SDS(Service Discovery Service) という名前だったようです。
CDS(Cluster Discovery Service) は upstream Cluster の設定を与える際に使われます。
RDS(Route Discovery Service) は HTTP connection manager に関連する xDS API であり、ルーティングの設定を変更することができます。
LDS(Listener Discovery Service) は Listener の設定を与える際に使われます。

xDS API の定義は Protocol Buffer @<fn>{protobuf} によって定義されて @<fn>{xds_protocol} います。

xDS API は @<img>{syucream_envoy_xdsapi} に示すように、 3 つの設定変更のための方法をサポートします。

1 個目は最もシンプルで、ファイルとして xDS API に設定内容を渡すことができます。
ファイルの形式は xDS API の定義に従った DiscoveryResponse メッセージ型で Protocol Buffer でエンコードされたバイナリや JSON 、 YAML が利用できます。

2 個目は gRPC でストリーミングで設定値を渡すものになります。この場合も Protocol Buffer のレスポンス用メッセージ型を渡すことで設定変更が可能になります。
gRPC を使う方法の場合は RDS と EDS 、のような複数の異なる設定項目をやり取り可能にする ADS(Aggregated Discovery Services) を利用することができ、 xDS API 提供の障壁を下げることができます。

3 個目は JSON REST API を提供する方法になります。この場合 Envoy が指定の API のエンドポイントをポーリングして、設定値に変更があった際に更新してくれます。
返却する JSON は上記の Protocol Buffer でのメッセージ定義を、 Protocol Buffer の JSON Mapping @<fn>{proto3_json_mapping} した形式に従います。

//image[syucream_envoy_xdsapi][Envoy xDS API][scale=0.9]

xDS API の存在は Envoy の運用に柔軟性を与え、また Istio のような Control Plane の実現を容易にしています。
実際に xDS API を利用して、 Cookpad では Istio より小規模な自作の Control Plane を構築 @<fn>{servicemesh_and_cookpad} するなど活用しています。

//footnote[protobuf][Protocol Buffer: https://developers.google.com/protocol-buffers/]
//footnote[xds_protocol][xDS REST and gRPC protocol: https://github.com/envoyproxy/data-plane-api/blob/master/XDS_PROTOCOL.md]
//footnote[proto3_json_mapping][Protocol Buffer JSON Mapping: https://developers.google.com/protocol-buffers/docs/proto3#json]
//footnote[servicemesh_and_cookpad][Service Mesh and Cookpad: https://techlife.cookpad.com/entry/2018/05/08/080000]

==== service discovery

先述の通り Envoy は転送時に転送先を解決する service discovery を行います。
この時、以下のような幾つかの方法を選択することができます。

 * Static
 * Strict DNS
 * Logical DNS
 * Original Destination
 * EDS

Static はもっともシンプルな、直接 upstream host の IP アドレスやポート番号を指定する方法です。

Strict DNS は DNS を使った upstream host の解決方法です。この際に複数の IP アドレスが返ってきた場合はロードバランシングされるよう Envoy が調整してくれます。

Logical DNS は Strict DNS と似た DNS を使った方法なのですが、 upstream host にコネクションを張るにあたり複数 IP アドレスが返却されてもその中の最初の IP アドレスのみを用います。これは DNS ラウンドロビンなど Envoy 以外で負荷分散することを考える際に有用で、 Envoy 公式のドキュメントとしては大規模な Web サービスと通信する際は Logical DNS を使うことを推奨する記述があります。

Original Destination は iptables の REDIRECT または TPROXY ターゲットを使って、あるいは HAProxy Proxy Protocol @<fn>{proxy_protocol} を伴って、 Envoy にリクエストがリダイレクトされた際の upstream host の解決方法です。この場合 Envoy はクライアントが送りたいオリジナルの送信先を upstream host として解決してくれます。 Original Destination は HTTP レベルでも使用することができ、x-envoy-orignal-dst-host ヘッダの値に upstream host として扱う IP アドレスとポート番号を指定できます。

最後に EDS の場合は EDS API を使って upstream host を解決できます。 EDS API を自前で実装することでより柔軟な service discovery が実現でき、また独自のロードバランシングの仕組みも組み込むことができるでしょう。

//footnote[proxy_protocol][HAProxy Proxy Protocol: http://www.haproxy.org/download/1.9/doc/proxy-protocol.txt]

==== トレーシング

Envoy が提供する大きな機能でありマイクロサービスの世界で問題になることとして、オブザーバビリティが挙げられます。
例えばこれが 1 つのモノリスで構築されたシステムであれば開発時にデバッグをしたければ従来のデバッガを使ったり、静的解析してコールグラフを出したりスタックトレースを出力してみたりすることができます。
しかし境界を明確にして自立分散して動作するマイクロサービスの世界では、従来は出来ていたこれらも困難になります。
代表的な問題として、多量のマイクロサービスが存在してそれらを連携する際に、あるリクエストがどのような経路を辿って関連しているのか紐づけが難しいことが挙げられます。

Envoy を作った Lyft では、マイクロサービスを繋いだ広範囲のスコープと個々のマイクロサービスのメトリクスを可視化したダッシュボードの構築をしてオブザーバビリティを確保しています。 @<fn>{lyft_dashboard}
これによって運用時にどこで障害が起こっていそうか、何が起因になってエラーが起こったかなどが確認しやすくなっています。

Envoy では各マイクロサービスの通信の関連付けを行いオブザーバビリティを向上するための幾つかの仕組みが提供されています。
これの仕組みは Envoy では HTTP connection manager によって提供されます。

 * リクエスト ID の生成
 * LightStep や Zipkin のようなトレーシングサービスとの連携
 * クライアントトレース ID の結合

Envoy のトレーシングのための ID 発行・伝搬イメージは @<img>{syucream_envoy_tracing} の通りです。

//image[syucream_envoy_tracing][Envoy のトレーシング][scale=0.8]

リクエスト ID として Envoy では x-request-id HTTP ヘッダを伝搬し、また必要であれば UUID を生成してヘッダの値として付与してくれます。
このリクエスト ID をログに記録しておくことで、後で複数のマイクロサービスのログを x-request-id で突き合わせてリクエストのフローを確認することができます。
またスマホアプリなどサーバサイドの外側のクライアントを含めたトレーシングを可能にするため、 x-client-trace-id HTTP ヘッダが付与されていた場合にはその値を x-request-id に追記してくれます。

Envoy にトレーシングを要求する方法は幾つか存在し、まず先述の x-client-trace-id または x-envoy-force-trace HTTP ヘッダが付与されている場合行ってくれます。
その他にも random_sampling ランタイム設定で指定された値に従ってランダムにトレーシングを行ってくれます。

//footnote[lyft_dashboard][Lyft’s Envoy dashboards: https://blog.envoyproxy.io/lyfts-envoy-dashboards-5c91738816b1]

==== サーキットブレーカー

分散システムにおいてしばしば障害点を切り離してシステム全体の動作を維持したり遅延を低減させるため、サーキットブレーカーを導入することがあると思います。
サーキットブレーカーのロジックをアプリケーションに含ませるのは手軽ではありますが、やはり個別のプログラミング言語で個々に実装していく必要があります。
Envoy でサーキットブレーカーをサポートすることにより、これらの問題を軽減することができます。

Envoy では HTTP connection manager により以下に上げるようないくつかの種類のサーキットブレーカーを提供します。

 * HTTP/1.1 Cluster 向け
 ** Cluster への最大コネクション数
 ** Cluster への最大未処理コネクション数
 * HTTP/2 Cluster 向け
 ** Cluster への最大リクエスト数

これらのサーキットブレーカーのしきい値を超えた際、 @<img>{syucream_envoy_circuitbreaker} Envoy は基本的に HTTP connection manager のルーティング機能によるリトライを行います。
サーキットブレーカーに引っかかる際のリトライ回数制限も設定項目で指定することができ、これに引っかかった際は Envoy の提供する stats のカウンタに記録されます。

//image[syucream_envoy_circuitbreaker][Envoy のサーキットブレーカー][scale=0.8]

==== グローバルレートリミット

Envoy はサービスを維持するため、過度な転送を避けるレートリミット機能も持ちます。
転送の制限を掛けるという意味では先述のサーキットブレーカーに近い機能ではありますが、この機能の目的と制限を掛ける対象リソースが異なります。

Envoy のグローバルレートリミットの動作イメージは @<img>{syucream_envoy_ratelimit} の通りです。
upstream のホストに対する転送制限を行うサーキットブレーカーに対して、グローバルレートリミットでは downstream に近い箇所で制限を行います。
Envoy は現在 Network level rate limit filter と HTTP level rate limit filter の２つのレートリミット機能を持ち、それぞれ Network Read Filter あるいは HTTP Stream Decode Filter の一種として実装されています。
また前者は新しいコネクションを作成する際に、後者はリクエスト毎に制限を超過していないかのチェックを行います。
さらに Envoy ではレートリミットの状態管理を外部サービスに任せる思想になっています。
具体的には Rate Limit Service @<fn>{rate_limit_service} インタフェースを実装した gRPC サービスを呼び出すような形になります。
Lyft ではこの Rate Limit Service のリファレンス実装 @<fn>{lyft_rls_refimpl} として Go 言語で実装して Redis をバックエンドにしたものを公開しています。

//image[syucream_envoy_ratelimit][Envoy のレートリミット][scale=0.7]

グローバルレートリミットによって、（Rate Limit Service の作りによるところはありますが）サーキットブレーカーより柔軟な転送制御ができるようになります。
また少数でレイテンシが大きめなの upstream ホストに多量の downstream からのリクエストが来るようなケースに、サーキットブレーカーの入念なチューニングなく制限を掛けることもできるようになります。

//footnote[rate_limit_service][rate limit service protocol: https://github.com/envoyproxy/envoy/blob/master/api/envoy/service/ratelimit/v2/rls.proto]
//footnote[lyft_rls_refimpl][lyft/ratelimit: https://github.com/lyft/ratelimit]

==== ロードバランサ

Envoy は upstream Cluster への接続が必要になった際に、幾つかの方法でロードバランシングしてくれます。
ロードバランシングの方法は upstream Cluster 毎に設定できます。
標準でサポートされているロードバランシング方法は以下の通りです。

 * 重み付きラウンドロビン
 * 重み付き最小リクエスト
 * リングハッシュ
 * Maglev
 * ランダム

重み付き、と付く方法はエンドポイントに重みを付けて、重みが大きいエンドポイントにより負荷を高めるようスケジュールできます。

リングハッシュと Maglev は両方とも、 upstream ホストに対するコンシステントハッシュを作成してバランシングに用いる方法です。
リングハッシュでは ketama @<fn>{ketama} 、 Maglev では名前の通り Maglev @<fn>{maglev} というアルゴリズムを用います。
この２つの機能差ですが、 Maglev の方が性能面で利点があるようで、将来的に Maglev がリングハッシュを置き換える想定のようです。

最後のランダムですが、シンプルにヘルスチェックが通っているホストにランダムに振り分けます。

//footnote[ketama][ketama: https://github.com/RJ/ketama]
//footnote[maglev][Maglev: https://static.googleusercontent.com/media/research.google.com/en//pubs/archive/44824.pdf]

=== nginx など従来のプロキシと何が違うのかについて

Envoy の技術的側面について考えていくと、 nginx などモダンでハイパフォーマンスなプロキシと何が違うのかと疑問を抱く方が現れるかと思います。
Envoy の公式ページでは、 Envoy は各アプリケーションとセットで動作し、オブザーバビリティなどマイクロサービスにおける問題を解くのに注力しているような記述が見受けられます。
また nginx を比較の対象とするなら、 Envoy はそれに比べて以下のような長所が存在します。

 * downstream だけでなく upstream への通信も HTTP/2 に対応している
 * nginx plus でサポートされるようなロードバランサの仕組みを Envoy では標準で搭載している

筆者の個人の意見としては、 Envoy は nginx など既存のプロキシと比べて機能の充実具合や拡張性でいうとまだ劣ると考えています。
マイクロサービス基盤を構築するチームが nginx の運用ノウハウを十分に持っている場合、 nginx を使ったサービスメッシュの構築を考えても良いかも知れません。

しかし Envoy は逆に機能が少ないゆえにターゲットとする課題を解くのに最低限の責務に集中しているとも捉えられます。
また Envoy では xDS API の提供により最初から動的に設定を変更していくことをサポートしている点も、大きな差分になるとも考えられます。

現状だと Istio との統合も考えると Envoy の今後が期待できるところですが、最近は nginx で gRPC のサポートが入ったり引き続き活発な開発が続いています。
更に Istio と nginx を統合させてサービスメッシュを構築するプロジェクト @<fn>{nginmesh} も、あまり活発ではないものの開発が進められているようです。
もしかしたら今後、解決したい問題によって使用するプロキシを選択してサービスメッシュを構築できる未来が訪れるのかも知れません。

//footnote[nginmesh][nginxinc/nginmesh: https://github.com/nginxinc/nginmesh]

== Envoy の試し方

ここまで解説してきた Envoy ですが、まずは実際の動作や設定値を確認してみた方がイメージもつかみやすいでしょう。
ここでは公式で提供されている Docker image を使って手軽に動作を確認しつつ、設定項目を追ってみようと思います。

=== Docker image をとりあえず動かす

Envoy はマイクロサービスの世界における課題を解決するために開発されたソフトウェアではありますが、それ単体でリバースプロキシとして動作させることが可能です。
ちょうど Envoy 公式提供の Docker image がそのようなシンプルな動作確認も想定してビルドされているので、シンプルにそれを動かしてみましょう。

さて公式の Docker image ですが、以下のように普通に docker pull して使用することができます。
この image では 10000 番ポートでプロキシリクエストの受け付けをしており、動作時にポートマッピングをすることで気軽に動作確認することができます。

//cmd{
$ docker pull envoyproxy/envoy
$ docker run -it -p 10000:10000 envoyproxy/envoy
//}

この Docker image とそれに含まれるデフォルトの設定ファイルからなされる構成は @<img>{syucream_envoy_example1} の通りです。

//image[syucream_envoy_example1][Envoy の動作例][scale=0.8]

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
  # Listener の設定
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

  # Cluster の設定
  clusters:
  - name: service_google
    connect_timeout: 0.25s
    type: LOGICAL_DNS
    dns_lookup_family: V4_ONLY
    lb_policy: ROUND_ROBIN
    hosts:
      - socket_address:
        address: google.com
        port_value: 443
    tls_context: { sni: www.google.com }
//}

まず目立つのは Listener の設定でしょう。
この例では前述の通り 10000 番ポートでリクエストを待ち受けます。

この Listener ではリクエストを受け付けた後、 Network Filter として HTTP connection manager を利用してルーティングを行うようにしています。
ここでは任意のドメイン、 / から始まるパスに対するリクエストをルーティング対象にしています。
他にルーティングのルールも無いためこの設定で動作する Envoy ではすべてのリクエストがこのルーティングルールの適用対象になるでしょう。
ルーティング対象のリクエストは google.com 向け Cluster へ転送されるようです。
合わせて Host を www.google.com に書き換えてもいます。

続いて Cluster の設定を確認してみましょう。
ここではシンプルに 1 Cluster で、かつそのメンバーとなる upstream ホストは google.com のものただ一つになります。
具体的には google.com の 443 番ポートに向かってリクエストを転送する格好になります。

以上の設定内容により、この例の Envoy ではリクエストに対して www.google.com 用のレスポンスを取得して downstream に返してくれる動作をします。


=== 複雑な構成を試してみる

公式の Docker image をそのまま動かすのではあまり面白く無いでしょう。
ここでは更に踏み込んで、 xDS API との連携やロードバランシングの動作の確認も行ってみましょう。
なお、ここに掲載するサンプルは GitHub の筆者のリポジトリ @<fn>{envoy_simple_example} に公開されています。

xDS API は gRPC サーバを立ててストリームで DiscoveryResponse メッセージを渡すのが王道のようですが、まともに構築するのはやや面倒です。
ここではコストを低減すべく REST API で HTTP 越しに静的な JSON ファイルを返すことで xDS API の動作を確認してみようと思います。
また全ての xDS 用の DiscoveryResponse を用意するのも面倒ですし、動作確認もしやすくメッセージ型があまり複雑でない EDS のみ対象にしてみます。

今回のデモの構成としては @<img>{syucream_envoy_example2} のような、以下の通りにしてみます。
まず Envoy と、それと連携する EDS API 、そして Envoy の upstream の endpoint となる 2 台のサーバから構成されます。

//image[syucream_envoy_example2][Envoy のより複雑な構成での動作例]

Envoy と EDS API の連携のため、 Envoy の設定ファイルには upstream Cluster の endpoint の解決方法を EDS にします。
また EDS API として参照する先の Cluster も別途設定しておきます。
EDS API は今回 REST API と提供するのでその指定と、 Envoy が EDS API を参照する頻度を指定しておきます。

//source[etc/envoy/envoy.yaml]{
...
static_resources:
  # Listener の設定。前の例とほぼ変わらない内容
  listeners:
  - name: listener_0
    address:
      socket_address:
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
                  cluster: cluster_0
          http_filters:
          - name: envoy.router

  clusters:
  # EDS API の Cluster 設定
  - name: eds_cluster
    type: LOGICAL_DNS
    connect_timeout: 0.25s
    dns_lookup_family: V4_ONLY
    hosts:
      - socket_address:
          # httpxds という名前の upstream ホストを参照する
          address: httpxds
          port_value: 8080
  # EDS API から返却された endpoint を参照する Cluster 設定
  - name: cluster_0
    type: EDS
    connect_timeout: 0.25s
    lb_policy: ROUND_ROBIN
    eds_cluster_config:
      eds_config:
        api_config_source:
          api_type: REST
          cluster_names: [eds_cluster]
          refresh_delay: 60s
//}

次に EDS API のレスポンスを作り込みます。
2 台の endpoint の IP アドレスを予め決めておき、 DiscoveryResponse の要素として ClusterLoadAssignment メッセージ型のレスポンスを用意しておきます。

//source[etc/httpxds/eds.json]{
{
  "resources": [
    {
      "@type": "type.googleapis.com/envoy.api.v2.ClusterLoadAssignment",
      "cluster_name": "cluster_0",
      "endpoints": [
        {
          "lb_endpoints": [
            {
              "endpoint": {
                "address": {
                  "socket_address": {
                    "protocol": "TCP",
                    "address": "172.16.238.10",
                    "port_value": 80
                  }
                }
              }
            }
          ]
        },
        {
          "lb_endpoints": [
            {
              "endpoint": {
                "address": {
                  "socket_address": {
                    "protocol": "TCP",
                    "address": "172.16.238.11",
                    "port_value": 80
                  }
                }
              }
            }
          ]
        }
      ]
    }
  ]
}
//}


この JSON ファイルを配信する方法は何でもいいのですが、今回は nginx で EDS API のエンドポイント v2/discovery:endpoints でリクエストされた際に返却できるようにしてみます。
ここでちょっとした注意点なのですが、 xDS API を JSON REST API 形式で立てる場合 Envoy は仕様上 POST メソッドでリクエストを送ってきます。
nginx で POST メソッドでのリクエストに対して静的なレスポンスを返す際、 POST メソッドを許可するような設定をする必要があります。

//source[etc/httpxds/xds.conf]{
server {
    listen       8080;
    server_name  localhost;

    # Allow POST method to get static responses
    error_page 405 = $uri;

    # Envoy EDS API
    location = /v2/discovery:endpoints {
        alias /usr/share/nginx/json/eds.json;
    }
}
//}

2 台の endpoint の構成も何でも良いでしょう。 EDS API と揃えて nginx で html ファイルを配信することにします。
全ての準備が出来たら、今回はシンプルに docker-compose で連携させてみます。

//source[docker-compose.yml]{
version: '3'

services:
  # EDS API や endpoint と疎通できる Envoy
  envoy:
    container_name: envoy
    image: envoyproxy/envoy
    volumes:
      - ./etc/envoy/envoy.yaml:/etc/envoy/envoy.yaml:ro
    links:
      - httpxds
      - endpoint0
      - endpoint1
    ports:
      - "10000:10000"
      - "9901:9901"
    networks:
      - app_net
    command: |
      /usr/local/bin/envoy \
        --service-cluster cluster0 \
        --service-node envoy0 \
        -c /etc/envoy/envoy.yaml

  # nginx based EDS API
  httpxds:
    container_name: httpxds
    image: nginx
    volumes:
      - ./etc/httpxds/eds.json:/usr/share/nginx/json/eds.json
      - ./etc/httpxds/xds.conf:/etc/nginx/conf.d/xds.conf:ro
    networks:
      - app_net

  # EDS API で返す用の IP アドレスを決めておく
  endpoint0:
    container_name: endpoint0
    image: nginx
    volumes:
      - ./etc/endpoint0/whoami.html:/usr/share/nginx/html/whoami.html:ro
    networks:
      app_net:
        ipv4_address: 172.16.238.10

  # EDS API で返す用の IP アドレスを決めておく
  endpoint1:
    container_name: endpoint1
    image: nginx
    volumes:
      - ./etc/endpoint1/whoami.html:/usr/share/nginx/html/whoami.html:ro
    networks:
      app_net:
        ipv4_address: 172.16.238.11

networks:
  app_net:
    driver: bridge
    ipam:
      driver: default
      config:
      -
        subnet: 172.16.238.0/24
//}

docker-compose で各コンテナを動作させて、実際に Envoy へリクエストを投げてみます。

//cmd{
$ docker-compose up
...
# EDS API に Envoy からのリクエストが定期的に来る
httpxds      | 172.16.238.3 - - [22/Sep/2018:14:39:21 +0000] "POST /v2/discovery:endpoints HTTP/1.1" 200 897 "-" "-" "172.16.238.3"
...
$ curl http://localhost:10000/whoami.html
...
$ curl http://localhost:10000/whoami.html
...
# docker-compose の出力にそれぞれリクエストが振られているログが出る
...
endpoint0    | 172.16.238.3 - - [22/Sep/2018:14:40:26 +0000] "GET /whoami.html HTTP/1.1" 200 113 "-" "curl/7.54.0" "-"
endpoint1    | 172.16.238.3 - - [22/Sep/2018:14:40:27 +0000] "GET /whoami.html HTTP/1.1" 200 113 "-" "curl/7.54.0" "-"
endpoint0    | 172.16.238.3 - - [22/Sep/2018:14:40:27 +0000] "GET /whoami.html HTTP/1.1" 200 113 "-" "curl/7.54.0" "-"
endpoint1    | 172.16.238.3 - - [22/Sep/2018:14:40:28 +0000] "GET /whoami.html HTTP/1.1" 200 113 "-" "curl/7.54.0" "-"
...
//}

以上により、無事に EDS API により配信された endpoint へ、負荷分散されつつリクエストが転送されたことが確認できました！
加えて EDS API のレスポンスを変更したらどうなるでしょうか。
ここでは EDS API で返却する endpoint を endpoint0 のみに限定するよう修正してみます。

//source[etc/httpxds/eds.json]{
{
  "resources": [
    {
      "@type": "type.googleapis.com/envoy.api.v2.ClusterLoadAssignment",
      "cluster_name": "cluster_0",
      "endpoints": [
        {
          "lb_endpoints": [
            {
              "endpoint": {
                "address": {
                  "socket_address": {
                    "protocol": "TCP",
                    "address": "172.16.238.10",
                    "port_value": 80
                  }
                }
              }
            }
          ]
        }
      ]
    }
  ]
}
//}

//cmd{
$ curl http://localhost:10000/whoami.html
...
$ curl http://localhost:10000/whoami.html
...
# docker-compose の出力に 1 endpoint 分しか出てこなくなる
...
endpoint0    | 172.16.238.3 - - [22/Sep/2018:14:44:35 +0000] "GET /whoami.html HTTP/1.1" 200 113 "-" "curl/7.54.0" "-"
endpoint0    | 172.16.238.3 - - [22/Sep/2018:14:44:36 +0000] "GET /whoami.html HTTP/1.1" 200 113 "-" "curl/7.54.0" "-"
endpoint0    | 172.16.238.3 - - [22/Sep/2018:14:44:37 +0000] "GET /whoami.html HTTP/1.1" 200 113 "-" "curl/7.54.0" "-"
endpoint0    | 172.16.238.3 - - [22/Sep/2018:14:44:37 +0000] "GET /whoami.html HTTP/1.1" 200 113 "-" "curl/7.54.0" "-"
endpoint0    | 172.16.238.3 - - [22/Sep/2018:14:44:38 +0000] "GET /whoami.html HTTP/1.1" 200 113 "-" "curl/7.54.0" "-"
...
//}

endpoint0 のみ返却するよう修正した後は、 endpoint1 へリクエストが到達せず endpoint0 のみが処理するようになっていることがわかります。
これにより EDS API で返却する endpoint の情報の更新が Envoy にも伝わっていることが見て取れます。

//footnote[envoy_simple_example][syucream/envoy-simple-example: https://github.com/syucream/envoy-simple-example]


== おまけ

ここからはおまけとして、本編に組み込むほどではない細かい話題、あるいは筆者が綺麗な構成を考えることをサボったがゆえに後回しにされた話題を紹介します。
おまけという表現、非常に便利ですよね… ！

=== Istio における Envoy の組み込まれ方

本編で簡単に紹介させて頂いた Istio ですが、このソフトウェアの公式ドキュメント @<fn>{istio_doc_envoy} には以下のような記述があります。

//quote{
Istio uses an extended version of Envoy proxy.
//}

Istio では Envoy を独自に拡張して組み込んでいるようです。さてどのような拡張をしているのでしょうか。

と、話題を振った上で直ぐに回答してしまうのですが、 Istio では Envoy の Network Filter と HTTP Filter を実装して Istio のコンポーネントと通信可能にすることで組み込んでいるようです。
少なくともこの記事を執筆している段階では、 Envoy をフォークしてコアの実装を拡張しているなど大胆な手段は取っていないようです。

Envoy とその Filter と、 Istio の連携イメージは @<img>{syucream_envoy_istio_mixer} の通りです。
Istio には Mixer というコンポーネントがあり、こいつがマイクロサービス間のアクセス制御やポリシー制御、メトリクスやログの記録を行います。
Istio ではこの Mixer と連携可能にすべく、 Network Filter および HTTP Filter を実装している形になります。

Istio において Envoy は Istio Proxy というコンポーネントとして組み込まれます。
そして Envoy の Network, HTTP Filter として Mixer へ前提条件チェックリクエストを送ってから通信を行ったり、通信後にレポートリクエストを送ったりします。
Network Filter としては TCP コネクションが作られる際に前提条件チェックを行い、クローズされる際にレポーティングを行います。
HTTP Filter でも同じようにリクエストを送る前に前提条件チェックを行い、その後レポーティングします。
HTTP Filter の方が機能的に充実していて、細かな機能のオンオフやチェックリクエストのキャッシュを行うこともできます。

余談ですがこの Mixer Filter の構成、筆者としてはパフォーマンス的な改善の余地があるのではないかと勘ぐってしまいます。
というのも Envoy へのリクエストドリブンで Mixer に対して問い合わせてチェックを行うとなると、その通信分リクエストの転送処理が遅れるのでは無いかと考えたためです。
HTTP Filter においては Mixer への問い合わせ結果をキャッシュできるのである程度軽減はされるかも知れませんが、このキャッシュのライフタイムを考えるのもややコストが高いようにも思えます。
筆者はまだ本格的に Istio を試したりパフォーマンステストを行ったりなどを行っていないので自信はありませんが、例えば xDS API のようにリクエストの処理とは別のパスで非同期に、 Mixer が提供するアクセス制御やポリシーなどの情報を Envoy に渡せるとよいのではと考えます。

ともかく Istio と Envoy の連携部分に関してはサービスメッシュの構築において重要度が高く、今後の発展や技術的な詳細も追っていきたいですね！

//image[syucream_envoy_istio_mixer][Envoy と Istio Mixer][scale=0.9]

//footnote[istio_proxy][Istio Proxy: https://github.com/istio/proxy]
//footnote[istio_doc_envoy][What is Istio? - Envoy: https://istio.io/docs/concepts/what-is-istio/#envoy]

=== Envoy ソースコードリーティング

今回筆者は本記事を執筆するにあたり、 Envoy のソースコードを部分的に読み解くことを試みました。
折角なので把握した限りの内容を本記事のおまけとして掲載させていただこうと思います。
このおまけの内容はあくまで筆者が読んだ範囲での理解であり、網羅性や正確性を担保できないことと、Envoy-1.7.1 を対象にしていることを予めご了承ください。

まず依存ライブラリについてですが、 Envoy は Boost @<fn>{boost} などの大きめの外部ライブラリをあまり使わず、標準ライブラリと自前での実装でなるべく完結させようとしているようです。
ただしイベント処理については前述の通り libevent に依存しています。また他の大きめの使用ライブラリとしては HTTP/2 処理に nghttp2 @<fn>{nghttp2} を、 HTTP Filter の Lua スクリプティングサポートのため LuaJIT @<fn>{luajit} に依存しています。

ビルドツールとしては Envoy は Bazel @<fn>{bazel} という Google 発のツールを使っているようです。
Bazel の主要な設定ファイルは bazel/ に配置されており、また各種サブディレクトリにも関連する BUILD ファイルが配置されています。
ちなみに前述の Istio Proxy でも Envoy にならって Bazel でビルドするように構築されています。

Envoy の基本的なヘッダファイルは典型的な C++ プロジェクトがそうしているように include/envoy/ に存在します。
ここに格納されているヘッダファイルの内容のクラス名などは本記事で出てくるあるいは設定ファイルでよく使われる用語が多々出てきます。
従ってアーキテクチャや設定ファイルを皮切りにソースコードリーティングをしたければ、ここを最初に眺めるのが良いかと思われます。
また、本書ではレートリミットの制限超過チェックや Istio における Envoy と Mixer の連携部分などに触れてきました。
それらの動作を理解する意味や、それらを参考に自分で Network Filter あるいは HTTP Filter を作りたくなった際などに備えて、 Filter に関するヘッダに目を通しておくのも有用でしょう。

Envoy の実装の多くは source/ に存在します。
その中でも広く使われる機能は common/ に、 main() から開始する処理のエントリポイントに当たる部分は exe/ に、サーバの初期化や設定ファイルの読み込み、ワーカスレッドの起動や停止などサーバとしてのコア実装部分は server/ に存在します。
筆者が読んだ限りで複雑かつ重要だと思われた server/ について更に踏み込みますと、 この中でも Envoy::Server::InstanceImpl クラスにサーバとしてのコアの実装がされています。
このクラスの実装内で本記事で現れた Dispatcher やスレッドローカルストレージ、 worker スレッドを生成する WorkerFactory の制御がされています。
WorkerFactory は worker スレッドの具体的な処理を実装した WorkerImpl クラスのインスタンス生成を行います。
具体的には worker スレッドの持つ Dispatcher を使ってイベントループを実行します。

Dispatcher やスレッドの実装は common/ の方に存在します。
Dispatcher は worker スレッドが処理すべきイベントに対する操作、例えば libevent の evtimer_assign() や event_add() を使ったタイマーイベントの追加などの操作を提供します。
また worker スレッドが event_base_loop() でイベントを待ち受ける処理の実装を持っています。
スレッドは非常にシンプルで、 pthread API の pthread_create(), pthread_join() を呼んでスレッドの開始と終了待ち受け操作を提供します。

時間が限られており、今回の記事の執筆に必要であろうスレッドやイベント処理部分のみを読んだ限りになってしまったため、筆者のソースコードリーティングは以上までとなりました。
あまり整理されていない内容ですが、これから Envoy の動作を深く理解したい方や機能拡張を行ってみたい方に少しでも力になれていれば幸いです。
また筆者としては時間が許されていれば HTTP connection manager や Cluster などコアな機能の周辺や Filter 、 xDS API 関連など外部システムとの連携に使われる機能の実装を読み解くと面白いのではないかと考えています。

//footnote[boost][Boost C++ Libraries: https://www.boost.org/]
//footnote[nghttp2][nghttp2/nghttp2: https://github.com/nghttp2/nghttp2]
//footnote[luajit][The LuaJIT Project: http://luajit.org/]
//footnote[bazel][Bazel - a fast, scalable, multi-language and extensible build system" - Bazel: https://bazel.build/]


== まとめ

マイクロサービスアーキテクチャと Envoy に関する記事、いかがでしたか？
Envoy はマイクロサービスという新しい世界において、サービスメッシュを構築するのを手助けしてくれるプロキシサーバです。
開発の経緯が Lyft でのマイクロサービス前提のサービス提供における課題を軽減するために作られたこともあり、同様の課題に衝突しそうになった場合には有効にはたらいてくれると思われます。
本記事の情報が少しでも何かの助けや今後の参考になれば幸いです。

マイクロサービス化は容易ではなく、 Envoy 含めて新しく学ぶべきことが多く、様々な障壁に衝突するかも知れません。
もし組織やプロダクトにマイクロサービスアーキテクチャがマッチしていると思われるのなら、一緒に困難に向かえる仲間を作り、失敗を恐れず挑戦していく必要があるかと筆者は考えます。

また Envoy は今回紹介しきれなかった運用の手助けになるような機能が他にも充実しています。
これには例えば Hot Restart という処理中のリクエストをドロップすることなく Envoy のリロードを行う機能、 優先度を加味したロードバランシング、 EDS API 以外の xDS API の利用などが挙げられます。
特に Hot Restart は実現するための技術的な取り組みが実装に色濃く反映されているため、もし今回以降マイクロサービスに関する記事を書く機会ができればぜひ取り組んでみたいと考えています。

余談ですが筆者のキャリアは大きめの Web 系企業のインフラ部隊で全社向けのリバースプロキシプラットフォームの開発・運用をすることから開始しています。
その為今回の Envoy の記事の執筆は原点回帰の意味もあり、過去を懐かしみながらマイクロサービスという新しい課題に取り組むという、少々の懐かしさを感じる不思議な作業となりました。

