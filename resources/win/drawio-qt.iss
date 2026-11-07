; drawio-qt — Windows 安装包脚本（Inno Setup 6.5+）
;
; 本地编译（在仓库根目录，先跑过 windeployqt 那套打包流程，暂存目录里要有
; 主程序 + Qt 运行时）：
;   set /p MyVer=<VERSION
;   iscc /DMyAppVersion=%MyVer% /DMyArch=x64 /DMyArchAllowed=x64compatible ^
;        /DMySourceDir=%CD%\dist\drawio-qt-%MyVer%-windows-x64 ^
;        /DMyOutputDir=%CD%\dist /DMyRepoRoot=%CD% ^
;        /DMyChineseIsl=%CD%\resources\win\ChineseSimplified.isl ^
;        /DMyIconFile=%CD%\resources\win\drawio-qt.ico ^
;        resources\win\drawio-qt.iss
;
; CI 的调用方式见 .github/workflows/release.yaml 的「Build the installer」步骤。
;
; 说明：源目录（MySourceDir）就是便携包 zip 打包的那棵暂存树——两者内容完全一致，
; 安装包只是把「解压到某个目录」换成「装到 Program Files 并建快捷方式」。
; Windows 产物有 zip 与 setup.exe 两种形态：zip 解压即用，setup.exe 走本脚本。
;
; 本目录里的 ChineseSimplified.isl 是社区维护的简体中文语言包
; （jrsoftware 的 istrans 收集，维护者 Zhenghan Yang <847320916@qq.com>，
;   https://github.com/kira-96/Inno-Setup-Chinese-Simplified-Translation），
; 按原样随仓库分发，要求 Inno Setup 6.5.0+。
;
; drawio-qt.ico 由 resources/icons/app.png 生成（16/24/32/48/64/128/256 全 PNG 帧）。
; exe 自身的 PE 图标来自 resources/win/drawio-qt.rc，与本脚本是同一份 ico；
; 这里仍随包装进 {app}，供安装程序、快捷方式与卸载项使用。

#ifndef MyAppVersion
  #define MyAppVersion "0.1.0"
#endif

; MyArch 只用于产物文件名；MyArchAllowed 是 Inno 的目标架构约束。
; 两者分开是因为文件名想写 x64，而 Inno 的约束值要写 x64compatible（6.3+）。
#ifndef MyArch
  #define MyArch "x64"
#endif
#ifndef MyArchAllowed
  #define MyArchAllowed "x64compatible"
#endif

#ifndef MySourceDir
  #define MySourceDir "..\..\dist\drawio-qt-" + MyAppVersion + "-windows-x64"
#endif
#ifndef MyOutputDir
  #define MyOutputDir "..\..\dist"
#endif
; 仓库根目录，用来取 LICENSE（相对本脚本所在目录）。
#ifndef MyRepoRoot
  #define MyRepoRoot "..\.."
#endif
; 中文翻译文件路径。ISPP 的 SourcePath 本身就是**脚本所在目录**（不是含文件名的
; 完整路径），这里补一个反斜杠再拼文件名。脚本用相对路径调用 ISCC 时 SourcePath
; 也可能是相对路径，那种情况请按上面命令行显式传绝对路径。
#ifndef MyChineseIsl
  #define MyChineseIsl AddBackslash(SourcePath) + "ChineseSimplified.isl"
#endif
; 安装程序图标；默认取本脚本同目录下的 ico。
#ifndef MyIconFile
  #define MyIconFile AddBackslash(SourcePath) + "drawio-qt.ico"
#endif

; 显示名与可执行文件名分开：程序叫 drawio-qt，进程与安装目录同名。
#define MyAppName "drawio-qt"
#define MyAppExeName "drawio-qt.exe"
#define MyAppPublisher "lhdjply"
#define MyAppUrl "https://github.com/lhdjply/drawio-qt"

[Setup]
; AppId 一旦发布就不要再改：升级安装与卸载识别都靠它。
; 这个 GUID 是 drawio-qt 自己的，不能与其他项目复用（否则两个软件会互相覆盖安装）。
AppId={{7C2A1F5E-3B64-4D89-9E01-A5F6C8D2B730}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppUrl}
AppSupportURL={#MyAppUrl}
AppUpdatesURL={#MyAppUrl}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
AllowNoIcons=yes
; 默认按用户安装（装到 %LOCALAPPDATA%\Programs，不弹 UAC）。程序的配置都走
; QSettings 的用户目录，不写安装目录，所以装到哪都不影响使用；
; 需要全机器安装时可在向导里切换。
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed={#MyArchAllowed}
ArchitecturesInstallIn64BitMode={#MyArchAllowed}
OutputDir={#MyOutputDir}
OutputBaseFilename=drawio-qt-{#MyAppVersion}-windows-{#MyArch}-setup
SetupIconFile={#MyIconFile}
UninstallDisplayIcon={app}\drawio-qt.ico
LicenseFile={#MyRepoRoot}\LICENSE
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
; 升级安装时程序可能正在跑：让 Inno 提示关闭并代为结束进程，
; 避免「装完了但跑的还是旧版本」。
CloseApplications=yes
RestartApplications=no
MinVersion=10.0

[Languages]
; 中文放首位 = 默认语言。ChineseSimplified.isl 需要 Inno Setup 6.5.0+。
Name: "chinese"; MessagesFile: "compiler:Default.isl,{#MyChineseIsl}"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; 暂存树是平的：主程序、Qt DLL 在根上，插件在 platforms/、imageformats/、
; styles/ 这类子目录里——必须递归，否则装完起不来。
Source: "{#MySourceDir}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion
; 图标单独装一份：快捷方式与卸载项直接指向它（exe 的 PE 图标是同一份，但
; 显式给路径不依赖 shell 去 exe 里取）
Source: "{#MyIconFile}"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\drawio-qt.ico"
Name: "{group}\卸载 {#MyAppName}"; Filename: "{uninstallexe}"; IconFilename: "{app}\drawio-qt.ico"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\drawio-qt.ico"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

; 不写 [UninstallDelete] 清理用户配置（QSettings 所在目录）：
; 文档与偏好属于用户数据，卸载程序不该替用户决定删不删。
