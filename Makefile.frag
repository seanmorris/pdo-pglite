# Expand includes without expanding macros in JS, before EM_JS stringifies it.
# Leave EM_JS undefined here; pdo_pglite.c includes emscripten.h during compilation.
# PHP substitutes srcdir/builddir when it includes this fragment at configure time.
PDO_PGLITE_JS_TEMPLATE = $(srcdir)/pdo_pglite_js.h.in
PDO_PGLITE_JS_HEADER = $(builddir)/generated/pdo_pglite_js.h
PDO_PGLITE_JS_DEPS = $(builddir)/generated/pdo_pglite_js.d

$(builddir)/pdo_pglite.lo: $(PDO_PGLITE_JS_HEADER)

# Group both outputs so parallel builds and a missing depfile run one compiler.
$(PDO_PGLITE_JS_HEADER) $(PDO_PGLITE_JS_DEPS) &: $(PDO_PGLITE_JS_TEMPLATE) $(srcdir)/Makefile.frag
	@mkdir -p "$(dir $(PDO_PGLITE_JS_HEADER))"
	@set -eu; \
		trap 'rm -f "$(PDO_PGLITE_JS_HEADER).tmp" "$(PDO_PGLITE_JS_DEPS).tmp"' 0 1 2 3 15; \
		$(CC) -E -P -CC -fdirectives-only -x c \
			-MMD -MP -MF "$(PDO_PGLITE_JS_DEPS).tmp" \
			-MQ "$(PDO_PGLITE_JS_HEADER)" -MQ "$(PDO_PGLITE_JS_DEPS)" \
			"$(PDO_PGLITE_JS_TEMPLATE)" -o "$(PDO_PGLITE_JS_HEADER).tmp"; \
		mv "$(PDO_PGLITE_JS_HEADER).tmp" "$(PDO_PGLITE_JS_HEADER)"; \
		mv "$(PDO_PGLITE_JS_DEPS).tmp" "$(PDO_PGLITE_JS_DEPS)"

# Do not regenerate included makefiles while removing build outputs.
ifeq ($(filter clean distclean clean-pdo-pglite-js,$(MAKECMDGOALS)),)
include $(PDO_PGLITE_JS_DEPS)
endif

.PHONY: clean-pdo-pglite-js
clean: clean-pdo-pglite-js
clean-pdo-pglite-js:
	rm -f "$(PDO_PGLITE_JS_HEADER)" "$(PDO_PGLITE_JS_DEPS)" "$(PDO_PGLITE_JS_HEADER).tmp" "$(PDO_PGLITE_JS_DEPS).tmp"
