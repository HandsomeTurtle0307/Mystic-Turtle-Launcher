![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=c%2B%2B)
![Qt](https://img.shields.io/badge/Qt-6.11.2-41CD52?logo=qt)
![CMake](https://img.shields.io/badge/CMake-3.19%2B-064F8C?logo=cmake)
![Minecraft](https://img.shields.io/badge/Minecraft-Launcher-62B47A?logo=minecraft)
![Status](https://img.shields.io/badge/Status-Active-brightgreen)

# MysticTurtle Launcher

MysticTurtle 是一个基于 **Qt 6 / C++17 / CMake** 的 Minecraft 启动器。采用GNU General Public License v3.0许可证。

> **非官方声明**：MysticTurtle 是独立开发的第三方 Minecraft 启动器，与 Mojang Studios、Microsoft 及其关联公司没有隶属、赞助、授权或认可关系。Minecraft 是 Mojang Studios 的商标。本项目不存储、不打包、不分发 Minecraft 游戏本体或资源文件；游戏文件仅由用户在本机通过官方或第三方镜像下载。请支持正版游戏，并遵守 [Minecraft EULA](https://www.minecraft.net/en-us/eula) 与 [使用指南](https://www.minecraft.net/en-us/usage-guidelines)。

## 文档导航

- [项目简介](#mysticturtle-launcher)
- [一些你需要知道的](#一些你需要知道的)
- [下载功能](#下载功能)
- [构建](#构建)
- [下载原理](#下载原理)
- [第三方鸣谢与声明](#第三方鸣谢与声明)
- [许可证](#许可证)

## 一些你需要知道的

> 此启动器的初衷是便于玩家体验游戏，游戏内的内容和mod作者、整合包作者、数据包作者以及服务器运营商相关，和启动器无关。如果你对游戏内容存在问题，请寻找相关责任方，而不是启动器。本应用由作者本人和DeepSeek Harness合力完成（大肥鱼！）。对于用户使用启动器在游玩游戏过程中，产生的任何纠纷，均和启动器无关，请广大玩家自行承担因自己的游戏行为而造成的任何后果。本项目为纯技术开源的本地工具，严格遵守GPL-3.0协议，不提供任何游戏本体的存储与分发，也不包含任何商业推广和充值入口，所包含功能均由API调用实现，请广大玩家支持正版游戏。对于如何实现下载功能，详细请见下方**下载原理**板块。

> 由于作者还是学生，并且心理状况有些欠佳，不能保证此项目可以持续开发，请大家见谅啦ovo~

## 为啥要做这个

我本人是个MC的老玩家（嗯嗯，是的），为了让大家能简便的体验MC，便开发了MysticTurtle Launcher，方便广大玩家体验游戏。

## 亮点

图形化界面，操作简便（部分设计灵感来自于PCL）

支持国内镜像源加速

界面操作丝滑

（额额，可能还有更多的功能，可是我现在想不起来了呜呜呜，就先写到这里了，可以查看最新版本的Release页面哦！）

## 一些声明

除了开篇说到的，还有一些常见问题。由于Microsoft Defender神奇的检测机制，这个启动器会被Microsoft Defender报毒（事实上，大部分我的世界启动器，譬如PCL和HMCL都会出现这个问题）。当你看到这个页面，不用理它就行。

由于我只是个学生，所以做的软件肯定还有很多不完备的地方，欢迎广大用户在Issues中提出意见，我一定会仔细考虑的！谢谢大家啦~

> 下面是一些针对于广大开发人员的说明，普通用户可以不用看下面哦~

---

## 下载功能

下载游戏页中的版本卡片和精选卡片可以执行下载：

1. 从 `piston-meta.mojang.com` 获取官方版本清单
2. 下载并 SHA1 校验所选版本的版本 JSON
3. 使用 `VersionParser` 解析客户端、依赖库、Windows natives 和资源索引
4. 使用 `DownloadManager` 以最多 8 个并发下载文件
5. 通过镜像加速与 SHA1 校验保证文件完整

默认下载目录为 `%APPDATA%\.minecraft`，可在主页右侧通过「游戏目录」修改。下载会包含版本 JSON、客户端、依赖库、natives、资源索引和资源对象。

## 下载模块基础类

`DownloadTask` 是下载模块的基础类，已通过 `DownloadManager` 接入下载游戏页：

- 自带工作线程，使用 `QNetworkAccessManager` 异步 GET
- 构造参数：下载 URL、本地绝对路径、预期 SHA1
- 信号：`progressChanged(int)`、`bytesProgressChanged(qint64, qint64)`、`succeeded(QString)`、`failed(QString)`、`finished()`
- 下载完成后计算 SHA1；不匹配会删除文件并发出 `failed`
- 网络/HTTP 5xx 错误自动重试 3 次，单次传输超时 30 秒
- SHA1 不匹配和本地文件写入错误不会重试

```cpp
auto *task = new DownloadTask(
    QUrl("https://example.com/file.jar"),
    "D:/Games/Minecraft/mods/example.jar",
    "0123456789abcdef0123456789abcdef01234567",
    this);

connect(task, &DownloadTask::progressChanged, this, [](int percent) {
    // percent: 0 - 100
});
connect(task, &DownloadTask::succeeded, this, [](const QString &filePath) {
    // 校验通过，文件位于 filePath
});
connect(task, &DownloadTask::failed, this, [](const QString &reason) {
    // 下载失败或 SHA1 校验失败
});
connect(task, &DownloadTask::finished, task, &QObject::deleteLater);
task->start();
```

实现文件：[DownloadTask.h](src/network/DownloadTask.h)、[DownloadTask.cpp](src/network/DownloadTask.cpp)。

## 下载队列管理

`DownloadManager` 负责调度多个 `DownloadTask`：

- 接收任务列表并接管任务所有权
- 默认最多同时运行 8 个任务，可通过 `setMaxConcurrent(1..8)` 下调
- 任意任务成功或失败后，自动启动队列中的下一个任务
- `overallProgress(completedTasks, totalTasks)`：已完成任务数 / 总数
- `overallPercentChanged(percent)`：按任务平均得到的平滑整体百分比
- 同时提供 `taskStarted`、`taskProgressChanged`、`taskSucceeded`、`taskFailed`、`finished`

> Qt 的 `QObject` 不可拷贝，因此任务列表使用 `QList<DownloadTask *>`，不能使用 `QList<DownloadTask>` 值列表。

```cpp
auto *manager = new DownloadManager(this);
connect(manager, &DownloadManager::overallProgress, this,
        [](int completed, int total) {
            // progressBar->setRange(0, total);
            // progressBar->setValue(completed);
        });
connect(manager, &DownloadManager::overallPercentChanged, this,
        [](int percent) { /* 可用于平滑进度条 */ });
connect(manager, &DownloadManager::finished, manager, &QObject::deleteLater);

QList<DownloadTask *> tasks;
tasks << new DownloadTask(url1, path1, sha1_1)
      << new DownloadTask(url2, path2, sha1_2);
manager->start(tasks);
```

实现文件：[DownloadManager.h](src/network/DownloadManager.h)、[DownloadManager.cpp](src/network/DownloadManager.cpp)。

## 国内镜像加速

`DownloadManager::getOptimizedUrl(QString)` 会把以下官方 CDN 域名替换为 BMCLAPI：

- `launcher.mojang.com`
- `piston-data.mojang.com`

示例：

```text
https://launcher.mojang.com/v1/objects/<hash>/client.jar
→ https://bmclapi2.bangbang93.com/v1/objects/<hash>/client.jar
```

`DownloadTask` 会自动对上述域名的 URL 使用镜像地址。若镜像连续超时 2 次，会自动切换回原始官方 URL，并继续执行重试逻辑。其他域名不受影响。

## 版本 JSON 解析

`VersionParser` 用于解析 Minecraft 的版本 JSON：

- 支持从文件路径或原始 JSON 字符串读取
- 提取 `client` 的下载链接、SHA1，以及本地保存路径
- 提取 `assetIndex` 的下载链接与 SHA1
- 遍历 `libraries`：
  - 按当前系统的 `rules` 过滤
  - 保留普通 Java library 的 `downloads.artifact`
  - 对存在 `natives.windows` 的库，额外提取对应的 Windows native classifier
- 返回相对 `.minecraft` 的保存路径（统一使用 `/`）

```cpp
VersionParser parser;
QString error;
if (!parser.parseFile("D:/Games/Minecraft/versions/1.21.4/1.21.4.json", &error)) {
    // error 包含具体原因
}

const auto &version = parser.version();
// version.client.url / version.client.sha1
// version.assetIndex.url

for (const VersionParser::Library &library : parser.libraries()) {
    // library.download.url / library.download.sha1
    // library.download.relativePath
    // library.isNative() 可区分 Windows native jar
}
```

标准保存路径示例（相对于 `.minecraft`）：

| 内容              | 保存路径                                                          |
| ----------------- | ----------------------------------------------------------------- |
| 客户端 JAR        | `versions/1.21.4/1.21.4.jar`                                      |
| 资源索引          | `assets/indexes/17.json`                                          |
| 普通 Java 库      | `libraries/org/lwjgl/lwjgl/3.3.3/lwjgl-3.3.3.jar`                 |
| Windows native 库 | `libraries/org/lwjgl/lwjgl/3.3.3/lwjgl-3.3.3-natives-windows.jar` |

Windows native 库先按上表路径下载并缓存；启动阶段再把其中的 DLL 解压到该版本自己的 natives 目录。

实现文件：[VersionParser.h](src/minecraft/VersionParser.h)、[VersionParser.cpp](src/minecraft/VersionParser.cpp)。

## 构建

需要安装 **Qt 6.11.2 MinGW 64-bit** 套件（包含 Qt Widgets / Gui / Core / Network）。
项目使用 **CMake + MinGW Makefiles**，编译器为 `g++` / `gcc`。

### Qt Creator

1. 用 Qt Creator 打开根目录的 `CMakeLists.txt`
2. 选择 **Desktop Qt 6.11.2 MinGW 64-bit** 套件
3. 构建并运行 `Mystic-Turtle-Launcher`

### 命令行

仓库内附带了一个固定使用 MinGW Makefiles 的构建脚本：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1
```

脚本默认使用：

- Qt：`D:\Qt\6.11.2\mingw_64`
- MinGW：`D:\Qt\Tools\mingw1310_64`
- CMake：`D:\Qt\Tools\CMake_64\bin\cmake.exe`
- 构建目录：`build-mingw`

如安装路径不同，修改脚本顶部的参数即可。也可以手动执行：

```powershell
$env:PATH = "D:\Qt\Tools\mingw1310_64\bin;D:\Qt\Tools\CMake_64\bin;$env:PATH"

cmake -S . -B build-mingw `
  -G "MinGW Makefiles" `
  -DCMAKE_BUILD_TYPE=Debug `
  -DCMAKE_PREFIX_PATH="D:\Qt\6.11.2\mingw_64"

cmake --build build-mingw --parallel
.\build-mingw\Mystic-Turtle-Launcher.exe
```

如果直接双击运行，需要先让 Qt 动态库可被找到；可在 Qt 命令行环境中运行，
或对可执行文件执行 `windeployqt`。

## 目录结构

```
CMakeLists.txt
main.cpp
mainwindow.h/.cpp            窗口外壳、Acrylic、窗口状态与浮层
src/
  Theme.*                    配色、字体、全局控件样式
  IconFactory.*              矢量图标绘制
  SampleData.*               本地版本 / 模组 / 商店 UI 示例数据
  account/AccountManager.*   多本地离线账户、当前启动账户与持久化
  account/AccountDialog.*    账户列表、添加、改名、删除与切换
  network/DownloadTask.*     多线程下载、SHA1 校验与失败重试
  network/DownloadManager.*  8 并发下载队列与整体进度
  network/MojangMirror.*     Mojang CDN 到 BMCLAPI 的 URL 转换
  minecraft/VersionParser.*  版本 JSON 解析
  minecraft/GameInstaller.*  版本基础文件下载流程
  minecraft/GameLauncher.*   版本 JSON 合并、Java 选择、natives 解压与启动进程
  minecraft/JavaRuntimeInstaller.* Adoptium JDK 下载、解压与路径保存
  minecraft/LoaderDownloader.* 加载器依赖 / Fabric API / 安装器下载
  minecraft/VersionManifest.* 官方完整版本清单获取与分类
  minecraft/LocalVersionScanner.*  本地版本与模组扫描
  platform/WindowEffects.*   Windows DWM / Acrylic 平台层
  widgets/                   无边框标题栏、导航、动画容器、卡片、抽屉等
  pages/                     主页 / 下载游戏 / 加载器选择 / 下载模组 / 设置
```

--- 

## 下载原理：

### 1. 获取版本清单

`DownloadGamePage` 用 `VersionManifestClient` 请求：

```text
https://piston-meta.mojang.com/mc/game/version_manifest_v2.json
```

如果当前下载源是 BMCLAPI，则优先使用镜像清单；失败时回退另一来源。

清单里每个版本包含：

- 版本号
- 版本类型
- 版本 JSON 地址
- 版本 JSON 的 SHA1

### 2. 下载版本 JSON

点击安装并选择加载器后，`GameInstaller::installFromManifest()` 下载：

```text
versions/<版本号>/<版本号>.json
```

下载完成后用 `VersionParser` 解析。

### 3. 解析需要下载的文件

版本 JSON 里主要有：

- `downloads.client`：客户端 JAR
- `assetIndex`：资源索引
- `libraries`：依赖库和 natives

解析时会处理：

- Windows / Linux / macOS 规则
- x86 / x64 / arm64 架构规则
- 旧版缺少 `downloads` 时的 Maven 路径
- natives classifier 的架构替换
- 依赖库本地相对路径

最终得到每个文件的：

- 下载 URL
- 本地保存路径
- SHA1（如果有）
- 是否需要下载

### 4. 下载基础文件

`GameInstaller` 把这些文件交给 `DownloadManager`：

- 客户端 JAR
- 资源索引
- 依赖库 JAR
- Windows natives JAR

`DownloadManager` 默认最多同时下载 8 个任务。每个 `DownloadTask`：

- 有自己的下载线程和网络管理器
- 写文件前创建目录
- 支持超时、失败重试
- 有 SHA1 时校验
- 根据下载源设置自动选择官方地址或 BMCLAPI
- 镜像失败时尝试回退官方地址

对应路径：

```text
libraries/
versions/<版本号>/<版本号>.jar
assets/indexes/
```

### 5. 下载资源对象

基础文件下载完后，`GameInstaller::downloadAssets()` 会读取资源索引：

```text
assets/indexes/<资源索引 ID>.json
```

里面的 `objects` 记录每个资源的：

- hash
- size

然后下载缺失资源到：

```text
assets/objects/<hash 前两位>/<hash>
```

已经存在且大小匹配的文件会跳过；下载后使用 SHA1 校验。

### 6. 下载加载器

基础文件和资源完成后，如果选择了加载器：

- **Fabric**
  - 获取 Fabric profile
  - 下载加载器依赖库
  - 下载 Fabric API 到 `mods`
  - 生成独立版本配置
- **Forge / NeoForge**
  - 下载官方安装器到 `installers`
  - 自动创建临时用户目录
  - 调用官方安装器
  - 检查是否生成新的版本配置
- **OptiFine**
  - 目前只下载安装器，需要手动运行

### 7. 进度显示

- 总进度按所有任务的平均进度汇总
- 最后一个任务完成前最多显示 99%
- 显示实时聚合下载速度
- 下载完成后进入加载器安装阶段
- 启动游戏时另有一个启动准备进度条

### 8. 当前实现的特点

- 不做整包校验，而是按单文件 SHA1 校验
- 不实现断点续传，失败文件会重新下载
- 已存在的正确资源会跳过
- 适用于新下载的版本；旧版本不会自动补齐缺失资源
- 加载器接口和游戏文件源是分开的，下载源设置主要影响 Minecraft 本体文件

核心实现集中在：

- [DownloadGamePage.cpp](src/pages/DownloadGamePage.cpp)
- [GameInstaller.cpp](src/minecraft/GameInstaller.cpp)
- [VersionParser.cpp](src/minecraft/VersionParser.cpp)
- [DownloadManager.cpp](src/network/DownloadManager.cpp)
- [DownloadTask.cpp](src/network/DownloadTask.cpp)
- [LoaderDownloader.cpp](src/minecraft/LoaderDownloader.cpp)
- [MojangMirror.cpp](src/network/MojangMirror.cpp)

---

## 第三方鸣谢与声明

MysticTurtle 的实现依赖 Mojang 的公开元数据、第三方镜像、开源框架和社区项目。以下名称、商标和版权归各自权利人所有，仅用于说明用途和致谢，不代表任何隶属、赞助或背书关系。

| 项目                                                | 在 MysticTurtle 中的用途                     | 一些说明                                                                                                                                              |
| --------------------------------------------------- | -------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------- |
| [Mojang Studios](https://www.minecraft.net/)        | Minecraft 版本清单、客户端、依赖库和资源文件 | Minecraft 及其相关内容的版权和商标归 Mojang Studios / Microsoft 所有；本项目不存储或重新分发游戏本体。                                                |
| [BMCLAPI](https://bmclapidoc.bangbang93.com/)       | 国内下载镜像                                 | 由 bangbang93 及贡献者维护的第三方镜像，不是 Mojang 官方服务。感谢 BMCLAPI 为国内玩家提供加速；使用时应遵守其接口说明和频率限制。                     |
| [Qt 6](https://www.qt.io/)                          | 图形界面、网络与跨平台基础库                 | Qt 开源版本按其 [LGPLv3/GPLv3 许可](https://www.qt.io/licensing/open-source-lgpl-obligations) 使用。本项目以 GPLv3 发布，并保留 Qt 的版权与许可声明。 |
| [Eclipse Adoptium / Temurin](https://adoptium.net/) | 自动下载并解压 Java 运行时                   | JDK 由 Eclipse Adoptium 提供，遵循其 GPLv2 + Classpath Exception 等许可；本项目不随附或重新分发 JDK。                                                 |
| [Fabric](https://fabricmc.net/)                     | Fabric Loader、Fabric Profile 与 Fabric API  | 版权归 FabricMC 及其贡献者所有，遵循各自项目许可证。                                                                                                  |
| [MinecraftForge](https://files.minecraftforge.net/) | Forge 版本列表与官方安装器                   | 版权归 MinecraftForge 团队及其贡献者所有；本项目调用官方安装器完成安装，不重新分发安装器。                                                            |
| [NeoForge](https://neoforged.net/)                  | NeoForge 版本列表与官方安装器                | 版权归 NeoForged 团队及其贡献者所有；本项目调用官方安装器完成安装，不重新分发安装器。                                                                 |
| [OptiFine](https://optifine.net/)                   | OptiFine 安装器下载                          | 版权归 OptiFine 作者 sp614x 及相关权利人所有；本项目仅下载安装器，需用户手动运行，不重新分发。                                                        |
| [Modrinth](https://modrinth.com/)                   | Fabric API 等 Mod 下载接口                   | 版权归 Modrinth 团队及相应 Mod 作者所有；下载内容遵循各 Mod 自身许可证。                                                                              |
| [PCL](https://github.com/Meloong-Git/PCL)           | 界面设计灵感                                 | 感谢 PCL 项目及社区提供的设计参考；本项目与其没有隶属关系。                                                                                           |
| DeepSeek Harness                                    | 开发协作                                     | 感谢大肥鱼（DeepSeek Harness）在开发过程中的协助！                                                                                                    |

### Minecraft 非官方声明

MysticTurtle 不是 Mojang Studios 或 Microsoft 的官方产品，也未获得其授权、赞助或认可。Minecraft、Mojang 及相关标识是 Mojang Studios 的商标或注册商标。本项目仅提供本地游戏管理、下载和启动工具，不提供游戏账号、正版验证绕过或在线服务器认证绕过。使用本项目下载或游玩游戏时，请确保你拥有合法的游戏副本，并遵守 Minecraft EULA、使用指南以及所连接服务器的规则。

### 第三方许可证

第三方组件、加载器、Mod、游戏文件和镜像服务分别遵循其自身许可证。本项目的 GPLv3 许可证不改变这些第三方内容的授权条件，也不对其作出任何额外保证。

## 许可证

本项目源代码采用 **GNU General Public License v3.0 or later（GPL-3.0-or-later）** 发布，完整许可证文本见 [LICENSE](LICENSE)。

你可以按照 GPLv3 的条款使用、修改和分发本项目源代码；分发修改版本时，请保留版权与许可证声明，并按 GPLv3 提供相应的完整源代码。
