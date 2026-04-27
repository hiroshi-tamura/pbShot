#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <memory>

class QNetworkAccessManager;
class QNetworkReply;
class QFile;

// OCR 用モデルのダウンロード/整合性チェック/パス管理。
// 全ファイルは exe 隣の models/ フォルダ（ポータブル）に閉じ込める。
class OcrModelManager : public QObject {
    Q_OBJECT
public:
    struct ModelFile {
        QString relativePath;   // models/ 配下の相対パス
        QStringList urls;       // ダウンロード元 (先頭から順に試す)
        qint64  expectedMin;    // 期待される最小サイズ (バイト)。破損検知用
    };

    explicit OcrModelManager(QObject* parent = nullptr);
    ~OcrModelManager() override;

    // 必要モデルが全て揃っているか
    bool isReady() const;

    // 不足ファイルだけ DL する。完了/失敗は signal で返す。
    // parent はモーダル進捗ダイアログのオーナー。nullptr 可。
    void ensureModelsAsync(QWidget* parent);

    // モデル削除（再 DL 用）
    void removeAll();

    // 各モデルのフルパス
    QString detModelPath() const;
    QString recModelPath() const;
    QString clsModelPath() const;
    QString jpDictPath()   const;

    // 全エントリ
    const QVector<ModelFile>& files() const { return m_files; }

signals:
    void ready();                      // 全モデル使用可能
    void failed(const QString& msg);   // DL/検証失敗

private slots:
    void onProgress(qint64 received, qint64 total);
    void onFinished();

private:
    void startNext();
    void startUrl();
    void cancelAll(const QString& msg);

    QString fullPath(const ModelFile& m) const;
    bool fileLooksValid(const ModelFile& m) const;

    QVector<ModelFile> m_files;
    QNetworkAccessManager* m_nam = nullptr;     // QObject 親 = this、Qt が回収
    QNetworkReply* m_reply = nullptr;           // deleteLater で個別管理
    std::unique_ptr<QFile> m_currentFile;       // .part への書き込み用
    int m_index = -1;
    int m_urlIndex = 0;

    class QProgressDialog* m_dlg = nullptr;     // parent ウィジェットが所有
};
