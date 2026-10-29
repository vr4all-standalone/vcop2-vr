# Build Virtua Cop 2 VR 0.2.11

Download the complete source ZIP from the project release; it contains the full pinned engine and dependency sources.

Use the named `vcop2-vr-source.zip` asset. It contains the current port and tests, every exported engine patch, a complete pinned MAME Git bundle, exact SDL/OpenXR dependency sources and licenses. GitHub's automatic project-page source archive is not this complete delivery. `SOURCE-MANIFEST.json` hashes port files; `SOURCE-DEPENDENCIES.json` hashes the dependency snapshots and MAME bundle. No game media, saves, private fixtures or signing keys are included.

## Tools and offline engine source

Use Windows, Git, MSYS2 MinGW64 GCC/make with MAME's build dependencies, CMake 3.22+ and Ninja, Python 3.12+, and JDK 21. Put the MinGW64 `bin` directory on PATH. The Python suite includes launcher behavior tests and needs `JAVA_HOME` set to JDK 21. CarnEvil's full Python suite additionally needs NumPy, Pillow and ModernGL. A desktop OpenGL context is required for real GPU checks. Private media/capture/APK checks are optional and report their missing-fixture skips.

From the extracted source directory, restore the supplied upstream source without a network fetch and apply its complete patch stack:

```powershell
git -c core.autocrlf=false clone --no-checkout engine-source.bundle engine
git -C engine checkout --detach f34f02505e32c1993c6a782b6814232cbfc74e36
Get-ChildItem patches/*.patch | Sort-Object Name | ForEach-Object {
  git -C engine apply --whitespace=nowarn $_.FullName
  if ($LASTEXITCODE) { throw "Engine patch failed: $($_.Name)" }
}
```

The bundle pin is MAME 0.289 `f34f02505e32c1993c6a782b6814232cbfc74e36`. The project engine scripts can also restore this bundle automatically and apply the exported patches in a separate compiler cache. Do not use another mod's patched engine. Production engine and host must retain their shared native ABI.

## Host build and public regression checks

Use a dedicated short build path of your choice. For example:

```powershell
$Build = 'D:/VRBuild/vcop2'
cmake -S . -B "$Build/host" -G Ninja -DCMAKE_BUILD_TYPE=Release -DVCOP2_CLIP_REFERENCE_SOURCE=engine/src/mame/sega/model2_v.cpp
cmake --build "$Build/host" -j 6
ctest --test-dir "$Build/host" --output-on-failure
python -B -m unittest discover -s tests -v
```

If your CMake resolves relative FILEPATH/PATH arguments against a different working directory, pass full paths to this extracted `engine` directory instead. All referenced engine text is supplied by the bundle; no original developer drive/cache is required. Host checks compile the actual renderer/presentation/settings code. They do not establish headset appearance, comfort or gameplay acceptance.

## Quest dependency and app build

Quest uses SDK platform 36.1/build-tools 36.0.0, NDK 29.0.14206865, JDK 21, ARM64 and API 29. Set the following paths to your own installed tools:

```powershell
$Sdk = 'D:/AndroidSdk'
$Ndk = "$Sdk/ndk/29.0.14206865"
$Java = 'D:/jdk-21'
$CMake = 'C:/Tools/cmake/bin/cmake.exe'
$Ninja = 'C:/Tools/ninja.exe'
$Common = @('-G','Ninja',"-DCMAKE_MAKE_PROGRAM=$Ninja",'-DCMAKE_BUILD_TYPE=Release',"-DCMAKE_TOOLCHAIN_FILE=$Ndk/build/cmake/android.toolchain.cmake",'-DANDROID_ABI=arm64-v8a','-DANDROID_PLATFORM=android-29','-DANDROID_STL=c++_shared')
& $CMake -S source-deps/SDL -B "$Build/sdl-android-build" @Common -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST=OFF -DSDL_TESTS=OFF "-DCMAKE_INSTALL_PREFIX=$Build/sdl-android"
& $CMake --build "$Build/sdl-android-build" -j 6
& $CMake --install "$Build/sdl-android-build"
& $CMake -S source-deps/OpenXR-SDK -B "$Build/openxr-android" @Common -DDYNAMIC_LOADER=ON -DBUILD_API_LAYERS=OFF -DBUILD_TESTS=OFF -DBUILD_TESTING=OFF
& $CMake --build "$Build/openxr-android" --target openxr_loader -j 6
./tools/build-quest.ps1 -MsysRoot D:/msys64 -PythonPath C:/Python/python.exe -WorkRoot $Build -AndroidSdk $Sdk -JavaHome $Java -CMakePath $CMake -NinjaPath $Ninja -SdlRoot "$Build/sdl-android" -OpenXRLoader "$Build/openxr-android/src/loader/libopenxr_loader.so" -SdlSource source-deps/SDL -OpenXRSource source-deps/OpenXR-SDK
```

Build-engine scripts expose `-Platform Windows|Android`, `-WorkEngine`, `-MsysRoot`, `-PythonPath` and `-Jobs`; Android also needs `-AndroidNdk` and `-SdlRoot`. The first Gradle assembly needs its normal downloaded dependencies; `-Offline` is available once cached. The bundled MAME/SDL/OpenXR sources themselves need no download. For Windows PCVR, build the engine with `-Platform Windows` and build the supplied OpenXR SDK's Windows loader with `DYNAMIC_LOADER=ON` and `BUILD_API_LAYERS=OFF`.

A missing development signing key is generated under ignored `private/`. That key cannot update the official app's signing identity; use an independent package identity or separate test device. Keep your key for subsequent local updates. Release credentials are intentionally absent. Package scripts are local software packagers and do not publish to GitHub. Read `--help` before choosing input/output paths. Game-file validation and native emulation still require your own supported media as documented in README.
