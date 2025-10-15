# jni/Application.mk
# ABIs alvo (ajuste conforme sua necessidade)
APP_ABI := arm64-v8a armeabi-v7a x86_64

# Plataforma (alinhe com minSdkVersion do seu app, se for empacotar depois)
APP_PLATFORM := android-24

# Otimização padrão
APP_OPTIM := release

# Toolchain (NDKs atuais já usam clang por padrão)
NDK_TOOLCHAIN_VERSION := clang
