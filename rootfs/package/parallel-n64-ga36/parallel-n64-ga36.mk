################################################################################
# ParaLLEl N64 for GA36-MB / goodluckOS — EXPERIMENTAL, UNTESTED RECIPE
################################################################################
PARALLEL_N64_GA36_VERSION = 649c6f898b22eeaece20bbefca6301cb120b126c
PARALLEL_N64_GA36_SITE = https://github.com/libretro/parallel-n64.git
PARALLEL_N64_GA36_SITE_METHOD = git
PARALLEL_N64_GA36_GIT_SUBMODULES = YES
PARALLEL_N64_GA36_LICENSE = GPL-2.0 (verify component licences before redistribution)
PARALLEL_N64_GA36_DEPENDENCIES = mesa3d zlib host-pkgconf

# Build with the same target libc, hard-float ABI and GLES libraries as goodluckOS.
# Upstream Makefile has its own CPUFLAGS; set them on the make command-line.

define PARALLEL_N64_GA36_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D) \
		platform=unix FORCE_GLES=1 GL_LIB=-lGLESv2 \
		CC="$(TARGET_CC)" CXX="$(TARGET_CXX)" \
		AR="$(TARGET_AR)" RANLIB="$(TARGET_RANLIB)" \
		CPUFLAGS="$(TARGET_CFLAGS) -DNO_ASM -DARM -D__arm__ -DARM_ASM -D__NEON_OPT -DNOSSE -DARM_FIX" \
		WITH_DYNAREC=arm HAVE_NEON=1 HAVE_PARALLEL=0 HAVE_PARALLEL_RSP=0
endef

define PARALLEL_N64_GA36_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/parallel_n64_libretro.so \
		$(TARGET_DIR)/usr/lib/libretro/parallel_n64_goodluck_libretro.so
endef

$(eval $(generic-package))
