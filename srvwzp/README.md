# srvwzp (NDK - ndk-build)

Projeto mínimo para compilar uma lib JNI com **Android NDK (ndk-build)**.

## Requisitos
- Android NDK (ex.: `~/Android/ndk/25.2.9519653`)
- `ndk-build` no caminho: `$ANDROID_NDK_HOME/ndk-build`

## Estrutura
```
srvwzp/
├─ jni/
│  ├─ Android.mk
│  └─ Application.mk
├─ native/
│  └─ srvwzp.c
└─ Makefile
```

## Build
```bash
export ANDROID_NDK_HOME=$HOME/Android/ndk/25.2.9519653
make compile
```

Saída esperada:
```
libs/
 ├─ arm64-v8a/libsrvwzp.so
 ├─ armeabi-v7a/libsrvwzp.so
 └─ x86_64/libsrvwzp.so
```

## Instalação (copia + strip)
```bash
make install
```

Destino padrão:
```
dist/jniLibs/
 ├─ arm64-v8a/libsrvwzp.so
 ├─ armeabi-v7a/libsrvwzp.so
 └─ x86_64/libsrvwzp.so
```

Para instalar direto no seu app:
```bash
make INSTALL_BASE=/caminho/do/app/app/src/main/jniLibs install
```
