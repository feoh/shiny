OF_ROOT ?= ../../..
include $(OF_ROOT)/libs/openFrameworksCompiled/project/makefileCommon/compile.project.mk

.PHONY: web web-run web-clean

WEB_SHELL := $(CURDIR)/web/shell.html
WEB_OUTPUT := bin/em/$(APPNAME)
EMMAKE ?= emmake
EMRUN ?= emrun
EMCC ?= emcc
EMCXX ?= em++
export EM_CACHE ?= $(CURDIR)/.emscripten-cache

web:
	$(EMMAKE) $(MAKE) Release OF_ROOT=$(OF_ROOT) CC=$(EMCC) CXX=$(EMCXX) PROJECT_EMSCRIPTEN_TEMPLATE=$(WEB_SHELL)

web-run: web
	$(EMRUN) $(WEB_OUTPUT)

web-clean:
	$(RM) -r $(WEB_OUTPUT)
