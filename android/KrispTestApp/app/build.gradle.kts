import java.util.Properties

plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.android)
}

android {
    namespace = "com.krisp.krisptestapp"
    compileSdk = 35

    defaultConfig {
        applicationId = "com.krisp.krisptestapp"
        minSdk = 21
        targetSdk = 35
        versionCode = 1
        versionName = "1.0"

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        ndk {
            abiFilters += listOf("arm64-v8a", "armeabi-v7a", "x86_64")
        }
        externalNativeBuild {
            cmake {
                // Read krisp.sdk.dir from local.properties (relative OK) or env var
                val lp = rootProject.file("local.properties")
                val props = Properties().apply {
                    if (lp.exists()) lp.inputStream().use { load(it) }
                }

                val krispSdkDir = props.getProperty("krisp.sdk.dir")
                    ?.let { rootProject.file(it).absolutePath } // handle relative paths
                    ?: System.getenv("KRISP_SDK_DIR")
                    ?: error("Set krisp.sdk.dir in local.properties or KRISP_SDK_DIR in env")

                arguments += listOf(
                    "-DANDROID_PLATFORM=android-35",
                    //"-DANDROID_STL=c++_static",
                    "-DANDROID_STL=c++_shared",
                    //"-DKRISP_LINK_TYPE=static",
                    "-DKRISP_LINK_TYPE=dynamic",
                    "-DKRISP_SDK_ROOT=$krispSdkDir"
                )
            }
        }

    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }
    kotlinOptions {
        jvmTarget = "11"
    }
    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }
    buildFeatures {
        viewBinding = true
    }
    ndkVersion = "28.0.13004108"
}

dependencies {

    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.appcompat)
    implementation(libs.material)
    implementation(libs.androidx.constraintlayout)
    testImplementation(libs.junit)
    androidTestImplementation(libs.androidx.junit)
    androidTestImplementation(libs.androidx.espresso.core)
}