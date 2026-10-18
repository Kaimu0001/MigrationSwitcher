// -----------------------------------------------------------------------------
// このファイルは CopyMigration Switcher の「入力サンプル」です。
// ツールで検索・置換を試すためのデータであり、ビルド対象ではありません。
// 誤ってプロジェクトへ追加してもリンクエラーにならないよう、
// MS_COMPILE_SAMPLES が定義されているときだけコンパイルされます。
//   複数ヒット/コメント警告の確認用サンプル
// -----------------------------------------------------------------------------
#ifdef MS_COMPILE_SAMPLES
/* =============================================================================
 * sample.c - Migration Switcher 動作確認用サンプル
 * 同一コードが複数箇所に存在する状態（6.6 複数ヒット処理の確認用）
 * ============================================================================= */
#include <stdio.h>
#include <string.h>

static int g_counter = 0;

/* 呼び出し側 1（インデントなし） */
int use_old_api(const char* name)
{
    int result = 0;
    old_function(name, g_counter);
    return result;
}

/* 呼び出し側 2（インデントあり: 6.8 インデント自動調整の確認用） */
int use_old_api_wrapped(const char* name)
{
    if (name == NULL) {
        return -1;
    }
    else {
        old_function(name, g_counter);
    }
    return 0;
}

/* 呼び出し側 3（コメント内に同一文字列: 16章 コメント内ヒット検出の確認用） */
int use_old_api_third(const char* name)
{
    /* 旧実装では old_function(name, g_counter); を呼んでいた */
    int result = old_function(name, g_counter);
    return result;
}

int main(void)
{
    printf("%d\n", use_old_api("a"));
    printf("%d\n", use_old_api_wrapped("b"));
    printf("%d\n", use_old_api_third("c"));
    return 0;
}
#endif
