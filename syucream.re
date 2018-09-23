= Envoy Proxy 入門

== はじめに

こんにちは。 @syu_cream です。普段は Web サービスを陰から支えるインフラ周辺の構築をしています。

よく分からないけどマイクロサービス流行っていますね！
Netflix などの大型サービスの事例を鑑みて、俺も俺もと足を踏み出す方々も結構多いのではないかと見受けられます。
国内でも例えば Cookpad が 2014 年にマイクロサービスを意識し始めたらしいブログ記事 @<fn>{cookpad_techblog} を公開しています。
かくいう筆者も業務上マイクロサービスを意識したアーキテクチャとソフトウェア開発・組織の構築に、まさに揉まれている最中でございます。

本記事ではそんなマイクロサービスの文脈でよく出てくる技術要素の一つであるサービスメッシュに関わる部分と、 Envoy というリバースプロキシについて触れていきます。

マイクロサービスは真面目に取り組むのなら、単にソフトウェア開発の仕方を少し変えるだとかツールを導入する程度では済まない変更が起こりうり、しばしば痛みを伴うものかと筆者は考えています。
本記事が読者の皆様にとっての、今後のマイクロサービス化の奔流を乗り越える道標のひとつとして機能することを願っています。

//footnote[cookpad_techblog][クックパッドとマイクロサービス: https://techlife.cookpad.com/entry/2014/09/08/093000]

== マイクロサービス概要

まずは Envoy の話の前に、簡単に必要最低限のマイクロサービスアーキテクチャに関する説明をします。

まずマイクロサービスの対比として、 @<img>{syucream_monolith_overview} のようなクライアント向け UI とサーバサードアプリケーション、データベースがそれぞれ 1 つずつ存在するモノリスの Web アプリケーションを考えてみましょう。
このような構成は珍しくなく、実現するためのライブラリやフレームワークの充実は今日では十分にされていると言えるでしょう。
しかしながらアプリケーションが実現するビジネスロジックが複雑化し、実装に関わるエンジニアが増員されるにつれて統制を取って開発を進めていくのが難しくなりがちです。
さらにモノリスであるがゆえ、エラーハンドリング漏れや遅延の発生がアプリケーション全体に影響しかねません。

//image[syucream_monolith_overview][モノリスのイメージ][scale=0.7]

これに対するマイクロサービスアーキテクチャですが、 Netflix などの巨大なサービス群を支えるアーキテクチャの思想のひとつです。
具体的には @<img>{syucream_microservices_overview} のようにあるサービスの機能や責任を細分化し、小規模で自律するマイクロサービスを協調動作させて構築します。
各マイクロサービスとしては自身の責任範囲に集中することになります。

//image[syucream_microservices_overview][マイクロサービスのイメージ][scale=0.7]

各マイクロサービスは疎結合ゆえ他機能に非依存な技術選択を行うことができ、たとえばそのマイクロサービスが担う責務を担うのにもっとも適切なプログラミング言語やデータベースを選択できます。
また特定のマイクロサービスのみスケールさせて少数のリソースで提供するサービス全体を安定運用したり、サーキットブレイカーのような仕組みで動作しないマイクロサービスの機能だけ提供を諦めつつ全体のサービスとしては機能を維持する選択もできます。
加えてマイクロサービスの境界を適切に線引きすることで、各々のマイクロサービスの開発・デプロイ・運用を個別のチームで実施することができ、ビジネスをより早く進めることができるかも知れません。
もちろんマイクロサービスアーキテクチャに沿う際にどこまで自由度を与えるか、どこまで細分化するかは議論の余地があります。例えば Netflix では主に Java でマイクロサービスを実装することが多いようで、 Java 実装のマイクロサービス向けの共有ライブラリが少なくとも数年前は存在したようです。

これらの話は理想的であり、マイクロサービスアーキテクチャに舵切りすることでビジネスを成長させてきた企業の成功体験を聞くとそれに従いたくなるかも知れません。
しかしながらマイクロサービスアーキテクチャは、モノリスとして構築するのとは別にマイクロサービス間のメッセージングやサーキットブレイカーなどの共通機能の提供方法、構成が複雑化することによるデバッグや運用の難易度の向上、組織編成になじむかの問題など様々な障壁があると思われます。
筆者としてもマイクロサービスアーキテクチャは馴染まないケースが多々存在し、これに従うにしても組織としてある程度覚悟をしてリスクを承知の上で取り掛かる必要があると考えています。


マイクロサービスアーキテクチャに関して深く知るための資料のひとつとして Martin Fowler 氏の記事 @<fn>{microservices} などの記事が参考になると思われます。

//footnote[microservices][Microservices - Martin Fowler: https://martinfowler.com/articles/microservices.html]


== マイクロサービス、そして Envoy と Istio

マイクロサービスアーキテクチャに従って実世界でサービスを構築していくと、前述したものやそれ以外の複雑な問題に遭遇することかと思います。
そしてそれらの問題を解決するソフトウェアも、世に多く公開されています。
この文脈でもっとも著名なソフトウェアは Kubernetes @<fn>{kubernetes} だと思います。 Kubernetes はインフラを抽象化して、アプリケーションコンテナや周辺システムを制御連携可能にし、コンテナの生死とセフルヒーリングも行ってくれます。
その他マイクロサービス間のメッセージングのために gRPC @<fn>{grpc} とそのエコシステムを使う場面も多いかと思われます。
更に非同期通信のために Apache Kafka @<fn>{apachekafka} などのメッセージキューを使う場面も場合によっては生じるでしょう。

その中でも本記事のテーマである Envoy は、上記とは異なるマイクロサービスの世界における課題を強くバックアップしてくれるソフトウェアです。
ここではまず、 Envoy と、それを利用したソフトウェアである Istio について触れます。

//footnote[kubernetes][Kubernetes: https://kubernetes.io/]
//footnote[grpc][gRPC: https://grpc.io/]
@<comment>{textlint-disable}
//footnote[apachekafka][Apache Kafka: https://kafka.apache.org/]
@<comment>{textlint-enable}

=== Envoy とは

Envoy Proxy @<fn>{envoyproxy} はライドシェアサービスを提供する Lyft によって作られた、マイクロサービスの世界の課題を解決するためのプロキシです。
Envoy は Cloud Native Computing Foundation @<fn>{cncf} のソフトウェアでもあります。

Envoy は C++11 で実装されており、ハイパフォーマンスでマイクロサービスの世界におけるネットワーキングとオブサーバビリティの問題を解決することを目指しています。
具体的に Envoy が持つ機能・特徴は以下のようになります。

 * 省メモリで高性能
 * HTTP/2 と gRPC のサポート (もちろん HTTP/1.1 もサポートしている)
 * 自動リトライ
 * サーキットブレイカー
 * 複数種類のロードバランシング
 * 動的に設定変更を可能にする API の提供
 * 分散トレーシングや特定データベースに特化したオブザーバビリティの提供

この中でも動的に設定変更可能にしている API の存在はユニークであると思われます。
Envoy は複数種類からなる xDS(x Discovery Service) API をサポートしており、外部システムにプロキシとしての振る舞いの設定管理を委ねることができます。
こうした柔軟な設定変更を可能にしているからこそ、マイクロサービス間の通信においてデータのやり取りを担う Data Plane として Envoy を使い、データのやり取りの仕方の管理を担う Control Plane をそれ以外(例えば後述の Istio )に任せる構成が取れます。

また Envoy はコンテナ上で動作させることを想定して開発されており、よくあるパターンとして @<img>{syucream_envoy_sidecar} に示すようなイメージで Kubernetes のアプリケーションコンテナと同じ Pod で動作させる Sidecar コンテナとして使われることがあります。
Envoy の担う機能はアプリケーションとは別コンテナで動作し、通信は gRPC などで行うことで特定のプログラミング言語に依存することなくマイクロサービスの構成に柔軟性を与えることができます。

//image[syucream_envoy_sidecar][Envoy の利用イメージ][scale=0.7]
//footnote[envoyproxy][Envoy Proxy: https://www.envoyproxy.io//]
//footnote[cncf][Cloud Native Computing Foundation: https://www.cncf.io/]

=== Istio とは

Istio @<fn>{istio} は Envoy で Data Plane を提供しつつ Control Plane も別途提供することで、マイクロサービス間のコネクションを変更・制御したり認証認可や暗号化によるセキュリティ担保を行うソフトウェアです。
現状だとターゲットとするインフラとして Kubernetes を前提にしています。

Istio では Envoy を一部拡張して Data Plane を実現するのに使います。
Envoy は Kubernetes でデプロイする Pod 全てに Sidecar として導入され、マイクロサービス間の通信時に Sidecar の Envoy 同士が通信処理を仲介するような動作になります。
マイクロサービスの世界では Pod の生き死にが頻繁に起こり、 Envoy のルーティングの設定を動的に更新できなければなりません。
Istio ではこの設定変更を実現するために Pilot というコンポーネントを持ち、 Envoy に対するルーティングルールを設定して Envoy に伝えるようにしています。

本記事では Envoy に主眼を起きたいため Istio については深く触れません。
詳しく知りたい方は先述の公式ページのリンクを辿ったり、実際の導入事例などを探してみることをおすすめいたします。

//footnote[istio][Istio: https://istio.io/]

== Envoy 詳解

ここからはより Envoy の詳細について提供する機能と使用している技術の側面から掘り下げていこうと思います。

=== Envoy アーキテクチャ概要

Envoy は先述の通りハイパフォーマンスであることを目指しており、モダンなプロキシが取るようなマルチスレッド・イベントドリブンな並列 I/O 処理を実装しています。
Envoy のアーキテクチャの概要を図示したものが @<img>{syucream_envoy_eventhandling} になります。

//image[syucream_envoy_eventhandling][Envoy のイベントハンドリング][scale=0.9]

Envoy のスレッドには役割分担があり、 main() から開始された単一の main スレッドとネットワーク I/O などのイベントを処理する複数の worker スレッドが存在します。
worker スレッドの制御には pthread API を利用しています。 C++11 でサポートが入った std::thread の機能はほとんど使われておりません。
なおこの worker スレッドの数は Envoy のコマンドラインオプション --concurrency で指定可能であり、指定しない場合はハードウェアスレッド数 ( std::thread::hardware_concurrency() から与えられる)分実行されるようです。

またイベント処理に関しては libevent @<fn>{libevent} を使っているようです。
Envoy では思想として 100 ％ノンブロッキングをうたっており、ネットワークやファイルの I/O 、内部的な処理をなるべくイベントドリブンで処理可能にしています。
各ワーカスレッドはそれぞれ libevent でイベントループを回すための、 Envoy 内部で Dispatcher と呼ばれる構造を持ち、これを介してイベントのハンドリングを可能にしています。

Envoy では更にスレッドローカルストレージを抽象化した実装を持ち、スレッド間の共有データを排除してロックなどによるパフォーマンス低下を回避しています。
スレッドローカルストレージでは C++11 からサポートが入った thread_local キーワードを用いて、スレッド毎に割り当てられた記憶領域に任意の動的生成されたオブジェクトを格納できます。どうやらここでは pthread API の pthread_get_specific() などを使っているようではないようです。
またスレッドローカルストレージでは、 slot という main スレッドからイベントループを介して（具体的には Dispatcher がサポートする、 0 秒後にタイマーイベントを発火させるメンバー関数を使って）値の更新が可能な領域も持ちます。

Envoy のアーキテクチャに関してより深く知りたい方は、 Kubecon EU 2018 の資料 @<fn>{kubecon_eu_2018} を参照してみると良いかもしれません！

//footnote[libevent][libevent: https://libevent.org/]
//footnote[kubecon_eu_2018][Kubecon EU 2018: https://speakerdeck.com/mattklein123/kubecon-eu-2018]

=== Envoy のリソース抽象化

Envoy ではネットワーク通信やプロキシ処理における様々なリソースを抽象化しています。
全体像は以下 @<img>{syucream_envoy_resources} の通りになります。ここではイメージを掴みやすいよう、クライアントのリクエストを受け付けてから転送するまでのフローを想定して図示しています。

//image[syucream_envoy_resources][Envoy の各種リソース][scale=0.8]

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

 * HTTP Filter のサポート
 * ルーティング
 * アクセスログの記録
 * トレーシングのためのリクエスト ID 発行
 * リクエスト・レスポンスヘッダの修正

HTTP Filter というのは Network Filter の HTTP 版であるようなイメージを浮かべていただけるといいと思います。
HTTP Filter として標準でサポートされている機能も多々あり、バッファリングや gzip 圧縮など nginx などの他のプロキシ実装でも広く存在するものや、 gRPC-HTTP/1.1 bridge など gRPC のサポートを厚くしている Envoy の特色が出ているものなど多岐にわたります。
また HTTP Filter では Lua スクリプトによる機能拡張もサポートされています。

ルーティングは HTTP リクエストに対して適切な upstream Cluster を決定してリクエストを転送する機能を提供します。
それの伴いバーチャルホストの提供やホストの書き換え、リトライ、優先度の解釈などの機能も提供します。

==== Cluster

Envoy におけるプロキシ先の upstream ホストをグループ化したものです。
upstream ホストはヘルスチェックされ生死判定をされ、 Envoy が実際に転送処理を行う際は生きている upstream ホストに対して、ロードバランシングポリシーを加味して転送先を決定することになります。

ちなみに Envoy が転送処理を行う際に upstream Cluster の host を探す必要があるのですが、これを service discovery と呼びます。

=== Envoy の特徴的な機能説明

Envoy には先に挙げたようなユニークな機能がいくつか存在します。
ここでは xDS API と分散トレーシングに関して深掘りしてみようと思います。

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
実際に xDS API を利用して、 Cookpad では Istio より小規模な自作の Control Plane を構築 @<fn>{servicemesh_and_cookpad} するなど活用しているようです。

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

Logical DNS は Strict DNS と似た DNS を使った方法なのですが、 upstream host にコネクションを張るにあたり複数 IP アドレスが返却されてもその中の最初の IP アドレスのみを用います。これは DNS ラウンドロビンなど Envoy 以外で負荷分散することを考える際に有用で、 Envoy 公式のドキュメントとしては大規模な Web サービスと通信する際は Logical DNS を使うと良いような記述があります。

Original Destination は iptables の REDIRECT または TPROXY ターゲットを使って、あるいは Proxy Protocol を伴って、 Envoy にリクエストがリダイレクトされた際の upstream host の解決方法です。この場合 Envoy はクライアントが送りたいオリジナルの送信先を upstream host として解決してくれます。 Original Destination は HTTP レベルでも使用することができ、x-envoy-orignal-dst-host ヘッダの値に upstream host として扱う IP アドレスとポート番号を指定できます。

最後に EDS の場合は EDS API を使って upstream host を解決できます。 EDS API を自前で実装することでより柔軟な service discovery が実現でき、また独自のロードバランシングの仕組みも組み込むことができるでしょう。

==== トレーシング

Envoy が提供する大きな機能であり、マイクロサービスの世界で問題になることとしてオブザーバビリティが挙げられます。
例えばこれが 1 つのモノリスで構築されたシステムであれば開発時にデバッグをしたければ従来のデバッガを使ったり、静的解析してコールグラフを出したりスタックトレースを出力してみたりすることができます。
しかし境界を明確にして自立分散して動作するマイクロサービスの世界では、従来は出来ていたこれらも対応するのが困難になります。
代表的な問題として、多量のマイクロサービスが存在してそれらを連携する際に、あるリクエストがどのような経路を辿って関連しているのか紐づけが難しいことが挙げられます。

Envoy ではこの関連付けを行ってオブザーバビリティを向上するための幾つかの仕組みが提供されています。
この仕組みは Envoy では HTTP connection manager によって提供されます。

 * リクエスト ID の生成
 * LightStep や Zipkin のようなトレーシングサービスとの連携
 * クライアントトレース ID の結合

Envoy のトレーシングのための ID 発行・伝搬イメージは @<img>{syucream_envoy_tracing} の通りです。

//image[syucream_envoy_tracing][Envoy のトレーシング][scale=0.7]

リクエスト ID として Envoy では x-request-id HTTP ヘッダを伝搬し、また必要であれば UUID を生成してヘッダの値として付与してくれます。
このリクエスト ID をログに記録しておくことで、後で複数のマイクロサービスのログを x-request-id で突き合わせてリクエストのフローを確認することができます。
またスマホアプリなどサーバサイドの外側のクライアントを含めたトレーシングを可能にするため、 x-client-trace-id HTTP ヘッダが付与されていた場合にはその値を x-request-id に追記してくれます。

Envoy にトレーシングを要求する方法は幾つか存在し、まず先述の x-client-trace-id または x-envoy-force-trace HTTP ヘッダが付与されている場合行ってくれます。
その他にも random_sampling ランタイム設定で指定された値に従ってランダムにトレーシングを行ってくれます。

==== サーキットブレイカー

分散システムにおいてしばしば障害点を切り離してシステム全体の動作を維持したり遅延を低減させるため、サーキットブレイカーを導入することがあると思われます。
サーキットブレイカーのロジックをアプリケーションに含ませるのは手軽ではありますが、やはり個別のプログラミング言語で個々に実装していく必要があります。
Envoy でサーキットブレイカーをサポートすることにより、これらの問題を軽減することができます。

Envoy では HTTP connection manager により以下に上げるような複数種類のサーキットブレイカーを提供します。

 * HTTP/1.1 Cluster 向け
 ** Cluster への最大コネクション数
 ** Cluster への最大未処理コネクション数
 * HTTP/2 Cluster 向け
 ** Cluster への最大リクエスト数
 * 共通
 ** Cluster への最大リトライ数

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
しかし逆に機能が少ないゆえにターゲットとする課題を解くのに最低限のことができるものと思われます。
加えて Envoy では xDS API の提供により最初から動的に設定を変更していくことをサポートしている点も、大きな差分になるとも考えられます。

現状だと Istio との統合も考えると Envoy の今後が期待できるところですが、最近は nginx で gRPC のサポートが入ったり引き続き活発な開発が続いていくと、解決したい問題によって使用するプロキシを選択してサービスメッシュを構築できる未来が訪れるのかも知れないと期待しています。


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
    type: LOGICAL_DNS
    dns_lookup_family: V4_ONLY
    lb_policy: ROUND_ROBIN
    hosts:
      - socket_address:
        address: google.com
        port_value: 443
    tls_context: { sni: www.google.com }
//}

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
EDS API は今回 REST API と提供するのでその指定と、 Envoy が EDS API を参照しにいく頻度を指定しておきます。

//source[etc/envoy/envoy.yaml]{
...
static_resources:
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
  - name: eds_cluster
    type: LOGICAL_DNS
    connect_timeout: 0.25s
    dns_lookup_family: V4_ONLY
    hosts:
      - socket_address:
          address: httpxds
          port_value: 8080
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
      /usr/local/bin/envoy --service-cluster cluster0 --service-node envoy0 -c /etc/envoy/envoy.yaml

  httpxds:
    container_name: httpxds
    image: nginx
    volumes:
      - ./etc/httpxds/eds.json:/usr/share/nginx/json/eds.json
      - ./etc/httpxds/xds.conf:/etc/nginx/conf.d/xds.conf:ro
    networks:
      - app_net

  endpoint0:
    container_name: endpoint0
    image: nginx
    volumes:
      - ./etc/endpoint0/whoami.html:/usr/share/nginx/html/whoami.html:ro
    networks:
      app_net:
        ipv4_address: 172.16.238.10

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

EDS API で返却する endpoint の情報の更新が Envoy にも伝わっていることが見て取れます。

//footnote[envoy_simple_example][syucream/envoy-simple-example: https://github.com/syucream/envoy-simple-example]

== おまけ: Envoy ソースコードリーティング

今回筆者は本記事を執筆するにあたり、 Envoy のソースコードを部分的に読み解くことを試みました。
折角なので把握した限りの内容を本記事のおまけとして掲載させていただこうと思います。
このおまけの内容はあくまで筆者が読んだ範囲での理解であり、網羅性や正確性を担保できないことと、Envoy-1.7.1 を対象にしていることを予めご了承ください。

まず依存ライブラリについてですが、 Envoy は Boost などの大きめの外部ライブラリをあまり使わず、標準ライブラリと自前での実装でなるべく完結させようとしているようです。
ただしイベント処理については前述の通り libevent に依存しています。また他の大きめの使用ライブラリとしては HTTP/2 処理に nghttp2 を、 HTTP Filter の Lua スクリプティングサポートのため LuaJIT に依存しています。

Envoy の基本的なヘッダファイルは典型的な C++ プロジェクトがそうしているように include/envoy/ に存在します。
ここに格納されているヘッダファイルの内容のクラス名などは本記事で出てくるあるいは設定ファイルでよく使われる用語が多々出てきます。
従ってアーキテクチャや設定ファイルを皮切りにソースコードリーティングをしたければ、ここを最初に眺めるのが良いかと思われます。

Envoy の実装の多くは source/ に存在します。
その中でも広く使われる機能は common/ に、 main() から開始する処理のエントリポイントに当たる部分は exe/ に、サーバの初期化や設定ファイルの読み込み、ワーカスレッドの起動や停止などサーバとしてのコア実装部分は server/ に存在します。
筆者が読んだ限りで複雑かつ重要である server/ について更に踏み込みますと、 この中でも Envoy::Server::InstanceImpl がサーバとしてのコアの実装がされています。
このクラスの実装内で本記事で現れた Dispatcher やスレッドローカルストレージ、 worker スレッドを生成する WorkerFactory の制御がされています。
WorkerFactory は WorkerImpl のインスタンス生成を行い、 worker スレッドの処理はこのクラスが担います。具体的には worker スレッドの持つ Dispatcher を使ってイベントループを実行します。

Dispatcher やスレッドの実装は common/ の方に存在します。
Dispatcher は worker スレッドが処理すべきイベントに対する操作を提供し、また worker スレッドが実行するイベントループ実行の実装を持っています。
スレッドは非常にシンプルで、 pthread API の pthread_create(), pthread_join() を呼んでスレッドの開始と終了待ち受け操作を提供します。

時間が限られていたため、記事の執筆に必要であろうスレッドやイベント処理部分のみを読んだ限りになってしまったため、筆者のソースコードリーティングは以上までとなりました。
時間が許されていれば HTTP connection manager や Cluster 、 xDS API 関連の実装を読み解くと面白いのではないかと考えています。


== まとめ

マイクロサービスアーキテクチャと Envoy に関する記事、いかがでしたか？
少しでも何かの助けや今後の参考になれば幸いです。
マイクロサービスは容易ではなく、ビジネスの複雑さによって難易度も大きく異なってくるかと思います。
また Envoy 含めて新しく学ぶべきことが多く、組織を巻き込む上で様々な問題にも衝突するかも知れません。
月並ですが、もしその組織やプロダクトにマイクロサービスアーキテクチャがマッチしていると思われるのなら、一緒に困難に向かえる仲間を作り、失敗を恐れず挑戦していく必要があるかと筆者は考えます。

余談ですが、筆者のキャリアは大きめの Web 系企業のインフラ部隊で全社向けリバースプロキシプラットフォームの開発・運用をすることから開始しています。
その為今回の Envoy というリバースプロキシの記事の執筆は原点回帰の意味もあり、過去を懐かしみながらマイクロサービスという新しい課題に取り組むという、時間的ギャップを感じる作業でした。

