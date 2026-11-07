#pragma once

// ---------------------------------------------------------------------------
//  更新详情对话框
//
//  只负责展示：版本号、发布日期、Release 正文（Markdown），以及一个跳转发布页
//  的按钮。不做下载 —— 见 UpdateChecker 顶部关于「不自动替换自身」的说明。
// ---------------------------------------------------------------------------

#include "app/UpdateChecker.h"

#include <QDialog>

namespace dq {

class UpdateDialog : public QDialog {
    Q_OBJECT

public:
    explicit UpdateDialog(const ReleaseInfo &release, QWidget *parent = nullptr);
};

} // namespace dq
