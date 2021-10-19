# This file is deprecated, see README.

ifeq ($(PAL_HOST),)
$(error include Makefile.configs before including manifest.mk)
endif

MAKEFILE_MANIFEST_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

%.manifest: %.manifest.template
	$(call cmd,manifest,$*,$(manifest_rules))

%.manifest: manifest.template
	$(call cmd,manifest,$*,$(manifest_rules))

ifeq ($(PAL_HOST),Linux-SGX)

# sgx manifest.sgx/sig/token
drop_manifest_suffix = $(filter-out manifest,$(sort $(patsubst %.manifest,%,$(1))))
expand_target_to_token = $(addsuffix .token,$(call drop_manifest_suffix,$(1)))
expand_target_to_sig = $(addsuffix .sig,$(call drop_manifest_suffix,$(1)))
expand_target_to_sgx = $(addsuffix .manifest.sgx,$(call drop_manifest_suffix,$(1)))

SGX_DIR := $(MAKEFILE_MANIFEST_DIR)/../Pal/src/host/Linux-SGX/
SGX_SIGNER_KEY ?= $(SGX_DIR)/signer/enclave-key.pem

$(SGX_SIGNER_KEY):
	$(error "Cannot find any enclave key. Generate $(abspath $(SGX_SIGNER_KEY)) or specify 'SGX_SIGNER_KEY=' with make")

%.token: %.sig
	$(call cmd,sgx_get_token)

%.sig %.manifest.sgx: %.manifest $(SGX_SIGNER_KEY)
	$(call cmd,sgx_sign)

-include $(wildcard *.manifest.sgx.d)

endif # Linux-SGX
