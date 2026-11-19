#pragma once

namespace demo::hooks
{
// 串起完整安装流程：读 cfg → 定位特征码 → 全有或全无校验 → 写补丁。
// 任何一步失败都只记日志并返回，绝不让异常越过 DllMain 边界。
void Install();
} // namespace demo::hooks
