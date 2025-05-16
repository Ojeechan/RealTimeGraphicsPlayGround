# このプロジェクトについて (日本語補足版)
このプロジェクトはグラフィックスエンジニアになりたいけれどなれていない私が、リアルタイムグラフィックスについて色々なテクニックを試してみるための場所です。

C/C++についてもどんどん書く練習をするための場所です。

リアルタイムグラフィックスを表示するにはどんな要素が必要なのか？という興味に基づき、独学で習得したVulkanを用いて一通り動くところまで実装してみました。

様々な書籍を元に独学中です:
- Real-Time Rendering: https://www.realtimerendering.com/
- Physically Based Rendering (オフライン向け): https://www.pbrt.org/
- Game Engine Architecture: https://www.gameenginebook.com/
- Effective C++シリーズ(C++98/C++03時代からC++11以降まで)
- and more

## 試してみたトピック
- 単純なForward RenderingによるGraphics Pipeline
- Deferred RenderingによるGraphics Pipeline(G-Bufferの設定方法)
- Vulkan Ray TracingによるRay Tracing Pipelineの実装
- GLSLによるシェーディング
   - PBRシェーディング
   - Ray Tracing Pipeline用の各シェーダー
- パイプラインの動的切り替え
	- Graphics Pipeline(Forward Rendering, Deferred Rendering)とRay Tracing Pipelineの切り替え
- Shadow Mapping
- カメラ
- ライト
- Ray Tracing in One Weekendのリアルタイム化
    - 元の実装はオフライン前提かつ簡易的なもの
	- 現時点では、それを純粋に実装することでどれぐらいノイズが出たりFPSが落ちるのか、ということが確認できる程度
- CMakeによるWindows/Linuxのビルド

進行中のプロジェクトのため、アウトプットしてみたいことがまだまだたくさんあります。
- グラフィックスAPIを抽象化するレイヤーを作成して、OpenGLやDirectXにも切り替えられるようにする
- ラスタライズとレイトレーシングを組み合わせたHybrid RenderingやJittered Renderingなど、UEあたりで使用されているレンダリング手法を自前で実装してみる
- USDに対応し、UnrealEditorのように編集モードと実行モードをUSDベースで行えるようにする
- DoFなどのポストエフェクト、PBRシェーディングのブラッシュアップなどシェーダー遊び
- GUI上からライトを編集できるようにする

などなど。

全貌は[README.mdのThings To Explore](README.md#things-to-explore)をご確認ください。

プログラムの土台として既存のチュートリアルを参考にしています:
- Vulkan Tutorial: https://vulkan-tutorial.com/
- NVIDIA Vulkan Ray Tracing Tutorial: https://nvpro-samples.github.io/vk_raytracing_tutorial_KHR
- Ray Tracing in One Weekend: https://raytracing.github.io/books/RayTracingInOneWeekend.html