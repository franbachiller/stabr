; STABR - instalador para Windows x64 (Inno Setup 6)
;
; Se compila con Scripts\package.ps1, que le pasa la version leida del
; CMakeLists.txt. Compilado a mano sin /DAppVersion sale como 0.0.0 a
; proposito: un instalador con la version equivocada es peor que uno que
; obviamente no es de release.
;
; Instala:
;   - el VST3 en la carpeta estandar del sistema (Common Files\VST3)
;   - los manuales en ingles y espanol, con accesos en el menu Inicio
;
; Los presets de fabrica van compilados dentro del plugin: no hay archivos que
; instalar. Los presets de usuario viven en Documentos\Narfbach\STABR\Presets
; y el desinstalador NO los borra: son del usuario.

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif

#define AppName "STABR"
#define Publisher "Narfbach"

[Setup]
; AppId fijo: si cambia, Windows trata cada version como un programa distinto
; y la actualizacion no reemplaza la anterior. NUNCA cambiarlo.
AppId={{8C5E2B7A-3F14-4D0B-9E2A-5B7C1D6F4A21}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#Publisher}
AppPublisherURL=https://narfbach.com
DefaultDirName={commonpf64}\{#Publisher}\{#AppName}
DefaultGroupName={#Publisher}\{#AppName}
DisableDirPage=yes
DisableProgramGroupPage=yes
OutputDir=..\build\installer
OutputBaseFilename=STABR-{#AppVersion}-Windows-x64-Setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
UninstallDisplayName={#AppName} {#AppVersion}
VersionInfoVersion={#AppVersion}
VersionInfoCompany={#Publisher}
VersionInfoProductName={#AppName}

[Files]
Source: "..\build\STABR_artefacts\Release\VST3\STABR.vst3\*"; DestDir: "{commoncf64}\VST3\STABR.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\docs\manual\*"; DestDir: "{app}\Manual"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\STABR Manual (English)"; Filename: "{app}\Manual\STABR-Manual-EN.html"
Name: "{group}\Manual de STABR (Español)"; Filename: "{app}\Manual\STABR-Manual-ES.html"
Name: "{group}\Uninstall STABR"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\Manual\STABR-Manual-EN.html"; Description: "Open the STABR manual"; Flags: postinstall shellexec skipifsilent unchecked
