# ===== Config =====
PROJECT_NAME := srvwzp

# NDK: aponte seu caminho (ou defina no ambiente)
ANDROID_NDK_HOME ?= $(HOME)/Android/ndk/25.2.9519653
NDK_BUILD := $(ANDROID_NDK_HOME)/ndk-build

# Saídas padrão do ndk-build:
# libs/<abi>/lib$(PROJECT_NAME).so
OUT_LIB_DIR := $(CURDIR)/libs

# Destino de instalação (estrutura jniLibs para apps Android)
INSTALL_BASE ?= $(CURDIR)/dist/jniLibs

# (Opcional) strip para reduzir tamanho
STRIP := $(ANDROID_NDK_HOME)/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip

.PHONY: compile install clean distclean help

help:
	@echo "Alvos:"
	@echo "  make compile     -> compila (usa jni/Application.mk p/ ABIs/plataforma)"
	@echo "  make install     -> copia .so para dist/jniLibs/<abi>/ e strip"
	@echo "  make clean       -> limpa objetos"
	@echo "  make distclean   -> limpa libs/ obj/ e dist/"
	@echo "Vars:"
	@echo "  ANDROID_NDK_HOME=$(ANDROID_NDK_HOME)"
	@echo "  INSTALL_BASE=$(INSTALL_BASE)"

compile:
	@test -x "$(NDK_BUILD)" || { echo "ERRO: ndk-build não encontrado em $(NDK_BUILD). Ajuste ANDROID_NDK_HOME."; exit 1; }
	@echo "==> ndk-build"
	@"$(NDK_BUILD)" -C "$(CURDIR)" NDK_PROJECT_PATH="$(CURDIR)"

install: compile
	@echo "==> Instalando bibliotecas"
	@mkdir -p "$(INSTALL_BASE)"
	@if [ ! -d "$(OUT_LIB_DIR)" ]; then echo "ERRO: Pasta 'libs' não encontrada. Rodou 'make compile'?"; exit 1; fi
	@set -e; \
	for so in $$(find "$(OUT_LIB_DIR)" -type f -name "lib$(PROJECT_NAME).so"); do \
	  abi=$$(basename $$(dirname "$$so")); \
	  dest="$(INSTALL_BASE)/$$abi"; \
	  echo "Copiando $$so -> $$dest/"; \
	  mkdir -p "$$dest"; \
	  cp "$$so" "$$dest/"; \
	  if [ -x "$(STRIP)" ]; then \
	    echo "Strip: $$dest/lib$(PROJECT_NAME).so"; \
	    "$(STRIP)" "$$dest/lib$(PROJECT_NAME).so"; \
	  fi; \
	done; \
	echo "OK! Instalado em $(INSTALL_BASE)/<ABI>/lib$(PROJECT_NAME).so"

clean:
	@echo "==> ndk-build clean"
	@-@"$(NDK_BUILD)" -C "$(CURDIR)" clean || true

distclean: clean
	@echo "==> removendo libs/ obj/ dist/"
	@rm -rf "$(CURDIR)/libs" "$(CURDIR)/obj" "$(INSTALL_BASE)"
