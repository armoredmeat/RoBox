; Inno Setup script for the RoBox Windows installer.
; Build it with Installer\build-installer.ps1 (which builds, tests, then runs this),
; or directly:  ISCC.exe /DAppVersion=0.9.0 Installer\RoBox.iss

#ifndef AppVersion
  #define AppVersion "0.9.0"
#endif
#define BuildDir "..\build\RoBox_artefacts\Release"

[Setup]
; The AppId identifies RoBox to Windows across versions. Never change it, or updates
; will install side by side instead of replacing the old version.
AppId={{6E2B6C1F-4A7D-4B8E-9C1A-52D0B0A7F3E1}
AppName=RoBox
AppVersion={#AppVersion}
AppVerName=RoBox {#AppVersion} (beta)
AppPublisher=RoBox Audio
DefaultDirName={autopf}\RoBox
DefaultGroupName=RoBox
DisableProgramGroupPage=yes
; VST3 plugins live in C:\Program Files\Common Files\VST3, which needs admin rights.
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir=..\dist
OutputBaseFilename=RoBox-{#AppVersion}-Setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
InfoBeforeFile=README.txt
UninstallDisplayIcon={app}\RoBox.exe
UninstallDisplayName=RoBox {#AppVersion}
; Ask to close a DAW that has RoBox loaded, so the plugin file can be replaced.
CloseApplications=yes

[Types]
Name: "full";   Description: "VST3 plugin and standalone app"
Name: "plugin"; Description: "VST3 plugin only"
Name: "custom"; Description: "Choose"; Flags: iscustom

[Components]
Name: "vst3";       Description: "RoBox VST3 plugin (for your DAW)";           Types: full plugin custom
Name: "standalone"; Description: "RoBox standalone app (runs without a DAW)"; Types: full

[Tasks]
Name: "desktopicon"; Description: "Desktop shortcut for the standalone app"; Components: standalone; Flags: unchecked

[Files]
; The VST3 is a folder ("bundle"); copy all of it.
Source: "{#BuildDir}\VST3\RoBox.vst3\*"; DestDir: "{commoncf64}\VST3\RoBox.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#BuildDir}\Standalone\RoBox.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
Source: "README.txt"; DestDir: "{app}"; Flags: ignoreversion isreadme

[Icons]
Name: "{group}\RoBox";             Filename: "{app}\RoBox.exe"; Components: standalone
Name: "{group}\RoBox Read Me";     Filename: "{app}\README.txt"
Name: "{group}\Uninstall RoBox";   Filename: "{uninstallexe}"
Name: "{autodesktop}\RoBox";       Filename: "{app}\RoBox.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\RoBox.exe"; Description: "Open the RoBox standalone app now"; Components: standalone; Flags: nowait postinstall skipifsilent unchecked

[UninstallDelete]
; Remove the plugin folder completely. Your own presets (Documents\RoBox) and the theme
; setting (AppData\Roaming\RoBox) are left alone.
Type: filesandordirs; Name: "{commoncf64}\VST3\RoBox.vst3"
