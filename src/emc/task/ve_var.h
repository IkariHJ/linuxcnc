#ifndef VE_VAR_H
#define VE_VAR_H

// ============================================================
// VE 变量（Variable Exchange）— task 与 PLC 通用变量交换区
// 数据在 task 本地，PLC 通过 NML 命令写入
// ============================================================

// 数据类型
enum EMC_VE_TYPE {
    VE_TYPE_BOOL = 0,
    VE_TYPE_INT,
    VE_TYPE_DOUBLE
};

// 访问权限
enum EMC_VE_ACCESS {
    VE_ACCESS_READ_ONLY = 0,    // PLC写, task读
    VE_ACCESS_WRITE_ONLY,        // task写, PLC读
    VE_ACCESS_READ_WRITE        // 双向
};

// 作用域
enum EMC_VE_SCOPE {
    VE_SCOPE_GLOBAL = 0,
    VE_SCOPE_CHANNEL
};

// 单个 VE 变量项
struct VE_VAR_ENTRY {
    char name[64];
    int  index;
    int  type;
    int  access;
    int  scope;
    int  sync;                  // 0=异步, 1=同步
    int  arraySize;
    int  byteSize;
    int  ready;                 // 同步模式：PLC写后置1, task读清0
    double *data;
};

// ===== 函数 API =====

// 从 INI 文件加载 VE 变量定义，返回加载数量
int emcVeVarLoadFromIni(const char *iniPath);

// 写 VE 变量（PLC/Qt HMI 通过 NML 命令调用）
// 返回 0 成功, 非0 失败（变量不存在/权限拒绝/越界）
int emcVeVarSet(const char *varName, int arrIndex, double value);

// 读 VE 变量（task/G代码 调用）
// ok 非空时填是否成功
double emcVeVarGet(const char *varName, int index, bool *ok = 0);

// 释放所有 VE 变量内存（task 退出时调用）
void emcVeVarShutdown(void);




// ===== VE 阻塞执行状态 =====
// VE 阻塞执行状态
enum VE_WAIT_STATE {
    VE_WAIT_NONE = 0,     // 正常，无等待
    VE_WAIT_PENDING,      // 已触发rewind，正在等motion空
    VE_WAIT_DONE          // motion已空，VE直接读真实值
};
extern VE_WAIT_STATE veWaitState;
extern bool veLineHasBlockingVE;  // 替代 veNanCount
extern bool veLineHasVE;
extern long veWaitOffset;



// 判断 VE 是否阻塞型（sync=1）
bool emcVeVarIsSync(const char *varName);


#endif // VE_VAR_H
