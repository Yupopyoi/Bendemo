# Bendemo

## Qt 環境

Qt 6.10.0
MSVC 2022 64-bit

### 必須ライブラリ

- Qt Multimedia
- Qt SerialPort

## 事始め

### Bendemoの入手

```powershell
git clone https://github.com/Yupopyoi/Bendemo.git
cd Bendemo
```

### OpenCVの導入

Bendemoディレクトリ下で、次のコマンドを実行する。

```powershell
# Get OpenCV Package
git clone https://github.com/microsoft/vcpkg

# Install OpenCV
$env:Path += ";C:\Windows\System32\WindowsPowerShell\v1.0"
.\vcpkg\bootstrap-vcpkg.bat
.\vcpkg\vcpkg.exe list

# This takes about 10 minutes.
.\vcpkg\vcpkg.exe install "opencv[world]:x64-windows"

.\vcpkg\vcpkg.exe install yaml-cpp:x64-windows

# Delete Regacy Build
Remove-Item -Recurse -Force .\build -ErrorAction SilentlyContinue

# Print CMake Config
$root = (Get-Location).Path + "\vcpkg\installed\x64-windows"
@{
  OpenCV_DIR        = 'share\opencv4'
  Protobuf_DIR      = 'share\protobuf'
  quirc_DIR         = 'share\quirc'
  absl_DIR          = 'share\absl'
  utf8_range_DIR    = 'share\utf8_range'
  CMAKE_PREFIX_PATH = ''
}.GetEnumerator() | ForEach-Object {
  Write-Output ("-D{0}:PATH={1}\{2}" -f $_.Key, $root, $_.Value)
}

# Please paste the above six lines into the CMake configuration (batch edit).

```

インストール等が正常に終了すると

```txt
-DCMAKE_PREFIX_PATH:PATH=E:\Bendemo\vcpkg\installed\x64-windows\
-DProtobuf_DIR:PATH=E:\Bendemo\vcpkg\installed\x64-windows\\share\protobuf
-Dabsl_DIR:PATH=E:\Bendemo\vcpkg\installed\x64-windows\\share\absl
-Dquirc_DIR:PATH=E:\Bendemo\vcpkg\installed\x64-windows\\share\quirc
-DOpenCV_DIR:PATH=E:\Bendemo\vcpkg\installed\x64-windows\\share\opencv4
-Dutf8_range_DIR:PATH=E:\Bendemo\vcpkg\installed\x64-windows\\share\utf8_range
```

のように、パスが６つ表示されます。これらは後に使用するため、コピーしておく。

### Qt Creatorの起動＆プロジェクト立ち上げ

Qt Creatorを起動し、Open Projectから、プロジェクトを選択する。
このとき、CMakeLists.txt をクリックすることでプロジェクトを立ち上げることができる。

### CMake設定＆実行

画面左端の「プロジェクト」を選択し、「ビルドと実行」の中から、Qt 6.10.0 MSVC2022 64bitのビルドを選択しておく。  
CMakeの中にある「Current Configuration」を選び、右端にある「一括編集」をクリックする。  
すると、テキストボックスが表れるので、そこに、先ほどコピーした６つのパスをそのままペーストする。
「OK」をクリックしてウインドウを閉じたのち、「CMakeの実行」をする。
