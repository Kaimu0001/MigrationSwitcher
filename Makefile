# =============================================================================
# Migration Switcher - Makefile (mingw-w64 / MSYS2 / MinGW)
# ビルド: make            → build/MigrationSwitcher.exe
#         make clean
# =============================================================================

# クロスコンパイラ（MSYS2 / MinGW-w64 ネイティブ環境では CROSS= を空にして上書き）
# ソースは UTF-8 (BOM 付き) / 改行 CRLF で保存されています。
# 実行文字セットは CP932 のままにするため、-fexec-charset=CP932 を維持しています。
CROSS    ?= x86_64-w64-mingw32-
CXX      := $(CROSS)g++
WINDRES  := $(CROSS)windres

SRCDIR   := src
BUILDDIR := build

CXXFLAGS := -std=c++17 -O2 -Wall -Wextra \
            -DUNICODE -D_UNICODE \
            -finput-charset=UTF-8 -fexec-charset=CP932 \
            -Isrc

LDFLAGS  := -mwindows -static -static-libgcc -static-libstdc++ \
            -lcomctl32 -lcomdlg32 -lshlwapi -lole32 -lshell32 -lgdi32 -luser32 -ladvapi32 -lwinhttp

SRCS := $(wildcard $(SRCDIR)/*.cpp)
OBJS := $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$(SRCS))
RES  := $(BUILDDIR)/app.res
TARGET := $(BUILDDIR)/MigrationSwitcher.exe

.PHONY: all clean run test

all: $(TARGET)

$(TARGET): $(OBJS) $(RES)
	$(CXX) $(OBJS) $(RES) -o $@ $(LDFLAGS)
	@echo "==> Built: $@"

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(RES): $(SRCDIR)/app.rc $(SRCDIR)/app.manifest $(SRCDIR)/resource.h | $(BUILDDIR)
	$(WINDRES) -O coff --codepage=65001 -I$(SRCDIR) -i $< -o $@

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

clean:
	rm -rf $(BUILDDIR)

run: $(TARGET)
	$(TARGET)

# ---------------------------------------------------------------------------
# 単体テスト（設計書 20.1）: GUI と同じオブジェクトをリンクしたコンソール版
#   make test
# ---------------------------------------------------------------------------
TEST_OBJS   := $(filter-out $(BUILDDIR)/main.o,$(OBJS)) $(BUILDDIR)/console_test.o
TEST_TARGET := $(BUILDDIR)/ms_test.exe

test: $(TEST_TARGET)
	$(TEST_TARGET)

$(BUILDDIR)/console_test.o: test/console_test.cpp | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -DMS_BUILD_CONSOLE_TESTS -Itest -c $< -o $@

$(TEST_TARGET): $(TEST_OBJS)
	$(CXX) $(TEST_OBJS) -o $@ -static -static-libgcc -static-libstdc++ \
	    -lcomctl32 -lcomdlg32 -lshlwapi -lole32 -lshell32 -lgdi32 -luser32 -ladvapi32 -lwinhttp
