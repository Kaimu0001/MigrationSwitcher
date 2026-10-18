// =============================================================================
// resource.h
// コントロール ID 定義（設計書 5.2 コントロール一覧 / 17.2.1 追加UI に対応）
// =============================================================================
#pragma once

// ---- 共通 ----
#define IDC_LBL_FILE            1000
#define IDC_EDIT_TARGETFILE     1001
#define IDC_BTN_BROWSE          1002
#define IDC_LBL_SWITCH          1003
#define IDC_EDIT_SWITCHNAME     1004
#define IDC_LBL_HINT_SWITCH     1005
#define IDC_TAB_MAIN            1006
#define IDC_PROGRESS            1007

#define IDC_BTN_SEARCH          1010
#define IDC_BTN_PREVIEW         1011
#define IDC_BTN_DRYRUN          1012
#define IDC_BTN_EXECUTE         1013
#define IDC_BTN_UNDO            1014
#define IDC_BTN_EXIT            1015

// ---- 基本設定タブ ----
#define IDC_GRP_STYLE           1100
#define IDC_RAD_STYLE_A         1101
#define IDC_RAD_STYLE_B         1102
#define IDC_RAD_STYLE_C         1103
#define IDC_RAD_STYLE_D         1104
#define IDC_LBL_TEMPLATE        1105
#define IDC_EDIT_TEMPLATE       1106

#define IDC_GRP_OPTION          1110
#define IDC_CHK_BACKUP          1111
#define IDC_CHK_REPORT          1112
#define IDC_CHK_LOG             1113
#define IDC_CHK_OPENREPORT      1114
#define IDC_CHK_DRYRUNONLY      1115
#define IDC_CHK_AUTOINDENT      1116
#define IDC_CHK_WARNCOMMENT     1117
#define IDC_CHK_WARNPREPROC     1118

#define IDC_GRP_HIT             1120
#define IDC_RAD_HIT_AUTO        1121
#define IDC_RAD_HIT_FIRST       1122
#define IDC_RAD_HIT_ALL         1123
#define IDC_RAD_HIT_MANUAL      1124

// ---- 旧コードタブ ----
#define IDC_EDIT_OLDCODE        1200
#define IDC_LBL_OLDINFO         1201

// ---- 新コードタブ ----
#define IDC_EDIT_NEWCODE        1300
#define IDC_LBL_NEWINFO         1301

// ---- 検索結果タブ ----
#define IDC_LIST_MATCHES        1400
#define IDC_LBL_RESULTINFO      1401
#define IDC_BTN_SELECTALL       1402
#define IDC_BTN_CLELSEL         1403
#define IDC_BTN_CLEARSEL        1403

// ---- プレビュータブ ----
#define IDC_EDIT_PREVIEW        1500
#define IDC_CMB_PREVIEWMODE     1501
#define IDC_BTN_OPENHTML        1502

// ---- ログタブ ----
#define IDC_EDIT_LOG            1600
#define IDC_BTN_CLEARLOG        1601

// ---- 一括処理タブ（設計書 17.2.1 追加UI）----
#define IDC_GRP_TARGETKIND      1700
#define IDC_RAD_TARGET_FILE     1701
#define IDC_RAD_TARGET_FOLDER   1702
#define IDC_LBL_FOLDER          1703
#define IDC_EDIT_FOLDER         1704
#define IDC_BTN_FOLDERBROWSE    1705
#define IDC_GRP_EXTFILTER       1706
#define IDC_CHK_EXT_BASE        1710   // 1710～1715: .c .cpp .h .hpp .cs .java
#define IDC_CHK_SUBDIRS         1720
#define IDC_BTN_SCAN            1721
#define IDC_BTN_FILESELALL      1722
#define IDC_BTN_FILESELNONE     1723
#define IDC_LBL_SCANINFO        1724
#define IDC_LIST_FILES          1725

// ---- スイッチ管理タブ（スイッチ定義ヘッダの一元管理）----
#define IDC_LBL_SWHEADER        1800
#define IDC_EDIT_SWHEADER       1801
#define IDC_BTN_SWHEADERBROWSE  1802
#define IDC_BTN_SWHEADERLOAD    1803
#define IDC_GRP_SWAUTO          1804
#define IDC_CHK_MANAGESWHEADER  1805
#define IDC_CHK_ADDINCLUDE      1806
#define IDC_LBL_SWHEADERNAME    1807
#define IDC_EDIT_SWHEADERNAME   1808
#define IDC_LBL_SWHEADERDIR     1809
#define IDC_EDIT_SWHEADERDIR    1810
#define IDC_BTN_SWHEADERDIR     1811
#define IDC_LBL_SWMEMO          1812
#define IDC_EDIT_SWMEMO         1813
#define IDC_LBL_SWINFO          1814
#define IDC_LIST_SWITCHES       1815
#define IDC_BTN_SWENABLE        1816
#define IDC_BTN_SWDISABLE       1817
#define IDC_BTN_SWREMOVE        1818
#define IDC_BTN_SWOPEN          1819
#define IDC_BTN_SWCREATE        1820

// ---- システム情報タブ（仕様書生成の入力）----
#define IDC_LBL_SYSNAME         1900
#define IDC_EDIT_SYSNAME        1901
#define IDC_LBL_SYSOVERVIEW     1902
#define IDC_EDIT_SYSOVERVIEW    1903
#define IDC_LBL_SYSPURPOSE      1904
#define IDC_EDIT_SYSPURPOSE     1905
#define IDC_LBL_SYSMOD          1906
#define IDC_EDIT_SYSMOD         1907
#define IDC_LBL_SYSEXTRA        1908
#define IDC_EDIT_SYSEXTRA       1909
#define IDC_BTN_SPECCHANGE      1910
#define IDC_BTN_SPECINSPECT     1911
#define IDC_BTN_SPECBOTH        1912
#define IDC_BTN_SPECOPEN        1913
#define IDC_EDIT_SPECOUT        1914
#define IDC_LBL_SPECINFO        1915
#define IDC_CHK_AIOPENREPORT    1916

#define IDC_LBL_SPECOUTNOTE    1917
#define IDC_BTN_PROMPTCHANGE    1930
#define IDC_BTN_PROMPTINSPECT   1931
#define IDC_BTN_PROMPTBOTH      1932
#define IDC_BTN_MAKECHANGE      1933
#define IDC_BTN_MAKEINSPECT     1934
#define IDC_LBL_PASTENOTE       1935

// ---- AI 連携（基本設定タブ）----
#define IDC_GRP_AI              1920
#define IDC_LBL_AIKEY           1921
#define IDC_EDIT_AIKEY          1922
#define IDC_LBL_AIENDPOINT      1923
#define IDC_EDIT_AIENDPOINT     1924
#define IDC_LBL_AIMODEL         1925
#define IDC_EDIT_AIMODEL        1926
#define IDC_LBL_AITIMEOUT       1927
#define IDC_EDIT_AITIMEOUT      1928
#define IDC_BTN_AITEST          1929
#define IDC_LBL_AIPROVIDER      1936
#define IDC_CMB_AIPROVIDER      1937

// ---- 検索条件タブ（後続版: 類似一致 / 指定行範囲 / 関数単位 / 出力形式 / WinMerge）----
#define IDC_GRP_SIMILAR         1150
#define IDC_CHK_SIMILAR         1151
#define IDC_LBL_SIMTHRESHOLD    1152
#define IDC_EDIT_SIMTHRESHOLD   1153
#define IDC_GRP_LINERANGE       1154
#define IDC_CHK_LINERANGE       1155
#define IDC_LBL_LINEFROM        1156
#define IDC_EDIT_LINEFROM       1157
#define IDC_LBL_LINETO          1158
#define IDC_EDIT_LINETO         1159
#define IDC_GRP_FUNCSCOPE       1160
#define IDC_CHK_FUNCSCOPE       1161
#define IDC_LBL_FUNCNAME        1162
#define IDC_EDIT_FUNCNAME       1163
#define IDC_GRP_OUTPUT          1164
#define IDC_CHK_REPORTMD        1165
#define IDC_CHK_REPORTJSON      1166
#define IDC_LBL_ENCODING        1167
#define IDC_CMB_ENCODING        1168
#define IDC_GRP_WINMERGE        1169
#define IDC_LBL_WINMERGE        1170
#define IDC_EDIT_WINMERGE       1171
#define IDC_BTN_WINMERGEBROWSE  1172
#define IDC_GRP_UNDOOPT         1173
#define IDC_LBL_UNDOMAX         1174
#define IDC_EDIT_UNDOMAX        1175
#define IDC_LBL_SEARCHINFO      1176

// ---- 検索結果タブ（後続版: エクスポート）----
#define IDC_BTN_EXPORTCSV       1404
#define IDC_BTN_EXPORTMD        1405
#define IDC_BTN_EXPORTJSON      1406

// ---- プレビュータブ（後続版: WinMerge 連携）----
#define IDC_BTN_WINMERGE        1503

// ---- メニュー ----
#define IDM_FILE_OPEN           2001
#define IDM_FILE_EXIT           2002
#define IDM_HELP_ABOUT          2003

#define IDM_APP_MANIFEST        1
