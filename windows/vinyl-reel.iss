#define MyAppName "Vinyl Reel"
#define MyAppPublisher "Podders"
#define MyAppURL "https://github.com/Podders/vinyl-reel-obs-plugin"
#ifndef MyAppVersion
  #define MyAppVersion "0.1.0"
#endif

[Setup]
AppId={{D6C0F1B3-36B5-4F05-AE3D-5AB0B7F8F1A1}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={commonappdata}\obs-studio\plugins\vinyl-reel
DisableDirPage=yes
DisableProgramGroupPage=yes
OutputBaseFilename=vinyl-reel-{#MyAppVersion}-windows-setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
UsePreviousAppDir=no
UninstallDisplayIcon={app}\bin\64bit\vinyl-reel.dll

[Files]
Source: "..\release\vinyl-reel\bin\64bit\*"; DestDir: "{app}\bin\64bit"; Flags: recursesubdirs createallsubdirs ignoreversion; Excludes: "*.pdb"
Source: "..\release\vinyl-reel\data\*"; DestDir: "{app}\data"; Flags: recursesubdirs createallsubdirs ignoreversion

[InstallDelete]
Type: files; Name: "{app}\bin\64bit\obs.dll"
Type: files; Name: "{app}\bin\64bit\obs-frontend-api.dll"

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"
