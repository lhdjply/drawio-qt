#pragma once

// ---------------------------------------------------------------------------
//  检查更新
//
//  只读一个来源：随 Release 上传的更新清单
//    https://github.com/lhdjply/drawio-qt/releases/latest/download/latest.json
//
//  为什么是它：
//   · 它是 Release 的一个普通资产，由 CDN 直出，不需要认证，也不占 API 配额；
//   · 它是 github.com 上的一个文件，第三方 GitHub 转发服务能原样代理，于是
//     不挂代理也能取到；
//   · 这个路径只在正式 Release 建好之后才存在，不会撞上「tag 已推、CI 还在
//     编译」的占位条目。
//
//  地址固定按 gh-proxy.org → ghfast.top → ghproxy.net 的顺序试，三个都失败就
//  报错。抖动对冲：到节点这条链路实测有部分连接会卡 2~5 秒（TCP 重传），单条
//  请求一旦撞上就要等到超时。因此首条请求每 700ms 还没结果就并行再补一条，
//  谁先成功用谁。正常情况（300~500ms）只发一条，不产生额外请求。
//
//  只做「查询 + 展示」两件事：不下载、不安装。安装包由用户自己到发布页取 ——
//  在缺少签名校验与包管理器的前提下让程序替换自己的二进制，风险远大于收益。
//
//  失败必须给出可读原因，不静默 —— 「没动静」最容易被理解成「没有更新」。
// ---------------------------------------------------------------------------

#include <QElapsedTimer>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVector>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

namespace dq {

// 一条 Release 里本功能用得到的字段
struct ReleaseInfo {
    QString version;     // 规范化后的版本号（去掉 tag 的 v 前缀与预发布后缀）
    QString tagName;     // 原始 tag，展示时保留作者的写法（如 v0.1.0）
    QString title;       // Release 标题，可能为空
    QString notes;       // 更新日志正文（清单里是 Markdown）
    QUrl    pageUrl;     // 发布页：浏览器打开后可看到安装包
    QString publishedAt; // ISO 8601 时间
};

class UpdateChecker : public QObject {
    Q_OBJECT

public:
    explicit UpdateChecker(QObject *parent = nullptr);
    ~UpdateChecker() override;

    // 内置的 GitHub 转发服务，按顺序尝试
    static QStringList builtinMirrors();

    // 镜像前缀，空列表表示直接访问清单地址（测试用；线上默认是三个内置镜像）
    void setMirrorPrefixes(const QStringList &prefixes);
    // 清单地址。默认是 releases/latest/download/latest.json；测试里指到本地服务。
    void setManifestUrl(const QUrl &url);

    // 同一时刻只允许一次检查（含重试与切镜像的间隔），重复调用直接忽略
    // （返回 false）。
    bool check();
    // 是否仍在进行中。重试等待期间 m_reply 为空，所以不能只看 m_reply。
    bool busy() const { return m_running; }

    // 比较版本号：只比数字段（0.10.0 > 0.9.9），忽略 v 前缀、预发布后缀与
    // 缺失的段（1.2 == 1.2.0）。返回 >0 表示 a 比 b 新。
    static int compareVersions(const QString &a, const QString &b);

signals:
    void upToDate(const QString &currentVersion);
    void updateAvailable(const ReleaseInfo &release);
    void failed(const QString &reason);

private:
    // 这一次要试的完整地址列表：镜像前缀 × 清单地址
    QVector<QUrl> candidates() const;
    // 本次最多发几条请求：地址表够长就每条一次，否则沿用对冲的 3 条
    int           attemptLimit() const;

    // 为当前地址发一条请求。同一时刻最多有 2 条在飞（原始 + 对冲）
    void sendAttempt();
    // 单条请求结束：成功则收下（并掐掉对冲），失败则按重试/换镜像推进
    void finish(QNetworkReply *reply);
    // 这一轮失败后的去向：再试 / 换镜像 / 报错
    void advanceAfterFailure(bool retryable);
    // 放弃当前所有在飞请求（已被采纳的结果会产生重复，也会留下无主连接）
    void cancelInflight();

    // 偶发抖动？4xx 属于确定性失败，不算
    bool isTransient(QNetworkReply *reply, bool timedOut) const;

    int     remainingBudgetMs() const;
    // 记下一条失败原因（去重），最终一起报给用户
    void    noteError(const QString &reason);
    QString failureMessage() const;

    // 解析成功并已发出结果时返回 true；内容坏了返回 false，由调用方换下一条地址
    bool parseManifest(const QByteArray &payload);
    void report(const ReleaseInfo &release);

    QUrl                   m_manifestUrl; // 更新清单（Release 资产）
    QStringList            m_mirrors;     // 镜像前缀，空表示直接访问
    QVector<QUrl>          m_urls;        // 这一次要试的地址
    int                    m_attempts = 0; // 已发出的请求数
    bool                   m_running = false;
    QElapsedTimer          m_clock;       // 本次检查的耗时，用于总预算
    QStringList            m_errors;      // 各地址的失败原因，最后一起报出
    QNetworkAccessManager *m_manager = nullptr;
    QList<QNetworkReply *> m_inflight;    // 在飞的请求（1~2 条）
    QTimer                *m_hedgeTimer = nullptr;
};

} // namespace dq
