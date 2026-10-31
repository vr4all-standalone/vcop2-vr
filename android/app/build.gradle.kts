plugins { id("com.android.application") }
android {
    namespace = "com.vrlightgunhub.vcop2"
    ndkVersion = "29.0.14206865"
    compileSdk { version = release(36) { minorApiLevel = 1 } }
    defaultConfig {
        applicationId = "com.vrlightgunhub.vcop2"
        minSdk = 29
        targetSdk = 36
        versionCode = 16
        versionName = "0.2.14"
        ndk { abiFilters += "arm64-v8a" }
    }
    signingConfigs { create("local") {
        storeFile = rootProject.file("../private/vcop2.keystore")
        storePassword = "android"
        keyAlias = "vcop2"
        keyPassword = "android"
    } }
    buildTypes { getByName("release") { signingConfig = signingConfigs.getByName("local"); isMinifyEnabled = false } }
    sourceSets.getByName("main").assets.srcDirs("../../assets")
    sourceSets.getByName("main").jniLibs.srcDirs(providers.gradleProperty("vcop2NativeDir").orNull ?: rootProject.file("../build/quest/jniLibs"))
    packaging { jniLibs { useLegacyPackaging = true } }
}

layout.buildDirectory.set(rootProject.layout.projectDirectory.dir("../build/android/app"))
