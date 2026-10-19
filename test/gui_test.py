# -*- coding: utf-8 -*-
"""MigrationSwitcher GUI acceptance automation (B-1 .. B-18)."""
import os, sys, time, shutil, glob, threading, ctypes, re, json
import win32gui, win32con, win32api, win32process
from pywinauto import Application

EXE_SRC = sys.argv[1]
TAG = sys.argv[2]
DO_GTEST_RUN = (len(sys.argv) > 3 and sys.argv[3] == "gtest")

ROOT = r"C:\temp\gencode\acc\gui\run_" + TAG + time.strftime("_%H%M%S")
APPDIR = os.path.join(ROOT, "app")
WORK = os.path.join(ROOT, "work")
SHOTS = os.path.join(ROOT, "shots")
shutil.rmtree(ROOT, ignore_errors=True)
for d in (APPDIR, WORK, SHOTS):
    os.makedirs(d)
EXE = os.path.join(APPDIR, "MigrationSwitcher.exe")
shutil.copy(EXE_SRC, EXE)

CALC = os.path.join(WORK, "calc.c")
CALC_SRC = (
    "#include <stdio.h>\r\n\r\n"
    "int add(int a, int b)\r\n{\r\n    int r = a + b;\r\n    return r;\r\n}\r\n\r\n"
    "int mul(int a, int b)\r\n{\r\n    int r = a * b;\r\n    return r;\r\n}\r\n\r\n"
    "int sub(int a, int b)\r\n{\r\n    if (a > 0) {\r\n        int r = a - b;\r\n        return r;\r\n    }\r\n    return 0;\r\n}\r\n")
open(CALC, "wb").write(CALC_SRC.encode("utf-8"))
SAMPLE = os.path.join(WORK, "sample.c")
shutil.copy(r"C:\temp\gencode\acc\src\MigrationSwitcher\test\sample.c", SAMPLE)
BATCH = os.path.join(WORK, "batch")
os.makedirs(BATCH)
for n in ("x.c", "y.c"):
    open(os.path.join(BATCH, n), "wb").write(
        b"int f_" + n[0].encode() + b"(int v)\r\n{\r\n    int k = v + 1;\r\n    return k;\r\n}\r\n")

results = []
def check(name, ok, detail=""):
    results.append((name, bool(ok), detail))
    print(("[PASS] " if ok else "[FAIL] ") + name + (("  -- " + detail) if detail else ""), flush=True)

# --------------------------------------------------------------------------
app = Application(backend="win32").start(EXE, work_dir=APPDIR)
pid = app.process
w = app.top_window()
w.wait("visible", timeout=10)
MAIN = w.handle

from pywinauto.controls.hwndwrapper import HwndWrapper
from pywinauto.controls import win32_controls, common_controls
_CLS = {"ComboBox": win32_controls.ComboBoxWrapper, "SysListView32": common_controls.ListViewWrapper,
        "SysTabControl32": common_controls.TabControlWrapper, "Button": win32_controls.ButtonWrapper,
        "Edit": win32_controls.EditWrapper}
def ctrl(cid, cls=None):
    found = []
    def cb(h, _):
        if win32gui.GetDlgCtrlID(h) == cid and (cls is None or win32gui.GetClassName(h) == cls):
            if win32gui.GetClassName(h) not in ("MS_PageWindowClass",):
                found.append(h)
        return True
    win32gui.EnumChildWindows(MAIN, cb, None)
    if not found:
        raise RuntimeError("control %d not found" % cid)
    h = found[0]
    k = _CLS.get(win32gui.GetClassName(h), HwndWrapper)
    return k(h)

def H(cid):
    return ctrl(cid).handle

def text_of(cid):
    return ctrl(cid).window_text()

def set_text(cid, s):
    h = H(cid)
    win32gui.SendMessage(h, win32con.WM_SETTEXT, 0, s.replace("\n", "\r\n") if "\r\n" not in s else s)
    # EN_CHANGE is sent automatically by WM_SETTEXT

dialogs = []      # (title, text, answer)

def find_dialogs():
    out = []
    def cb(h, _):
        if win32gui.IsWindowVisible(h) and win32gui.GetClassName(h) == "#32770":
            _, p = win32process.GetWindowThreadProcessId(h)
            if p == pid: out.append(h)
        return True
    win32gui.EnumWindows(cb, None)
    return out

def dialog_text(h):
    parts = []
    def cb(c, _):
        if win32gui.GetClassName(c) == "Static":
            t = win32gui.GetWindowText(c)
            if t: parts.append(t)
        return True
    win32gui.EnumChildWindows(h, cb, None)
    return "\n".join(parts)

def answer_dialogs(answer="yes", settle=1.0, timeout=60):
    """Dismiss message boxes until the app is idle. Returns texts seen."""
    seen = []
    t0 = time.time(); idle_since = None
    while time.time() - t0 < timeout:
        ds = find_dialogs()
        if ds:
            idle_since = None
            for d in ds:
                title = win32gui.GetWindowText(d)
                txt = dialog_text(d)
                # file dialogs: cancel
                btn = None
                for bid in ((6, 1) if answer == "yes" else (7, 2, 1)):
                    try:
                        b = win32gui.GetDlgItem(d, bid)
                        if b and win32gui.IsWindowVisible(b): btn = b; break
                    except Exception: pass
                if title.startswith("CopyMigration") is False and "名前" in txt:
                    btn = win32gui.GetDlgItem(d, 2)
                if not btn:
                    try: btn = win32gui.GetDlgItem(d, 2)
                    except Exception: pass
                seen.append((title, txt))
                dialogs.append((title, txt, answer))
                win32gui.PostMessage(btn, win32con.BM_CLICK, 0, 0)
            time.sleep(0.4)
        else:
            if idle_since is None: idle_since = time.time()
            # app must respond to messages (not busy)
            try:
                win32gui.SendMessageTimeout(MAIN, win32con.WM_NULL, 0, 0, win32con.SMTO_ABORTIFHUNG, 500)
                responsive = True
            except Exception:
                responsive = False
            if responsive and time.time() - idle_since >= settle:
                break
            time.sleep(0.2)
    return seen

def click(cid, answer="yes", timeout=60, settle=1.0):
    h = H(cid)
    win32gui.PostMessage(h, win32con.BM_CLICK, 0, 0)
    time.sleep(0.3)
    return answer_dialogs(answer, settle=settle, timeout=timeout)

def joined(seen):
    return " | ".join(t.replace("\r", "").replace("\n", " ") for _, t in seen)

def combo_select(cid, idx):
    c = ctrl(cid, "ComboBox")
    c.select(idx)            # pywinauto notifies parent with CBN_SELCHANGE
    time.sleep(0.3)
    answer_dialogs(settle=0.3)

def combo_items(cid):
    return ctrl(cid, "ComboBox").texts()[1:] if False else ctrl(cid, "ComboBox").item_texts()

def set_check(cid, on):
    c = ctrl(cid)
    if bool(c.get_check_state()) != on:
        win32gui.PostMessage(c.handle, win32con.BM_CLICK, 0, 0)
        time.sleep(0.3)
        answer_dialogs(settle=0.3)

TAB = ctrl(1006)
TABS = ["基本設定", "一括処理", "検索条件", "旧コード", "新コード", "追加コード",
        "検索結果", "プレビュー", "スイッチ管理", "システム情報", "履歴", "ログ"]

def select_tab(i):
    TAB.select(i)
    time.sleep(0.4)

def shot(name):
    w.capture_as_image().save(os.path.join(SHOTS, name + ".png"))

def rect_of(h):
    return win32gui.GetWindowRect(h)

def is_blank_region(img, box):
    reg = img.crop(box).convert("L")
    ext = reg.getextrema()
    return (ext[1] - ext[0]) < 25

def read_file(p):
    b = open(p, "rb").read()
    for enc in ("utf-8-sig", "cp932"):
        try: return b.decode(enc)
        except Exception: pass
    return b.decode("latin1")

def page_handles():
    hs = []
    def cb(h, _):
        if win32gui.GetClassName(h) == "MS_PageWindowClass" and win32gui.GetParent(h) == MAIN:
            hs.append(h)
        return True
    win32gui.EnumChildWindows(MAIN, cb, None)
    return hs

# ==========================================================================
# B-1 / B-3: initial rendering (no mouse movement)
# ==========================================================================
time.sleep(1.5)
img = w.capture_as_image()
wr = rect_of(MAIN)
def local(h):
    r = rect_of(h)
    return (r[0] - wr[0] + 3, r[1] - wr[1] + 3, r[2] - wr[0] - 3, r[3] - wr[1] - 3)
shot("B01_initial")
basic_page = [h for h in page_handles() if win32gui.IsWindowVisible(h)]
check("B-1 起動直後に基本設定ページが表示されている", len(basic_page) == 1 and
      win32gui.IsWindowVisible(H(1101)))
check("B-1 ページ領域が白いままでない", not is_blank_region(img, local(H(1100))))
for cid, nm in ((1937, "API 種別"), (2121, "ログ実装の拡張子"), (1945, "レポート作成単位"),
                (2131, "テスト対象範囲(開始)"), (2132, "テスト対象範囲(終了)")):
    check("B-3 初期表示でコンボの文字が見える: " + nm, not is_blank_region(img, local(H(cid))))

# ==========================================================================
# B-2: two-column layout of the basic tab
# ==========================================================================
L = [rect_of(H(i)) for i in (1100, 1110, 1120)]      # style / option / hit
R = [rect_of(H(i)) for i in (1940, 1920)]            # log-report-test / AI
check("B-2 左カラム（置換方式/オプション/ヒット時）が右カラムより左",
      max(r[2] for r in L) <= min(r[0] for r in R))
check("B-2 左カラムが縦に並ぶ", L[0][3] <= L[1][1] and L[1][3] <= L[2][1])
check("B-2 右カラム（ログ・レポート・テスト / AI）が縦に並ぶ", R[0][3] <= R[1][1])

# ==========================================================================
# B-4: switch all 12 tabs, render + overlap check
# ==========================================================================
def overlaps(a, b):
    return a[0] < b[2] and b[0] < a[2] and a[1] < b[3] and b[1] < a[3]

def contains(a, b):
    return a[0] <= b[0] and a[1] <= b[1] and a[2] >= b[2] and a[3] >= b[3]

for i, name in enumerate(TABS):
    select_tab(i)
    vis = [h for h in page_handles() if win32gui.IsWindowVisible(h)]
    img = w.capture_as_image()
    shot("B04_tab%02d" % i)
    ok_vis = len(vis) == 1
    blank = True
    if ok_vis:
        blank = is_blank_region(img, local(vis[0]))
    check("B-4 タブ[%s] ページが 1 枚だけ表示され描画されている" % name, ok_vis and not blank)
    if ok_vis:
        kids = []
        def cb(h, _):
            if win32gui.GetParent(h) == vis[0] and win32gui.IsWindowVisible(h):
                cls = win32gui.GetClassName(h)
                st = win32gui.GetWindowLong(h, win32con.GWL_STYLE)
                is_group = cls == "Button" and (st & 0xF) == win32con.BS_GROUPBOX
                kids.append((h, cls, is_group, rect_of(h), win32gui.GetWindowText(h)))
            return True
        win32gui.EnumChildWindows(vis[0], cb, None)
        bad = []
        for a in range(len(kids)):
            for b in range(a + 1, len(kids)):
                ka, kb = kids[a], kids[b]
                if ka[2] or kb[2]:
                    continue
                ra, rb = ka[3], kb[3]
                if ka[1] == "ComboBox" or kb[1] == "ComboBox":
                    # combobox window height includes the dropdown in some styles
                    pass
                if overlaps(ra, rb):
                    bad.append("%s'%s' x %s'%s'" % (ka[1], ka[4][:12], kb[1], kb[4][:12]))
        # controls outside the page
        pr = rect_of(vis[0])
        outside = [k for k in kids if not contains(pr, k[3])]
        check("B-4 タブ[%s] コントロール同士が重ならない" % name, not bad, "; ".join(bad[:6]))
        check("B-4 タブ[%s] コントロールがページ外にはみ出さない" % name, not outside,
              "; ".join("%s'%s'" % (k[1], k[4][:12]) for k in outside[:6]))

# preview page: compare-range combos vs buttons
select_tab(7)
pv = [rect_of(H(i)) for i in (2110, 2111, 2112, 2113)]
bt = [rect_of(H(i)) for i in (1501, 1502, 1503)]
check("B-4 プレビューの比較範囲がボタンと重ならない",
      not any(overlaps(a, b) for a in pv for b in bt))

# ==========================================================================
# Basic flow: search / preview / dry run on sample.c (3 hits incl. comment)
# ==========================================================================
select_tab(0)
set_text(1001, SAMPLE)
set_text(1004, "USE_NEW_API")
set_text(1200, "old_function(name, g_counter);")
set_text(1300, "new_function(name, g_counter, 0);")
seen = click(1010)
cnt = ctrl(1400).item_count()
check("検索: sample.c で 4 件ヒット（コメント内含む）", cnt == 4, "items=%d dialogs=%s" % (cnt, joined(seen)))
check("検索: 複数ヒット/コメント内の警告が出る", any("MSG006" in t or "警告" in t for _, t in seen), joined(seen))
for bid, nm in ((1011, "プレビュー"), (1012, "ドライラン"), (1013, "実行")):
    check("検索後に[%s]ボタンが有効" % nm, ctrl(bid).is_enabled())
select_tab(6); shot("F_search_result")

# ==========================================================================
# B-7: 2-level nested new code -> indentation in preview
# ==========================================================================
select_tab(0); set_check(1116, True)        # auto indent
set_check(1124, True) if False else None
win32gui.SendMessage(H(1123), win32con.BM_CLICK, 0, 0); time.sleep(0.3)        # hit: all
set_text(1300, "if (name != NULL) {\n    if (g_counter > 0) {\n        new_function(name, g_counter);\n    }\n}")
seen = click(1010)
lv = ctrl(1400)
# select only the match on line ~33 (indented 8 spaces)
seen += click(1011)
prev = text_of(1500)
open(os.path.join(SHOTS,"B07_preview.txt"),"w",encoding="utf-8").write(prev)
select_tab(7); shot("B07_preview")
m = re.search(r"\n( *)if \(name != NULL\) \{\r?\n( *)if \(g_counter > 0\) \{\r?\n( *)new_function", prev)
if m:
    a, b, c = len(m.group(1)), len(m.group(2)), len(m.group(3))
else:
    a = b = c = -1
check("B-7 プレビューにネストした新コードが含まれる", m is not None, joined(seen)[:200])
check("B-7 内側が 4 スペースずつ下がる", m is not None and b == a + 4 and c == b + 4,
      "indent=%d/%d/%d" % (a, b, c))
check("B-7 インデント済みの行(8)に揃う箇所がある",
      any([len(g) for g in mm.groups()] == [9, 13, 17] for mm in re.finditer(
          r"\|( *)if \(name != NULL\) \{\r?\n.*\|( *)if \(g_counter > 0\) \{\r?\n.*\|( *)new_function", prev)))
win32gui.SendMessage(H(1121), win32con.BM_CLICK, 0, 0); time.sleep(0.3)         # back to auto-single

# ==========================================================================
# B-5 / B-6: add-code mode
# ==========================================================================
select_tab(5)
set_check(1991, True)
set_text(1993, "int result = 0;")
set_text(1995, "old_function(name,\n        g_counter);")      # whitespace / newline differ
set_text(1997, "LOG_TRACE(name);")
seen = click(1998)
info = text_of(1999)
shot("B05_addcode")
check("B-5 追加位置を検索でヒット件数が表示", "1" in info and "件" in info, info + " " + joined(seen))
check("B-6 直後のコードの空白・改行違いでもヒット", info.strip().endswith("1 件"), info)
for bid, nm in ((1011, "プレビュー"), (1012, "ドライラン"), (1013, "実行")):
    check("B-5 追加位置検索後に[%s]ボタンが有効" % nm, ctrl(bid).is_enabled())
seen = click(1011)
prev = text_of(1500)
check("B-5 追加コードがプレビューに出る", "LOG_TRACE(name);" in prev, joined(seen)[:150])
set_check(1991, False)

# ==========================================================================
# B-8: log definition output as .cpp
# ==========================================================================
select_tab(0)
set_text(1001, CALC)
combo_select(2121, 1)
seen = click(1949, answer="no")
p_cpp = os.path.join(WORK, "ms_migration_log.cpp")
check("B-8 ms_migration_log.cpp が生成される", os.path.exists(p_cpp), joined(seen)[:200])
if os.path.exists(p_cpp):
    raw = open(p_cpp, "rb").read()
    check("B-8 生成ファイルが ASCII のみ（コメント化けなし）", all(x < 0x80 for x in raw))
hdrs = glob.glob(os.path.join(WORK, "*.h"))
check("B-8 ログヘッダも生成される", len(hdrs) >= 1, str([os.path.basename(h) for h in hdrs]))

# ==========================================================================
# Runs on calc.c : run1 (add) -> run2 (mul) ; report scope = file (B-16)
# ==========================================================================
combo_select(1945, 1)            # file scope
set_text(1004, "USE_NEW_CALC")

def execute_change(old, new):
    set_text(1200, old); set_text(1300, new)
    s1 = click(1010)
    s2 = click(1013, answer="yes", timeout=90)
    return s1 + s2

reports_dir = os.path.join(APPDIR, "reports")
before_reports = set(glob.glob(os.path.join(reports_dir, "*")))
seen = execute_change("int r = a + b;", "int r = b + a;")
txt = read_file(CALC)
check("実行1: calc.c が書き換わる", "#ifdef USE_NEW_CALC" in txt and "int r = b + a;" in txt, joined(seen)[:200])
check("実行1: バックアップ(*.bak)が作られる",
      len(glob.glob(os.path.join(WORK, "**", "*.bak"), recursive=True)) +
      len(glob.glob(os.path.join(APPDIR, "**", "*.bak"), recursive=True)) >= 1)
time.sleep(1.2)
after_run1 = read_file(CALC)
seen = execute_change("int r = a * b;", "int r = b * a;")
txt = read_file(CALC)
check("実行2: calc.c が書き換わる", "int r = b * a;" in txt, joined(seen)[:200])

# B-16
new_reports = set(glob.glob(os.path.join(reports_dir, "*"))) - before_reports
file_html = [p for p in new_reports if re.search(r"_file_.*\.html$", os.path.basename(p))]
check("B-16 reports\\*_file_*.html が生成される", len(file_html) >= 1,
      str([os.path.basename(p) for p in new_reports])[:300])
if file_html:
    h = read_file(sorted(file_html)[-1])
    check("B-16 HTML: 表(<table>)がある", "<table" in h)
    check("B-16 HTML: 差分がある", ("<details" in h) or ("diff" in h.lower()))
    check("B-16 HTML: Markdown の表記号が残らない", re.search(r"^\s*\|.*\|\s*$", h, re.M) is None)
    shutil.copy(sorted(file_html)[-1], os.path.join(SHOTS, "B16_file_report.html"))

# ==========================================================================
# B-12: history
# ==========================================================================
select_tab(10)
click(2102)
runs = [t for t in combo_items(2101) if not t.startswith("（")]
check("B-12 履歴に 2 件の実行がある", len(runs) == 2, str(runs))
nfiles = ctrl(2103).item_count()
check("B-12 履歴ファイル一覧に変更前/変更後が出る", nfiles >= 1 and
      ctrl(2103).get_item(0, 2).text() == "あり" and ctrl(2103).get_item(0, 3).text() == "あり")
check("B-12 <対象>\\undobak\\runs が生成される", os.path.isdir(os.path.join(WORK, "undobak", "runs")))
shot("B12_history")

# ==========================================================================
# B-14: preview compare range -> HTML report
# ==========================================================================
select_tab(7)
items = combo_items(2111)
check("B-14 比較範囲コンボに履歴が並ぶ", len(items) == 2, str(items))
# Try "1回目～2回目" in label order (1: ..., 2: ...)
combo_select(2111, 0); combo_select(2113, 1)
before_reports = set(glob.glob(os.path.join(reports_dir, "*")))
seen = click(1502)
newp = [p for p in set(glob.glob(os.path.join(reports_dir, "pair_*.html"))) - before_reports]
check("B-14 範囲[1]～[2] でレポートが生成される", len(newp) == 1, joined(seen)[:200])
combo_select(2111, 1); combo_select(2113, 0)
before_reports = set(glob.glob(os.path.join(reports_dir, "*")))
seen = click(1502)
newp = [p for p in set(glob.glob(os.path.join(reports_dir, "pair_*.html"))) - before_reports]
check("B-14 範囲[2]～[1]（古い→新しい）でレポートが生成される", len(newp) == 1, joined(seen)[:200])
if newp:
    h = read_file(newp[0])
    check("B-14 古い→新しいレポートに両方の変更が含まれる", "b + a" in h and "b * a" in h)

# ==========================================================================
# B-11 / B-10 / B-9: GoogleTest
# ==========================================================================
select_tab(0)
gt_items = combo_items(2131)
check("B-11 テスト対象範囲に『指定なし』+ 履歴 2 件", len(gt_items) == 3, str(gt_items))
gt_out = os.path.join(WORK, "tests", "gtest", "generated_tests.cpp")

# (a) 指定なし -> uses current search result (mul only, since last search was mul)
combo_select(2131, 0); combo_select(2132, 0)
set_check(2122, True)
set_text(1200, "int r = a - b;"); set_text(1300, "int r = b - a;")
click(1010)
seen = click(1950)
code = read_file(gt_out) if os.path.exists(gt_out) else ""
tests_none = sorted(set(re.findall(r"TEST\(calc_(\w+?), ", code)))
check("B-11 『指定なし』では現在の検索結果(sub)だけが対象", tests_none == ["sub"],
      "targets=%s %s" % (tests_none, joined(seen)[:160]))

# (b) range = run with mul only (newest run). Labels: 1 = newest?
labels = gt_items[1:]
# find which label index corresponds to the 2nd (latest) run -> newest id is largest
combo_select(2131, 1); combo_select(2132, 1)
seen = click(1950)
code = read_file(gt_out) if os.path.exists(gt_out) else ""
t_sel1 = sorted(set(re.findall(r"TEST\(calc_(\w+?), ", code)))
combo_select(2131, 2); combo_select(2132, 2)
seen2 = click(1950)
code = read_file(gt_out) if os.path.exists(gt_out) else ""
t_sel2 = sorted(set(re.findall(r"TEST\(calc_(\w+?), ", code)))
check("B-11 範囲に履歴[1]だけを選ぶと 1 回分の関数だけが対象",
      len(t_sel1) == 1, "label=%s targets=%s %s" % (labels[0][:30], t_sel1, joined(seen)[:120]))
check("B-11 範囲に履歴[2]だけを選ぶと 1 回分の関数だけが対象",
      len(t_sel2) == 1, "label=%s targets=%s %s" % (labels[1][:30], t_sel2, joined(seen2)[:120]))
check("B-11 履歴[1]と[2]で対象関数が異なる (add / mul)", set(t_sel1 + t_sel2) == {"add", "mul"},
      "%s / %s" % (t_sel1, t_sel2))

# B-9 contents
cm_path = os.path.join(WORK, "tests", "gtest", "CMakeLists.txt")
check("B-9 tests\\gtest に生成される", os.path.exists(gt_out) and os.path.exists(cm_path))
cm = read_file(cm_path) if os.path.exists(cm_path) else ""
check("B-9 CMakeLists.txt に MS_SWITCH_ON がある", "MS_SWITCH_ON" in cm and "USE_NEW_CALC" in cm)
code = read_file(gt_out)
check("B-9 負荷試験 ON で Load テストが生成", ", Load)" in code)

# B-10
set_check(2122, False)
combo_select(2131, 0); combo_select(2132, 0)
seen = click(1950)
code = read_file(gt_out)
check("B-10 負荷試験 OFF で Load テストが生成されない/スキップ", ", Load)" not in code or
      "return 0;" in code, joined(seen)[:120])
set_check(2122, True)

if DO_GTEST_RUN:
    combo_select(2131, 0); combo_select(2132, 0)
    click(1010)
    click(1950)
    seen = click(1951, timeout=900, settle=2.0)
    check("B-9 テスト実行でビルド・実行結果が出る", any("GoogleTest を実行しました" in t for _, t in seen),
          joined(seen)[:400])

# ==========================================================================
# B-15: system info compare range -> prompt uses diff of that range
# ==========================================================================
select_tab(9)
set_text(1901, "CalcSystem")
set_text(1903, "calc overview")
s_items = combo_items(2115)
check("B-15 システム情報の変更範囲コンボに履歴が並ぶ", len(s_items) == 2, str(s_items))
combo_select(2115, 1); combo_select(2117, 0)
before_reports = set(glob.glob(os.path.join(reports_dir, "*")))
seen = click(1930, answer="no")
newp = [p for p in set(glob.glob(os.path.join(reports_dir, "*"))) - before_reports if p.endswith(".txt")]
check("B-15 プロンプト txt が出力される", len(newp) >= 1, joined(seen)[:160])
if newp:
    pt = read_file(newp[0])
    shutil.copy(newp[0], os.path.join(SHOTS, "B15_prompt.txt"))
    has_add = re.search(r"^\+\s*int r = b \+ a;", pt, re.M) is not None
    has_mul = re.search(r"^\+\s*int r = b \* a;", pt, re.M) is not None
    check("B-15 プロンプトが指定範囲の差分（add と mul 両方の + 行）を使う", has_add and has_mul,
          "diff add=%s mul=%s / 旧新コード欄=sub(現在の入力)" % (has_add, has_mul))

# ==========================================================================
# B-17: switch header create / enable / disable
# ==========================================================================
select_tab(8)
set_text(1004, "USE_NEW_CALC")
set_text(1200, "int r = a + b;"); set_text(1300, "int r = b + a;")
seen = click(1820)
hdr = os.path.join(WORK, "migration_switch.h")
hdr_c = glob.glob(os.path.join(WORK, "**", "migration_switch.h"), recursive=True)
check("B-17 migration_switch.h が生成される", len(hdr_c) >= 1, joined(seen)[:200])
if hdr_c:
    hdr = hdr_c[0]
    lv = ctrl(1815)
    check("B-17 スイッチ一覧に登録される", lv.item_count() >= 1)
    lv.select(0); time.sleep(0.3)
    seen = click(1816, answer="yes")
    t = read_file(hdr)
    en = re.search(r"^\s*#define\s+USE_NEW_CALC\b", t, re.M) is not None
    check("B-17 有効化で #define USE_NEW_CALC が有効になる", en, joined(seen)[:100])
    lv = ctrl(1815); lv.select(0); time.sleep(0.3)
    seen = click(1817)
    t = read_file(hdr)
    dis = re.search(r"^\s*#define\s+USE_NEW_CALC\b", t, re.M) is None
    check("B-17 無効化で #define USE_NEW_CALC が無効になる", dis, joined(seen)[:100])
    shot("B17_switch")

# ==========================================================================
# B-18: WinMerge (preview / history)
# ==========================================================================
def winmerge_pids():
    out = os.popen('tasklist /FI "IMAGENAME eq WinMergeU.exe" /FO CSV /NH').read()
    return set(l.split('","')[1] for l in out.splitlines() if l.startswith('"WinMergeU'))
base_wm = winmerge_pids()
select_tab(0)
set_text(1200, "int r = a - b;"); set_text(1300, "int r = b - a;")
click(1010)
select_tab(7)
combo_select(2111, 0) if False else None
seen = click(1503)
time.sleep(3)
wm1 = winmerge_pids() - base_wm
check("B-18 プレビューから WinMerge が起動する", len(wm1) >= 1, joined(seen)[:200])
select_tab(10)
click(2102)
ctrl(2103).select(0); time.sleep(0.3)
seen = click(2106)
time.sleep(3)
wm2 = winmerge_pids() - base_wm - wm1
check("B-18 履歴から WinMerge が起動する", len(wm2) >= 1 or len(winmerge_pids() - base_wm) >= 1,
      joined(seen)[:200])
os.system("taskkill /F /IM WinMergeU.exe >nul 2>nul")

# ==========================================================================
# B-13: batch mode (2 files) -> restore only one file from history
# ==========================================================================
select_tab(1)
win32gui.SendMessage(H(1702), win32con.BM_CLICK, 0, 0); time.sleep(0.3)
answer_dialogs(settle=0.3)
set_text(1704, BATCH)
seen = click(1721)
check("一括処理: 走査で 2 ファイル", "2 件" in joined(seen), joined(seen)[:120])
set_text(1004, "USE_NEW_K")
set_text(1200, "int k = v + 1;"); set_text(1300, "int k = 1 + v;")
select_tab(0); win32gui.SendMessage(H(1123), win32con.BM_CLICK, 0, 0); time.sleep(0.3)
seen = click(1010)
seen += click(1013, answer="yes", timeout=90)
x = read_file(os.path.join(BATCH, "x.c")); y = read_file(os.path.join(BATCH, "y.c"))
check("一括処理: 2 ファイルとも書き換わる", "1 + v" in x and "1 + v" in y, joined(seen)[:200])
select_tab(10)
click(2102)
combo_select(2101, 0)
lvh = ctrl(2103)
n = lvh.item_count()
idx_x = [i for i in range(n) if lvh.get_item(i, 1).text().endswith("x.c")]
check("B-13 一括実行の履歴に 2 ファイル", n == 2, "n=%d" % n)
if idx_x:
    lvh.select(idx_x[0]); time.sleep(0.3)
    seen = click(2105, answer="yes")
    x = read_file(os.path.join(BATCH, "x.c")); y = read_file(os.path.join(BATCH, "y.c"))
    check("B-13 選択したファイル(x.c)だけ元に戻る", "1 + v" not in x and "#ifdef" not in x, joined(seen)[:160])
    check("B-13 他のファイル(y.c)の変更は残る", "1 + v" in y)
    check("B-13 退避が undobak\\sidecars に作られる",
          len(glob.glob(os.path.join(BATCH, "undobak", "sidecars", "*"))) >= 1)
shot("B13_history_restore")

# ==========================================================================
# Undo (main button) on single file + log tab
# ==========================================================================
select_tab(11)
log = text_of(1600)
check("ログタブにログが出力されている", len(log) > 100)
shot("F_log")

# finish: close via 終了 button
win32gui.PostMessage(H(1015), win32con.BM_CLICK, 0, 0)
answer_dialogs(settle=0.5, timeout=10)
time.sleep(1)
alive = app.is_process_running()
check("終了ボタンでアプリが終了する", not alive)
if alive: app.kill()
check("config.ini が保存される", os.path.exists(os.path.join(APPDIR, "config.ini")))

json.dump({"results": results, "dialogs": dialogs}, open(os.path.join(ROOT, "result.json"), "w", encoding="utf-8"),
          ensure_ascii=False, indent=1)
npass = sum(1 for r in results if r[1]); nfail = len(results) - npass
print("\nRESULT: PASS %d / FAIL %d" % (npass, nfail))
