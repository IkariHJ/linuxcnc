#include "ve_var.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <map>
#include <algorithm>

#include "rcs_print.hh"

// ============================================================
// 全局变量表（变量名小写 → 条目）
// ============================================================
static std::map<std::string, VE_VAR_ENTRY> veVarTable;

// ============================================================
// 字符串转换
// ============================================================
static int veTypeFromStr(const char *s)
{
    if (!s || !*s) return VE_TYPE_DOUBLE;
    if (strcasecmp(s, "BOOLEAN") == 0) return VE_TYPE_BOOL;
    if (strcasecmp(s, "SGN08") == 0 || strcasecmp(s, "UNS08") == 0 ||
        strcasecmp(s, "SGN16") == 0 || strcasecmp(s, "UNS16") == 0 ||
        strcasecmp(s, "SGN32") == 0 || strcasecmp(s, "UNS32") == 0)
        return VE_TYPE_INT;
    if (strcasecmp(s, "REAL32") == 0 || strcasecmp(s, "REAL64") == 0)
        return VE_TYPE_DOUBLE;
    return VE_TYPE_DOUBLE;
}

static int veAccessFromStr(const char *s)
{
    if (!s || !*s) return VE_ACCESS_READ_WRITE;
    if (strcasecmp(s, "READ_ONLY") == 0)  return VE_ACCESS_READ_ONLY;
    if (strcasecmp(s, "WRITE_ONLY") == 0) return VE_ACCESS_WRITE_ONLY;
    return VE_ACCESS_READ_WRITE;
}

static int veScopeFromStr(const char *s)
{
    if (!s || !*s) return VE_SCOPE_GLOBAL;
    if (strcasecmp(s, "CHANNEL") == 0) return VE_SCOPE_CHANNEL;
    return VE_SCOPE_GLOBAL;
}

static int veBoolFromStr(const char *s)
{
    if (!s || !*s) return 0;
    if (strcasecmp(s, "TRUE") == 0 || strcmp(s, "1") == 0) return 1;
    return 0;
}

static int veTypeElemSize(int type)
{
    switch (type) {
    case VE_TYPE_BOOL:   return 1;
    case VE_TYPE_INT:    return 4;
    case VE_TYPE_DOUBLE: return 8;
    default:             return 8;
    }
}

// ============================================================
// INI 加载
// ============================================================
int emcVeVarLoadFromIni(const char *iniPath)
{
    if (!iniPath || !*iniPath) return -1;

    FILE *fp = fopen(iniPath, "r");
    if (!fp) {
        rcs_print_error("VE: cannot open %s\n", iniPath);
        return -1;
    }

    char line[1024];
    char section[256] = {0};
    bool inVeSection = false;
    VE_VAR_ENTRY e;
    memset(&e, 0, sizeof(e));

    int loaded = 0;
    std::map<int, std::string> indexUsed;

    #define VE_COMMIT() do { \
        if (inVeSection && e.name[0]) { \
            int sz = (e.arraySize > 0) ? e.arraySize : 1; \
            int expectElem = veTypeElemSize(e.type); \
            int expectTotal = expectElem * sz; \
            if (e.byteSize != expectTotal) { \
                rcs_print_error("VE WARN: [%s] size=%d mismatch, expect=%d\n", \
                    e.name, e.byteSize, expectTotal); \
            } \
            if (e.index >= 0) { \
                auto dup = indexUsed.find(e.index); \
                if (dup != indexUsed.end()) { \
                    rcs_print_error("VE WARN: [%s] index=%d duplicate [%s]\n", \
                        e.name, e.index, dup->second.c_str()); \
                } else { \
                    indexUsed[e.index] = e.name; \
                } \
            } \
            e.data = (double *)calloc(sz, sizeof(double)); \
            if (e.data) { \
                std::string key(e.name); \
                std::transform(key.begin(), key.end(), key.begin(), ::tolower); \
                veVarTable[key] = e; \
                loaded++; \
                rcs_print("VE: [%s] idx=%d type=%d acc=%d sync=%d arr=%d\n", \
                    e.name, e.index, e.type, e.access, e.sync, sz); \
            } else { \
                rcs_print_error("VE: malloc failed for %s\n", e.name); \
            } \
        } \
    } while(0)

    while (fgets(line, sizeof(line), fp)) {
        char *p = line;
        while (*p == ' ' || *p == '\t' || *p == '\r') p++;
        if (*p == ';' || *p == '#' || *p == '\n' || *p == '\0') continue;

        if (*p == '[') {
            VE_COMMIT();
            memset(&e, 0, sizeof(e));

            char *end = strchr(p, ']');
            if (!end) continue;
            *end = '\0';
            strncpy(section, p + 1, sizeof(section) - 1);

            if (strcasecmp(section, "total") == 0) {
                inVeSection = false;
                continue;
            }
            if (strncasecmp(section, "VE.", 3) == 0) {
                inVeSection = true;
                strncpy(e.name, section + 3, sizeof(e.name) - 1);
                e.scope = VE_SCOPE_GLOBAL;
                e.access = VE_ACCESS_READ_WRITE;
                e.type = VE_TYPE_DOUBLE;
                e.index = -1;
                e.sync = 0;
                e.arraySize = 0;
                e.byteSize = 0;
                e.ready = 0;
            } else {
                inVeSection = false;
            }
            continue;
        }

        if (!inVeSection) continue;

        char *eq = strchr(p, '=');
        if (!eq) continue;
        *eq = '\0';
        char *k = p;

        // ★ 去掉 key 尾部的空格/tab
        char *ke = eq - 1;
        while (ke > k && (*ke == ' ' || *ke == '\t')) {
            *ke = '\0'; ke--;
        }

        char *v = eq + 1;
        while (*v == ' ' || *v == '\t') v++;

        char *ve = v + strlen(v) - 1;
        while (ve > v && (*ve == '\n' || *ve == '\r' || *ve == ' ' || *ve == '\t')) {
            *ve = '\0'; ve--;
        }

        if      (strcasecmp(k, "index") == 0)              e.index = atoi(v);
        else if (strcasecmp(k, "type") == 0)               e.type = veTypeFromStr(v);
        else if (strcasecmp(k, "access") == 0)             e.access = veAccessFromStr(v);
        else if (strcasecmp(k, "scope") == 0)              e.scope = veScopeFromStr(v);
        else if (strcasecmp(k, "synchronization") == 0)    e.sync = veBoolFromStr(v);
        else if (strcasecmp(k, "arraySize") == 0)          e.arraySize = atoi(v);
        else if (strcasecmp(k, "size") == 0)               e.byteSize = atoi(v);
    }

    VE_COMMIT();
    #undef VE_COMMIT

    fclose(fp);
    rcs_print("VE: loaded %d variables\n", loaded);
    return loaded;
}

// ============================================================
// 写 VE 变量
// ============================================================
int emcVeVarSet(const char *varName, int arrIndex, double value)
{
    if (!varName || !*varName) return -1;

    std::string key(varName);
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);

    auto it = veVarTable.find(key);
    if (it == veVarTable.end()) return -2;     // 变量不存在

    if (it->second.access == VE_ACCESS_READ_ONLY) return -3;  // 只读

    int sz = (it->second.arraySize > 0) ? it->second.arraySize : 1;
    if (arrIndex < 0 || arrIndex >= sz) return -4;   // 越界

    it->second.data[arrIndex] = value;

    // 同步模式：标记就绪
    if (it->second.sync) {
        it->second.ready = 1;
    }
    return 0;
}

// ============================================================
// 读 VE 变量
// ============================================================
double emcVeVarGet(const char *varName, int index, bool *ok)
{
    if (ok) *ok = false;
    if (!varName || !*varName) return 0.0;

    std::string key(varName);
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);

    auto it = veVarTable.find(key);
    if (it == veVarTable.end()) return 0.0;

    if (it->second.access == VE_ACCESS_WRITE_ONLY) return 0.0;  // 只写

    int sz = (it->second.arraySize > 0) ? it->second.arraySize : 1;
    if (index < 0 || index >= sz) return 0.0;

    double v = it->second.data[index];
    if (ok) *ok = true;
    return v;
}

// ============================================================
// 释放内存
// ============================================================
void emcVeVarShutdown(void)
{
    for (auto &kv : veVarTable) {
        if (kv.second.data) {
            free(kv.second.data);
            kv.second.data = NULL;
        }
    }
    veVarTable.clear();
}
